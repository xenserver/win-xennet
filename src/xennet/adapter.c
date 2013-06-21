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

#pragma warning(disable:4711)

//
// List of supported OIDs.
//

static NDIS_STATUS
AdapterStop (
    IN  PADAPTER    Adapter
    );

static NDIS_STATUS
AdapterSetRegistrationAttributes (
    IN  PADAPTER Adapter
    );

static NDIS_STATUS
AdapterSetGeneralAttributes (
    IN  PADAPTER Adapter
    );

static NDIS_STATUS
AdapterSetOffloadAttributes (
    IN  PADAPTER Adapter
    );

static VOID
AdapterProcessSGList (
    IN PDEVICE_OBJECT       DeviceObject,
    IN PVOID                Reserved,
    IN PSCATTER_GATHER_LIST SGL,
    IN PVOID                Context
    );

static NDIS_STATUS
AdapterSetInformation (
    IN  PADAPTER            Adapter,
    IN  PNDIS_OID_REQUEST   NdisRequest
    );

static NDIS_STATUS
AdapterQueryInformation (
    IN  PADAPTER            Adapter,
    IN  PNDIS_OID_REQUEST   NdisRequest
    );

static NDIS_OID XennetSupportedOids[] =
{
    OID_GEN_SUPPORTED_LIST,
    OID_GEN_HARDWARE_STATUS,
    OID_GEN_MEDIA_SUPPORTED,
    OID_GEN_MEDIA_IN_USE,
    OID_GEN_PHYSICAL_MEDIUM,
    OID_GEN_CURRENT_LOOKAHEAD,
    OID_GEN_MAXIMUM_LOOKAHEAD,
    OID_GEN_MAXIMUM_FRAME_SIZE,
    OID_GEN_MAXIMUM_TOTAL_SIZE,
    OID_GEN_RECEIVE_BLOCK_SIZE,
    OID_GEN_TRANSMIT_BLOCK_SIZE,
    OID_GEN_MAC_OPTIONS,
    OID_GEN_LINK_SPEED,
    OID_GEN_MEDIA_CONNECT_STATUS,
    OID_GEN_VENDOR_DESCRIPTION,
    OID_GEN_VENDOR_DRIVER_VERSION,
    OID_GEN_DRIVER_VERSION,
    OID_GEN_MAXIMUM_SEND_PACKETS,
    OID_GEN_VENDOR_ID,
    OID_GEN_CURRENT_PACKET_FILTER,
    OID_GEN_XMIT_OK,
    OID_GEN_RCV_OK,
    OID_GEN_XMIT_ERROR,
    OID_GEN_RCV_ERROR,
    OID_GEN_RCV_CRC_ERROR,
    OID_GEN_RCV_NO_BUFFER,
    OID_GEN_TRANSMIT_QUEUE_LENGTH,
    OID_GEN_TRANSMIT_BUFFER_SPACE,
    OID_GEN_RECEIVE_BUFFER_SPACE,
    OID_GEN_STATISTICS,
    OID_GEN_DIRECTED_BYTES_XMIT,
    OID_GEN_DIRECTED_FRAMES_XMIT,
    OID_GEN_MULTICAST_BYTES_XMIT,
    OID_GEN_MULTICAST_FRAMES_XMIT,
    OID_GEN_BROADCAST_BYTES_XMIT,
    OID_GEN_BROADCAST_FRAMES_XMIT,
    OID_GEN_DIRECTED_BYTES_RCV,
    OID_GEN_DIRECTED_FRAMES_RCV,
    OID_GEN_MULTICAST_BYTES_RCV,
    OID_GEN_MULTICAST_FRAMES_RCV,
    OID_GEN_BROADCAST_BYTES_RCV,
    OID_GEN_BROADCAST_FRAMES_RCV,
    OID_GEN_INTERRUPT_MODERATION,
    OID_802_3_RCV_ERROR_ALIGNMENT,
    OID_802_3_XMIT_ONE_COLLISION,
    OID_802_3_XMIT_MORE_COLLISIONS,
    OID_OFFLOAD_ENCAPSULATION,
    OID_TCP_OFFLOAD_PARAMETERS,
    OID_PNP_CAPABILITIES,
    OID_PNP_QUERY_POWER,
    OID_PNP_SET_POWER,
};

#define INITIALIZE_NDIS_OBJ_HEADER(obj, type) do {               \
    (obj).Header.Type = NDIS_OBJECT_TYPE_ ## type ;              \
    (obj).Header.Revision = NDIS_ ## type ## _REVISION_1;        \
    (obj).Header.Size = sizeof(obj);                             \
} while (0)

//
// Scatter gather allocate handler callback.
// Should never get called.
//
static VOID
AdapterAllocateComplete (
    IN NDIS_HANDLE              MiniportAdapterContext,
    IN PVOID                    VirtualAddress,
    IN PNDIS_PHYSICAL_ADDRESS   PhysicalAddress,
    IN ULONG                    Length,
    IN PVOID                    Context
    )
{
    UNREFERENCED_PARAMETER(MiniportAdapterContext);
    UNREFERENCED_PARAMETER(VirtualAddress);
    UNREFERENCED_PARAMETER(PhysicalAddress);
    UNREFERENCED_PARAMETER(Length);
    UNREFERENCED_PARAMETER(Context);

    ASSERT(FALSE);

    return;
}

//
// Required NDIS6 handler.
// Should never get called.
//
VOID
AdapterCancelOidRequest (
    IN  PADAPTER    Adapter,
    IN  PVOID       RequestId
    )
{
    UNREFERENCED_PARAMETER(Adapter);
    UNREFERENCED_PARAMETER(RequestId);

    return;
}

//
// Required NDIS6 handler.
// Should never get called.
//

VOID 
AdapterCancelSendNetBufferLists (
    IN  PADAPTER    Adapter,
    IN  PVOID       CancelId
    )
{
    UNREFERENCED_PARAMETER(Adapter);
    UNREFERENCED_PARAMETER(CancelId);

    return;
}

BOOLEAN 
AdapterCheckForHang (
    IN  PADAPTER Adapter
    )
{
    UNREFERENCED_PARAMETER(Adapter);

    return FALSE;
}

//
// Frees resources obtained by AdapterInitialize.
//
static VOID
AdapterCleanup (
    IN  PADAPTER Adapter
    )
{
    Trace("====>\n");

    TransmitterDelete(&Adapter->Transmitter);
    ReceiverCleanup(&Adapter->Receiver);

    if (Adapter->NdisDmaHandle != NULL)
        NdisMDeregisterScatterGatherDma(Adapter->NdisDmaHandle);

    if (Adapter->AcquiredInterfaces) {
        VIF(Release, Adapter->VifInterface);
        Adapter->VifInterface = NULL;
    }

    Trace("<====\n");
    return;
}

//
// Frees adapter storage.
//
VOID
AdapterDelete (
    IN  OUT PADAPTER* Adapter
    )
{
    ASSERT(Adapter != NULL);

    if (*Adapter) {
        AdapterCleanup(*Adapter);
        ExFreePool(*Adapter);
        *Adapter = NULL;
    }

    return;
}

//
// Stops adapter and frees all resources.
//
VOID 
AdapterHalt (
    IN  PADAPTER                Adapter,
    IN  NDIS_HALT_ACTION        HaltAction
    )
{
    NDIS_STATUS ndisStatus;

    UNREFERENCED_PARAMETER(HaltAction);


    ndisStatus = AdapterStop(Adapter);
    if (ndisStatus == NDIS_STATUS_SUCCESS) {
        AdapterDelete(&Adapter);
    }

    return;
}

static VOID
AdapterMediaStateChange(
    IN  PADAPTER                Adapter
    )
{
    NDIS_LINK_STATE             LinkState;
    NDIS_STATUS_INDICATION      StatusIndication;

    NdisZeroMemory(&LinkState, sizeof (NDIS_LINK_STATE));

    LinkState.Header.Revision = NDIS_LINK_STATE_REVISION_1;
    LinkState.Header.Type = NDIS_OBJECT_TYPE_DEFAULT;
    LinkState.Header.Size = sizeof(NDIS_LINK_STATE);

    VIF(QueryMediaState,
        Adapter->VifInterface,
        &LinkState.MediaConnectState,
        &LinkState.RcvLinkSpeed,
        &LinkState.MediaDuplexState);

    if (LinkState.MediaConnectState == MediaConnectStateUnknown) {
        Info("LINK: STATE UNKNOWN\n");
    } else if (LinkState.MediaConnectState == MediaConnectStateDisconnected) {
        Info("LINK: DOWN\n");
    } else {
        ASSERT3U(LinkState.MediaConnectState, ==, MediaConnectStateConnected);

        if (LinkState.MediaDuplexState == MediaDuplexStateHalf) 
            Info("LINK: UP: SPEED=%u DUPLEX=HALF\n", LinkState.RcvLinkSpeed);
        else if (LinkState.MediaDuplexState == MediaDuplexStateFull)
            Info("LINK: UP: SPEED=%u DUPLEX=FULL\n", LinkState.RcvLinkSpeed);
        else
            Info("LINK: UP: SPEED=%u DUPLEX=UNKNOWN\n", LinkState.RcvLinkSpeed);
    }

    LinkState.XmitLinkSpeed = LinkState.RcvLinkSpeed;

    NdisZeroMemory(&StatusIndication, sizeof (NDIS_STATUS_INDICATION));

    StatusIndication.Header.Type = NDIS_OBJECT_TYPE_STATUS_INDICATION;
    StatusIndication.Header.Revision = NDIS_STATUS_INDICATION_REVISION_1;
    StatusIndication.Header.Size = sizeof (NDIS_STATUS_INDICATION);
    StatusIndication.SourceHandle = Adapter->NdisAdapterHandle;
    StatusIndication.StatusCode = NDIS_STATUS_LINK_STATE;
    StatusIndication.StatusBuffer = &LinkState;
    StatusIndication.StatusBufferSize = sizeof (NDIS_LINK_STATE);

    NdisMIndicateStatusEx(Adapter->NdisAdapterHandle, &StatusIndication);
}


//
// Initializes adapter by allocating required resources and connects to 
// netback.
//

static VOID
AdapterVifCallback(
    IN  PVOID                   Context,
    IN  XENVIF_CALLBACK_TYPE    Type,
    ...)
{
    PADAPTER                    Adapter = Context;
    va_list                     Arguments;

    va_start(Arguments, Type);

    switch (Type) {
    case XENVIF_CALLBACK_COMPLETE_PACKETS: {
        PXENVIF_TRANSMITTER_PACKET HeadPacket;

        HeadPacket = va_arg(Arguments, PXENVIF_TRANSMITTER_PACKET);

        TransmitterCompletePackets(Adapter->Transmitter, HeadPacket);
        break;
    }
    case XENVIF_CALLBACK_RECEIVE_PACKETS: {
        PLIST_ENTRY List;

        List = va_arg(Arguments, PLIST_ENTRY);

        ReceiverReceivePackets(&Adapter->Receiver, List);
        break;
    }
    case XENVIF_CALLBACK_MEDIA_STATE_CHANGE: {
        AdapterMediaStateChange(Adapter);
        break;
    }
    }

    va_end(Arguments);
}

