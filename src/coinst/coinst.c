/* Copyright (c) Citrix Systems Inc.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, 
 * with or without modification, are permitted provided 
 * that the following conditions are met:
 * 
 * *   Redistributions of source code must retain the above 
 *     copyright notice, this list of conditions and the 
 *     following disclaimer.
 * *   Redistributions in binary form must reproduce the above 
 *     copyright notice, this list of conditions and the 
 *     following disclaimer in the documentation and/or other 
 *     materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND 
 * CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, 
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF 
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE 
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR 
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, 
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR 
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, 
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING 
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
 * SUCH DAMAGE.
 */

#define INITGUID

#include <windows.h>
#include <ws2def.h>
#include <ws2ipdef.h>
#include <iphlpapi.h>
#include <setupapi.h>
#include <devguid.h>
#include <stdio.h>
#include <stdlib.h>
#include <strsafe.h>
#include <malloc.h>

#include <tcpip.h>
#include <version.h>

__user_code;

#define MAXIMUM_BUFFER_SIZE 1024

#define SERVICES_KEY "SYSTEM\\CurrentControlSet\\Services"

#define PARAMETERS_KEY(_Driver)    \
        SERVICES_KEY ## "\\" ## #_Driver ## "\\Parameters"

#define ADDRESSES_KEY(_Driver)    \
        SERVICES_KEY ## "\\" ## #_Driver ## "\\Addresses"

#define ALIASES_KEY(_Driver)    \
        SERVICES_KEY ## "\\" ## #_Driver ## "\\Aliases"

#define CLASS_KEY "SYSTEM\\CurrentControlSet\\Control\\Class"

#define NSI_KEY "SYSTEM\\CurrentControlSet\\Control\\Nsi"

static VOID
#pragma prefast(suppress:6262) // Function uses '1036' bytes of stack: exceeds /analyze:stacksize'1024'
__Log(
    IN  const CHAR  *Format,
    IN  ...
    )
{
    TCHAR               Buffer[MAXIMUM_BUFFER_SIZE];
    va_list             Arguments;
    size_t              Length;
    SP_LOG_TOKEN        LogToken;
    DWORD               Category;
    DWORD               Flags;
    HRESULT             Result;

    va_start(Arguments, Format);
    Result = StringCchVPrintf(Buffer, MAXIMUM_BUFFER_SIZE, Format, Arguments);
    va_end(Arguments);

    if (Result != S_OK && Result != STRSAFE_E_INSUFFICIENT_BUFFER)
        return;

    Result = StringCchLength(Buffer, MAXIMUM_BUFFER_SIZE, &Length);
    if (Result != S_OK)
        return;

    LogToken = SetupGetThreadLogToken();
    Category = TXTLOG_VENDOR;
    Flags = TXTLOG_DETAILS;

    SetupWriteTextLog(LogToken, Category, Flags, Buffer);
    Length = __min(MAXIMUM_BUFFER_SIZE - 1, Length + 2);

    __analysis_assume(Length < MAXIMUM_BUFFER_SIZE);
    __analysis_assume(Length >= 2);
    Buffer[Length] = '\0';
    Buffer[Length - 1] = '\n';
    Buffer[Length - 2] = '\r';

    OutputDebugString(Buffer);
}

#define Log(_Format, ...) \
        __Log(__MODULE__ "|" __FUNCTION__ ": " _Format, __VA_ARGS__)

static PTCHAR
GetErrorMessage(
    IN  DWORD   Error
    )
{
    PTCHAR      Message;
    ULONG       Index;

    FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | 
                  FORMAT_MESSAGE_FROM_SYSTEM |
                  FORMAT_MESSAGE_IGNORE_INSERTS,
                  NULL,
                  Error,
                  MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                  (LPTSTR)&Message,
                  0,
                  NULL);

    for (Index = 0; Message[Index] != '\0'; Index++) {
        if (Message[Index] == '\r' || Message[Index] == '\n') {
            Message[Index] = '\0';
            break;
        }
    }

    return Message;
}

static const CHAR *
FunctionName(
    IN  DI_FUNCTION Function
    )
{
#define _NAME(_Function)        \
        case DIF_ ## _Function: \
            return #_Function;

    switch (Function) {
    _NAME(INSTALLDEVICE);
    _NAME(REMOVE);
    _NAME(SELECTDEVICE);
    _NAME(ASSIGNRESOURCES);
    _NAME(PROPERTIES);
    _NAME(FIRSTTIMESETUP);
    _NAME(FOUNDDEVICE);
    _NAME(SELECTCLASSDRIVERS);
    _NAME(VALIDATECLASSDRIVERS);
    _NAME(INSTALLCLASSDRIVERS);
    _NAME(CALCDISKSPACE);
    _NAME(DESTROYPRIVATEDATA);
    _NAME(VALIDATEDRIVER);
    _NAME(MOVEDEVICE);
    _NAME(DETECT);
    _NAME(INSTALLWIZARD);
    _NAME(DESTROYWIZARDDATA);
    _NAME(PROPERTYCHANGE);
    _NAME(ENABLECLASS);
    _NAME(DETECTVERIFY);
    _NAME(INSTALLDEVICEFILES);
    _NAME(ALLOW_INSTALL);
    _NAME(SELECTBESTCOMPATDRV);
    _NAME(REGISTERDEVICE);
    _NAME(NEWDEVICEWIZARD_PRESELECT);
    _NAME(NEWDEVICEWIZARD_SELECT);
    _NAME(NEWDEVICEWIZARD_PREANALYZE);
    _NAME(NEWDEVICEWIZARD_POSTANALYZE);
    _NAME(NEWDEVICEWIZARD_FINISHINSTALL);
    _NAME(INSTALLINTERFACES);
    _NAME(DETECTCANCEL);
    _NAME(REGISTER_COINSTALLERS);
    _NAME(ADDPROPERTYPAGE_ADVANCED);
    _NAME(ADDPROPERTYPAGE_BASIC);
    _NAME(TROUBLESHOOTER);
    _NAME(POWERMESSAGEWAKE);
    default:
        break;
    }

    return "UNKNOWN";

#undef  _NAME
}

