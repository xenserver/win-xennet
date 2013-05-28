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
#include <Ifdef.h>
#include <setupapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <strsafe.h>
#include <malloc.h>

#include <version.h>

__user_code;

#define MAXIMUM_BUFFER_SIZE 1024

#define ENUM_KEY "SYSTEM\\CurrentControlSet\\Enum"

#define CLASS_KEY "SYSTEM\\CurrentControlSet\\Control\\Class"

#define SERVICES_KEY "SYSTEM\\CurrentControlSet\\Services"

#define NSI_KEY "SYSTEM\\CurrentControlSet\\Control\\Nsi"

#define PARAMETERS_KEY(_Driver)    \
        SERVICES_KEY ## "\\" ## #_Driver ## "\\Parameters"

#define ALIASES_KEY(_Driver)    \
        SERVICES_KEY ## "\\" ## #_Driver ## "\\Aliases"

#define STATUS_KEY(_Driver)    \
        SERVICES_KEY ## "\\" ## #_Driver ## "\\Status"

#define SOFTWARE_KEY "SOFTWARE\\Citrix"

#define NET_SETTINGS_KEY    \
        SOFTWARE_KEY ## "\\" ## "XenToolsNetSettings\\XEN\\VIF"

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
    DWORD              Category;
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

    __analysis_assume(Length < MAXIMUM_BUFFER_SIZE);
    __analysis_assume(Length >= 2);
    Length = __min(MAXIMUM_BUFFER_SIZE - 1, Length + 2);
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

#define MAXIMUM_DEVICE_NAME_LENGTH  32
#define MAXIMUM_ALIAS_LENGTH        128

typedef struct _EMULATED_DEVICE {
    TCHAR   Device[MAXIMUM_DEVICE_NAME_LENGTH];
    TCHAR   Alias[MAXIMUM_ALIAS_LENGTH];
} EMULATED_DEVICE, *PEMULATED_DEVICE;

static PEMULATED_DEVICE
GetEmulatedDeviceTable(
    VOID
    )
{
    HKEY                Key;
    HRESULT             Error;
    DWORD               Values;
    DWORD               Index;
    PEMULATED_DEVICE    Table;

    Error = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                         ALIASES_KEY(XENFILT) "\\VIF",
                         0,
                         KEY_READ,
                         &Key);
    if (Error != ERROR_SUCCESS) {
        SetLastError(Error);
        goto fail1;
    }

    Error = RegQueryInfoKey(Key,
                            NULL,
                            NULL,
                            NULL,
                            NULL,
                            NULL,
                            NULL,
                            &Values,
                            NULL,
                            NULL,
                            NULL,
                            NULL);
    if (Error != ERROR_SUCCESS) {
        SetLastError(Error);
        goto fail2;
    }

    Table = malloc(sizeof (EMULATED_DEVICE) * (Values + 1));
    if (Table == NULL)
        goto fail3;

    memset(Table, 0, sizeof (EMULATED_DEVICE) * (Values + 1));

    for (Index = 0; Index < Values; Index++) {
        PEMULATED_DEVICE    Entry = &Table[Index];
        ULONG               DeviceNameLength;
        ULONG               AliasLength;
        ULONG               Type;

        DeviceNameLength = MAXIMUM_DEVICE_NAME_LENGTH * sizeof (TCHAR);
        AliasLength = MAXIMUM_ALIAS_LENGTH * sizeof (TCHAR);

        Error = RegEnumValue(Key,
                             Index,
                             (LPTSTR)Entry->Device,
                             &DeviceNameLength,
                             NULL,
                             &Type,
                             (LPBYTE)Entry->Alias,
                             &AliasLength);
        if (Error != ERROR_SUCCESS) {
            SetLastError(Error);
            goto fail4;
        }

        if (Type != REG_SZ) {
            SetLastError(ERROR_BAD_FORMAT);
            goto fail5;
        }
    }

    RegCloseKey(Key);

    return Table;

fail5:
    Log("fail5");

fail4:
    Log("fail4");

    free(Table);

fail3:
    Log("fail3");

fail2:
    Log("fail2");

    RegCloseKey(Key);

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