NDIS_STATUS
AdapterGetAdvancedSettings(
    IN PADAPTER pAdapter
    )
{
    NDIS_CONFIGURATION_OBJECT configObject;
    NDIS_HANDLE hConfigurationHandle;
    NDIS_STRING ndisValue;
    PNDIS_CONFIGURATION_PARAMETER pNdisData;
    NDIS_STATUS ndisStatus;
    NTSTATUS status;

    configObject.Header.Type = NDIS_OBJECT_TYPE_CONFIGURATION_OBJECT;
    configObject.Header.Revision = NDIS_CONFIGURATION_OBJECT_REVISION_1;
    configObject.Header.Size = NDIS_SIZEOF_CONFIGURATION_OBJECT_REVISION_1;
    configObject.NdisHandle = pAdapter->NdisAdapterHandle;
    configObject.Flags = 0;

    ndisStatus = NdisOpenConfigurationEx(&configObject, &hConfigurationHandle);

    status = STATUS_UNSUCCESSFUL;
    if (ndisStatus != NDIS_STATUS_SUCCESS)
        goto fail1;

#define read_property(field, name, default_val) \
    do { \
        RtlInitUnicodeString(&ndisValue, name); \
        NdisReadConfiguration(&ndisStatus, &pNdisData, hConfigurationHandle, &ndisValue, NdisParameterInteger); \
        if (ndisStatus == NDIS_STATUS_SUCCESS) { \
            pAdapter->Properties.field = pNdisData->ParameterData.IntegerData; \
        } else { \
            pAdapter->Properties.field = default_val; \
        } \
    } while (FALSE);

    read_property(ipv4_csum, L"*IPChecksumOffloadIPv4", 3);
    read_property(tcpv4_csum, L"*TCPChecksumOffloadIPv4", 3);
    read_property(udpv4_csum, L"*UDPChecksumOffloadIPv4", 3);
    read_property(tcpv6_csum, L"*TCPChecksumOffloadIPv6", 3);
    read_property(udpv6_csum, L"*UDPChecksumOffloadIPv6", 3);
    read_property(lsov4, L"*LSOv2IPv4", 1);
    read_property(lsov6, L"*LSOv2IPv6", 1);
    read_property(lrov4, L"LROIPv4", 1);
    read_property(lrov6, L"LROIPv6", 1);
    read_property(need_csum_value, L"NeedChecksumValue", 1);

    NdisCloseConfiguration(hConfigurationHandle);

    return NDIS_STATUS_SUCCESS;

fail1:
    Error("fail1\n");
    return NDIS_STATUS_FAILURE;
}

NDIS_STATUS 
AdapterInitialize (
    IN  PADAPTER    Adapter,
    IN  NDIS_HANDLE AdapterHandle
    )
{
    NDIS_STATUS ndisStatus;
    NDIS_SG_DMA_DESCRIPTION DmaDescription;
    NTSTATUS status;

    Trace("====>\n");

    Adapter->NdisAdapterHandle = AdapterHandle;

    RtlZeroMemory(&Adapter->Capabilities, sizeof (Adapter->Capabilities));

    Adapter->Transmitter = ExAllocatePoolWithTag(NonPagedPool, sizeof(TRANSMITTER), ' TEN');
    if (!Adapter->Transmitter) {
        ndisStatus = NDIS_STATUS_RESOURCES;
        goto exit;
    }

    RtlZeroMemory(Adapter->Transmitter, sizeof (TRANSMITTER));

    ndisStatus = ReceiverInitialize(&Adapter->Receiver);
    if (ndisStatus != NDIS_STATUS_SUCCESS) {
        goto exit;
    }

    ndisStatus = TransmitterInitialize(Adapter->Transmitter, Adapter);
    if (ndisStatus != NDIS_STATUS_SUCCESS) {
        goto exit;
    }

    ndisStatus = AdapterGetAdvancedSettings(Adapter);
    if (ndisStatus != NDIS_STATUS_SUCCESS) {
        goto exit;
    }

    ndisStatus = AdapterSetRegistrationAttributes(Adapter);
    if (ndisStatus != NDIS_STATUS_SUCCESS) {
        goto exit;
    }

    ndisStatus = AdapterSetGeneralAttributes(Adapter);
    if (ndisStatus != NDIS_STATUS_SUCCESS) {
        goto exit;
    }

    ndisStatus = AdapterSetOffloadAttributes(Adapter);
    if (ndisStatus != NDIS_STATUS_SUCCESS) {
        goto exit;
    }

    NdisZeroMemory(&DmaDescription, sizeof(DmaDescription));

    DmaDescription.Header.Type = NDIS_OBJECT_TYPE_SG_DMA_DESCRIPTION;
    DmaDescription.Header.Revision = NDIS_SG_DMA_DESCRIPTION_REVISION_1;
    DmaDescription.Header.Size = sizeof(NDIS_SG_DMA_DESCRIPTION);
    DmaDescription.Flags = NDIS_SG_DMA_64_BIT_ADDRESS;
    DmaDescription.MaximumPhysicalMapping = 65536;    
    DmaDescription.ProcessSGListHandler = AdapterProcessSGList;
    DmaDescription.SharedMemAllocateCompleteHandler = AdapterAllocateComplete;

    ndisStatus = NdisMRegisterScatterGatherDma(Adapter->NdisAdapterHandle,
                                               &DmaDescription,
                                               &Adapter->NdisDmaHandle);
    if (ndisStatus != NDIS_STATUS_SUCCESS)
        Adapter->NdisDmaHandle = NULL;

    ASSERT(!Adapter->Enabled);
    VIF(Acquire, Adapter->VifInterface);

    status = VIF(Enable,
                 Adapter->VifInterface,
                 AdapterVifCallback,
                 Adapter);
    if (NT_SUCCESS(status)) {
        TransmitterEnable(Adapter->Transmitter);
        Adapter->Enabled = TRUE;
        ndisStatus = NDIS_STATUS_SUCCESS;
    } else {
        ndisStatus = NDIS_STATUS_FAILURE;
    }

exit:
    Trace("<==== (%08x)\n", ndisStatus);
    return ndisStatus;
}

//
// Scatter gather process handler callback.
// Should never get called.
//
static VOID
AdapterProcessSGList (
    IN PDEVICE_OBJECT       DeviceObject,
    IN PVOID                Reserved,
    IN PSCATTER_GATHER_LIST SGL,
    IN PVOID                Context
    )
{
    UNREFERENCED_PARAMETER(DeviceObject);
    UNREFERENCED_PARAMETER(Reserved);
    UNREFERENCED_PARAMETER(SGL);
    UNREFERENCED_PARAMETER(Context);

    ASSERT(FALSE);

    return;
}

//
// Get\Set OID handler.
//
NDIS_STATUS 
AdapterOidRequest (
    IN  PADAPTER            Adapter,
    IN  PNDIS_OID_REQUEST   NdisRequest
    )
{
    NDIS_STATUS ndisStatus;

    UNREFERENCED_PARAMETER(Adapter);
    UNREFERENCED_PARAMETER(NdisRequest);
    
    switch (NdisRequest->RequestType) {
        case NdisRequestSetInformation:            
            ndisStatus = AdapterSetInformation(Adapter, NdisRequest);
            break;
                
        case NdisRequestQueryInformation:
        case NdisRequestQueryStatistics:
            ndisStatus = AdapterQueryInformation(Adapter, NdisRequest);
            break;

        default:
            ndisStatus = NDIS_STATUS_NOT_SUPPORTED;
            break;
    };

    return ndisStatus;
}

//
// Temporarily pauses adapter.
//
NDIS_STATUS
AdapterPause (
    IN  PADAPTER                        Adapter,
    IN  PNDIS_MINIPORT_PAUSE_PARAMETERS MiniportPauseParameters
    )
{
    UNREFERENCED_PARAMETER(MiniportPauseParameters);

    Trace("====>\n");

    if (!Adapter->Enabled)
        goto done;

    VIF(Disable,
        Adapter->VifInterface);

    AdapterMediaStateChange(Adapter);

    Adapter->Enabled = FALSE;

done:
    Trace("<====\n");
    return NDIS_STATUS_SUCCESS;
}

//
// Handles PNP and Power events. NOP.
//
VOID 
AdapterPnPEventHandler (
    IN  PADAPTER                Adapter,
    IN  PNET_DEVICE_PNP_EVENT   NetDevicePnPEvent
    )
{
    UNREFERENCED_PARAMETER(Adapter);


    switch (NetDevicePnPEvent->DevicePnPEvent) {
        case NdisDevicePnPEventQueryRemoved:
            break;

        case NdisDevicePnPEventRemoved:
            break;       

        case NdisDevicePnPEventSurpriseRemoved:
            break;

        case NdisDevicePnPEventQueryStopped:
            break;

        case NdisDevicePnPEventStopped:
            break;      
            
        case NdisDevicePnPEventPowerProfileChanged:
            break;      
            
        default:
            break;         
    };

    return;
}

//
// Reports general statistics to NDIS.
//
static NDIS_STATUS 
AdapterQueryGeneralStatistics (
    IN  PADAPTER                Adapter,
    IN  PNDIS_STATISTICS_INFO   NdisStatisticsInfo
    )
{
    NDIS_STATUS ndisStatus = NDIS_STATUS_SUCCESS;
    XENVIF_PACKET_STATISTICS Statistics;

    VIF(QueryPacketStatistics,
        Adapter->VifInterface,
        &Statistics);

    NdisZeroMemory(NdisStatisticsInfo, sizeof(NDIS_STATISTICS_INFO));
    NdisStatisticsInfo->Header.Revision = NDIS_OBJECT_REVISION_1;
    NdisStatisticsInfo->Header.Type = NDIS_OBJECT_TYPE_DEFAULT;
    NdisStatisticsInfo->Header.Size = sizeof(NDIS_STATISTICS_INFO);

    NdisStatisticsInfo->SupportedStatistics |= NDIS_STATISTICS_FLAGS_VALID_RCV_ERROR;
    NdisStatisticsInfo->ifInErrors =
        Statistics.Receiver.BackendError +
        Statistics.Receiver.FrontendError;

    NdisStatisticsInfo->SupportedStatistics |= NDIS_STATISTICS_FLAGS_VALID_RCV_DISCARDS;
    NdisStatisticsInfo->ifInDiscards = Statistics.Receiver.Drop;

    NdisStatisticsInfo->SupportedStatistics |= NDIS_STATISTICS_FLAGS_VALID_BYTES_RCV;
    NdisStatisticsInfo->ifHCInOctets = Statistics.Receiver.UnicastBytes +
                                       Statistics.Receiver.MulticastBytes +
                                       Statistics.Receiver.BroadcastBytes;

    NdisStatisticsInfo->SupportedStatistics |= NDIS_STATISTICS_FLAGS_VALID_DIRECTED_BYTES_RCV;
    NdisStatisticsInfo->ifHCInUcastOctets = Statistics.Receiver.UnicastBytes;

    NdisStatisticsInfo->SupportedStatistics |= NDIS_STATISTICS_FLAGS_VALID_DIRECTED_FRAMES_RCV;
    NdisStatisticsInfo->ifHCInUcastPkts = Statistics.Receiver.Unicast;

    NdisStatisticsInfo->SupportedStatistics |= NDIS_STATISTICS_FLAGS_VALID_MULTICAST_BYTES_RCV;
    NdisStatisticsInfo->ifHCInMulticastOctets = Statistics.Receiver.MulticastBytes;  

    NdisStatisticsInfo->SupportedStatistics |= NDIS_STATISTICS_FLAGS_VALID_MULTICAST_FRAMES_RCV;
    NdisStatisticsInfo->ifHCInMulticastPkts = Statistics.Receiver.Multicast;  

    NdisStatisticsInfo->SupportedStatistics |= NDIS_STATISTICS_FLAGS_VALID_BROADCAST_BYTES_RCV;
    NdisStatisticsInfo->ifHCInBroadcastOctets = Statistics.Receiver.BroadcastBytes;  

    NdisStatisticsInfo->SupportedStatistics |= NDIS_STATISTICS_FLAGS_VALID_BROADCAST_FRAMES_RCV;
    NdisStatisticsInfo->ifHCInBroadcastPkts = Statistics.Receiver.Broadcast;  

    NdisStatisticsInfo->SupportedStatistics |= NDIS_STATISTICS_FLAGS_VALID_XMIT_ERROR;
    NdisStatisticsInfo->ifOutErrors =
        Statistics.Transmitter.BackendError +
        Statistics.Transmitter.FrontendError;

    NdisStatisticsInfo->SupportedStatistics |= NDIS_STATISTICS_FLAGS_VALID_BYTES_XMIT;
    NdisStatisticsInfo->ifHCOutOctets = Statistics.Transmitter.UnicastBytes + 
                                        Statistics.Transmitter.MulticastBytes + 
                                        Statistics.Transmitter.BroadcastBytes;

    NdisStatisticsInfo->SupportedStatistics |= NDIS_STATISTICS_FLAGS_VALID_DIRECTED_BYTES_XMIT;
    NdisStatisticsInfo->ifHCOutUcastOctets = Statistics.Transmitter.UnicastBytes;     

    NdisStatisticsInfo->SupportedStatistics |= NDIS_STATISTICS_FLAGS_VALID_DIRECTED_FRAMES_XMIT;
    NdisStatisticsInfo->ifHCOutUcastPkts = Statistics.Transmitter.Unicast;     

    NdisStatisticsInfo->SupportedStatistics |= NDIS_STATISTICS_FLAGS_VALID_MULTICAST_BYTES_XMIT;    
    NdisStatisticsInfo->ifHCOutMulticastOctets = Statistics.Transmitter.MulticastBytes; 

    NdisStatisticsInfo->SupportedStatistics |= NDIS_STATISTICS_FLAGS_VALID_MULTICAST_FRAMES_XMIT;    
    NdisStatisticsInfo->ifHCOutMulticastPkts = Statistics.Transmitter.MulticastBytes;

    NdisStatisticsInfo->SupportedStatistics |= NDIS_STATISTICS_FLAGS_VALID_BROADCAST_BYTES_XMIT;
    NdisStatisticsInfo->ifHCOutBroadcastOctets = Statistics.Transmitter.BroadcastBytes; 

    NdisStatisticsInfo->SupportedStatistics |= NDIS_STATISTICS_FLAGS_VALID_BROADCAST_FRAMES_XMIT;
    NdisStatisticsInfo->ifHCOutBroadcastPkts = Statistics.Transmitter.Broadcast;

    NdisStatisticsInfo->SupportedStatistics |= NDIS_STATISTICS_FLAGS_VALID_XMIT_DISCARDS;
    NdisStatisticsInfo->ifOutDiscards = 0;

    return ndisStatus;
}