static PTCHAR
GetProperty(
    IN  HDEVINFO            DeviceInfoSet,
    IN  PSP_DEVINFO_DATA    DeviceInfoData,
    IN  DWORD               Index
    )
{
    DWORD                   Type;
    DWORD                   PropertyLength;
    PTCHAR                  Property;
    HRESULT                 Error;

    if (!SetupDiGetDeviceRegistryProperty(DeviceInfoSet,
                                          DeviceInfoData,
                                          Index,
                                          &Type,
                                          NULL,
                                          0,
                                          &PropertyLength)) {
        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER)
            goto fail1;
    }

    if (Type != REG_SZ) {
        SetLastError(ERROR_BAD_FORMAT);
        goto fail2;
    }

    PropertyLength += sizeof (TCHAR);

    Property = malloc(PropertyLength);
    if (Property == NULL)
        goto fail3;

    memset(Property, 0, PropertyLength);

    if (!SetupDiGetDeviceRegistryProperty(DeviceInfoSet,
                                          DeviceInfoData,
                                          Index,
                                          NULL,
                                          (PBYTE)Property,
                                          PropertyLength,
                                          NULL))
        goto fail4;

    return Property;

fail4:
    free(Property);

fail3:
    Log("fail3");

fail2:
    Log("fail2");

fail1:
    Error = GetLastError();

    {
        PTCHAR  Message;

        Message = GetErrorMessage(Error);
        Log("fail1 (%s)", Message);
        LocalFree(Message);
    }

    return NULL;
}

static BOOLEAN
SetFriendlyName(
    IN  HDEVINFO            DeviceInfoSet,
    IN  PSP_DEVINFO_DATA    DeviceInfoData,
    IN  PTCHAR              Description,
    IN  PTCHAR              Location
    )
{
    TCHAR                   FriendlyName[MAX_PATH];
    DWORD                   FriendlyNameLength;
    HRESULT                 Result;
    HRESULT                 Error;

    Result = StringCbPrintf(FriendlyName,
                            MAX_PATH,
                            TEXT("%s #%s"),
                            Description,
                            Location);
    if (!SUCCEEDED(Result))
        goto fail1;

    FriendlyNameLength = (DWORD)(strlen(FriendlyName) + sizeof (TCHAR));

    if (!SetupDiSetDeviceRegistryProperty(DeviceInfoSet,
                                          DeviceInfoData,
                                          SPDRP_FRIENDLYNAME,
                                          (PBYTE)FriendlyName,
                                          FriendlyNameLength))
        goto fail2;

    Log("%s", FriendlyName);

    return TRUE;

fail2:
    Log("fail2");

fail1:
    Error = GetLastError();

    {
        PTCHAR  Message;

        Message = GetErrorMessage(Error);
        Log("fail1 (%s)", Message);
        LocalFree(Message);
    }

    return FALSE;
}

static BOOLEAN
ParseMacAddress(
    IN  PCHAR               Buffer,
    OUT PETHERNET_ADDRESS   Address
    )
{
    ULONG                   Length;
    HRESULT                 Error;

    Length = 0;
    for (;;) {
        CHAR    Character;
        UCHAR   Byte;

        Character = *Buffer++;
        if (Character == '\0')
            break;

        if (Character >= '0' && Character <= '9')
            Byte = Character - '0';
        else if (Character >= 'A' && Character <= 'F')
            Byte = 0x0A + Character - 'A';
        else if (Character >= 'a' && Character <= 'f')
            Byte = 0x0A + Character - 'a';
        else
            break;

        Byte <<= 4;

        Character = *Buffer++;
        if (Character == '\0')
            break;

        if (Character >= '0' && Character <= '9')
            Byte += Character - '0';
        else if (Character >= 'A' && Character <= 'F')
            Byte += 0x0A + Character - 'A';
        else if (Character >= 'a' && Character <= 'f')
            Byte += 0x0A + Character - 'a';
        else
            break;

        Address->Byte[Length++] = Byte;

        // Skip over any separator
        if (*Buffer == ':' || *Buffer == '-')
            Buffer++;
    }

    if (Length != ETHERNET_ADDRESS_LENGTH) {
        SetLastError(ERROR_BAD_FORMAT);
        goto fail1;
    }

    return TRUE;

fail1:
    Error = GetLastError();

    {
        PTCHAR  Message;

        Message = GetErrorMessage(Error);
        Log("fail1 (%s)", Message);
        LocalFree(Message);
    }

    return FALSE;
}


static PETHERNET_ADDRESS
GetPermanentAddress(
    IN  PTCHAR          Location
    )
{
    HRESULT             Error;
    HKEY                AddressesKey;
    DWORD               MaxValueLength;
    DWORD               BufferLength;
    PTCHAR              Buffer;
    DWORD               Type;
    BOOLEAN             Success;
    PETHERNET_ADDRESS   Address;

    Error = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                         ADDRESSES_KEY(XENVIF),
                         0,
                         KEY_READ,
                         &AddressesKey);
    if (Error != ERROR_SUCCESS) {
        SetLastError(Error);
        goto fail1;
    }

    Error = RegQueryInfoKey(AddressesKey,
                            NULL,
                            NULL,
                            NULL,
                            NULL,
                            NULL,
                            NULL,
                            NULL,
                            NULL,
                            &MaxValueLength,
                            NULL,
                            NULL);
    if (Error != ERROR_SUCCESS) {
        SetLastError(Error);
        goto fail2;
    }

    BufferLength = MaxValueLength + sizeof (TCHAR);

    Buffer = malloc(BufferLength);
    if (Buffer == NULL)
        goto fail3;

    memset(Buffer, 0, BufferLength);

    Error = RegQueryValueEx(AddressesKey,
                            Location,
                            NULL,
                            &Type,
                            (LPBYTE)Buffer,
                            &BufferLength);
    if (Error != ERROR_SUCCESS) {
        SetLastError(Error);
        goto fail4;
    }

    if (Type != REG_SZ) {
        SetLastError(ERROR_BAD_FORMAT);
        goto fail5;
    }

    Address = malloc(sizeof (ETHERNET_ADDRESS));
    if (Address == NULL)
        goto fail6;

    Success = ParseMacAddress(Buffer, Address);
    if (!Success)
        goto fail7;

    free(Buffer);

    RegCloseKey(AddressesKey);

    Log("%02X:%02X:%02X:%02X:%02X:%02X",
        Address->Byte[0],
        Address->Byte[1],
        Address->Byte[2],
        Address->Byte[3],
        Address->Byte[4],
        Address->Byte[5]);

    return Address;

fail7:
    Log("fail7");

    free(Address);

fail6:
    Log("fail6");

fail5:
    Log("fail5");

fail4:
    Log("fail4");

    free(Buffer);

fail3:
    Log("fail3");

fail2:
    Log("fail2");

    RegCloseKey(AddressesKey);

fail1:
    Error = GetLastError();

    {
        PTCHAR  Message;

        Message = GetErrorMessage(Error);
        Log("fail1 (%s)", Message);
        LocalFree(Message);
    }

    return NULL;
}

