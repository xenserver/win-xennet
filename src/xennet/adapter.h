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

#pragma once

#define XENNET_INTERFACE_TYPE           NdisInterfaceInternal

#define XENNET_MEDIA_TYPE               NdisMedium802_3

#define XENNET_MAC_OPTIONS              (NDIS_MAC_OPTION_COPY_LOOKAHEAD_DATA |  \
                                         NDIS_MAC_OPTION_TRANSFERS_NOT_PEND |   \
                                         NDIS_MAC_OPTION_NO_LOOPBACK |          \
                                         NDIS_MAC_OPTION_8021P_PRIORITY |       \
                                         NDIS_MAC_OPTION_SUPPORTS_MAC_ADDRESS_OVERWRITE)

typedef struct _PROPERTIES {
    int ipv4_csum;
    int tcpv4_csum;
    int udpv4_csum;
    int tcpv6_csum;
    int udpv6_csum;
    int need_csum_value;
    int lsov4;
    int lsov6;
    int lrov4;
    int lrov6;
} PROPERTIES, *PPROPERTIES;

struct _ADAPTER {
    LIST_ENTRY              ListEntry;
    PXENVIF_VIF_INTERFACE   VifInterface;
    BOOLEAN                 AcquiredInterfaces;
    ULONG                   MaximumFrameSize;
    ULONG                   CurrentLookahead;
    NDIS_HANDLE             NdisAdapterHandle;
    NDIS_HANDLE             NdisDmaHandle;
    NDIS_PNP_CAPABILITIES   Capabilities;
    PROPERTIES              Properties;
    RECEIVER                Receiver;
    PTRANSMITTER            Transmitter;
    BOOLEAN                 Enabled;
    NDIS_OFFLOAD            Offload;
};

MINIPORT_CANCEL_OID_REQUEST AdapterCancelOidRequest;
VOID
AdapterCancelOidRequest (
    IN  NDIS_HANDLE NdisHandle,
    IN  PVOID       RequestId
    );

MINIPORT_CANCEL_SEND AdapterCancelSendNetBufferLists;
VOID 
AdapterCancelSendNetBufferLists (
    IN  NDIS_HANDLE NdisHandle,
    IN  PVOID       CancelId
    );

MINIPORT_CHECK_FOR_HANG AdapterCheckForHang;
BOOLEAN 
AdapterCheckForHang (
    IN  NDIS_HANDLE NdisHandle
    );

VOID
AdapterDelete (
    IN  OUT PADAPTER* Adapter
    );

MINIPORT_HALT AdapterHalt;
VOID 
AdapterHalt (
    IN  NDIS_HANDLE         NdisHandle,
    IN  NDIS_HALT_ACTION    HaltAction
    );

NDIS_STATUS 
AdapterInitialize (
    IN  PADAPTER    Adapter,
    IN  NDIS_HANDLE AdapterHandle
    );

MINIPORT_OID_REQUEST AdapterOidRequest;
NDIS_STATUS 
AdapterOidRequest (
    IN  NDIS_HANDLE         NdisHandle,
    IN  PNDIS_OID_REQUEST   NdisRequest
    );

MINIPORT_PAUSE AdapterPause;
NDIS_STATUS 
AdapterPause (
    IN  NDIS_HANDLE                     NdisHandle,
    IN  PNDIS_MINIPORT_PAUSE_PARAMETERS MiniportPauseParameters
    );

MINIPORT_DEVICE_PNP_EVENT_NOTIFY AdapterPnPEventHandler;
VOID 
AdapterPnPEventHandler (
    IN  NDIS_HANDLE             NdisHandle,
    IN  PNET_DEVICE_PNP_EVENT   NetDevicePnPEvent
    );

MINIPORT_RESET AdapterReset;
NDIS_STATUS 
AdapterReset (
    IN  NDIS_HANDLE     MiniportAdapterContext,
    OUT PBOOLEAN        AddressingReset
    );

MINIPORT_RESTART AdapterRestart;
NDIS_STATUS 
AdapterRestart (
    IN  NDIS_HANDLE                         MiniportAdapterContext,
    IN  PNDIS_MINIPORT_RESTART_PARAMETERS   MiniportRestartParameters
    );

MINIPORT_RETURN_NET_BUFFER_LISTS AdapterReturnNetBufferLists;
VOID 
AdapterReturnNetBufferLists (
    IN  NDIS_HANDLE         MiniportAdapterContext,
    IN  PNET_BUFFER_LIST    NetBufferLists,
    IN  ULONG               ReturnFlags
    );

MINIPORT_SEND_NET_BUFFER_LISTS AdapterSendNetBufferLists;
VOID 
AdapterSendNetBufferLists (
    IN  NDIS_HANDLE         MiniportAdapterContext,
    IN  PNET_BUFFER_LIST    NetBufferList,
    IN  NDIS_PORT_NUMBER    PortNumber,
    IN  ULONG               SendFlags
    );

MINIPORT_SHUTDOWN AdapterShutdown;
VOID 
AdapterShutdown (
    IN  NDIS_HANDLE             MiniportAdapterContext,
    IN  NDIS_SHUTDOWN_ACTION    ShutdownAction
    );

extern VOID
ReceiverReceivePackets(
    IN  PRECEIVER   Receiver,
    IN  PLIST_ENTRY List
    );