static VOID
GetPacketFilter(PADAPTER Adapter, PULONG PacketFilter)
{
    XENVIF_MAC_FILTER_LEVEL UnicastFilterLevel;
    XENVIF_MAC_FILTER_LEVEL MulticastFilterLevel;
    XENVIF_MAC_FILTER_LEVEL BroadcastFilterLevel;

    VIF(QueryFilterLevel,
        Adapter->VifInterface,
        ETHERNET_ADDRESS_UNICAST,
        &UnicastFilterLevel);

    VIF(QueryFilterLevel,
        Adapter->VifInterface,
        ETHERNET_ADDRESS_MULTICAST,
        &MulticastFilterLevel);

    VIF(QueryFilterLevel,
        Adapter->VifInterface,
        ETHERNET_ADDRESS_BROADCAST,
        &BroadcastFilterLevel);

    *PacketFilter = 0;

    if (UnicastFilterLevel == MAC_FILTER_ALL) {
        ASSERT3U(MulticastFilterLevel, ==, MAC_FILTER_ALL);
        ASSERT3U(BroadcastFilterLevel, ==, MAC_FILTER_ALL);

        *PacketFilter |= NDIS_PACKET_TYPE_PROMISCUOUS;
        return;
    } else if (UnicastFilterLevel == MAC_FILTER_MATCHING) {
        *PacketFilter |= NDIS_PACKET_TYPE_DIRECTED;
    }

    if (MulticastFilterLevel == MAC_FILTER_ALL)
        *PacketFilter |= NDIS_PACKET_TYPE_ALL_MULTICAST;
    else if (MulticastFilterLevel == MAC_FILTER_MATCHING)
        *PacketFilter |= NDIS_PACKET_TYPE_MULTICAST;

    if (BroadcastFilterLevel == MAC_FILTER_ALL)
        *PacketFilter |= NDIS_PACKET_TYPE_BROADCAST;
}

#define MIN(_x, _y) (((_x) < (_y)) ? (_x) : (_y))