static BOOLEAN
GetNetLuid(
    IN  PETHERNET_ADDRESS   Address,
    OUT PNET_LUID           *NetLuid
    )
{
    PMIB_IF_TABLE2          Table;
    DWORD                   Index;
    PMIB_IF_ROW2            Row;
    HRESULT                 Error;

    Error = GetIfTable2(&Table);
    if (Error != ERROR_SUCCESS) {
        SetLastError(Error);
        goto fail1;
    }

    for (Index = 0; Index < Table->NumEntries; Index++) {
        Row = &Table->Table[Index];

        if (!(Row->InterfaceAndOperStatusFlags.HardwareInterface))
            continue;

        if (Row->PhysicalAddressLength != sizeof (ETHERNET_ADDRESS))
            continue;

        if (memcmp(Row->PhysicalAddress,
                   Address,
                   sizeof (ETHERNET_ADDRESS)) == 0)
            goto found;
    }

    *NetLuid = NULL;
    goto done;

found:
    *NetLuid = malloc(sizeof (NET_LUID));
    if (*NetLuid == NULL)
        goto fail2;

    (*NetLuid)->Value = Row->InterfaceLuid.Value;

done:
    FreeMibTable(Table);

    return TRUE;

fail2:
    Log("fail2");

    FreeMibTable(Table);

fail1:
    Error = GetLastError();

    {
        PTCHAR  Message;

        Message = GetErrorMessage(Error);
        Log("fail1 (%s)", Message);
        LocalFree(Message);
    }

    return FALSE;
}

static BOOLEAN
OpenClassKey(
    IN  const GUID  *Guid,
    OUT PHKEY       Key
    )
{   
    DWORD           KeyLength;
    PTCHAR          KeyName;
    HRESULT         Result;
    HRESULT         Error;

    KeyLength = sizeof (CLASS_KEY) +
                sizeof ("\\{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}") +
                sizeof (TCHAR);

    KeyName = malloc(KeyLength);
    if (KeyName == NULL)
        goto fail1;

    Result = StringCbPrintf(KeyName,
                            KeyLength,
                            "%s\\{%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}",
                            CLASS_KEY,
                            Guid->Data1,
                            Guid->Data2,
                            Guid->Data3,
                            Guid->Data4[0],
                            Guid->Data4[1],
                            Guid->Data4[2],
                            Guid->Data4[3],
                            Guid->Data4[4],
                            Guid->Data4[5],
                            Guid->Data4[6],
                            Guid->Data4[7]);
    if (!SUCCEEDED(Result)) {
        SetLastError(ERROR_BUFFER_OVERFLOW);
        goto fail2;
    }

    Log("%s", KeyName);

    Error = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                         KeyName,
                         0,
                         KEY_READ,
                         Key);
    if (Error != ERROR_SUCCESS) {
        SetLastError(Error);
        goto fail3;
    }

    free(KeyName);

    return TRUE;

fail3:
    Log("fail3");

fail2:
    Log("fail2");

    free(KeyName);

fail1:
    Error = GetLastError();

    {
        PTCHAR  Message;
        Message = GetErrorMessage(Error);
        Log("fail1 (%s)", Message);
        LocalFree(Message);
    }

    return FALSE;
}

static BOOLEAN
FindAliasSoftwareKeyName(
    IN  PETHERNET_ADDRESS   Address,
    OUT PTCHAR              *Name
    )
{
    BOOLEAN                 Success;
    PNET_LUID               NetLuid;
    HKEY                    NetKey;
    HRESULT                 Error;
    DWORD                   SubKeys;
    DWORD                   MaxSubKeyLength;
    DWORD                   SubKeyLength;
    PTCHAR                  SubKeyName;
    DWORD                   Index;
    HKEY                    SubKey;

    Success = GetNetLuid(Address, &NetLuid);
    if (!Success)
        goto fail1;

    SubKeyName = NULL;

    if (NetLuid == NULL)
        goto done;

    Log("%08x.%08x",
        NetLuid->Info.IfType,
        NetLuid->Info.NetLuidIndex);

    Success = OpenClassKey(&GUID_DEVCLASS_NET, &NetKey);
    if (!Success)
        goto fail2;

    Error = RegQueryInfoKey(NetKey,
                            NULL,
                            NULL,
                            NULL,
                            &SubKeys,
                            &MaxSubKeyLength,
                            NULL,
                            NULL,
                            NULL,
                            NULL,
                            NULL,
                            NULL);
    if (Error != ERROR_SUCCESS) {
        SetLastError(Error);
        goto fail3;
    }

    SubKeyLength = MaxSubKeyLength + sizeof (TCHAR);

    SubKeyName = malloc(SubKeyLength);
    if (SubKeyName == NULL)
        goto fail4;

    for (Index = 0; Index < SubKeys; Index++) {
        DWORD   Length;
        DWORD   Type;
        DWORD   IfType;
        DWORD   NetLuidIndex;

        SubKeyLength = MaxSubKeyLength + sizeof (TCHAR);
        memset(SubKeyName, 0, SubKeyLength);

        Error = RegEnumKeyEx(NetKey,
                             Index,
                             (LPTSTR)SubKeyName,
                             &SubKeyLength,
                             NULL,
                             NULL,
                             NULL,
                             NULL);
        if (Error != ERROR_SUCCESS) {
            SetLastError(Error);
            goto fail5;
        }

        Error = RegOpenKeyEx(NetKey,
                             SubKeyName,
                             0,
                             KEY_READ,
                             &SubKey);
        if (Error != ERROR_SUCCESS)
            continue;

        Length = sizeof (DWORD);
        Error = RegQueryValueEx(SubKey,
                                "*IfType",
                                NULL,
                                &Type,
                                (LPBYTE)&IfType,
                                &Length);
        if (Error != ERROR_SUCCESS ||
            Type != REG_DWORD)
            goto loop;

        Length = sizeof (DWORD);
        Error = RegQueryValueEx(SubKey,
                                "NetLuidIndex",
                                NULL,
                                &Type,
                                (LPBYTE)&NetLuidIndex,
                                &Length);
        if (Error != ERROR_SUCCESS ||
            Type != REG_DWORD)
            goto loop;

        if (NetLuid->Info.IfType == IfType &&
            NetLuid->Info.NetLuidIndex == NetLuidIndex)
            goto found;

loop:
        RegCloseKey(SubKey);
    }

    SetLastError(ERROR_FILE_NOT_FOUND);
    goto fail6;

found:
    Log("%s", SubKeyName);

    RegCloseKey(SubKey);

    RegCloseKey(NetKey);

    free(NetLuid);

done:
    *Name = SubKeyName;
    return TRUE;

fail6:
    Log("fail6");

fail5:
    Log("fail5");

    free(SubKeyName);

fail4:
    Log("fail4");

fail3:
    Log("fail3");

    RegCloseKey(NetKey);

fail2:
    Log("fail2");

    free(NetLuid);

fail1:
    Error = GetLastError();

    {
        PTCHAR  Message;
        Message = GetErrorMessage(Error);
        Log("fail1 (%s)", Message);
        LocalFree(Message);
    }

    return FALSE;
}