static DECLSPEC_NOINLINE HRESULT
OpenSoftwareKey(
    IN  HDEVINFO            DeviceInfoSet,
    IN  PSP_DEVINFO_DATA    DeviceInfoData,
    OUT PHKEY               SoftwareKey
    )
{
    HKEY                    Key;
    HRESULT                 Error;

    Key = SetupDiOpenDevRegKey(DeviceInfoSet,
                               DeviceInfoData,
                               DICS_FLAG_GLOBAL,
                               0,
                               DIREG_DRV,
                               KEY_READ);
    if (Key == INVALID_HANDLE_VALUE) {
        SetLastError(ERROR_PATH_NOT_FOUND);
        goto fail1;
    }

    *SoftwareKey = Key;

    return ERROR_SUCCESS;

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

// {4d36e972-e325-11ce-bfc1-08002be10318}
DEFINE_GUID(GUID_NET_DEVICE, 
            0x4d36e972,
            0xe325,
            0x11ce,
            0xbf,
            0xc1,
            0x08,
            0x00,
            0x2b,
            0xe1,
            0x03,
            0x18
            );

static HRESULT
OpenAliasHardwareKey(
    IN  PTCHAR  AliasDeviceID,
    IN  PTCHAR  AliasInstanceID,
    OUT PHKEY   HardwareKey
    )
{
    DWORD       PathLength;
    PTCHAR      Path;
    HRESULT     Result;
    HKEY        Key;
    HRESULT     Error;
    DWORD       SubKeys;
    DWORD       MaxSubKeyLength;
    PTCHAR      Name;
    DWORD       Index;
    HKEY        SubKey;

    Log("====> (%s) (%s)", AliasDeviceID, AliasInstanceID);

    PathLength = (DWORD)(strlen(ENUM_KEY) +
                         1 +
                         strlen(AliasDeviceID) +
                         1) * sizeof (TCHAR);

    Path = malloc(PathLength);
    if (Path == NULL)
        goto fail1;

    Result = StringCbPrintf(Path,
                            PathLength,
                            "%s\\%s",
                            ENUM_KEY,
                            AliasDeviceID);
    if (!SUCCEEDED(Result)) {
        SetLastError(ERROR_BUFFER_OVERFLOW);
        goto fail2;
    }

    Log("%s", Path);

    Error = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                         Path,
                         0,
                         KEY_READ,
                         &Key);
    if (Error != ERROR_SUCCESS) {
        SetLastError(Error);
        goto fail3;
    }

    Error = RegQueryInfoKey(Key,
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
        goto fail4;
    }

    Name = malloc(MaxSubKeyLength + sizeof (TCHAR));
    if (Name == NULL)
        goto fail5;

    for (Index = 0; Index < SubKeys; Index++) {
        DWORD   NameLength;
        PTCHAR  InstanceID;

        NameLength = MaxSubKeyLength + sizeof (TCHAR);
        memset(Name, 0, NameLength);

        Error = RegEnumKeyEx(Key,
                             Index,
                             (LPTSTR)Name,
                             &NameLength,
                             NULL,
                             NULL,
                             NULL,
                             NULL);
        if (Error != ERROR_SUCCESS) {
            SetLastError(Error);
            goto fail6;
        }

        Log("%s", Name);

        Error = RegOpenKeyEx(Key,
                             Name,
                             0,
                             KEY_READ,
                             &SubKey);
        if (Error != ERROR_SUCCESS) {
            SetLastError(Error);
            goto fail7;
        }

        InstanceID = strrchr(Name, '&');
        InstanceID++;

        if (_stricmp(AliasInstanceID, InstanceID) == 0)
            goto done;
        
        RegCloseKey(SubKey);
    }

    Error = ERROR_FILE_NOT_FOUND;
    goto fail8;

done:
    free(Name);

    RegCloseKey(Key);

    free(Path);

    *HardwareKey = SubKey;

    Log("<====");

    return ERROR_SUCCESS;

fail8:
    Log("fail8");

fail7:
    Log("fail7");

fail6:
    Log("fail6");

    free(Name);

fail5:
    Log("fail5");

fail4:
    Log("fail4");

    RegCloseKey(Key);

fail3:
    Log("fail3");

fail2:
    Log("fail2");

    free(Path);

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

static HRESULT
OpenAliasSoftwareKey(
    IN  PTCHAR  Alias,
    OUT PHKEY   SoftwareKey
    )
{
    DWORD       BufferLength;
    PTCHAR      Buffer;
    PTCHAR      AliasDeviceID;
    PTCHAR      AliasInstanceID;
    HRESULT     Error;
    HKEY        HardwareKey;
    DWORD       MaxValueLength;
    DWORD       DriverLength;
    PTCHAR      Driver;
    DWORD       Type;
    DWORD       PathLength;
    PTCHAR      Path;
    HRESULT     Result;
    HKEY        Key;

    Log("====>");

    BufferLength = (DWORD)(strlen(Alias) +
                           1) * sizeof (TCHAR);

    Buffer = malloc(BufferLength);
    if (Buffer == NULL)
        goto fail1;

    memset(Buffer, 0, BufferLength);

    memcpy(Buffer, Alias, strlen(Alias));

    AliasDeviceID = Buffer;

    AliasInstanceID = strchr(Buffer, '#');
    *AliasInstanceID++ = '\0';

    Error = OpenAliasHardwareKey(AliasDeviceID,
                                 AliasInstanceID,
                                 &HardwareKey);
    if (Error != ERROR_SUCCESS) {
        SetLastError(Error);
        goto fail2;
    }

    Error = RegQueryInfoKey(HardwareKey,
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
        goto fail3;
    }

    DriverLength = MaxValueLength + sizeof (TCHAR);

    Driver = malloc(DriverLength);
    if (Driver == NULL)
        goto fail4;

    memset(Driver, 0, DriverLength);

    Error = RegQueryValueEx(HardwareKey,
                            "Driver",
                            NULL,
                            &Type,
                            (LPBYTE)Driver,
                            &DriverLength);
    if (Error != ERROR_SUCCESS) {
        SetLastError(Error);
        goto fail5;
    }

    if (Type != REG_SZ) {
        SetLastError(ERROR_BAD_FORMAT);
        goto fail6;
    }

    Log("%s", Driver);

    PathLength = (ULONG)(strlen(CLASS_KEY) +
                         1 +
                         strlen(Driver) +
                         1) * sizeof (TCHAR);

    Path = malloc(PathLength);
    if (Path == NULL)
        goto fail7;

    Result = StringCbPrintf(Path,
                            PathLength,
                            "%s\\%s",
                            CLASS_KEY,
                            Driver);
    if (!SUCCEEDED(Result)) {
        SetLastError(ERROR_BUFFER_OVERFLOW);
        goto fail8;
    }

    Error = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                         Path,
                         0,
                         KEY_READ,
                         &Key);
    if (Error != ERROR_SUCCESS) {
        SetLastError(Error);
        goto fail9;
    }

    free(Path);

    free(Driver);

    RegCloseKey(HardwareKey);

    free(Buffer);

    *SoftwareKey = Key;

    Log("<====");

    return ERROR_SUCCESS;

fail9:
    Log("fail9");

fail8:
    Log("fail8");

    free(Path);

fail7:
    Log("fail7");

fail6:
    Log("fail6");

fail5:
    Log("fail5");

    free(Driver);

fail4:
    Log("fail4");

fail3:
    Log("fail3");

    RegCloseKey(HardwareKey);

fail2:
    Log("fail2");

    free(Buffer);

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

static PTCHAR
GetInterface(
    IN  HKEY    Key
    )
{
    HRESULT     Error;
    HKEY        SubKey;
    DWORD       MaxValueLength;
    DWORD       RootDeviceLength;
    PTCHAR      RootDevice;
    DWORD       Type;

    Error = RegOpenKeyEx(Key,
                         "Linkage",
                         0,
                         KEY_READ,
                         &SubKey);
    if (Error != ERROR_SUCCESS) {
        SetLastError(Error);
        goto fail1;
    }

    Error = RegQueryInfoKey(SubKey,
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

    Error = RegQueryValueEx(SubKey,
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

    RegCloseKey(SubKey);

    return RootDevice;

fail4:
    Log("fail4");

fail3:
    Log("fail3");

    free(RootDevice);

fail2:
    Log("fail2");

    RegCloseKey(SubKey);

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
    IN  PTCHAR  TargetPath,
    IN  PTCHAR  SourcePath
    )
{
    HKEY        TargetKey;
    HKEY        SourceKey;
    HRESULT     Error;
    DWORD       Values;
    DWORD       MaxNameLength;
    PTCHAR      Name;
    DWORD       MaxValueLength;
    LPBYTE      Value;
    DWORD       Index;

    Log("TARGET: %s", TargetPath);
    Log("SOURCE: %s", SourcePath);

    Error = RegCreateKeyEx(HKEY_LOCAL_MACHINE,
                           TargetPath,
                           0,
                           NULL,
                           REG_OPTION_NON_VOLATILE,
                           KEY_ALL_ACCESS,
                           NULL,
                           &TargetKey,
                           NULL);
    if (Error != ERROR_SUCCESS) {
        SetLastError(Error);
        goto fail1;
    }

    Error = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                         SourcePath,
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

        Error = RegSetValueEx(TargetKey,
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
    RegCloseKey(TargetKey);

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

    RegCloseKey(TargetKey);

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
    IN  PTCHAR  TargetPrefix,
    IN  PTCHAR  TargetNode,
    IN  PTCHAR  SourcePrefix,
    IN  PTCHAR  SourceNode
    )
{
    DWORD       Length;
    PTCHAR      SourcePath;
    PTCHAR      TargetPath;
    HRESULT     Result;
    HRESULT     Error;
    BOOLEAN     Success;

    Length = (DWORD)((strlen(TargetPrefix) +
                      strlen(TargetNode) +
                      1) * sizeof (TCHAR));

    TargetPath = malloc(Length);
    if (TargetPath == NULL)
        goto fail1;

    Result = StringCbPrintf(TargetPath,
                            Length,
                            "%s%s",
                            TargetPrefix,
                            TargetNode);
    if (!SUCCEEDED(Result)) {
        SetLastError(ERROR_BUFFER_OVERFLOW);
        goto fail2;
    }

    Length = (DWORD)((strlen(SourcePrefix) +
                      strlen(SourceNode) +
                      1) * sizeof (TCHAR));

    SourcePath = malloc(Length);
    if (SourcePath == NULL)
        goto fail3;

    Result = StringCbPrintf(SourcePath,
                            Length,
                            "%s%s",
                            SourcePrefix,
                            SourceNode);
    if (!SUCCEEDED(Result)) {
        SetLastError(ERROR_BUFFER_OVERFLOW);
        goto fail4;
    }

    Success = CopyValues(TargetPath, SourcePath);

    free(SourcePath);
    free(TargetPath);

    return Success;

fail4:
    Log("fail4");

    free(SourcePath);

fail3:
    Log("fail3");

fail2:
    Log("fail2");

    free(TargetPath);

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
    IN  PTCHAR  TargetPath,
    IN  PTCHAR  TargetPrefix,
    IN  PTCHAR  SourcePath,
    IN  PTCHAR  SourcePrefix
    )
{
    HKEY        TargetKey;
    HKEY        SourceKey;
    HRESULT     Error;
    DWORD       Values;
    DWORD       MaxNameLength;
    PTCHAR      Name;
    DWORD       MaxValueLength;
    LPBYTE      Value;
    DWORD       Index;

    Log("TARGET: %s", TargetPath);
    Log("SOURCE: %s", SourcePath);

    Error = RegCreateKeyEx(HKEY_LOCAL_MACHINE,
                           TargetPath,
                           0,
                           NULL,
                           REG_OPTION_NON_VOLATILE,
                           KEY_ALL_ACCESS,
                           NULL,
                           &TargetKey,
                           NULL);
    if (Error != ERROR_SUCCESS) {
        SetLastError(Error);
        goto fail1;
    }

    Error = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                         SourcePath,
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

        if (strncmp(Name, SourcePrefix, sizeof (ULONG64) * 2) != 0)
            continue;

        Log("READ: %s", Name);

        memcpy(Name, TargetPrefix, sizeof (ULONG64) * 2);

        Log("WRITE: %s", Name);

        Error = RegSetValueEx(TargetKey,
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
    RegCloseKey(TargetKey);

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

    RegCloseKey(TargetKey);

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
GetNetLuid(
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
MigrateSettings(
    IN  HKEY    SourceKey,
    IN  HKEY    TargetKey
    )
{
    PTCHAR      SourcePrefix;
    PTCHAR      TargetPrefix;
    BOOLEAN     Success;
    HRESULT     Error;

    Log("====>");

    Success = TRUE;

    SourcePrefix = GetInterface(SourceKey);

    if (SourcePrefix == NULL)
        goto fail1;

    TargetPrefix = GetInterface(TargetKey);

    if (TargetPrefix == NULL)
        goto fail2;

    Success &= CopyParameters(PARAMETERS_KEY(NetBT) "\\Interfaces\\Tcpip_", TargetPrefix,
                              PARAMETERS_KEY(NetBT) "\\Interfaces\\Tcpip_", SourcePrefix);
    Success &= CopyParameters(PARAMETERS_KEY(Tcpip) "\\Interfaces\\", TargetPrefix,
                              PARAMETERS_KEY(Tcpip) "\\Interfaces\\", SourcePrefix);
    Success &= CopyParameters(PARAMETERS_KEY(Tcpip6) "\\Interfaces\\", TargetPrefix,
                              PARAMETERS_KEY(Tcpip6) "\\Interfaces\\", SourcePrefix);

    free(TargetPrefix);
    free(SourcePrefix);

    SourcePrefix = GetNetLuid(SourceKey);

    if (SourcePrefix == NULL)
        goto fail3;

    TargetPrefix = GetNetLuid(TargetKey);

    if (TargetPrefix == NULL)
        goto fail4;

    Success &= CopyIpVersion6Addresses(NSI_KEY "\\{eb004a01-9b1a-11d4-9123-0050047759bc}\\10\\", TargetPrefix,
                                       NSI_KEY "\\{eb004a01-9b1a-11d4-9123-0050047759bc}\\10\\", SourcePrefix);

    free(TargetPrefix);
    free(SourcePrefix);

    Log("<====");

    return Success;

fail4:
    Log("fail4");

    free(SourcePrefix);
    goto fail1;

fail3:
    Log("fail3");

fail2:
    Log("fail2");

    free(SourcePrefix);

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

static FORCEINLINE BOOLEAN
__MigrateFromEmulated(
    IN  PTCHAR              Location,
    IN  HDEVINFO            DeviceInfoSet,
    IN  PSP_DEVINFO_DATA    DeviceInfoData
    )
{
    PEMULATED_DEVICE        Table;
    DWORD                   Index;
    BOOLEAN                 Success;
    HRESULT                 Error;
    HKEY                    SourceKey;
    HKEY                    TargetKey;

    Log("====>");

    Table = GetEmulatedDeviceTable();
    if (Table == NULL)
        goto fail1;

    Success = FALSE;

    for (Index = 0; strlen(Table[Index].Alias) != 0; Index++) {
        if (_stricmp(Location, Table[Index].Device) == 0) {
            Error = OpenAliasSoftwareKey(Table[Index].Alias,
                                         &SourceKey);
            if (Error != ERROR_SUCCESS)
                goto fail2;

            Error = OpenSoftwareKey(DeviceInfoSet,
                                    DeviceInfoData,
                                    &TargetKey);
            if (Error != ERROR_SUCCESS)
                goto fail3;

            Success = MigrateSettings(SourceKey, TargetKey);

            RegCloseKey(TargetKey);
            RegCloseKey(SourceKey);

            break;
        }
    }

    free(Table);

    Log("<====");

    return Success;

fail3:
    RegCloseKey(SourceKey);

fail2:
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

static FORCEINLINE BOOLEAN
__MigrateToEmulated(
    IN  HDEVINFO            DeviceInfoSet,
    IN  PSP_DEVINFO_DATA    DeviceInfoData,
    IN  PTCHAR              Location
    )
{
    PEMULATED_DEVICE        Table;
    DWORD                   Index;
    BOOLEAN                 Success;
    HRESULT                 Error;
    HKEY                    SourceKey;
    HKEY                    TargetKey;

    Log("====>");

    Table = GetEmulatedDeviceTable();
    if (Table == NULL)
        goto fail1;

    Success = FALSE;

    for (Index = 0; strlen(Table[Index].Alias) != 0; Index++) {
        if (_stricmp(Location, Table[Index].Device) == 0) {
            Error = OpenSoftwareKey(DeviceInfoSet,
                                    DeviceInfoData,
                                    &SourceKey);
            if (Error != ERROR_SUCCESS)
                goto fail3;

            Error = OpenAliasSoftwareKey(Table[Index].Alias,
                                         &TargetKey);
            if (Error != ERROR_SUCCESS)
                goto fail2;

            Success = MigrateSettings(SourceKey, TargetKey);
            break;
        }
    }

    free(Table);

    Log("<====");

    return Success;

fail3:
    RegCloseKey(SourceKey);

fail2:
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

static FORCEINLINE BOOLEAN
__MigrateFromPV(
    IN  PTCHAR              Location,
    IN  HDEVINFO            DeviceInfoSet,
    IN  PSP_DEVINFO_DATA    DeviceInfoData
    )
{
    HKEY                    TargetKey;
    PTCHAR                  TargetPrefix;
    DWORD                   Length;
    PTCHAR                  SourcePrefix;
    BOOLEAN                 Success;
    HRESULT                 Result;
    HRESULT                 Error;

    Log("====>");

    Success = TRUE;

    Length = (DWORD)((strlen(NET_SETTINGS_KEY) +
                      strlen("\\") +
                      strlen(Location) +
                      strlen("\\") +
                      1) * sizeof (TCHAR));

    SourcePrefix = malloc(Length);
    if (SourcePrefix == NULL)
        goto fail1;

    Result = StringCbPrintf(SourcePrefix,
                            Length,
                            "%s\\%s\\",
                            NET_SETTINGS_KEY,
                            Location);
    if (!SUCCEEDED(Result)) {
        SetLastError(ERROR_BUFFER_OVERFLOW);
        goto fail2;
    }

    Error = OpenSoftwareKey(DeviceInfoSet,
                            DeviceInfoData,
                            &TargetKey);
    if (Error != ERROR_SUCCESS)
        goto fail3;

    TargetPrefix = GetInterface(TargetKey);

    if (TargetPrefix == NULL)
        goto fail4;

    Success &= CopyParameters(PARAMETERS_KEY(NetBT) "\\Interfaces\\Tcpip_", TargetPrefix,
                              SourcePrefix, "nbt");
    Success &= CopyParameters(PARAMETERS_KEY(Tcpip) "\\Interfaces\\", TargetPrefix,
                              SourcePrefix, "tcpip");
    Success &= CopyParameters(PARAMETERS_KEY(Tcpip6) "\\Interfaces\\", TargetPrefix,
                              SourcePrefix, "tcpip6");

    free(TargetPrefix);

    TargetPrefix = GetNetLuid(TargetKey);

    if (TargetPrefix == NULL)
        goto fail5;

    Success &= CopyIpVersion6Addresses(NSI_KEY "\\{eb004a01-9b1a-11d4-9123-0050047759bc}\\10\\", TargetPrefix,
                                       SourcePrefix, "IPv6_Address____");

    free(TargetPrefix);

    RegCloseKey(TargetKey);
    free(SourcePrefix);

    Log("<====");

    return Success;

fail5:
    Log("fail5");

fail4:
    Log("fail4");

    RegCloseKey(TargetKey);

fail3:
    Log("fail3");

fail2:
    Log("fail");

    free(SourcePrefix);

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

static FORCEINLINE BOOLEAN
__MigrateToPV(
    IN  HDEVINFO            DeviceInfoSet,
    IN  PSP_DEVINFO_DATA    DeviceInfoData,
    IN  PTCHAR              Location
    )
{
    PTCHAR                  TargetPrefix;
    DWORD                   Length;
    HKEY                    SourceKey;
    PTCHAR                  SourcePrefix;
    BOOLEAN                 Success;
    HRESULT                 Result;
    HRESULT                 Error;

    Log("====>");

    Success = TRUE;

    Length = (DWORD)((strlen(NET_SETTINGS_KEY) +
                      strlen("\\") +
                      strlen(Location) +
                      strlen("\\") +
                      1) * sizeof (TCHAR));

    TargetPrefix = malloc(Length);
    if (TargetPrefix == NULL)
        goto fail1;

    Result = StringCbPrintf(TargetPrefix,
                            Length,
                            "%s\\%s\\",
                            NET_SETTINGS_KEY,
                            Location);
    if (!SUCCEEDED(Result)) {
        SetLastError(ERROR_BUFFER_OVERFLOW);
        goto fail2;
    }

    Error = OpenSoftwareKey(DeviceInfoSet,
                            DeviceInfoData,
                            &SourceKey);
    if (Error != ERROR_SUCCESS)
        goto fail3;

    SourcePrefix = GetInterface(SourceKey);

    if (SourcePrefix == NULL)
        goto fail4;

    Success &= CopyParameters(TargetPrefix, "nbt",
                              PARAMETERS_KEY(NetBT) "\\Interfaces\\Tcpip_", SourcePrefix);
    Success &= CopyParameters(TargetPrefix, "tcpip",
                              PARAMETERS_KEY(Tcpip) "\\Interfaces\\", SourcePrefix);
    Success &= CopyParameters(TargetPrefix, "tcpip6",
                              PARAMETERS_KEY(Tcpip6) "\\Interfaces\\", SourcePrefix);

    free(SourcePrefix);

    SourcePrefix = GetNetLuid(SourceKey);

    if (SourcePrefix == NULL)
        goto fail5;

    Success &= CopyIpVersion6Addresses(TargetPrefix, "IPv6_Address____",
                                       NSI_KEY "\\{eb004a01-9b1a-11d4-9123-0050047759bc}\\10\\", SourcePrefix);

    free(SourcePrefix);

    RegCloseKey(SourceKey);
    free(TargetPrefix);

    Log("<====");

    return Success;

fail5:
    Log("fail5");

fail4:
    Log("fail4");

    RegCloseKey(SourceKey);

fail3:
    Log("fail3");

fail2:
    Log("fail2");

    free(TargetPrefix);

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
InstallDevice(
    IN  PTCHAR  Class,
    IN  PTCHAR  Device
    )
{
    HKEY        Key;
    DWORD       OldLength;
    DWORD       NewLength;
    DWORD       Type;
    HRESULT     Error;
    PTCHAR      Devices;
    ULONG       Offset;

    Error = RegCreateKeyEx(HKEY_LOCAL_MACHINE,
                           PARAMETERS_KEY(XENFILT),
                           0,
                           NULL,
                           REG_OPTION_NON_VOLATILE,
                           KEY_ALL_ACCESS,
                           NULL,
                           &Key,
                           NULL);
    if (Error != ERROR_SUCCESS) {
        SetLastError(Error);
        goto fail1;
    }

    OldLength = 0;
    Error = RegQueryValueEx(Key,
                            Class,
                            NULL,
                            &Type,
                            NULL,
                            &OldLength);
    if (Error != ERROR_SUCCESS) {
        if (Error != ERROR_FILE_NOT_FOUND) {
            SetLastError(Error);
            goto fail2;
        }

        OldLength = sizeof (TCHAR);
        Type = REG_MULTI_SZ;
    }

    if (Type != REG_MULTI_SZ) {
        SetLastError(ERROR_BAD_FORMAT);
        goto fail3;
    }

    NewLength = OldLength + (DWORD)((strlen(Device) + 1) * sizeof (TCHAR));

    Devices = malloc(NewLength);
    if (Devices == NULL)
        goto fail4;

    memset(Devices, 0, NewLength);

    Offset = 0;
    if (OldLength != sizeof (TCHAR)) {
        Error = RegQueryValueEx(Key,
                                Class,
                                NULL,
                                NULL,
                                (PBYTE)Devices,
                                &OldLength);
        if (Error != ERROR_SUCCESS) {
            SetLastError(Error);
            goto fail5;
        }

        while (Devices[Offset] != '\0') {
            ULONG   DeviceLength;

            DeviceLength = (ULONG)strlen(&Devices[Offset]) / sizeof (TCHAR);

            if (_stricmp(&Devices[Offset], Device) == 0) {
                Log("%s already present", Device);
                goto done;
            }

            Offset += DeviceLength + 1;
        }
    }

    memmove(&Devices[Offset], Device, strlen(Device));
    Log("added %s", Device);

    Error = RegSetValueEx(Key,
                          Class,
                          0,
                          Type,
                          (PBYTE)Devices,
                          NewLength);
    if (Error != ERROR_SUCCESS) {
        SetLastError(Error);
        goto fail6;
    }

done:
    free(Devices);

    RegCloseKey(Key);

    return TRUE;

fail6:
    Log("fail6");

fail5:
    Log("fail5");

    free(Devices);

fail4:
    Log("fail4");

fail3:
    Log("fail3");

fail2:
    Log("fail2");

    RegCloseKey(Key);

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
RemoveDevice(
    IN  PTCHAR  Class,
    IN  PTCHAR  Device
    )
{
    HKEY        Key;
    DWORD       OldLength;
    DWORD       NewLength;
    DWORD       Type;
    HRESULT     Error;
    PTCHAR      Devices;
    ULONG       Offset;
    ULONG       DeviceLength;

    Error = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                         PARAMETERS_KEY(XENFILT),
                         0,
                         KEY_ALL_ACCESS,
                         &Key);
    if (Error != ERROR_SUCCESS) {
        SetLastError(Error);
        goto fail1;
    }

    OldLength = 0;
    Error = RegQueryValueEx(Key,
                            Class,
                            NULL,
                            &Type,
                            NULL,
                            &OldLength);
    if (Error != ERROR_SUCCESS) {
        if (Error != ERROR_FILE_NOT_FOUND) {
            SetLastError(Error);
            goto fail2;
        }

        goto done;
    }

    if (Type != REG_MULTI_SZ) {
        SetLastError(ERROR_BAD_FORMAT);
        goto fail3;
    }

    Devices = malloc(OldLength);
    if (Devices == NULL)
        goto fail4;

    memset(Devices, 0, OldLength);

    Error = RegQueryValueEx(Key,
                            Class,
                            NULL,
                            NULL,
                            (PBYTE)Devices,
                            &OldLength);
    if (Error != ERROR_SUCCESS) {
        SetLastError(Error);
        goto fail5;
    }

    Offset = 0;
    DeviceLength = 0;
    while (Devices[Offset] != '\0') {
        DeviceLength = (ULONG)strlen(&Devices[Offset]) / sizeof (TCHAR);

        if (_stricmp(&Devices[Offset], Device) == 0)
            goto remove;

        Offset += DeviceLength + 1;
    }

    free(Devices);
    goto done;

remove:
    NewLength = OldLength - ((DeviceLength + 1) * sizeof (TCHAR));

    memmove(&Devices[Offset],
            &Devices[Offset + DeviceLength + 1],
            (NewLength - Offset) * sizeof (TCHAR));
            
    Log("removed %s", Device);

    if (NewLength == 1) {
        Error = RegDeleteValue(Key,
                               Class);
    } else {
        Error = RegSetValueEx(Key,
                              Class,
                              0,
                              Type,
                              (PBYTE)Devices,
                              NewLength);
    }
    if (Error != ERROR_SUCCESS) {
        SetLastError(Error);
        goto fail6;
    }

    free(Devices);

done:
    RegCloseKey(Key);

    return TRUE;

fail6:
    Log("fail6");

fail5:
    Log("fail5");

    free(Devices);

fail4:
    Log("fail4");

fail3:
    Log("fail3");

fail2:
    Log("fail2");

    RegCloseKey(Key);

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
IsDeviceEmulated(
    IN  PTCHAR      Class,
    IN  PTCHAR      Device,
    OUT PBOOLEAN    Present
    )
{
    HKEY            Key;
    DWORD           Length;
    DWORD           Type;
    HRESULT         Error;
    PTCHAR          Devices;
    ULONG           Offset;

    *Present = FALSE;

    Error = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                         STATUS_KEY(XENFILT),
                         0,
                         KEY_READ,
                         &Key);
    if (Error != ERROR_SUCCESS) {
        if (Error == ERROR_FILE_NOT_FOUND)
            goto done;

        SetLastError(Error);
        goto fail1;
    }

    Length = 0;
    Error = RegQueryValueEx(Key,
                            Class,
                            NULL,
                            &Type,
                            NULL,
                            &Length);
    if (Error != ERROR_SUCCESS) {
        if (Error == ERROR_FILE_NOT_FOUND)
            goto done;
    }

    if (Type != REG_MULTI_SZ) {
        SetLastError(ERROR_BAD_FORMAT);
        goto fail2;
    }

    Devices = malloc(Length);
    if (Devices == NULL)
        goto fail3;

    memset(Devices, 0, Length);

    Error = RegQueryValueEx(Key,
                            Class,
                            NULL,
                            NULL,
                            (PBYTE)Devices,
                            &Length);
    if (Error != ERROR_SUCCESS) {
        SetLastError(Error);
        goto fail4;
    }

    Offset = 0;
    while (Devices[Offset] != '\0') {
        ULONG   DeviceLength;

        DeviceLength = (ULONG)strlen(&Devices[Offset]) / sizeof (TCHAR);

        if (_stricmp(&Devices[Offset], Device) == 0) {
            *Present = TRUE;
            break;
        }

        Offset += DeviceLength + 1;
    }

    free(Devices);

    RegCloseKey(Key);

done:
    return TRUE;

fail4:
    Log("fail4");

    free(Devices);

fail3:
    Log("fail3");

fail2:
    Log("fail2");

    RegCloseKey(Key);

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
RequestReboot(
    IN  HDEVINFO            DeviceInfoSet,
    IN  PSP_DEVINFO_DATA    DeviceInfoData
    )
{
    SP_DEVINSTALL_PARAMS    DeviceInstallParams;
    HRESULT                 Error;

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

static FORCEINLINE HRESULT
__DifInstallPreProcess(
    IN  HDEVINFO                    DeviceInfoSet,
    IN  PSP_DEVINFO_DATA            DeviceInfoData,
    IN  PCOINSTALLER_CONTEXT_DATA   Context
    )
{
    HRESULT                         Error;
    HKEY                            Key;
    PTCHAR                          Interface;
    BOOLEAN                         NeedMigrateSettings;

    Log("====>");

    Error = OpenSoftwareKey(DeviceInfoSet,
                            DeviceInfoData,
                            &Key);

    if (Error == ERROR_SUCCESS) {
        Interface = GetInterface(Key);
        RegCloseKey(Key);
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
    HRESULT                         Error;
    BOOLEAN                         NeedMigrateSettings;
    PTCHAR                          Description;
    PTCHAR                          Location;
    BOOLEAN                         MigratedSettings;
    BOOLEAN                         Present;
    BOOLEAN                         Success;

    Log("====>");

    Error = Context->InstallResult;
    if (Error != NO_ERROR) {
        SetLastError(Error);
        goto fail1;
    }

    NeedMigrateSettings = (BOOLEAN)(ULONG_PTR)Context->PrivateData;

    Description = GetProperty(DeviceInfoSet, DeviceInfoData, SPDRP_DEVICEDESC);
    if (Description == NULL)
        goto fail2;

    Location = GetProperty(DeviceInfoSet, DeviceInfoData, SPDRP_LOCATION_INFORMATION);
    if (Location == NULL)
        goto fail3;

    Success = SetFriendlyName(DeviceInfoSet, DeviceInfoData, Description, Location);
    if (!Success)
        goto fail4;

    MigratedSettings = FALSE;

    if (NeedMigrateSettings) {
        MigratedSettings = __MigrateFromPV(Location, DeviceInfoSet, DeviceInfoData);
        if (!MigratedSettings)
            MigratedSettings = __MigrateFromEmulated(Location, DeviceInfoSet, DeviceInfoData);
    }

    Success = InstallDevice("VIF", Location);
    if (!Success)
        goto fail5;

    Success = IsDeviceEmulated("VIF", Location, &Present);
    if (!Success)
        goto fail6;

    if (!MigratedSettings && !Present)
        goto done;

    Success = RequestReboot(DeviceInfoSet, DeviceInfoData);
    if (!Success)
        goto fail7;

done:
    free(Location);
    free(Description);

    Log("<====");

    return NO_ERROR;

fail7:
    Log("fail7");

fail6:
    Log("fail6");

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
    PTCHAR                          Location;
    HRESULT                         Error;

    Log("====>");

    Location = GetProperty(DeviceInfoSet, DeviceInfoData, SPDRP_LOCATION_INFORMATION);
    if (Location == NULL)
        goto fail1;

    Context->PrivateData = Location;

    (VOID) __MigrateToEmulated(DeviceInfoSet, DeviceInfoData, Location);
    (VOID) __MigrateToPV(DeviceInfoSet, DeviceInfoData, Location);

    Log("<====");

    return ERROR_DI_POSTPROCESSING_REQUIRED; 

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
__DifRemovePostProcess(
    IN  HDEVINFO                    DeviceInfoSet,
    IN  PSP_DEVINFO_DATA            DeviceInfoData,
    IN  PCOINSTALLER_CONTEXT_DATA   Context
    )
{
    HRESULT                         Error;
    PTCHAR                          Location;
    PEMULATED_DEVICE                Table;
    BOOLEAN                         Success;

    Log("====>");

    Error = Context->InstallResult;
    if (Error != NO_ERROR) {
        SetLastError(Error);
        goto fail1;
    }

    Location = Context->PrivateData;

    Success = RemoveDevice("VIF", Location);
    if (!Success)
        goto fail2;

    Table = GetEmulatedDeviceTable();
    if (Table == NULL) {
        Success = FALSE;
    } else {
        ULONG   Index;

        Success = TRUE;

        for (Index = 0; strlen(Table[Index].Alias) != 0; Index++) {
            if (_stricmp(Location, Table[Index].Device) == 0) {
                Success = RequestReboot(DeviceInfoSet, DeviceInfoData);
                break;
            }
        }

        free(Table);
    }

    if (!Success)
        goto fail3;

    free(Location);

    Log("<====");

    return NO_ERROR;

fail3:
    Log("fail3");

fail2:
    Log("fail2");

    free(Location);

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
            Log("%s PreProcessing",
                FunctionName(Function));

            Error = NO_ERROR;
        } else {
            Log("%s PostProcessing (%08x)",
                FunctionName(Function),
                Context->InstallResult);

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