//
// Handles OID queries.
//
static NDIS_STATUS 
AdapterQueryInformation (
    IN  PADAPTER            Adapter,
    IN  PNDIS_OID_REQUEST   NdisRequest
    )
{
    ULONG bytesAvailable = 0;
    ULONG bytesNeeded = 0;
    ULONG bytesWritten = 0;
    BOOLEAN doCopy = TRUE;
    PVOID info = NULL;
    ULONGLONG infoData;
    ULONG informationBufferLength;
    PVOID informationBuffer;
    NDIS_INTERRUPT_MODERATION_PARAMETERS intModParams;
    NDIS_STATUS ndisStatus = NDIS_STATUS_SUCCESS;
    NDIS_OID oid;

    informationBuffer = NdisRequest->DATA.QUERY_INFORMATION.InformationBuffer;
    informationBufferLength = NdisRequest->DATA.QUERY_INFORMATION.InformationBufferLength;
    oid = NdisRequest->DATA.QUERY_INFORMATION.Oid;
    switch (oid) {
        case OID_PNP_CAPABILITIES:
            Trace("PNP_CAPABILITIES\n");

            info = &Adapter->Capabilities;
            bytesAvailable = sizeof(Adapter->Capabilities);
            break;

        case OID_PNP_QUERY_POWER:
            Trace("QUERY_POWER\n");

            bytesNeeded = sizeof(NDIS_DEVICE_POWER_STATE);
            if (informationBufferLength >= bytesNeeded) {
                PNDIS_DEVICE_POWER_STATE state;

                state = (PNDIS_DEVICE_POWER_STATE)informationBuffer;
                switch (*state) {
                case NdisDeviceStateD0:
                    Trace("D0\n");
                    break;

                case NdisDeviceStateD1:
                    Trace("D1\n");
                    break;

                case NdisDeviceStateD2:
                    Trace("D2\n");
                    break;

                case NdisDeviceStateD3:
                    Trace("D3\n");
                    break;
                }
            }
            break;

        case OID_GEN_SUPPORTED_LIST:
            info = &XennetSupportedOids[0];
            bytesAvailable = sizeof(XennetSupportedOids);
            break;

        case OID_GEN_HARDWARE_STATUS:
            infoData = NdisHardwareStatusReady;
            info = &infoData;
            bytesAvailable = sizeof(ULONG);
            break;

        case OID_GEN_MEDIA_SUPPORTED:
        case OID_GEN_MEDIA_IN_USE:
            infoData = XENNET_MEDIA_TYPE;
            info = &infoData;
            bytesAvailable = sizeof(ULONG);
            break;

        case OID_GEN_MAXIMUM_LOOKAHEAD:
            infoData = Adapter->MaximumFrameSize;
            info = &infoData;
            bytesAvailable = sizeof(ULONG);
            break;

        case OID_GEN_TRANSMIT_BUFFER_SPACE:
            VIF(QueryTransmitterRingSize,
                Adapter->VifInterface,
                (PULONG)&infoData);
            infoData *= Adapter->MaximumFrameSize;
            info = &infoData;
            bytesAvailable = sizeof(ULONG);
            break;

        case OID_GEN_RECEIVE_BUFFER_SPACE:
            VIF(QueryTransmitterRingSize,
                Adapter->VifInterface,
                (PULONG)&infoData);
            infoData *= Adapter->MaximumFrameSize;
            info = &infoData;
            bytesAvailable = sizeof(ULONG);
            break;

        case OID_GEN_TRANSMIT_BLOCK_SIZE:
        case OID_GEN_RECEIVE_BLOCK_SIZE:
            infoData = Adapter->MaximumFrameSize;
            info = &infoData;
            bytesAvailable = sizeof(ULONG);
            break;

        case OID_GEN_VENDOR_DESCRIPTION:
            info = "Citrix";
            bytesAvailable = (ULONG)strlen(info) + 1;
            break;

        case OID_GEN_VENDOR_DRIVER_VERSION:
            infoData = ((MAJOR_VERSION << 8) | MINOR_VERSION) << 8;
            info = &infoData;
            bytesAvailable = sizeof(ULONG);
            break;

        case OID_GEN_DRIVER_VERSION:
            infoData = (6 << 8) | 0;
            info = &infoData;
            bytesAvailable = sizeof(ULONG);
            break;

        case OID_GEN_MAC_OPTIONS:
            infoData = XENNET_MAC_OPTIONS;
            info = &infoData;
            bytesAvailable = sizeof(ULONG);
            break;
        
        case OID_GEN_STATISTICS:
            doCopy = FALSE;

            bytesAvailable = sizeof(NDIS_STATISTICS_INFO);
            if (informationBufferLength >= bytesAvailable) {
                ndisStatus = AdapterQueryGeneralStatistics(Adapter, 
                                                           informationBuffer);

            }

            break;

        case OID_802_3_MULTICAST_LIST: {
            ULONG Count;

            doCopy = FALSE;

            VIF(QueryMulticastAddresses,
                Adapter->VifInterface,
                NULL,
                &Count);
            bytesAvailable = Count * ETHERNET_ADDRESS_LENGTH;

            if (informationBufferLength >= bytesAvailable) {
                NTSTATUS status;

                status = VIF(QueryMulticastAddresses,
                             Adapter->VifInterface,
                             informationBuffer,
                             &Count);
                if (!NT_SUCCESS(status))
                    ndisStatus = NDIS_STATUS_FAILURE;
            }

            break;
        }
        case OID_802_3_PERMANENT_ADDRESS:
            VIF(QueryPermanentAddress,
                Adapter->VifInterface,
                (PETHERNET_ADDRESS)&infoData);
            info = &infoData;
            bytesAvailable = sizeof (ETHERNET_ADDRESS);
            break;

        case OID_802_3_CURRENT_ADDRESS:
            VIF(QueryCurrentAddress,
                Adapter->VifInterface,
                (PETHERNET_ADDRESS)&infoData);
            info = &infoData;
            bytesAvailable = sizeof (ETHERNET_ADDRESS);
            break;

        case OID_GEN_MAXIMUM_FRAME_SIZE:
            infoData = Adapter->MaximumFrameSize -
                       sizeof (ETHERNET_TAGGED_HEADER);
            info = &infoData;
            bytesAvailable = sizeof(ULONG);
            break;

        case OID_GEN_MAXIMUM_TOTAL_SIZE:
            infoData = Adapter->MaximumFrameSize -
                       sizeof (ETHERNET_TAGGED_HEADER) +
                       sizeof (ETHERNET_UNTAGGED_HEADER);

            info = &infoData;
            bytesAvailable = sizeof(ULONG);
            break;

        case OID_GEN_CURRENT_LOOKAHEAD:
            infoData = Adapter->CurrentLookahead;
            info = &infoData;
            bytesAvailable = sizeof(ULONG);
            break;

        case OID_GEN_VENDOR_ID:
            infoData = 0x5853;
            info = &infoData;
            bytesAvailable = sizeof(ULONG);
            break;

        case OID_GEN_LINK_SPEED: {
            ULONG64 LinkSpeed;

            VIF(QueryMediaState,
                Adapter->VifInterface,
                NULL,
                &LinkSpeed,
                NULL);

            infoData = (ULONG)(LinkSpeed / 100);
            info = &infoData;
            bytesAvailable = sizeof(ULONG);
            break;
        }
        case OID_GEN_MEDIA_CONNECT_STATUS: {
            NET_IF_MEDIA_CONNECT_STATE MediaConnectState;

            VIF(QueryMediaState,
                Adapter->VifInterface,
                &MediaConnectState,
                NULL,
                NULL);

            infoData = (MediaConnectState != MediaConnectStateDisconnected) ?
                       NdisMediaStateConnected :
                       NdisMediaStateDisconnected;
            info = &infoData;
            bytesAvailable = sizeof(ULONG);
            break;
        }
        case OID_GEN_MAXIMUM_SEND_PACKETS:
            infoData = 16;
            info = &infoData;
            bytesAvailable = sizeof(ULONG);
            break;

        case OID_GEN_CURRENT_PACKET_FILTER:
            GetPacketFilter(Adapter, (PULONG)&infoData);
            info = &infoData;
            bytesAvailable = sizeof(ULONG);
            break;

        case OID_GEN_XMIT_OK: {
            XENVIF_PACKET_STATISTICS    Statistics;

            VIF(QueryPacketStatistics,
                Adapter->VifInterface,
                &Statistics);

            infoData = Statistics.Transmitter.Unicast +
                       Statistics.Transmitter.Multicast +
                       Statistics.Transmitter.Broadcast;

            info = &infoData;
            bytesAvailable = sizeof(ULONGLONG);
            break;
        }
        case OID_GEN_RCV_OK: {
            XENVIF_PACKET_STATISTICS    Statistics;

            VIF(QueryPacketStatistics,
                Adapter->VifInterface,
                &Statistics);

            infoData = Statistics.Receiver.Unicast +
                       Statistics.Receiver.Multicast +
                       Statistics.Receiver.Broadcast;

            info = &infoData;
            bytesAvailable = sizeof(ULONGLONG);
            break;
        }
        case OID_GEN_XMIT_ERROR: {
            XENVIF_PACKET_STATISTICS    Statistics;

            VIF(QueryPacketStatistics,
                Adapter->VifInterface,
                &Statistics);

            infoData = (ULONG)(Statistics.Transmitter.BackendError +
                               Statistics.Transmitter.FrontendError);
            info = &infoData;
            bytesAvailable = sizeof(ULONG);
            break;
        }
        case OID_GEN_RCV_ERROR: {
            XENVIF_PACKET_STATISTICS    Statistics;

            VIF(QueryPacketStatistics,
                Adapter->VifInterface,
                &Statistics);

            infoData = (ULONG)(Statistics.Receiver.BackendError +
                               Statistics.Receiver.FrontendError);
            info = &infoData;
            bytesAvailable = sizeof(ULONG);
            break;
        }
        case OID_GEN_RCV_NO_BUFFER:
            infoData = 0;   // We'd need to query VIF TX drop stats from dom0
            info = &infoData;
            bytesAvailable = sizeof(ULONG);
            break;

        case OID_GEN_TRANSMIT_QUEUE_LENGTH:
            infoData = 0;
            info = &infoData;
            bytesAvailable = sizeof(ULONG);
            break;

        case OID_802_3_MAXIMUM_LIST_SIZE:
            infoData = MAXIMUM_MULTICAST_ADDRESS_COUNT;
            info = &infoData;
            bytesAvailable = sizeof(ULONG);
            break;

        case OID_IP4_OFFLOAD_STATS:
        case OID_IP6_OFFLOAD_STATS:
        case OID_GEN_SUPPORTED_GUIDS:
            ndisStatus = NDIS_STATUS_NOT_SUPPORTED;
            break;

        case OID_GEN_RCV_CRC_ERROR:
            infoData = 0;
            info = &infoData;
            bytesAvailable = sizeof(ULONG);
            break;

        case OID_802_3_RCV_ERROR_ALIGNMENT:
        case OID_802_3_XMIT_ONE_COLLISION:
        case OID_802_3_XMIT_MORE_COLLISIONS:
            infoData = 0;
            info = &infoData;
            bytesAvailable = sizeof(ULONG);
            break;

        case OID_GEN_DIRECTED_BYTES_XMIT: {
            XENVIF_PACKET_STATISTICS    Statistics;

            VIF(QueryPacketStatistics,
                Adapter->VifInterface,
                &Statistics);

            infoData = (ULONG)Statistics.Transmitter.UnicastBytes;
            info = &infoData;
            bytesAvailable = sizeof(ULONG);
            break;
        }
        case OID_GEN_DIRECTED_FRAMES_XMIT: {
            XENVIF_PACKET_STATISTICS    Statistics;

            VIF(QueryPacketStatistics,
                Adapter->VifInterface,
                &Statistics);

            infoData = (ULONG)Statistics.Transmitter.Unicast;
            info = &infoData;
            bytesAvailable = sizeof(ULONG);
            break;
        }
        case OID_GEN_MULTICAST_BYTES_XMIT: {
            XENVIF_PACKET_STATISTICS    Statistics;

            VIF(QueryPacketStatistics,
                Adapter->VifInterface,
                &Statistics);

            infoData = (ULONG)Statistics.Transmitter.MulticastBytes;
            info = &infoData;
            bytesAvailable = sizeof(ULONG);
            break;
        }
        case OID_GEN_MULTICAST_FRAMES_XMIT: {
            XENVIF_PACKET_STATISTICS    Statistics;

            VIF(QueryPacketStatistics,
                Adapter->VifInterface,
                &Statistics);

            infoData = (ULONG)Statistics.Transmitter.Multicast;
            info = &infoData;
            bytesAvailable = sizeof(ULONG);
            break;
        }
        case OID_GEN_BROADCAST_BYTES_XMIT: {
            XENVIF_PACKET_STATISTICS    Statistics;

            VIF(QueryPacketStatistics,
                Adapter->VifInterface,
                &Statistics);

            infoData = (ULONG)Statistics.Transmitter.BroadcastBytes;
            info = &infoData;
            bytesAvailable = sizeof(ULONG);
            break;
        }
        case OID_GEN_BROADCAST_FRAMES_XMIT: {
            XENVIF_PACKET_STATISTICS    Statistics;

            VIF(QueryPacketStatistics,
                Adapter->VifInterface,
                &Statistics);

            infoData = (ULONG)Statistics.Transmitter.Broadcast;
            info = &infoData;
            bytesAvailable = sizeof(ULONG);
            break;
        }
        case OID_GEN_DIRECTED_BYTES_RCV: {
            XENVIF_PACKET_STATISTICS    Statistics;

            VIF(QueryPacketStatistics,
                Adapter->VifInterface,
                &Statistics);

            infoData = (ULONG)Statistics.Receiver.UnicastBytes;
            info = &infoData;
            bytesAvailable = sizeof(ULONG);
            break;
        }
        case OID_GEN_DIRECTED_FRAMES_RCV: {
            XENVIF_PACKET_STATISTICS    Statistics;

            VIF(QueryPacketStatistics,
                Adapter->VifInterface,
                &Statistics);

            infoData = (ULONG)Statistics.Receiver.Unicast;
            info = &infoData;
            bytesAvailable = sizeof(ULONG);
            break;
        }
        case OID_GEN_MULTICAST_BYTES_RCV: {
            XENVIF_PACKET_STATISTICS    Statistics;

            VIF(QueryPacketStatistics,
                Adapter->VifInterface,
                &Statistics);

            infoData = (ULONG)Statistics.Receiver.MulticastBytes;
            info = &infoData;
            bytesAvailable = sizeof(ULONG);
            break;
        }
        case OID_GEN_MULTICAST_FRAMES_RCV: {
            XENVIF_PACKET_STATISTICS    Statistics;

            VIF(QueryPacketStatistics,
                Adapter->VifInterface,
                &Statistics);

            infoData = (ULONG)Statistics.Receiver.Multicast;
            info = &infoData;
            bytesAvailable = sizeof(ULONG);
            break;
        }
        case OID_GEN_BROADCAST_BYTES_RCV: {
            XENVIF_PACKET_STATISTICS    Statistics;

            VIF(QueryPacketStatistics,
                Adapter->VifInterface,
                &Statistics);

            infoData = (ULONG)Statistics.Receiver.BroadcastBytes;
            info = &infoData;
            bytesAvailable = sizeof(ULONG);
            break;
        }
        case OID_GEN_BROADCAST_FRAMES_RCV: {
            XENVIF_PACKET_STATISTICS    Statistics;

            VIF(QueryPacketStatistics,
                Adapter->VifInterface,
                &Statistics);

            infoData = (ULONG)Statistics.Receiver.Broadcast;
            info = &infoData;
            bytesAvailable = sizeof(ULONG);
            break;
        }
        case OID_GEN_INTERRUPT_MODERATION:
            intModParams.Header.Type = NDIS_OBJECT_TYPE_DEFAULT;
            intModParams.Header.Revision = NDIS_INTERRUPT_MODERATION_PARAMETERS_REVISION_1;
            intModParams.Header.Size = sizeof(NDIS_INTERRUPT_MODERATION_PARAMETERS);
            intModParams.Flags = 0;
            intModParams.InterruptModeration = NdisInterruptModerationNotSupported;
            info = &intModParams;
            bytesAvailable = sizeof(intModParams);
            break;

        // We don't handle these since NDIS 6.0 is supposed to do this for us
        case OID_GEN_MAC_ADDRESS:
        case OID_GEN_MAX_LINK_SPEED:
            ndisStatus = NDIS_STATUS_NOT_SUPPORTED;
            break;

		// ignore these common unwanted OIDs
		case OID_GEN_INIT_TIME_MS:
		case OID_GEN_RESET_COUNTS:
		case OID_GEN_MEDIA_SENSE_COUNTS:
            ndisStatus = NDIS_STATUS_NOT_SUPPORTED;
            break;

        default:
            ndisStatus = NDIS_STATUS_NOT_SUPPORTED;
            break;
    };

    if (ndisStatus == NDIS_STATUS_SUCCESS) {
        if (bytesAvailable <= informationBufferLength) {
            bytesNeeded = bytesAvailable;
            bytesWritten = bytesAvailable;
        } else {
            bytesNeeded = bytesAvailable;
            bytesWritten = informationBufferLength;
            ndisStatus = NDIS_STATUS_BUFFER_TOO_SHORT;
        }

        if (bytesWritten && doCopy) {
            NdisMoveMemory(informationBuffer, info, bytesWritten);

            if (oid == OID_GEN_XMIT_OK || oid == OID_GEN_RCV_OK)
                ndisStatus = NDIS_STATUS_SUCCESS;
        }
    }
    
    NdisRequest->DATA.QUERY_INFORMATION.BytesWritten = bytesWritten;
    NdisRequest->DATA.QUERY_INFORMATION.BytesNeeded = bytesNeeded;
    return ndisStatus;
}

NDIS_STATUS 
AdapterReset (
    IN  NDIS_HANDLE     MiniportAdapterContext,
    OUT PBOOLEAN        AddressingReset
    )
{
    UNREFERENCED_PARAMETER(MiniportAdapterContext);


    *AddressingReset = FALSE;

    return NDIS_STATUS_SUCCESS;
}

//
// Restarts a paused adapter.
//
NDIS_STATUS
AdapterRestart (
    IN  PADAPTER                            Adapter,
    IN  PNDIS_MINIPORT_RESTART_PARAMETERS   MiniportRestartParameters
    )
{
    NTSTATUS                                status;
    NDIS_STATUS                             ndisStatus;

    UNREFERENCED_PARAMETER(MiniportRestartParameters);

    Trace("====>\n");

    if (Adapter->Enabled) {
        ndisStatus = NDIS_STATUS_SUCCESS;
        goto done;
    }

    status = VIF(Enable,
                 Adapter->VifInterface,
                 AdapterVifCallback,
                 Adapter);
    if (NT_SUCCESS(status)) {
        TransmitterEnable(Adapter->Transmitter);
        Adapter->Enabled = TRUE;
        ndisStatus = NDIS_STATUS_SUCCESS;
    } else {
        ndisStatus = NDIS_STATUS_FAILURE;
    }

done:
    Trace("<====\n");
    return ndisStatus;
}

//
// Recycle of received net buffer lists.
//
VOID 
AdapterReturnNetBufferLists (
    IN  PADAPTER            Adapter,
    IN  PNET_BUFFER_LIST    NetBufferLists,
    IN  ULONG               ReturnFlags
    )
{
    ReceiverReturnNetBufferLists(&Adapter->Receiver,
                                 NetBufferLists,
                                 ReturnFlags);

    return;
}