static BOOLEAN
SetAliasSoftwareKeyName(
    IN  HKEY    SoftwareKey,
    IN  PTCHAR  Name
    )
{
    HRESULT     Error;

    Error = RegSetValueEx(SoftwareKey,
                          "Alias",
                          0,
                          REG_SZ,
                          (LPBYTE)Name,
                          (DWORD)((strlen(Name) + 1) * sizeof (TCHAR)));
    if (Error != ERROR_SUCCESS) {
        SetLastError(Error);
        goto fail1;
    }

    return TRUE;

fail1:
    Error = GetLastError();

    {
        PTCHAR  Message;
        Message = GetErrorMessage(Error);
        Log("fail1 (%s)", Message);
        LocalFree(Message);
    }

    return FALSE;
}

static HKEY
OpenAliasSoftwareKey(
    IN  PTCHAR  Name
    )
{
    BOOLEAN     Success;
    HKEY        NetKey;
    HKEY        SoftwareKey;
    HRESULT     Error;

    Success = OpenClassKey(&GUID_DEVCLASS_NET, &NetKey);
    if (!Success)
        goto fail1;

    Error = RegOpenKeyEx(NetKey,
                         Name,
                         0,
                         KEY_ALL_ACCESS,
                         &SoftwareKey);
    if (Error != ERROR_SUCCESS) {
        SetLastError(Error);
        goto fail2;
    }

    RegCloseKey(NetKey);

    return SoftwareKey;

fail2:
    Log("fail2");

    RegCloseKey(NetKey);

fail1:
    Error = GetLastError();

    {
        PTCHAR  Message;
        Message = GetErrorMessage(Error);
        Log("fail1 (%s)", Message);
        LocalFree(Message);
    }

    return NULL;
}

static HKEY
OpenSoftwareKey(
    IN  HDEVINFO            DeviceInfoSet,
    IN  PSP_DEVINFO_DATA    DeviceInfoData
    )
{
    HKEY                    Key;
    HRESULT                 Error;

    Key = SetupDiOpenDevRegKey(DeviceInfoSet,
                               DeviceInfoData,
                               DICS_FLAG_GLOBAL,
                               0,
                               DIREG_DRV,
                               KEY_ALL_ACCESS);
    if (Key == INVALID_HANDLE_VALUE) {
        SetLastError(ERROR_PATH_NOT_FOUND);
        goto fail1;
    }

    return Key;

fail1:
    Error = GetLastError();

    {
        PTCHAR  Message;

        Message = GetErrorMessage(Error);
        Log("fail1 (%s)", Message);
        LocalFree(Message);
    }

    return NULL;
}

static PTCHAR
GetInterfaceName(
    IN  HKEY    SoftwareKey
    )
{
    HRESULT     Error;
    HKEY        LinkageKey;
    DWORD       MaxValueLength;
    DWORD       RootDeviceLength;
    PTCHAR      RootDevice;
    DWORD       Type;

    Error = RegOpenKeyEx(SoftwareKey,
                         "Linkage",
                         0,
                         KEY_READ,
                         &LinkageKey);
    if (Error != ERROR_SUCCESS) {
        SetLastError(Error);
        goto fail1;
    }

    Error = RegQueryInfoKey(LinkageKey,
                            NULL,
                            NULL,
                            NULL,
                            NULL,
                            NULL,
                            NULL,
                            NULL,
                            NULL,
                            &MaxValueLength,
                            NULL,
                            NULL);
    if (Error != ERROR_SUCCESS) {
        SetLastError(Error);
        goto fail2;
    }

    RootDeviceLength = MaxValueLength + sizeof (TCHAR);

    RootDevice = malloc(RootDeviceLength);
    if (RootDevice == NULL)
        goto fail2;

    memset(RootDevice, 0, RootDeviceLength);

    Error = RegQueryValueEx(LinkageKey,
                            "RootDevice",
                            NULL,
                            &Type,
                            (LPBYTE)RootDevice,
                            &RootDeviceLength);
    if (Error != ERROR_SUCCESS) {
        SetLastError(Error);
        goto fail3;
    }

    if (Type != REG_MULTI_SZ) {
        SetLastError(ERROR_BAD_FORMAT);
        goto fail4;
    }

    RegCloseKey(LinkageKey);

    return RootDevice;

fail4:
    Log("fail4");

fail3:
    Log("fail3");

    free(RootDevice);

fail2:
    Log("fail2");

    RegCloseKey(LinkageKey);

fail1:
    Error = GetLastError();

    {
        PTCHAR  Message;

        Message = GetErrorMessage(Error);
        Log("fail1 (%s)", Message);
        LocalFree(Message);
    }

    return NULL;
}

