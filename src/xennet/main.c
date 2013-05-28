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

#include <version.h>

#include "common.h"

#pragma NDIS_INIT_FUNCTION(DriverEntry)

//
// Global miniport data.
//

static NDIS_HANDLE MiniportDriverHandle;

extern NDIS_STATUS 
MiniportInitialize (
    IN  NDIS_HANDLE                        MiniportAdapterHandle,
    IN  NDIS_HANDLE                        MiniportDriverContext,
    IN  PNDIS_MINIPORT_INIT_PARAMETERS     MiniportInitParameters
    );

typedef struct _XENNET_CONTEXT {
    PDEVICE_CAPABILITIES    Capabilities;
    PIO_COMPLETION_ROUTINE  CompletionRoutine;
    PVOID                   CompletionContext;
    UCHAR                   CompletionControl;
} XENNET_CONTEXT, *PXENNET_CONTEXT;

static NTSTATUS (*NdisDispatchPnp)(PDEVICE_OBJECT, PIRP);

static NTSTATUS
__QueryCapabilities(
    IN  PDEVICE_OBJECT      DeviceObject,
    IN  PIRP                Irp,
    IN  PVOID               _Context
    )
{
    PXENNET_CONTEXT         Context = _Context;
    NTSTATUS                status;

    Trace("====>\n");

    Context->Capabilities->SurpriseRemovalOK = 1;

    if (Context->CompletionRoutine != NULL &&
        (Context->CompletionControl & SL_INVOKE_ON_SUCCESS))
        status = Context->CompletionRoutine(DeviceObject, Irp, Context->CompletionContext);
    else
        status = STATUS_SUCCESS;

    ExFreePool(Context);

    Trace("<====\n");

    return status;
}

NTSTATUS 
QueryCapabilities(
    IN PDEVICE_OBJECT       DeviceObject,
    IN PIRP                 Irp
    )
{
    PIO_STACK_LOCATION      StackLocation;
    PXENNET_CONTEXT         Context;
    NTSTATUS                status;

    Trace("====>\n");

    Trace("%p\n", DeviceObject); 

    StackLocation = IoGetCurrentIrpStackLocation(Irp);

    Context = ExAllocatePoolWithTag(NonPagedPool, sizeof (XENNET_CONTEXT), ' TEN');
    if (Context != NULL) {
        Context->Capabilities = StackLocation->Parameters.DeviceCapabilities.Capabilities;
        Context->CompletionRoutine = StackLocation->CompletionRoutine;
        Context->CompletionContext = StackLocation->Context;
        Context->CompletionControl = StackLocation->Control;

        StackLocation->CompletionRoutine = __QueryCapabilities;
        StackLocation->Context = Context;
        StackLocation->Control = SL_INVOKE_ON_SUCCESS;
    }

    status = NdisDispatchPnp(DeviceObject, Irp);

    Trace("<====\n");

    return status;    
}

NTSTATUS 
DispatchPnp(
    IN PDEVICE_OBJECT   DeviceObject,
    IN PIRP             Irp
    )
{
    PIO_STACK_LOCATION  StackLocation;
    UCHAR               MinorFunction;
    NTSTATUS            status;

    StackLocation = IoGetCurrentIrpStackLocation(Irp);
    MinorFunction = StackLocation->MinorFunction;

    switch (StackLocation->MinorFunction) {
    case IRP_MN_QUERY_CAPABILITIES:
        status = QueryCapabilities(DeviceObject, Irp);
        break;

    default:
        status = NdisDispatchPnp(DeviceObject, Irp);
        break;
    }

    return status;
}

NTSTATUS 
DispatchFail(
    IN PDEVICE_OBJECT   DeviceObject,
    IN PIRP             Irp
    )
{
    NTSTATUS            status;

    UNREFERENCED_PARAMETER(DeviceObject);

    Trace("%p\n", Irp);

    status = STATUS_UNSUCCESSFUL;

    Irp->IoStatus.Status = status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    return status;
}