//
// Used to send net buffer lists.
//
VOID 
AdapterSendNetBufferLists (
    IN  PADAPTER            Adapter,
    IN  PNET_BUFFER_LIST    NetBufferList,
    IN  NDIS_PORT_NUMBER    PortNumber,
    IN  ULONG               SendFlags
    )
{
    TransmitterSendNetBufferLists(Adapter->Transmitter,
                                  NetBufferList,
                                  PortNumber,
                                  SendFlags);
}

#define XENNET_MEDIA_MAX_SPEED 1000000000ull

#define XENNET_SUPPORTED_PACKET_FILTERS     \
        (NDIS_PACKET_TYPE_DIRECTED |        \
         NDIS_PACKET_TYPE_MULTICAST |       \
         NDIS_PACKET_TYPE_ALL_MULTICAST |   \
         NDIS_PACKET_TYPE_BROADCAST |       \
         NDIS_PACKET_TYPE_PROMISCUOUS)

//
// Sets general adapter attributes. 
//
static NDIS_STATUS
AdapterSetGeneralAttributes (
    IN  PADAPTER Adapter
    )
{
    PNDIS_MINIPORT_ADAPTER_ATTRIBUTES adapterAttributes;
    NDIS_MINIPORT_ADAPTER_GENERAL_ATTRIBUTES generalAttributes;
    NDIS_STATUS ndisStatus;

    NdisZeroMemory(&generalAttributes, 
                   sizeof(NDIS_MINIPORT_ADAPTER_GENERAL_ATTRIBUTES));

    generalAttributes.Header.Type = 
                    NDIS_OBJECT_TYPE_MINIPORT_ADAPTER_GENERAL_ATTRIBUTES;

    generalAttributes.Header.Revision = 
                    NDIS_MINIPORT_ADAPTER_GENERAL_ATTRIBUTES_REVISION_1;

    generalAttributes.Header.Size = 
                    sizeof(NDIS_MINIPORT_ADAPTER_GENERAL_ATTRIBUTES);

    generalAttributes.MediaType = XENNET_MEDIA_TYPE;

    VIF(QueryMaximumFrameSize,
        Adapter->VifInterface,
        (PULONG)&Adapter->MaximumFrameSize);

    generalAttributes.MtuSize = Adapter->MaximumFrameSize - sizeof (ETHERNET_TAGGED_HEADER);
    generalAttributes.MaxXmitLinkSpeed = XENNET_MEDIA_MAX_SPEED;
    generalAttributes.MaxRcvLinkSpeed = XENNET_MEDIA_MAX_SPEED;
    generalAttributes.XmitLinkSpeed = XENNET_MEDIA_MAX_SPEED;
    generalAttributes.RcvLinkSpeed = XENNET_MEDIA_MAX_SPEED;
    generalAttributes.MediaConnectState = MediaConnectStateConnected;
    generalAttributes.MediaDuplexState = MediaDuplexStateFull;
    generalAttributes.LookaheadSize = Adapter->MaximumFrameSize;
    generalAttributes.PowerManagementCapabilities = &Adapter->Capabilities;
    generalAttributes.MacOptions = XENNET_MAC_OPTIONS;

    generalAttributes.SupportedPacketFilters = XENNET_SUPPORTED_PACKET_FILTERS;
        
    generalAttributes.MaxMulticastListSize = MAXIMUM_MULTICAST_ADDRESS_COUNT;
    generalAttributes.MacAddressLength = ETHERNET_ADDRESS_LENGTH;

    VIF(QueryPermanentAddress,
        Adapter->VifInterface,
        (PETHERNET_ADDRESS)&generalAttributes.PermanentMacAddress);
    VIF(QueryCurrentAddress,
        Adapter->VifInterface,
        (PETHERNET_ADDRESS)&generalAttributes.CurrentMacAddress);

    generalAttributes.PhysicalMediumType = NdisPhysicalMedium802_3;
    generalAttributes.RecvScaleCapabilities = NULL;
    generalAttributes.AccessType = NET_IF_ACCESS_BROADCAST;
    generalAttributes.DirectionType = NET_IF_DIRECTION_SENDRECEIVE;
    generalAttributes.ConnectionType = NET_IF_CONNECTION_DEDICATED;
    generalAttributes.IfType = IF_TYPE_ETHERNET_CSMACD; 
    generalAttributes.IfConnectorPresent = TRUE;

    generalAttributes.SupportedStatistics = NDIS_STATISTICS_XMIT_OK_SUPPORTED |
                                            NDIS_STATISTICS_XMIT_ERROR_SUPPORTED |
                                            NDIS_STATISTICS_DIRECTED_BYTES_XMIT_SUPPORTED |
                                            NDIS_STATISTICS_DIRECTED_FRAMES_XMIT_SUPPORTED |
                                            NDIS_STATISTICS_MULTICAST_BYTES_XMIT_SUPPORTED |
                                            NDIS_STATISTICS_MULTICAST_FRAMES_XMIT_SUPPORTED |
                                            NDIS_STATISTICS_BROADCAST_BYTES_XMIT_SUPPORTED |
                                            NDIS_STATISTICS_BROADCAST_FRAMES_XMIT_SUPPORTED |
                                            NDIS_STATISTICS_RCV_OK_SUPPORTED |
                                            NDIS_STATISTICS_RCV_ERROR_SUPPORTED |
                                            NDIS_STATISTICS_DIRECTED_BYTES_RCV_SUPPORTED |
                                            NDIS_STATISTICS_DIRECTED_FRAMES_RCV_SUPPORTED |
                                            NDIS_STATISTICS_MULTICAST_BYTES_RCV_SUPPORTED |
                                            NDIS_STATISTICS_MULTICAST_FRAMES_RCV_SUPPORTED |
                                            NDIS_STATISTICS_BROADCAST_BYTES_RCV_SUPPORTED |
                                            NDIS_STATISTICS_BROADCAST_FRAMES_RCV_SUPPORTED |
                                            NDIS_STATISTICS_GEN_STATISTICS_SUPPORTED;
                      
    generalAttributes.SupportedOidList = XennetSupportedOids;
    generalAttributes.SupportedOidListLength = sizeof(XennetSupportedOids);
    adapterAttributes = 
                (PNDIS_MINIPORT_ADAPTER_ATTRIBUTES)&generalAttributes;

    ndisStatus = NdisMSetMiniportAttributes(Adapter->NdisAdapterHandle,
                                            adapterAttributes);

    return ndisStatus;
}

#define DISPLAY_OFFLOAD(_Offload)                                   \
        do {                                                        \
            if ((_Offload).Checksum.IPv4Receive.IpChecksum)         \
                Info("Checksum.IPv4Receive.IpChecksum ON\n");       \
            else                                                    \
                Info("Checksum.IPv4Receive.IpChecksum OFF\n");      \
                                                                    \
            if ((_Offload).Checksum.IPv4Receive.TcpChecksum)        \
                Info("Checksum.IPv4Receive.TcpChecksum ON\n");      \
            else                                                    \
                Info("Checksum.IPv4Receive.TcpChecksum OFF\n");     \
                                                                    \
            if ((_Offload).Checksum.IPv4Receive.UdpChecksum)        \
                Info("Checksum.IPv4Receive.UdpChecksum ON\n");      \
            else                                                    \
                Info("Checksum.IPv4Receive.UdpChecksum OFF\n");     \
                                                                    \
            if ((_Offload).Checksum.IPv6Receive.TcpChecksum)        \
                Info("Checksum.IPv6Receive.TcpChecksum ON\n");      \
            else                                                    \
                Info("Checksum.IPv6Receive.TcpChecksum OFF\n");     \
                                                                    \
            if ((_Offload).Checksum.IPv6Receive.UdpChecksum)        \
                Info("Checksum.IPv6Receive.UdpChecksum ON\n");      \
            else                                                    \
                Info("Checksum.IPv6Receive.UdpChecksum OFF\n");     \
                                                                    \
            if ((_Offload).Checksum.IPv4Transmit.IpChecksum)        \
                Info("Checksum.IPv4Transmit.IpChecksum ON\n");      \
            else                                                    \
                Info("Checksum.IPv4Transmit.IpChecksum OFF\n");     \
                                                                    \
            if ((_Offload).Checksum.IPv4Transmit.TcpChecksum)       \
                Info("Checksum.IPv4Transmit.TcpChecksum ON\n");     \
            else                                                    \
                Info("Checksum.IPv4Transmit.TcpChecksum OFF\n");    \
                                                                    \
            if ((_Offload).Checksum.IPv4Transmit.UdpChecksum)       \
                Info("Checksum.IPv4Transmit.UdpChecksum ON\n");     \
            else                                                    \
                Info("Checksum.IPv4Transmit.UdpChecksum OFF\n");    \
                                                                    \
            if ((_Offload).Checksum.IPv6Transmit.TcpChecksum)       \
                Info("Checksum.IPv6Transmit.TcpChecksum ON\n");     \
            else                                                    \
                Info("Checksum.IPv6Transmit.TcpChecksum OFF\n");    \
                                                                    \
            if ((_Offload).Checksum.IPv6Transmit.UdpChecksum)       \
                Info("Checksum.IPv6Transmit.UdpChecksum ON\n");     \
            else                                                    \
                Info("Checksum.IPv6Transmit.UdpChecksum OFF\n");    \
                                                                    \
            if ((_Offload).LsoV2.IPv4.MaxOffLoadSize != 0)          \
                Info("LsoV2.IPv4.MaxOffLoadSize = %u\n",            \
                     (_Offload).LsoV2.IPv4.MaxOffLoadSize);         \
            else                                                    \
                Info("LsoV2.IPv4 OFF\n");                           \
                                                                    \
            if ((_Offload).LsoV2.IPv6.MaxOffLoadSize != 0)          \
                Info("LsoV2.IPv6.MaxOffLoadSize = %u\n",            \
                     (_Offload).LsoV2.IPv6.MaxOffLoadSize);         \
            else                                                    \
                Info("LsoV2.IPv6 OFF\n");                           \
        } while (FALSE)