static BOOLEAN
CopyValues(
    IN  PTCHAR  DestinationKeyName,
    IN  PTCHAR  SourceKeyName
    )
{
    HKEY        DestinationKey;
    HKEY        SourceKey;
    HRESULT     Error;
    DWORD       Values;
    DWORD       MaxNameLength;
    PTCHAR      Name;
    DWORD       MaxValueLength;
    LPBYTE      Value;
    DWORD       Index;

    Log("DESTINATION: %s", DestinationKeyName);
    Log("SOURCE: %s", SourceKeyName);

    Error = RegCreateKeyEx(HKEY_LOCAL_MACHINE,
                           DestinationKeyName,
                           0,
                           NULL,
                           REG_OPTION_NON_VOLATILE,
                           KEY_ALL_ACCESS,
                           NULL,
                           &DestinationKey,
                           NULL);
    if (Error != ERROR_SUCCESS) {
        SetLastError(Error);
        goto fail1;
    }

    Error = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                         SourceKeyName,
                         0,
                         KEY_ALL_ACCESS,
                         &SourceKey);
    if (Error != ERROR_SUCCESS) {
        SetLastError(Error);
        goto fail2;
    }

    Error = RegQueryInfoKey(SourceKey,
                            NULL,
                            NULL,
                            NULL,
                            NULL,
                            NULL,
                            NULL,
                            &Values,
                            &MaxNameLength,
                            &MaxValueLength,
                            NULL,
                            NULL);
    if (Error != ERROR_SUCCESS) {
        SetLastError(Error);
        goto fail3;
    }

    if (Values == 0)
        goto done;

    MaxNameLength += sizeof (TCHAR);

    Name = malloc(MaxNameLength);
    if (Name == NULL)
        goto fail4;

    Value = malloc(MaxValueLength);
    if (Value == NULL)
        goto fail5;

    for (Index = 0; Index < Values; Index++) {
        DWORD   NameLength;
        DWORD   ValueLength;
        DWORD   Type;

        NameLength = MaxNameLength;
        memset(Name, 0, NameLength);

        ValueLength = MaxValueLength;
        memset(Value, 0, ValueLength);

        Error = RegEnumValue(SourceKey,
                             Index,
                             (LPTSTR)Name,
                             &NameLength,
                             NULL,
                             &Type,
                             Value,
                             &ValueLength);
        if (Error != ERROR_SUCCESS) {
            SetLastError(Error);
            goto fail6;
        }

        Error = RegSetValueEx(DestinationKey,
                              Name,
                              0,
                              Type,
                              Value,
                              ValueLength);
        if (Error != ERROR_SUCCESS) {
            SetLastError(Error);
            goto fail7;
        }

        Log("COPIED %s", Name);
    }

    free(Value);
    free(Name);

    RegCloseKey(SourceKey);
    RegCloseKey(DestinationKey);

done:
    return TRUE;

fail7:
    Log("fail7");

fail6:
    Log("fail6");

    free(Value);

fail5:
    Log("fail5");

    free(Name);

fail4:
    Log("fail4");

fail3:
    Log("fail3");

    RegCloseKey(SourceKey);

fail2:
    Log("fail2");

    RegCloseKey(DestinationKey);

fail1:
    Error = GetLastError();

    {
        PTCHAR  Message;

        Message = GetErrorMessage(Error);
        Log("fail1 (%s)", Message);
        LocalFree(Message);
    }

    return FALSE;
}

static BOOLEAN
CopyParameters(
    IN  PTCHAR  Prefix,
    IN  PTCHAR  DestinationName,
    IN  PTCHAR  SourceName
    )
{
    DWORD       Length;
    PTCHAR      SourceKeyName;
    PTCHAR      DestinationKeyName;
    HRESULT     Result;
    HRESULT     Error;
    BOOLEAN     Success;

    Length = (DWORD)((strlen(Prefix) +
                      strlen(DestinationName) +
                      1) * sizeof (TCHAR));

    DestinationKeyName = malloc(Length);
    if (DestinationKeyName == NULL)
        goto fail1;

    Result = StringCbPrintf(DestinationKeyName,
                            Length,
                            "%s%s",
                            Prefix,
                            DestinationName);
    if (!SUCCEEDED(Result)) {
        SetLastError(ERROR_BUFFER_OVERFLOW);
        goto fail2;
    }

    Length = (DWORD)((strlen(Prefix) +
                      strlen(SourceName) +
                      1) * sizeof (TCHAR));

    SourceKeyName = malloc(Length);
    if (SourceKeyName == NULL)
        goto fail3;

    Result = StringCbPrintf(SourceKeyName,
                            Length,
                            "%s%s",
                            Prefix,
                            SourceName);
    if (!SUCCEEDED(Result)) {
        SetLastError(ERROR_BUFFER_OVERFLOW);
        goto fail4;
    }

    Success = CopyValues(DestinationKeyName, SourceKeyName);

    free(SourceKeyName);
    free(DestinationKeyName);

    return Success;

fail4:
    Log("fail4");

    free(SourceKeyName);

fail3:
    Log("fail3");

fail2:
    Log("fail2");

    free(DestinationKeyName);

fail1:
    Error = GetLastError();

    {
        PTCHAR  Message;

        Message = GetErrorMessage(Error);
        Log("fail1 (%s)", Message);
        LocalFree(Message);
    }

    return FALSE;
}

static BOOLEAN
CopyIpVersion6Addresses(
    IN  PTCHAR  KeyName,
    IN  PTCHAR  DestinationValueName,
    IN  PTCHAR  SourceValueName
    )
{
    HKEY        DestinationKey;
    HKEY        SourceKey;
    HRESULT     Error;
    DWORD       Values;
    DWORD       MaxNameLength;
    PTCHAR      Name;
    DWORD       MaxValueLength;
    LPBYTE      Value;
    DWORD       Index;

    Log("DESTINATION: %s\\%s", KeyName, DestinationValueName);
    Log("SOURCE: %s\\%s", KeyName, SourceValueName);

    Error = RegCreateKeyEx(HKEY_LOCAL_MACHINE,
                           KeyName,
                           0,
                           NULL,
                           REG_OPTION_NON_VOLATILE,
                           KEY_ALL_ACCESS,
                           NULL,
                           &DestinationKey,
                           NULL);
    if (Error != ERROR_SUCCESS) {
        SetLastError(Error);
        goto fail1;
    }

    Error = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                         KeyName,
                         0,
                         KEY_ALL_ACCESS,
                         &SourceKey);
    if (Error != ERROR_SUCCESS) {
        SetLastError(Error);
        goto fail2;
    }

    Error = RegQueryInfoKey(SourceKey,
                            NULL,
                            NULL,
                            NULL,
                            NULL,
                            NULL,
                            NULL,
                            &Values,
                            &MaxNameLength,
                            &MaxValueLength,
                            NULL,
                            NULL);
    if (Error != ERROR_SUCCESS) {
        SetLastError(Error);
        goto fail3;
    }

    if (Values == 0)
        goto done;

    MaxNameLength += sizeof (TCHAR);

    Name = malloc(MaxNameLength);
    if (Name == NULL)
        goto fail4;

    Value = malloc(MaxValueLength);
    if (Value == NULL)
        goto fail5;

    for (Index = 0; Index < Values; Index++) {
        DWORD   NameLength;
        DWORD   ValueLength;
        DWORD   Type;

        NameLength = MaxNameLength;
        memset(Name, 0, NameLength);

        ValueLength = MaxValueLength;
        memset(Value, 0, ValueLength);

        Error = RegEnumValue(SourceKey,
                             Index,
                             (LPTSTR)Name,
                             &NameLength,
                             NULL,
                             &Type,
                             Value,
                             &ValueLength);
        if (Error != ERROR_SUCCESS) {
            SetLastError(Error);
            goto fail6;
        }

        if (strncmp(Name, SourceValueName, sizeof (ULONG64) * 2) != 0)
            continue;

        Log("READ: %s", Name);

        memcpy(Name, DestinationValueName, sizeof (ULONG64) * 2);

        Log("WRITE: %s", Name);

        Error = RegSetValueEx(DestinationKey,
                              Name,
                              0,
                              Type,
                              Value,
                              ValueLength);
        if (Error != ERROR_SUCCESS) {
            SetLastError(Error);
            goto fail7;
        }
    }

    free(Value);
    free(Name);

    RegCloseKey(SourceKey);
    RegCloseKey(DestinationKey);

