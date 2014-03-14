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

#define INITGUID 1

#include "common.h"

#pragma warning( disable : 4098 )

extern NTSTATUS AllocAdapter(PADAPTER *Adapter);

static NTSTATUS
QueryVifInterface(
    IN  PDEVICE_OBJECT      DeviceObject,
    IN  PADAPTER            Adapter
    )
{
    KEVENT                  Event;
    IO_STATUS_BLOCK         StatusBlock;
    PIRP                    Irp;
    PIO_STACK_LOCATION      StackLocation;
    INTERFACE               Interface;
    NTSTATUS                status;

    KeInitializeEvent(&Event, NotificationEvent, FALSE);
    RtlZeroMemory(&StatusBlock, sizeof(IO_STATUS_BLOCK));
    RtlZeroMemory(&Interface, sizeof(INTERFACE));

    Irp = IoBuildSynchronousFsdRequest(IRP_MJ_PNP,
                                       DeviceObject,
                                       NULL,
                                       0,
                                       NULL,
                                       &Event,
                                       &StatusBlock);

    status = STATUS_UNSUCCESSFUL;
    if (Irp == NULL)
        goto fail1;

    StackLocation = IoGetNextIrpStackLocation(Irp);
    StackLocation->MinorFunction = IRP_MN_QUERY_INTERFACE;

    StackLocation->Parameters.QueryInterface.InterfaceType = &GUID_VIF_INTERFACE;
    StackLocation->Parameters.QueryInterface.Size = sizeof (INTERFACE);
    StackLocation->Parameters.QueryInterface.Version = VIF_INTERFACE_VERSION;
    StackLocation->Parameters.QueryInterface.Interface = &Interface;
    
    Irp->IoStatus.Status = STATUS_NOT_SUPPORTED;

    status = IoCallDriver(DeviceObject, Irp);
    if (status == STATUS_PENDING) {
        KeWaitForSingleObject(&Event,
                              Executive,
                              KernelMode,
                              FALSE,
                              NULL);
        status = StatusBlock.Status;
    }

    if (!NT_SUCCESS(status))
        goto fail2;

    status = STATUS_INVALID_PARAMETER;
    if (Interface.Version != VIF_INTERFACE_VERSION)
        goto fail3;

    Adapter->VifInterface = Interface.Context;

    return STATUS_SUCCESS;

fail3:
    Error("fail3\n");

fail2:
    Error("fail2\n");

fail1:
    Error("fail1 (%08x)\n", status);

    return status;
}

NDIS_STATUS 
MiniportInitialize (
    IN  NDIS_HANDLE                        MiniportAdapterHandle,
    IN  NDIS_HANDLE                        MiniportDriverContext,
    IN  PNDIS_MINIPORT_INIT_PARAMETERS     MiniportInitParameters
    )
{
    PADAPTER adapter = NULL;
    NDIS_STATUS ndisStatus;
    PDEVICE_OBJECT pdo;
    NTSTATUS status;

    UNREFERENCED_PARAMETER(MiniportDriverContext);
    UNREFERENCED_PARAMETER(MiniportInitParameters);

    Trace("====>\n");

    status = AllocAdapter(&adapter);

    if (!NT_SUCCESS(status) || adapter == NULL) {
        ndisStatus = NDIS_STATUS_RESOURCES;
        goto exit;
    }

    RtlZeroMemory(adapter, sizeof (ADAPTER));

    pdo = NULL;
    NdisMGetDeviceProperty(MiniportAdapterHandle,
                           &pdo,
                           NULL,
                           NULL,
                           NULL,
                           NULL);

    status = QueryVifInterface(pdo, adapter);
    if (!NT_SUCCESS(status)) {
        ndisStatus = NDIS_STATUS_ADAPTER_NOT_FOUND;
        goto exit;
    }

    adapter->AcquiredInterfaces = TRUE;

    ndisStatus = AdapterInitialize(adapter, MiniportAdapterHandle);
    if (ndisStatus != NDIS_STATUS_SUCCESS) {
        goto exit;
    }

exit:
    if (ndisStatus != NDIS_STATUS_SUCCESS) {
        if (adapter != NULL) {
            AdapterDelete(&adapter);
        }
    }

    Trace("<====\n");
    return ndisStatus;
}