static NDIS_STATUS
AdapterSetOffloadAttributes(
    IN  PADAPTER Adapter
    )
{
    PNDIS_MINIPORT_ADAPTER_ATTRIBUTES adapterAttributes;
    NDIS_MINIPORT_ADAPTER_OFFLOAD_ATTRIBUTES offloadAttributes;
    XENVIF_OFFLOAD_OPTIONS Options;
    NDIS_OFFLOAD current;
    NDIS_OFFLOAD supported;
    NDIS_STATUS ndisStatus;

    Adapter->Receiver.OffloadOptions.Value = 0;
    Adapter->Receiver.OffloadOptions.OffloadTagManipulation = 1;

    if (Adapter->Properties.need_csum_value)
        Adapter->Receiver.OffloadOptions.NeedChecksumValue = 1;

    if (Adapter->Properties.lrov4) {
        Adapter->Receiver.OffloadOptions.OffloadIpVersion4LargePacket = 1;
        Adapter->Receiver.OffloadOptions.NeedLargePacketSplit = 1;
    }

    if (Adapter->Properties.lrov6) {
        Adapter->Receiver.OffloadOptions.OffloadIpVersion6LargePacket = 1;
        Adapter->Receiver.OffloadOptions.NeedLargePacketSplit = 1;
    }

    Adapter->Transmitter->OffloadOptions.Value = 0;
    Adapter->Transmitter->OffloadOptions.OffloadTagManipulation = 1;

    NdisZeroMemory(&offloadAttributes, sizeof(offloadAttributes));
    NdisZeroMemory(&current, sizeof(current));
    NdisZeroMemory(&supported, sizeof(supported));
    
    VIF(UpdateOffloadOptions,
        Adapter->VifInterface,
        Adapter->Receiver.OffloadOptions);

    supported.Header.Type = NDIS_OBJECT_TYPE_OFFLOAD;
    supported.Header.Revision = NDIS_OFFLOAD_REVISION_1;
    supported.Header.Size = sizeof(supported);

    supported.Checksum.IPv4Receive.Encapsulation = NDIS_ENCAPSULATION_IEEE_802_3;

    supported.Checksum.IPv4Receive.IpChecksum = 1;
    supported.Checksum.IPv4Receive.IpOptionsSupported = 1;

    supported.Checksum.IPv4Receive.TcpChecksum = 1;
    supported.Checksum.IPv4Receive.TcpOptionsSupported = 1;

    supported.Checksum.IPv4Receive.UdpChecksum = 1;

    supported.Checksum.IPv6Receive.Encapsulation = NDIS_ENCAPSULATION_IEEE_802_3;

    supported.Checksum.IPv6Receive.IpExtensionHeadersSupported = 1;

    supported.Checksum.IPv6Receive.TcpChecksum = 1;
    supported.Checksum.IPv6Receive.TcpOptionsSupported = 1;

    supported.Checksum.IPv6Receive.UdpChecksum = 1;

    VIF(QueryOffloadOptions,
        Adapter->VifInterface,
        &Options);

    supported.Checksum.IPv4Transmit.Encapsulation = NDIS_ENCAPSULATION_IEEE_802_3;

    if (Options.OffloadIpVersion4HeaderChecksum) {
        supported.Checksum.IPv4Transmit.IpChecksum = 1;
        supported.Checksum.IPv4Transmit.IpOptionsSupported = 1;
    }

    if (Options.OffloadIpVersion4TcpChecksum) {
        supported.Checksum.IPv4Transmit.TcpChecksum = 1;
        supported.Checksum.IPv4Transmit.TcpOptionsSupported = 1;
    }

    if (Options.OffloadIpVersion4UdpChecksum)
        supported.Checksum.IPv4Transmit.UdpChecksum = 1;

    supported.Checksum.IPv6Transmit.Encapsulation = NDIS_ENCAPSULATION_IEEE_802_3;

    supported.Checksum.IPv6Transmit.IpExtensionHeadersSupported = 1;

    if (Options.OffloadIpVersion6TcpChecksum) {
        supported.Checksum.IPv6Transmit.TcpChecksum = 1;
        supported.Checksum.IPv6Transmit.TcpOptionsSupported = 1;
    }

    if (Options.OffloadIpVersion6UdpChecksum)
        supported.Checksum.IPv6Transmit.UdpChecksum = 1;

    if (Options.OffloadIpVersion4LargePacket) {
        ULONG Size;

        VIF(QueryLargePacketSize,
            Adapter->VifInterface,
            4,
            &Size);

        supported.LsoV2.IPv4.Encapsulation = NDIS_ENCAPSULATION_IEEE_802_3;
        supported.LsoV2.IPv4.MaxOffLoadSize = Size;
        supported.LsoV2.IPv4.MinSegmentCount = 2;
    }

    if (Options.OffloadIpVersion6LargePacket) {
        ULONG Size;

        VIF(QueryLargePacketSize,
            Adapter->VifInterface,
            6,
            &Size);

        supported.LsoV2.IPv6.Encapsulation = NDIS_ENCAPSULATION_IEEE_802_3;
        supported.LsoV2.IPv6.MaxOffLoadSize = Size;
        supported.LsoV2.IPv6.MinSegmentCount = 2;
        supported.LsoV2.IPv6.IpExtensionHeadersSupported = 1;
        supported.LsoV2.IPv6.TcpOptionsSupported = 1;
    }

    current = supported;

    if (!(Adapter->Properties.ipv4_csum & 2))
        current.Checksum.IPv4Receive.IpChecksum = 0;

    if (!(Adapter->Properties.tcpv4_csum & 2))
        current.Checksum.IPv4Receive.TcpChecksum = 0;

    if (!(Adapter->Properties.udpv4_csum & 2))
        current.Checksum.IPv4Receive.UdpChecksum = 0;

    if (!(Adapter->Properties.tcpv6_csum & 2))
        current.Checksum.IPv6Receive.TcpChecksum = 0;

    if (!(Adapter->Properties.udpv6_csum & 2))
        current.Checksum.IPv6Receive.UdpChecksum = 0;

    if (!(Adapter->Properties.ipv4_csum & 1))
        current.Checksum.IPv4Transmit.IpChecksum = 0;

    if (!(Adapter->Properties.tcpv4_csum & 1))
        current.Checksum.IPv4Transmit.TcpChecksum = 0;

    if (!(Adapter->Properties.udpv4_csum & 1))
        current.Checksum.IPv4Transmit.UdpChecksum = 0;

    if (!(Adapter->Properties.tcpv6_csum & 1))
        current.Checksum.IPv6Transmit.TcpChecksum = 0;

    if (!(Adapter->Properties.udpv6_csum & 1))
        current.Checksum.IPv6Transmit.UdpChecksum = 0;

    if (!(Adapter->Properties.lsov4)) {
        current.LsoV2.IPv4.MaxOffLoadSize = 0;
        current.LsoV2.IPv4.MinSegmentCount = 0;
    }

    if (!(Adapter->Properties.lsov6)) {
        current.LsoV2.IPv6.MaxOffLoadSize = 0;
        current.LsoV2.IPv6.MinSegmentCount = 0;
    }

    if (!RtlEqualMemory(&Adapter->Offload, &current, sizeof (NDIS_OFFLOAD))) {
        Adapter->Offload = current;

        DISPLAY_OFFLOAD(current);
    }

    offloadAttributes.Header.Type =
        NDIS_OBJECT_TYPE_MINIPORT_ADAPTER_OFFLOAD_ATTRIBUTES;
    offloadAttributes.Header.Revision =
        NDIS_MINIPORT_ADAPTER_OFFLOAD_ATTRIBUTES_REVISION_1;
    offloadAttributes.Header.Size = sizeof(offloadAttributes);
    offloadAttributes.DefaultOffloadConfiguration = &current;
    offloadAttributes.HardwareOffloadCapabilities = &supported;

    adapterAttributes =
        (PNDIS_MINIPORT_ADAPTER_ATTRIBUTES)&offloadAttributes;
    ndisStatus = NdisMSetMiniportAttributes(Adapter->NdisAdapterHandle,
                                            adapterAttributes);

    return ndisStatus;
}

static void
AdapterIndicateOffloadChanged (
    IN  PADAPTER Adapter
    )
{
    NDIS_STATUS_INDICATION indication;
    NDIS_OFFLOAD offload;

    NdisZeroMemory(&offload, sizeof(offload));
    INITIALIZE_NDIS_OBJ_HEADER(offload, OFFLOAD);

    offload.Checksum.IPv4Receive.Encapsulation = NDIS_ENCAPSULATION_IEEE_802_3;

    if (Adapter->Receiver.OffloadOptions.OffloadIpVersion4HeaderChecksum) {
        offload.Checksum.IPv4Receive.IpChecksum = 1;
        offload.Checksum.IPv4Receive.IpOptionsSupported = 1;
    }

    if (Adapter->Receiver.OffloadOptions.OffloadIpVersion4TcpChecksum) {
        offload.Checksum.IPv4Receive.TcpChecksum = 1;
        offload.Checksum.IPv4Receive.TcpOptionsSupported = 1;
    }

    if (Adapter->Receiver.OffloadOptions.OffloadIpVersion4UdpChecksum) {
        offload.Checksum.IPv4Receive.UdpChecksum = 1;
    }

    offload.Checksum.IPv6Receive.Encapsulation = NDIS_ENCAPSULATION_IEEE_802_3;

    offload.Checksum.IPv6Receive.IpExtensionHeadersSupported = 1;

    if (Adapter->Receiver.OffloadOptions.OffloadIpVersion6TcpChecksum) {
        offload.Checksum.IPv6Receive.TcpChecksum = 1;
        offload.Checksum.IPv6Receive.TcpOptionsSupported = 1;
    }

    if (Adapter->Receiver.OffloadOptions.OffloadIpVersion6UdpChecksum) {
        offload.Checksum.IPv6Receive.UdpChecksum = 1;
    }

    VIF(UpdateOffloadOptions,
        Adapter->VifInterface,
        Adapter->Receiver.OffloadOptions);

    offload.Checksum.IPv4Transmit.Encapsulation = NDIS_ENCAPSULATION_IEEE_802_3;

    if (Adapter->Transmitter->OffloadOptions.OffloadIpVersion4HeaderChecksum) {
        offload.Checksum.IPv4Transmit.IpChecksum = 1;
        offload.Checksum.IPv4Transmit.IpOptionsSupported = 1;
    }

    if (Adapter->Transmitter->OffloadOptions.OffloadIpVersion4TcpChecksum) {
        offload.Checksum.IPv4Transmit.TcpChecksum = 1;
        offload.Checksum.IPv4Transmit.TcpOptionsSupported = 1;
    }

    if (Adapter->Transmitter->OffloadOptions.OffloadIpVersion4UdpChecksum) {
        offload.Checksum.IPv4Transmit.UdpChecksum = 1;
    }

    offload.Checksum.IPv6Transmit.Encapsulation = NDIS_ENCAPSULATION_IEEE_802_3;

    offload.Checksum.IPv6Transmit.IpExtensionHeadersSupported = 1;

    if (Adapter->Transmitter->OffloadOptions.OffloadIpVersion6TcpChecksum) {
        offload.Checksum.IPv6Transmit.TcpChecksum = 1;
        offload.Checksum.IPv6Transmit.TcpOptionsSupported = 1;
    }

    if (Adapter->Transmitter->OffloadOptions.OffloadIpVersion6UdpChecksum) {
        offload.Checksum.IPv6Transmit.UdpChecksum = 1;
    }

    if (Adapter->Transmitter->OffloadOptions.OffloadIpVersion4LargePacket) {
        ULONG Size;

        VIF(QueryLargePacketSize,
            Adapter->VifInterface,
            4,
            &Size);

        offload.LsoV2.IPv4.Encapsulation = NDIS_ENCAPSULATION_IEEE_802_3;
        offload.LsoV2.IPv4.MaxOffLoadSize = Size;
        offload.LsoV2.IPv4.MinSegmentCount = 2;
    }

    if (Adapter->Transmitter->OffloadOptions.OffloadIpVersion6LargePacket) {
        ULONG Size;

        VIF(QueryLargePacketSize,
            Adapter->VifInterface,
            6,
            &Size);

        offload.LsoV2.IPv6.Encapsulation = NDIS_ENCAPSULATION_IEEE_802_3;
        offload.LsoV2.IPv6.MaxOffLoadSize = Size;
        offload.LsoV2.IPv6.MinSegmentCount = 2;
        offload.LsoV2.IPv6.IpExtensionHeadersSupported = 1;
        offload.LsoV2.IPv6.TcpOptionsSupported = 1;
    }

    if (!RtlEqualMemory(&Adapter->Offload, &offload, sizeof (NDIS_OFFLOAD))) {
        Adapter->Offload = offload;

        DISPLAY_OFFLOAD(offload);
    }

    NdisZeroMemory(&indication, sizeof(indication));
    INITIALIZE_NDIS_OBJ_HEADER(indication, STATUS_INDICATION);
    indication.SourceHandle = Adapter->NdisAdapterHandle;
    indication.StatusCode = NDIS_STATUS_TASK_OFFLOAD_CURRENT_CONFIG;
    indication.StatusBuffer = &offload;
    indication.StatusBufferSize = sizeof(offload);

    NdisMIndicateStatusEx(Adapter->NdisAdapterHandle, &indication);

}

static NDIS_STATUS
SetMulticastAddresses(PADAPTER Adapter, PETHERNET_ADDRESS Address, ULONG Count)
{
    NTSTATUS status;

    ASSERT3U(Count, <=, MAXIMUM_MULTICAST_ADDRESS_COUNT);

    status = VIF(UpdateMulticastAddresses,
                 Adapter->VifInterface,
                 Address,
                 Count);
    if (!NT_SUCCESS(status))
        return NDIS_STATUS_INVALID_DATA;

    return NDIS_STATUS_SUCCESS;
}