done:

    return TRUE;

fail7:
    Log("fail7");

fail6:
    Log("fail6");

    free(Value);

fail5:
    Log("fail5");

    free(Name);

fail4:
    Log("fail4");

fail3:
    Log("fail3");

    RegCloseKey(SourceKey);

fail2:
    Log("fail2");

    RegCloseKey(DestinationKey);

fail1:
    Error = GetLastError();

    {
        PTCHAR  Message;

        Message = GetErrorMessage(Error);
        Log("fail1 (%s)", Message);
        LocalFree(Message);
    }

    return FALSE;
}

static PTCHAR
GetIpVersion6AddressValueName(
    IN  HKEY    Key
    )
{
    HRESULT     Error;
    DWORD       MaxValueLength;
    DWORD       ValueLength;
    LPDWORD     Value;
    DWORD       Type;
    NET_LUID    NetLuid;
    DWORD       BufferLength;
    PTCHAR      Buffer;
    HRESULT     Result;

    memset(&NetLuid, 0, sizeof (NetLuid));

    Error = RegQueryInfoKey(Key,
                            NULL,
                            NULL,
                            NULL,    
                            NULL,
                            NULL,
                            NULL,
                            NULL,
                            NULL,
                            &MaxValueLength,
                            NULL,
                            NULL);
    if (Error != ERROR_SUCCESS) {
        SetLastError(Error);
        goto fail1;
    }

    ValueLength = MaxValueLength;

    Value = malloc(ValueLength);
    if (Value == NULL)
        goto fail2;

    memset(Value, 0, ValueLength);

    Error = RegQueryValueEx(Key,
                            "NetLuidIndex",
                            NULL,
                            &Type,
                            (LPBYTE)Value,
                            &ValueLength);
    if (Error != ERROR_SUCCESS) {
        SetLastError(Error);
        goto fail3;
    }

    if (Type != REG_DWORD) {
        SetLastError(ERROR_BAD_FORMAT);
        goto fail4;
    }

    NetLuid.Info.NetLuidIndex = *Value;

    Error = RegQueryValueEx(Key,
                            "*IfType",
                            NULL,
                            &Type,
                            (LPBYTE)Value,
                            &ValueLength);
    if (Error != ERROR_SUCCESS) {
        SetLastError(Error);
        goto fail5;
    }

    if (Type != REG_DWORD) {
        SetLastError(ERROR_BAD_FORMAT);
        goto fail6;
    }

    NetLuid.Info.IfType = *Value;

    BufferLength = ((sizeof (ULONG64) * 2) + 1) * sizeof (TCHAR);

    Buffer = malloc(BufferLength);
    if (Buffer == NULL)
        goto fail7;

    Result = StringCbPrintf(Buffer,
                            BufferLength,
                            "%016llx",
                            _byteswap_uint64(NetLuid.Value));
    if (!SUCCEEDED(Result)) {
        SetLastError(ERROR_BUFFER_OVERFLOW);
        goto fail8;
    }

    free(Value);

    return Buffer;

fail8:
    Log("fail8");

    free(Buffer);

fail7:
    Log("fail7");

fail6:
    Log("fail6");

fail5:
    Log("fail5");

fail4:
    Log("fail4");

fail3:
    Log("fail3");

    free(Value);

fail2:
    Log("fail2");

fail1:
    Error = GetLastError();

    {
        PTCHAR  Message;

        Message = GetErrorMessage(Error);
        Log("fail1 (%s)", Message);
        LocalFree(Message);
    }

    return NULL;
}

static BOOLEAN
CopySettings(
    IN  HKEY    DestinationKey,
    IN  HKEY    SourceKey
    )
{
    PTCHAR      DestinationName;
    PTCHAR      SourceName;
    BOOLEAN     Success;
    HRESULT     Error;

    Log("====>");

    Success = TRUE;

    SourceName = GetInterfaceName(SourceKey);

    if (SourceName == NULL)
        goto fail1;

    DestinationName = GetInterfaceName(DestinationKey);

    if (DestinationName == NULL)
        goto fail2;

    Success &= CopyParameters(PARAMETERS_KEY(NetBT) "\\Interfaces\\Tcpip_",
                              DestinationName,
                              SourceName);
    Success &= CopyParameters(PARAMETERS_KEY(Tcpip) "\\Interfaces\\",
                              DestinationName,
                              SourceName);
    Success &= CopyParameters(PARAMETERS_KEY(Tcpip6) "\\Interfaces\\",
                              DestinationName,
                              SourceName);

    free(DestinationName);
    free(SourceName);

    SourceName = GetIpVersion6AddressValueName(SourceKey);

    if (SourceName == NULL)
        goto fail3;

    DestinationName = GetIpVersion6AddressValueName(DestinationKey);

    if (DestinationName == NULL)
        goto fail4;

    Success &= CopyIpVersion6Addresses(NSI_KEY "\\{eb004a01-9b1a-11d4-9123-0050047759bc}\\10\\",
                                       DestinationName,
                                       SourceName);

    free(DestinationName);
    free(SourceName);

    Log("<====");

    return Success;

fail4:
    Log("fail4");

    free(SourceName);
    goto fail1;

fail3:
    Log("fail3");

fail2:
    Log("fail2");

    free(SourceName);

fail1:
    Error = GetLastError();

    {
        PTCHAR  Message;

        Message = GetErrorMessage(Error);
        Log("fail1 (%s)", Message);
        LocalFree(Message);
    }

    return FALSE;
}

static BOOLEAN
RequestUnplug(
    VOID          
    )
{
    return TRUE;
}

