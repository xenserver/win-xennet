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

VOID
AdapterCancelOidRequest (
    IN  PADAPTER    Adapter,
    IN  PVOID       RequestId
    );

VOID 
AdapterCancelSendNetBufferLists (
    IN  PADAPTER    Adapter,
    IN  PVOID       CancelId
    );

BOOLEAN 
AdapterCheckForHang (
    IN  PADAPTER Adapter
    );

VOID
AdapterDelete (
    IN  OUT PADAPTER* Adapter
    );

VOID 
AdapterHalt (
    IN  PADAPTER            Adapter,
    IN  NDIS_HALT_ACTION    HaltAction
    );

NDIS_STATUS 
AdapterInitialize (
    IN  PADAPTER    Adapter,
    IN  NDIS_HANDLE AdapterHandle
    );

NDIS_STATUS 
AdapterOidRequest (
    IN  PADAPTER            Adapter,
    IN  PNDIS_OID_REQUEST   NdisRequest
    );

NDIS_STATUS 
AdapterPause (
    IN  PADAPTER                        Adapter,
    IN  PNDIS_MINIPORT_PAUSE_PARAMETERS MiniportPauseParameters
    );

VOID 
AdapterPnPEventHandler (
    IN  PADAPTER                Adapter,
    IN  PNET_DEVICE_PNP_EVENT   NetDevicePnPEvent
    );

NDIS_STATUS 
AdapterReset (
    IN  NDIS_HANDLE     MiniportAdapterContext,
    OUT PBOOLEAN        AddressingReset
    );

NDIS_STATUS 
AdapterRestart (
    IN  PADAPTER                            Adapter,
    IN  PNDIS_MINIPORT_RESTART_PARAMETERS   MiniportRestartParameters
    );

VOID 
AdapterReturnNetBufferLists (
    IN  PADAPTER            Adapter,
    IN  PNET_BUFFER_LIST    NetBufferLists,
    IN  ULONG               ReturnFlags
    );

VOID 
AdapterSendNetBufferLists (
    IN  PADAPTER            Adapter,
    IN  PNET_BUFFER_LIST    NetBufferList,
    IN  NDIS_PORT_NUMBER    PortNumber,
    IN  ULONG               SendFlags
    );

VOID 
AdapterShutdown (
    IN  PADAPTER                Adapter,
    IN  NDIS_SHUTDOWN_ACTION    ShutdownAction
    );

extern VOID
ReceiverReceivePackets(
    IN  PRECEIVER   Receiver,
    IN  PLIST_ENTRY List
    );