static NDIS_STATUS
SetPacketFilter(PADAPTER Adapter, PULONG PacketFilter)
{
    XENVIF_MAC_FILTER_LEVEL UnicastFilterLevel;
    XENVIF_MAC_FILTER_LEVEL MulticastFilterLevel;
    XENVIF_MAC_FILTER_LEVEL BroadcastFilterLevel;

    if (*PacketFilter & ~XENNET_SUPPORTED_PACKET_FILTERS)
        return NDIS_STATUS_INVALID_PARAMETER;

    if (*PacketFilter & NDIS_PACKET_TYPE_PROMISCUOUS) {
        UnicastFilterLevel = MAC_FILTER_ALL;
        MulticastFilterLevel = MAC_FILTER_ALL;
        BroadcastFilterLevel = MAC_FILTER_ALL;
        goto done;
    }

    if (*PacketFilter & NDIS_PACKET_TYPE_DIRECTED)
        UnicastFilterLevel = MAC_FILTER_MATCHING;
    else
        UnicastFilterLevel = MAC_FILTER_NONE;

    if (*PacketFilter & NDIS_PACKET_TYPE_ALL_MULTICAST)
        MulticastFilterLevel = MAC_FILTER_ALL;
    else if (*PacketFilter & NDIS_PACKET_TYPE_MULTICAST)
        MulticastFilterLevel = MAC_FILTER_MATCHING;
    else
        MulticastFilterLevel = MAC_FILTER_NONE;

    if (*PacketFilter & NDIS_PACKET_TYPE_BROADCAST)
        BroadcastFilterLevel = MAC_FILTER_ALL;
    else
        BroadcastFilterLevel = MAC_FILTER_NONE;

done:
    VIF(UpdateFilterLevel,
        Adapter->VifInterface,
        ETHERNET_ADDRESS_UNICAST,
        UnicastFilterLevel);

    VIF(UpdateFilterLevel,
        Adapter->VifInterface,
        ETHERNET_ADDRESS_MULTICAST,
        MulticastFilterLevel);

    VIF(UpdateFilterLevel,
        Adapter->VifInterface,
        ETHERNET_ADDRESS_BROADCAST,
        BroadcastFilterLevel);

    return NDIS_STATUS_SUCCESS;
}

//
// Set OID handler.
//
static NDIS_STATUS 
AdapterSetInformation (
    IN  PADAPTER            Adapter,
    IN  PNDIS_OID_REQUEST   NdisRequest
    )
{
    ULONG addressCount;
    ULONG bytesNeeded = 0;
    ULONG bytesRead = 0;
    PVOID informationBuffer;
    ULONG informationBufferLength;
    NDIS_STATUS ndisStatus = NDIS_STATUS_SUCCESS;
    NDIS_OID oid;
    BOOLEAN offloadChanged;

    informationBuffer = NdisRequest->DATA.SET_INFORMATION.InformationBuffer;
    informationBufferLength = NdisRequest->DATA.SET_INFORMATION.InformationBufferLength;
    oid = NdisRequest->DATA.QUERY_INFORMATION.Oid;
    switch (oid) {
        case OID_PNP_SET_POWER:
            bytesNeeded = sizeof(NDIS_DEVICE_POWER_STATE);
            if (informationBufferLength >= bytesNeeded) {
                PNDIS_DEVICE_POWER_STATE state;

                state = (PNDIS_DEVICE_POWER_STATE)informationBuffer;
                switch (*state) {
                case NdisDeviceStateD0:
                    Info("SET_POWER: D0\n");
                    break;

                case NdisDeviceStateD1:
                    Info("SET_POWER: D1\n");
                    break;

                case NdisDeviceStateD2:
                    Info("SET_POWER: D2\n");
                    break;

                case NdisDeviceStateD3:
                    Info("SET_POWER: D3\n");
                    break;
                }
            }
            break;

        case OID_GEN_MACHINE_NAME:
            ndisStatus = NDIS_STATUS_NOT_SUPPORTED;
            break;

        case OID_GEN_CURRENT_LOOKAHEAD:
            bytesNeeded = sizeof(ULONG);
            Adapter->CurrentLookahead = Adapter->MaximumFrameSize;
            if (informationBufferLength == sizeof(ULONG)) {
                Adapter->CurrentLookahead = *(PULONG)informationBuffer;
                bytesRead = sizeof(ULONG);
            }

            break;

        case OID_GEN_CURRENT_PACKET_FILTER:
            bytesNeeded = sizeof(ULONG);
            if (informationBufferLength == sizeof(ULONG)) {
                ndisStatus = SetPacketFilter(Adapter, (PULONG)informationBuffer);
                bytesRead = sizeof(ULONG);
            }

            break;

        case OID_802_3_MULTICAST_LIST:
            bytesNeeded = ETHERNET_ADDRESS_LENGTH;
            if (informationBufferLength % ETHERNET_ADDRESS_LENGTH == 0) {
                addressCount = informationBufferLength / ETHERNET_ADDRESS_LENGTH;

                ndisStatus = SetMulticastAddresses(Adapter, informationBuffer, addressCount);
                if (ndisStatus == NDIS_STATUS_SUCCESS)
                    bytesRead = informationBufferLength;
            } else {
                ndisStatus = NDIS_STATUS_INVALID_LENGTH;
            }

            break;

        case OID_GEN_INTERRUPT_MODERATION:
            ndisStatus = NDIS_STATUS_INVALID_DATA;
            break;

        case OID_OFFLOAD_ENCAPSULATION: {
            PNDIS_OFFLOAD_ENCAPSULATION offloadEncapsulation;

            bytesNeeded = sizeof(*offloadEncapsulation);
            if (informationBufferLength >= bytesNeeded) {
                XENVIF_OFFLOAD_OPTIONS Options;

                bytesRead = bytesNeeded;
                offloadEncapsulation = informationBuffer;
                ndisStatus = NDIS_STATUS_SUCCESS;

                if (offloadEncapsulation->IPv4.Enabled == NDIS_OFFLOAD_SET_ON) {
                    if (offloadEncapsulation->IPv4.EncapsulationType != NDIS_ENCAPSULATION_IEEE_802_3)
                        ndisStatus = NDIS_STATUS_INVALID_PARAMETER;
                }

                if (offloadEncapsulation->IPv6.Enabled == NDIS_OFFLOAD_SET_ON) {
                    if (offloadEncapsulation->IPv6.EncapsulationType != NDIS_ENCAPSULATION_IEEE_802_3)
                        ndisStatus = NDIS_STATUS_INVALID_PARAMETER;
                }

                VIF(QueryOffloadOptions,
                    Adapter->VifInterface,
                    &Options);
                
                Adapter->Transmitter->OffloadOptions.Value = 0;
                Adapter->Transmitter->OffloadOptions.OffloadTagManipulation = 1;

                if ((Adapter->Properties.lsov4) && (Options.OffloadIpVersion4LargePacket))
                    Adapter->Transmitter->OffloadOptions.OffloadIpVersion4LargePacket = 1;

                if ((Adapter->Properties.lsov6) && (Options.OffloadIpVersion6LargePacket))
                    Adapter->Transmitter->OffloadOptions.OffloadIpVersion6LargePacket = 1;

                if ((Adapter->Properties.ipv4_csum & 1) && Options.OffloadIpVersion4HeaderChecksum)
                    Adapter->Transmitter->OffloadOptions.OffloadIpVersion4HeaderChecksum = 1;

                if ((Adapter->Properties.tcpv4_csum & 1) && Options.OffloadIpVersion4TcpChecksum)
                    Adapter->Transmitter->OffloadOptions.OffloadIpVersion4TcpChecksum = 1;

                if ((Adapter->Properties.udpv4_csum & 1) && Options.OffloadIpVersion4UdpChecksum)
                    Adapter->Transmitter->OffloadOptions.OffloadIpVersion4UdpChecksum = 1;

                if ((Adapter->Properties.tcpv6_csum & 1) && Options.OffloadIpVersion6TcpChecksum)
                    Adapter->Transmitter->OffloadOptions.OffloadIpVersion6TcpChecksum = 1;

                if ((Adapter->Properties.udpv6_csum & 1) && Options.OffloadIpVersion6UdpChecksum)
                    Adapter->Transmitter->OffloadOptions.OffloadIpVersion6UdpChecksum = 1;

                Adapter->Receiver.OffloadOptions.Value = 0;
                Adapter->Receiver.OffloadOptions.OffloadTagManipulation = 1;

                if (Adapter->Properties.need_csum_value)
                    Adapter->Receiver.OffloadOptions.NeedChecksumValue = 1;

                if (Adapter->Properties.lrov4) {
                    Adapter->Receiver.OffloadOptions.OffloadIpVersion4LargePacket = 1;
                    Adapter->Receiver.OffloadOptions.NeedLargePacketSplit = 1;
                }

                if (Adapter->Properties.lrov6) {
                    Adapter->Receiver.OffloadOptions.OffloadIpVersion6LargePacket = 1;
                    Adapter->Receiver.OffloadOptions.NeedLargePacketSplit = 1;
                }

                if (Adapter->Properties.ipv4_csum & 2)
                    Adapter->Receiver.OffloadOptions.OffloadIpVersion4HeaderChecksum = 1;

                if (Adapter->Properties.tcpv4_csum & 2)
                    Adapter->Receiver.OffloadOptions.OffloadIpVersion4TcpChecksum = 1;

                if (Adapter->Properties.udpv4_csum & 2)
                    Adapter->Receiver.OffloadOptions.OffloadIpVersion4UdpChecksum = 1;

                if (Adapter->Properties.tcpv6_csum & 2)
                    Adapter->Receiver.OffloadOptions.OffloadIpVersion6TcpChecksum = 1;

                if (Adapter->Properties.udpv6_csum & 2)
                    Adapter->Receiver.OffloadOptions.OffloadIpVersion6UdpChecksum = 1;

                AdapterIndicateOffloadChanged(Adapter);
            }
            break;
        }
        case OID_TCP_OFFLOAD_PARAMETERS: {
            PNDIS_OFFLOAD_PARAMETERS offloadParameters;

            bytesNeeded = sizeof(*offloadParameters);
            if (informationBufferLength >= bytesNeeded) {
                bytesRead = bytesNeeded;
                offloadParameters = informationBuffer;
                ndisStatus = NDIS_STATUS_SUCCESS;

#define no_change(x)  ((x) == NDIS_OFFLOAD_PARAMETERS_NO_CHANGE)

                if (!no_change(offloadParameters->IPsecV1))
                    ndisStatus = NDIS_STATUS_INVALID_PARAMETER;
                    
                if (!no_change(offloadParameters->LsoV1))
                    ndisStatus = NDIS_STATUS_INVALID_PARAMETER;

                if (!no_change(offloadParameters->TcpConnectionIPv4))
                    ndisStatus = NDIS_STATUS_INVALID_PARAMETER;

                if (!no_change(offloadParameters->TcpConnectionIPv6))
                    ndisStatus = NDIS_STATUS_INVALID_PARAMETER;

                if (!no_change(offloadParameters->LsoV2IPv4)) {
                    XENVIF_OFFLOAD_OPTIONS  Options;

                    VIF(QueryOffloadOptions,
                        Adapter->VifInterface,
                        &Options);

                    if (!(Options.OffloadIpVersion4LargePacket))
                        ndisStatus = NDIS_STATUS_INVALID_PARAMETER;
                }

                if (!no_change(offloadParameters->LsoV2IPv6)) {
                    XENVIF_OFFLOAD_OPTIONS  Options;

                    VIF(QueryOffloadOptions,
                        Adapter->VifInterface,
                        &Options);

                    if (!(Options.OffloadIpVersion6LargePacket))
                        ndisStatus = NDIS_STATUS_INVALID_PARAMETER;
                }

#define rx_enabled(x) ((x) == NDIS_OFFLOAD_PARAMETERS_TX_RX_ENABLED ||       \
                       (x) == NDIS_OFFLOAD_PARAMETERS_RX_ENABLED_TX_DISABLED)
#define tx_enabled(x) ((x) == NDIS_OFFLOAD_PARAMETERS_TX_RX_ENABLED ||       \
                       (x) == NDIS_OFFLOAD_PARAMETERS_TX_ENABLED_RX_DISABLED)

                if (ndisStatus == NDIS_STATUS_SUCCESS) {
                    offloadChanged = FALSE;

                    if (offloadParameters->LsoV2IPv4 == NDIS_OFFLOAD_PARAMETERS_LSOV2_ENABLED) {
                        if (!Adapter->Transmitter->OffloadOptions.OffloadIpVersion4LargePacket) {
                            Adapter->Transmitter->OffloadOptions.OffloadIpVersion4LargePacket = 1;
                            offloadChanged = TRUE;
                        }
                    } else if (offloadParameters->LsoV2IPv4 == NDIS_OFFLOAD_PARAMETERS_LSOV2_DISABLED) {
                        if (Adapter->Transmitter->OffloadOptions.OffloadIpVersion4LargePacket) {
                            Adapter->Transmitter->OffloadOptions.OffloadIpVersion4LargePacket = 0;
                            offloadChanged = TRUE;
                        }
                    }

                    if (offloadParameters->LsoV2IPv6 == NDIS_OFFLOAD_PARAMETERS_LSOV2_ENABLED) {
                        if (!Adapter->Transmitter->OffloadOptions.OffloadIpVersion6LargePacket) {
                            Adapter->Transmitter->OffloadOptions.OffloadIpVersion6LargePacket = 1;
                            offloadChanged = TRUE;
                        }
                    } else if (offloadParameters->LsoV2IPv6 == NDIS_OFFLOAD_PARAMETERS_LSOV2_DISABLED) {
                        if (Adapter->Transmitter->OffloadOptions.OffloadIpVersion6LargePacket) {
                            Adapter->Transmitter->OffloadOptions.OffloadIpVersion6LargePacket = 0;
                            offloadChanged = TRUE;
                        }
                    }

                    if (tx_enabled(offloadParameters->IPv4Checksum)) {
                        if (!Adapter->Transmitter->OffloadOptions.OffloadIpVersion4HeaderChecksum) {
                            Adapter->Transmitter->OffloadOptions.OffloadIpVersion4HeaderChecksum = 1;
                            offloadChanged = TRUE;
                        }
                    } else {
                        if (Adapter->Transmitter->OffloadOptions.OffloadIpVersion4HeaderChecksum) {
                            Adapter->Transmitter->OffloadOptions.OffloadIpVersion4HeaderChecksum = 0;
                            offloadChanged = TRUE;
                        }
                    }

                    if (tx_enabled(offloadParameters->TCPIPv4Checksum)) {
                        if (!Adapter->Transmitter->OffloadOptions.OffloadIpVersion4TcpChecksum) {
                            Adapter->Transmitter->OffloadOptions.OffloadIpVersion4TcpChecksum = 1;
                            offloadChanged = TRUE;
                        }
                    } else {
                        if (Adapter->Transmitter->OffloadOptions.OffloadIpVersion4TcpChecksum) {
                            Adapter->Transmitter->OffloadOptions.OffloadIpVersion4TcpChecksum = 0;
                            offloadChanged = TRUE;
                        }
                    }

                    if (tx_enabled(offloadParameters->UDPIPv4Checksum)) {
                        if (!Adapter->Transmitter->OffloadOptions.OffloadIpVersion4UdpChecksum) {
                            Adapter->Transmitter->OffloadOptions.OffloadIpVersion4UdpChecksum = 1;
                            offloadChanged = TRUE;
                        }
                    } else {
                        if (Adapter->Transmitter->OffloadOptions.OffloadIpVersion4UdpChecksum) {
                            Adapter->Transmitter->OffloadOptions.OffloadIpVersion4UdpChecksum = 0;
                            offloadChanged = TRUE;
                        }
                    }

                    if (tx_enabled(offloadParameters->TCPIPv6Checksum)) {
                        if (!Adapter->Transmitter->OffloadOptions.OffloadIpVersion6TcpChecksum) {
                            Adapter->Transmitter->OffloadOptions.OffloadIpVersion6TcpChecksum = 1;
                            offloadChanged = TRUE;
                        }
                    } else {
                        if (Adapter->Transmitter->OffloadOptions.OffloadIpVersion6TcpChecksum) {
                            Adapter->Transmitter->OffloadOptions.OffloadIpVersion6TcpChecksum = 0;
                            offloadChanged = TRUE;
                        }
                    }

                    if (tx_enabled(offloadParameters->UDPIPv6Checksum)) {
                        if (!Adapter->Transmitter->OffloadOptions.OffloadIpVersion6UdpChecksum) {
                            Adapter->Transmitter->OffloadOptions.OffloadIpVersion6UdpChecksum = 1;
                            offloadChanged = TRUE;
                        }
                    } else {
                        if (Adapter->Transmitter->OffloadOptions.OffloadIpVersion6UdpChecksum) {
                            Adapter->Transmitter->OffloadOptions.OffloadIpVersion6UdpChecksum = 0;
                            offloadChanged = TRUE;
                        }
                    }

                    if (rx_enabled(offloadParameters->IPv4Checksum)) {
                        if (!Adapter->Receiver.OffloadOptions.OffloadIpVersion4HeaderChecksum) {
                            Adapter->Receiver.OffloadOptions.OffloadIpVersion4HeaderChecksum = 1;
                            offloadChanged = TRUE;
                        }
                    } else {
                        if (Adapter->Receiver.OffloadOptions.OffloadIpVersion4HeaderChecksum) {
                            Adapter->Receiver.OffloadOptions.OffloadIpVersion4HeaderChecksum = 0;
                            offloadChanged = TRUE;
                        }
                    }

                    if (rx_enabled(offloadParameters->TCPIPv4Checksum)) {
                        if (!Adapter->Receiver.OffloadOptions.OffloadIpVersion4TcpChecksum) {
                            Adapter->Receiver.OffloadOptions.OffloadIpVersion4TcpChecksum = 1;
                            offloadChanged = TRUE;
                        }
                    } else {
                        if (Adapter->Receiver.OffloadOptions.OffloadIpVersion4TcpChecksum) {
                            Adapter->Receiver.OffloadOptions.OffloadIpVersion4TcpChecksum = 0;
                            offloadChanged = TRUE;
                        }
                    }

                    if (rx_enabled(offloadParameters->UDPIPv4Checksum)) {
                        if (!Adapter->Receiver.OffloadOptions.OffloadIpVersion4UdpChecksum) {
                            Adapter->Receiver.OffloadOptions.OffloadIpVersion4UdpChecksum = 1;
                            offloadChanged = TRUE;
                        }
                    } else {
                        if (Adapter->Receiver.OffloadOptions.OffloadIpVersion4UdpChecksum) {
                            Adapter->Receiver.OffloadOptions.OffloadIpVersion4UdpChecksum = 0;
                            offloadChanged = TRUE;
                        }
                    }

                    if (rx_enabled(offloadParameters->TCPIPv6Checksum)) {
                        if (!Adapter->Receiver.OffloadOptions.OffloadIpVersion6TcpChecksum) {
                            Adapter->Receiver.OffloadOptions.OffloadIpVersion6TcpChecksum = 1;
                            offloadChanged = TRUE;
                        }
                    } else {
                        if (Adapter->Receiver.OffloadOptions.OffloadIpVersion6TcpChecksum) {
                            Adapter->Receiver.OffloadOptions.OffloadIpVersion6TcpChecksum = 0;
                            offloadChanged = TRUE;
                        }
                    }

                    if (rx_enabled(offloadParameters->UDPIPv6Checksum)) {
                        if (!Adapter->Receiver.OffloadOptions.OffloadIpVersion6UdpChecksum) {
                            Adapter->Receiver.OffloadOptions.OffloadIpVersion6UdpChecksum = 1;
                            offloadChanged = TRUE;
                        }
                    } else {
                        if (Adapter->Receiver.OffloadOptions.OffloadIpVersion6UdpChecksum) {
                            Adapter->Receiver.OffloadOptions.OffloadIpVersion6UdpChecksum = 0;
                            offloadChanged = TRUE;
                        }
                    }

#undef tx_enabled
#undef rx_enabled
#undef no_change

                    if (offloadChanged)
                        AdapterIndicateOffloadChanged(Adapter);
                }
            } else {
                ndisStatus = NDIS_STATUS_INVALID_LENGTH;
            }
            break;
        }
        default:
            ndisStatus = NDIS_STATUS_NOT_SUPPORTED;
            break;
    };

    NdisRequest->DATA.SET_INFORMATION.BytesNeeded = bytesNeeded;
    if (ndisStatus == NDIS_STATUS_SUCCESS) {
        NdisRequest->DATA.SET_INFORMATION.BytesRead = bytesRead;
    }

    return ndisStatus;
}