NTSTATUS 
DriverEntry (
    IN  PDRIVER_OBJECT   DriverObject,
    IN  PUNICODE_STRING  RegistryPath
    )
{
    NDIS_STATUS ndisStatus;
    NDIS_MINIPORT_DRIVER_CHARACTERISTICS mpChars;
    NDIS_CONFIGURATION_OBJECT ConfigurationObject;
    NDIS_HANDLE ConfigurationHandle;
    NDIS_STRING ParameterName;
    PNDIS_CONFIGURATION_PARAMETER ParameterValue;
    ULONG FailCreateClose;
    ULONG FailDeviceControl;

    ExInitializeDriverRuntime(DrvRtPoolNxOptIn);

    Trace("====>\n");

    Info("%s (%s)\n",
         MAJOR_VERSION_STR "." MINOR_VERSION_STR "." MICRO_VERSION_STR "." BUILD_NUMBER_STR,
         DAY_STR "/" MONTH_STR "/" YEAR_STR);

    if (*InitSafeBootMode > 0)
        return NDIS_STATUS_SUCCESS;

    //
    // Register miniport with NDIS.
    //

    NdisZeroMemory(&mpChars, sizeof(mpChars));
    mpChars.Header.Type = NDIS_OBJECT_TYPE_MINIPORT_DRIVER_CHARACTERISTICS,
    mpChars.Header.Size = sizeof(NDIS_MINIPORT_DRIVER_CHARACTERISTICS);
    mpChars.Header.Revision = NDIS_MINIPORT_DRIVER_CHARACTERISTICS_REVISION_1;

    mpChars.MajorNdisVersion = 6;
    mpChars.MinorNdisVersion = 0;
    mpChars.MajorDriverVersion = MAJOR_VERSION;
    mpChars.MinorDriverVersion = MINOR_VERSION;

    mpChars.CancelOidRequestHandler = AdapterCancelOidRequest;
    mpChars.CancelSendHandler = AdapterCancelSendNetBufferLists;
    mpChars.CheckForHangHandlerEx = AdapterCheckForHang;
    mpChars.InitializeHandlerEx = MiniportInitialize;
    mpChars.HaltHandlerEx = AdapterHalt;
    mpChars.OidRequestHandler = AdapterOidRequest;    
    mpChars.PauseHandler = AdapterPause;      
    mpChars.DevicePnPEventNotifyHandler  = AdapterPnPEventHandler;
    mpChars.ResetHandlerEx = AdapterReset;
    mpChars.RestartHandler = AdapterRestart;    
    mpChars.ReturnNetBufferListsHandler  = AdapterReturnNetBufferLists;
    mpChars.SendNetBufferListsHandler = AdapterSendNetBufferLists;
    mpChars.ShutdownHandlerEx = AdapterShutdown;
    mpChars.UnloadHandler = DriverUnload;

    MiniportDriverHandle = NULL;
    ndisStatus = NdisMRegisterMiniportDriver(DriverObject,
                                             RegistryPath,
                                             NULL,
                                             &mpChars,
                                             &MiniportDriverHandle);
    if (ndisStatus != NDIS_STATUS_SUCCESS) {
        Error("Failed (0x%08X) to register miniport.\n", ndisStatus);
        goto fail;
    }

    ConfigurationObject.Header.Type = NDIS_OBJECT_TYPE_CONFIGURATION_OBJECT;
    ConfigurationObject.Header.Revision = NDIS_CONFIGURATION_OBJECT_REVISION_1;
    ConfigurationObject.Header.Size = NDIS_SIZEOF_CONFIGURATION_OBJECT_REVISION_1;
    ConfigurationObject.NdisHandle = MiniportDriverHandle;
    ConfigurationObject.Flags = 0;

    ndisStatus = NdisOpenConfigurationEx(&ConfigurationObject, &ConfigurationHandle);
    if (ndisStatus != NDIS_STATUS_SUCCESS) {
        Error("Failed (0x%08X) to open driver configuration.\n", ndisStatus);
        NdisMDeregisterMiniportDriver(MiniportDriverHandle);
        goto fail;
    }

    RtlInitUnicodeString(&ParameterName, L"FailCreateClose");

    NdisReadConfiguration(&ndisStatus,
                          &ParameterValue,
                          ConfigurationHandle,
                          &ParameterName,
                          NdisParameterInteger);
    if (ndisStatus == NDIS_STATUS_SUCCESS &&
        ParameterValue->ParameterType == NdisParameterInteger)
        FailCreateClose = ParameterValue->ParameterData.IntegerData;
    else
        FailCreateClose = 0;

    RtlInitUnicodeString(&ParameterName, L"FailDeviceControl");

    NdisReadConfiguration(&ndisStatus,
                          &ParameterValue,
                          ConfigurationHandle,
                          &ParameterName,
                          NdisParameterInteger);
    if (ndisStatus == NDIS_STATUS_SUCCESS &&
        ParameterValue->ParameterType == NdisParameterInteger)
        FailDeviceControl = ParameterValue->ParameterData.IntegerData;
    else
        FailDeviceControl = 0;

    NdisCloseConfiguration(ConfigurationHandle);
    ndisStatus = NDIS_STATUS_SUCCESS;

    NdisDispatchPnp = DriverObject->MajorFunction[IRP_MJ_PNP];
    DriverObject->MajorFunction[IRP_MJ_PNP] = DispatchPnp;

    if (FailCreateClose != 0) {
        DriverObject->MajorFunction[IRP_MJ_CREATE] = DispatchFail;
        DriverObject->MajorFunction[IRP_MJ_CLOSE] = DispatchFail;
    }

    if (FailDeviceControl != 0) {
        DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DispatchFail;
    }

fail:
    Trace("<====\n");
    return ndisStatus;
}

VOID 
DriverUnload (
    IN  PDRIVER_OBJECT  DriverObject
    )
{
    UNREFERENCED_PARAMETER(DriverObject);

    Trace("====>\n");

    if (MiniportDriverHandle)
        NdisMDeregisterMiniportDriver(MiniportDriverHandle);

    Trace("<====\n");
}