static BOOLEAN
RequestReboot(
    IN  HDEVINFO            DeviceInfoSet,
    IN  PSP_DEVINFO_DATA    DeviceInfoData
    )
{
    SP_DEVINSTALL_PARAMS    DeviceInstallParams;

    DeviceInstallParams.cbSize = sizeof (DeviceInstallParams);

    if (!SetupDiGetDeviceInstallParams(DeviceInfoSet,
                                       DeviceInfoData,
                                       &DeviceInstallParams))
        goto fail1;

    DeviceInstallParams.Flags |= DI_NEEDREBOOT;

    Log("Flags = %08x", DeviceInstallParams.Flags);

    if (!SetupDiSetDeviceInstallParams(DeviceInfoSet,
                                       DeviceInfoData,
                                       &DeviceInstallParams))
        goto fail2;

    return TRUE;

fail2:
fail1:
    return FALSE;
}

static FORCEINLINE HRESULT
__DifInstallPreProcess(
    IN  HDEVINFO                    DeviceInfoSet,
    IN  PSP_DEVINFO_DATA            DeviceInfoData,
    IN  PCOINSTALLER_CONTEXT_DATA   Context
    )
{
    HKEY                            SoftwareKey;
    PTCHAR                          Interface;
    BOOLEAN                         NeedMigrateSettings;

    Log("====>");

    SoftwareKey = OpenSoftwareKey(DeviceInfoSet, DeviceInfoData);

    if (SoftwareKey != NULL) {
        Interface = GetInterfaceName(SoftwareKey);
        RegCloseKey(SoftwareKey);
    } else {
        Interface = NULL;
    }

    if (Interface != NULL) {
        free(Interface);

        NeedMigrateSettings = FALSE;
    } else {
        NeedMigrateSettings = TRUE;
    }

    Log("NeedMigrateSettings = %s",
        (NeedMigrateSettings) ? "TRUE" : "FALSE");

    Context->PrivateData = (PVOID)NeedMigrateSettings;

    Log("<====");

    return ERROR_DI_POSTPROCESSING_REQUIRED; 
}

static FORCEINLINE HRESULT
__DifInstallPostProcess(
    IN  HDEVINFO                    DeviceInfoSet,
    IN  PSP_DEVINFO_DATA            DeviceInfoData,
    IN  PCOINSTALLER_CONTEXT_DATA   Context
    )
{
    BOOLEAN                         NeedMigrateSettings;
    BOOLEAN                         Success;
    PTCHAR                          Description;
    PTCHAR                          Location;
    PETHERNET_ADDRESS               Address;
    PTCHAR                          Name;
    HKEY                            Source;
    HKEY                            Destination;
    HRESULT                         Error;

    UNREFERENCED_PARAMETER(DeviceInfoSet);
    UNREFERENCED_PARAMETER(DeviceInfoData);

    Log("====>");

    Error = Context->InstallResult;
    if (Error != NO_ERROR) {
        SetLastError(Error);
        goto fail1;
    }

    NeedMigrateSettings = (BOOLEAN)(ULONG_PTR)Context->PrivateData;

    Description = GetProperty(DeviceInfoSet,
                              DeviceInfoData,
                              SPDRP_DEVICEDESC);
    if (Description == NULL)
        goto fail2;

    Location = GetProperty(DeviceInfoSet,
                           DeviceInfoData,
                           SPDRP_LOCATION_INFORMATION);
    if (Location == NULL)
        goto fail3;

    Success = SetFriendlyName(DeviceInfoSet,
                              DeviceInfoData,
                              Description,
                              Location);
    if (!Success)
        goto fail4;

    Address = GetPermanentAddress(Location);
    if (Address == NULL)
        goto fail5;

    Success = FindAliasSoftwareKeyName(Address, &Name);
    if (!Success)
        goto fail6;

    if (Name == NULL || !NeedMigrateSettings)
        goto done;

    Source = OpenAliasSoftwareKey(Name);
    if (Source == NULL)
        goto fail7;

    Destination = OpenSoftwareKey(DeviceInfoSet, DeviceInfoData);
    if (Destination == NULL)
        goto fail8;

    Success = SetAliasSoftwareKeyName(Destination, Name);
    if (!Success)
        goto fail9;

    Success = CopySettings(Destination, Source);
    if (!Success)
        goto fail10;

    Success = RequestUnplug();
    if (!Success)
        goto fail11;

    Success = RequestReboot(DeviceInfoSet, DeviceInfoData);
    if (!Success)
        goto fail12;

    RegCloseKey(Destination);

    RegCloseKey(Source);

done:
    if (Name != NULL)
        free(Name);

    free(Address);

    free(Location);

    free(Description);

    Log("<====");

    return NO_ERROR;

fail12:
    Log("fail12");

fail11:
    Log("fail11");

fail10:
    Log("fail10");

fail9:
    Log("fail9");

    RegCloseKey(Destination);

fail8:
    Log("fail8");

    RegCloseKey(Source);

fail7:
    Log("fail7");

    free(Name);

fail6:
    Log("fail6");

    free(Address);

fail5:
    Log("fail5");

fail4:
    Log("fail4");

    free(Location);

fail3:
    Log("fail3");

    free(Description);

fail2:
    Log("fail2");

fail1:
    Error = GetLastError();

    {
        PTCHAR  Message;

        Message = GetErrorMessage(Error);
        Log("fail1 (%s)", Message);
        LocalFree(Message);
    }

    return Error;
}

static DECLSPEC_NOINLINE HRESULT
DifInstall(
    IN  HDEVINFO                    DeviceInfoSet,
    IN  PSP_DEVINFO_DATA            DeviceInfoData,
    IN  PCOINSTALLER_CONTEXT_DATA   Context
    )
{
    SP_DEVINSTALL_PARAMS            DeviceInstallParams;
    HRESULT                         Error;

    DeviceInstallParams.cbSize = sizeof (DeviceInstallParams);

    if (!SetupDiGetDeviceInstallParams(DeviceInfoSet,
                                       DeviceInfoData,
                                       &DeviceInstallParams))
        goto fail1;

    Log("Flags = %08x", DeviceInstallParams.Flags);

    Error = (!Context->PostProcessing) ?
            __DifInstallPreProcess(DeviceInfoSet, DeviceInfoData, Context) :
            __DifInstallPostProcess(DeviceInfoSet, DeviceInfoData, Context);

    return Error;

fail1:
    Error = GetLastError();

    {
        PTCHAR  Message;

        Message = GetErrorMessage(Error);
        Log("fail1 (%s)", Message);
        LocalFree(Message);
    }

    return Error;
}

static FORCEINLINE HRESULT
__DifRemovePreProcess(
    IN  HDEVINFO                    DeviceInfoSet,
    IN  PSP_DEVINFO_DATA            DeviceInfoData,
    IN  PCOINSTALLER_CONTEXT_DATA   Context
    )
{
    UNREFERENCED_PARAMETER(DeviceInfoSet);
    UNREFERENCED_PARAMETER(DeviceInfoData);
    UNREFERENCED_PARAMETER(Context);

    Log("<===>");

    return ERROR_DI_POSTPROCESSING_REQUIRED; 
}