//
// Sets miniport registration attributes.
//
static NDIS_STATUS
AdapterSetRegistrationAttributes (
    IN  PADAPTER Adapter
    )
{
    PNDIS_MINIPORT_ADAPTER_ATTRIBUTES adapterAttributes;
    NDIS_MINIPORT_ADAPTER_REGISTRATION_ATTRIBUTES registrationAttributes;
    NDIS_STATUS ndisStatus;


    NdisZeroMemory(&registrationAttributes, 
                   sizeof(NDIS_MINIPORT_ADAPTER_REGISTRATION_ATTRIBUTES));

    registrationAttributes.Header.Type = 
                NDIS_OBJECT_TYPE_MINIPORT_ADAPTER_REGISTRATION_ATTRIBUTES;

    registrationAttributes.Header.Revision = 
                NDIS_MINIPORT_ADAPTER_REGISTRATION_ATTRIBUTES_REVISION_1;

    registrationAttributes.Header.Size = 
                sizeof(NDIS_MINIPORT_ADAPTER_REGISTRATION_ATTRIBUTES);

    registrationAttributes.MiniportAdapterContext = (NDIS_HANDLE)Adapter;
    registrationAttributes.AttributeFlags = NDIS_MINIPORT_ATTRIBUTES_BUS_MASTER |
                                            NDIS_MINIPORT_ATTRIBUTES_NO_HALT_ON_SUSPEND;
    
    registrationAttributes.CheckForHangTimeInSeconds = 0;
    registrationAttributes.InterfaceType = XENNET_INTERFACE_TYPE;

    adapterAttributes = 
                (PNDIS_MINIPORT_ADAPTER_ATTRIBUTES)&registrationAttributes;

    ndisStatus = NdisMSetMiniportAttributes(Adapter->NdisAdapterHandle,
                                            adapterAttributes);

    return ndisStatus;
}

//
// Shuts down adapter.
//
VOID 
AdapterShutdown (
    IN  PADAPTER                Adapter,
    IN  NDIS_SHUTDOWN_ACTION    ShutdownAction
    )
{
    UNREFERENCED_PARAMETER(ShutdownAction);

    if (ShutdownAction != NdisShutdownBugCheck)
        AdapterStop(Adapter);

    return;
}

//
// Stops adapter. Waits for currently transmitted packets to complete.
// Stops transmission of new packets.
// Stops received packet indication to NDIS.
//
static NDIS_STATUS
AdapterStop (
IN  PADAPTER    Adapter
)
{
    Trace("====>\n");

    if (!Adapter->Enabled)
        goto done;

    VIF(Disable,
        Adapter->VifInterface);

    Adapter->Enabled = FALSE;

done:
    Trace("<====\n");
    return NDIS_STATUS_SUCCESS;
}