static FORCEINLINE HRESULT
__DifRemovePostProcess(
    IN  HDEVINFO                    DeviceInfoSet,
    IN  PSP_DEVINFO_DATA            DeviceInfoData,
    IN  PCOINSTALLER_CONTEXT_DATA   Context
    )
{
    HRESULT                         Error;

    UNREFERENCED_PARAMETER(DeviceInfoSet);
    UNREFERENCED_PARAMETER(DeviceInfoData);

    Log("====>");

    Error = Context->InstallResult;
    if (Error != NO_ERROR) {
        SetLastError(Error);
        goto fail1;
    }

    Log("<====");

    return NO_ERROR;

fail1:
    Error = GetLastError();

    {
        PTCHAR  Message;

        Message = GetErrorMessage(Error);
        Log("fail1 (%s)", Message);
        LocalFree(Message);
    }

    return Error;
}

static DECLSPEC_NOINLINE HRESULT
DifRemove(
    IN  HDEVINFO                    DeviceInfoSet,
    IN  PSP_DEVINFO_DATA            DeviceInfoData,
    IN  PCOINSTALLER_CONTEXT_DATA   Context
    )
{
    SP_DEVINSTALL_PARAMS            DeviceInstallParams;
    HRESULT                         Error;

    DeviceInstallParams.cbSize = sizeof (DeviceInstallParams);

    if (!SetupDiGetDeviceInstallParams(DeviceInfoSet,
                                       DeviceInfoData,
                                       &DeviceInstallParams))
        goto fail1;

    Log("Flags = %08x", DeviceInstallParams.Flags);

    Error = (!Context->PostProcessing) ?
            __DifRemovePreProcess(DeviceInfoSet, DeviceInfoData, Context) :
            __DifRemovePostProcess(DeviceInfoSet, DeviceInfoData, Context);

    return Error;

fail1:
    Error = GetLastError();

    {
        PTCHAR  Message;

        Message = GetErrorMessage(Error);
        Log("fail1 (%s)", Message);
        LocalFree(Message);
    }

    return Error;
}

DWORD CALLBACK
Entry(
    IN  DI_FUNCTION                 Function,
    IN  HDEVINFO                    DeviceInfoSet,
    IN  PSP_DEVINFO_DATA            DeviceInfoData,
    IN  PCOINSTALLER_CONTEXT_DATA   Context
    )
{
    HRESULT                         Error;

    Log("%s (%s) ===>",
        MAJOR_VERSION_STR "." MINOR_VERSION_STR "." MICRO_VERSION_STR "." BUILD_NUMBER_STR,
        DAY_STR "/" MONTH_STR "/" YEAR_STR);

    if (!Context->PostProcessing) {
        Log("%s PreProcessing",
            FunctionName(Function));
    } else {
        Log("%s PostProcessing (%08x)",
            FunctionName(Function),
            Context->InstallResult);
    }

    switch (Function) {
    case DIF_INSTALLDEVICE: {
        SP_DRVINFO_DATA         DriverInfoData;
        BOOLEAN                 DriverInfoAvailable;

        DriverInfoData.cbSize = sizeof (DriverInfoData);
        DriverInfoAvailable = SetupDiGetSelectedDriver(DeviceInfoSet,
                                                       DeviceInfoData,
                                                       &DriverInfoData) ?
                              TRUE :
                              FALSE;

        // The NET class installer will call DIF_REMOVE even in the event of
        // a NULL driver add, so we don't need to do it here. However, the
        // default installer (for the NULL driver) fails for some reason so
        // we squash the error in post-processing.
        if (DriverInfoAvailable) {
            Error = DifInstall(DeviceInfoSet, DeviceInfoData, Context);
        } else {
            if (!Context->PostProcessing) {
                Log("%s PreProcessing",
                    FunctionName(Function));

                Error = ERROR_DI_POSTPROCESSING_REQUIRED; 
            } else {
                Log("%s PostProcessing (%08x)",
                    FunctionName(Function),
                    Context->InstallResult);

                Error = NO_ERROR;
            }
        }
        break;
    }
    case DIF_REMOVE:
        Error = DifRemove(DeviceInfoSet, DeviceInfoData, Context);
        break;
    default:
        if (!Context->PostProcessing) {
            Error = NO_ERROR;
        } else {
            Error = Context->InstallResult;
        }

        break;
    }

    Log("%s (%s) <===",
        MAJOR_VERSION_STR "." MINOR_VERSION_STR "." MICRO_VERSION_STR "." BUILD_NUMBER_STR,
        DAY_STR "/" MONTH_STR "/" YEAR_STR);

    return (DWORD)Error;
}

DWORD CALLBACK
Version(
    IN  HWND        Window,
    IN  HINSTANCE   Module,
    IN  PTCHAR      Buffer,
    IN  INT         Reserved
    )
{
    UNREFERENCED_PARAMETER(Window);
    UNREFERENCED_PARAMETER(Module);
    UNREFERENCED_PARAMETER(Buffer);
    UNREFERENCED_PARAMETER(Reserved);

    Log("%s (%s)",
        MAJOR_VERSION_STR "." MINOR_VERSION_STR "." MICRO_VERSION_STR "." BUILD_NUMBER_STR,
        DAY_STR "/" MONTH_STR "/" YEAR_STR);

    return NO_ERROR;
}

static FORCEINLINE const CHAR *
__ReasonName(
    IN  DWORD       Reason
    )
{
#define _NAME(_Reason)          \
        case DLL_ ## _Reason:   \
            return #_Reason;

    switch (Reason) {
    _NAME(PROCESS_ATTACH);
    _NAME(PROCESS_DETACH);
    _NAME(THREAD_ATTACH);
    _NAME(THREAD_DETACH);
    default:
        break;
    }

    return "UNKNOWN";

#undef  _NAME
}

BOOL WINAPI
DllMain(
    IN  HINSTANCE   Module,
    IN  DWORD       Reason,
    IN  PVOID       Reserved
    )
{
    UNREFERENCED_PARAMETER(Module);
    UNREFERENCED_PARAMETER(Reserved);

    Log("%s (%s): %s",
        MAJOR_VERSION_STR "." MINOR_VERSION_STR "." MICRO_VERSION_STR "." BUILD_NUMBER_STR,
        DAY_STR "/" MONTH_STR "/" YEAR_STR,
        __ReasonName(Reason));

    return TRUE;
}
