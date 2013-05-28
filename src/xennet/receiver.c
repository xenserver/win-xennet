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

#include "common.h"

#pragma warning(disable:4711)

NDIS_STATUS
ReceiverInitialize (
    IN  PRECEIVER                   Receiver
    )
{
    PADAPTER                        Adapter;
    NDIS_STATUS                     ndisStatus = NDIS_STATUS_SUCCESS;
    NET_BUFFER_LIST_POOL_PARAMETERS poolParameters;
    ULONG                           Cpu;

    Receiver->PutList = NULL;
    for (Cpu = 0; Cpu < MAXIMUM_PROCESSORS; Cpu++)
        Receiver->GetList[Cpu] = NULL;

    Adapter = CONTAINING_RECORD(Receiver, ADAPTER, Receiver);

    NdisZeroMemory(&poolParameters, sizeof(NET_BUFFER_LIST_POOL_PARAMETERS));
    poolParameters.Header.Type = NDIS_OBJECT_TYPE_DEFAULT;
    poolParameters.Header.Revision =
        NET_BUFFER_LIST_POOL_PARAMETERS_REVISION_1;
    poolParameters.Header.Size = sizeof(poolParameters);
    poolParameters.ProtocolId = 0;
    poolParameters.ContextSize = 0;
    poolParameters.fAllocateNetBuffer = TRUE;
    poolParameters.PoolTag = ' TEN';

    Receiver->NetBufferListPool =
        NdisAllocateNetBufferListPool(Adapter->NdisAdapterHandle,
                                      &poolParameters);

    if (!Receiver->NetBufferListPool)
        ndisStatus = NDIS_STATUS_RESOURCES;

    return ndisStatus;
}

VOID 
ReceiverCleanup (
    IN  PRECEIVER       Receiver
    )
{
    ULONG               Cpu;
    PNET_BUFFER_LIST    NetBufferList;

    ASSERT(Receiver != NULL);

    for (Cpu = 0; Cpu < MAXIMUM_PROCESSORS; Cpu++) {
        NetBufferList = Receiver->GetList[Cpu];
        while (NetBufferList != NULL) {
            PNET_BUFFER_LIST    Next;

            Next = NET_BUFFER_LIST_NEXT_NBL(NetBufferList);
            NET_BUFFER_LIST_NEXT_NBL(NetBufferList) = NULL;

            NdisFreeNetBufferList(NetBufferList);

            NetBufferList = Next;
        }
    }

    NetBufferList = Receiver->PutList;
    while (NetBufferList != NULL) {
        PNET_BUFFER_LIST    Next;

        Next = NET_BUFFER_LIST_NEXT_NBL(NetBufferList);
        NET_BUFFER_LIST_NEXT_NBL(NetBufferList) = NULL;

        NdisFreeNetBufferList(NetBufferList);

        NetBufferList = Next;
    }

    if (Receiver->NetBufferListPool) {
        NdisFreeNetBufferListPool(Receiver->NetBufferListPool);
        Receiver->NetBufferListPool = NULL;
    }

    return;
}

PNET_BUFFER_LIST
ReceiverAllocateNetBufferList(
    IN  PRECEIVER       Receiver,
    IN  PMDL            Mdl,
    IN  ULONG           Offset,
    IN  ULONG           Length
    )
{
    ULONG               Cpu;
    PNET_BUFFER_LIST    NetBufferList;

    Cpu = KeGetCurrentProcessorNumber();

    NetBufferList = Receiver->GetList[Cpu];

    if (NetBufferList == NULL)
        Receiver->GetList[Cpu] = InterlockedExchangePointer(&Receiver->PutList, NULL);

    NetBufferList = Receiver->GetList[Cpu];

    if (NetBufferList != NULL) {
        PNET_BUFFER NetBuffer;

        Receiver->GetList[Cpu] = NET_BUFFER_LIST_NEXT_NBL(NetBufferList);
        NET_BUFFER_LIST_NEXT_NBL(NetBufferList) = NULL;

        NetBuffer = NET_BUFFER_LIST_FIRST_NB(NetBufferList);
        NET_BUFFER_FIRST_MDL(NetBuffer) = Mdl;
        NET_BUFFER_CURRENT_MDL(NetBuffer) = Mdl;
        NET_BUFFER_DATA_OFFSET(NetBuffer) = Offset;
        NET_BUFFER_DATA_LENGTH(NetBuffer) = Length;
        NET_BUFFER_CURRENT_MDL_OFFSET(NetBuffer) = Offset;
    } else {
        NetBufferList = NdisAllocateNetBufferAndNetBufferList(Receiver->NetBufferListPool,
                                                              0,
                                                              0,
                                                              Mdl,
                                                              Offset,
                                                              Length);
        ASSERT(IMPLY(NetBufferList != NULL, NET_BUFFER_LIST_NEXT_NBL(NetBufferList) == NULL));
    }

    return NetBufferList;
}        

VOID
ReceiverReleaseNetBufferList(
    IN  PRECEIVER           Receiver,
    IN  PNET_BUFFER_LIST    NetBufferList,
    IN  BOOLEAN             Cache
    )
{
    if (Cache) {
        PNET_BUFFER_LIST    Old;
        PNET_BUFFER_LIST    New;

        ASSERT3P(NET_BUFFER_LIST_NEXT_NBL(NetBufferList), ==, NULL);

        do {
            Old = Receiver->PutList;

            NET_BUFFER_LIST_NEXT_NBL(NetBufferList) = Old;
            New = NetBufferList;
        } while (InterlockedCompareExchangePointer(&Receiver->PutList, New, Old) != Old);
    } else {
        NdisFreeNetBufferList(NetBufferList);
    }
}

static FORCEINLINE ULONG
__ReceiverReturnNetBufferLists(
    IN  PRECEIVER           Receiver,
    IN  PNET_BUFFER_LIST    NetBufferList,
    IN  BOOLEAN             Cache
    )
{
    PADAPTER                Adapter;
    ULONG                   Count;

    Adapter = CONTAINING_RECORD(Receiver, ADAPTER, Receiver);

    Count = 0;
    while (NetBufferList != NULL) {
        PNET_BUFFER_LIST        Next;
        PNET_BUFFER             NetBuffer;
        PMDL                    Mdl;
        PXENVIF_RECEIVER_PACKET Packet;

        Next = NET_BUFFER_LIST_NEXT_NBL(NetBufferList);
        NET_BUFFER_LIST_NEXT_NBL(NetBufferList) = NULL;

        NetBuffer = NET_BUFFER_LIST_FIRST_NB(NetBufferList);
        ASSERT3P(NET_BUFFER_NEXT_NB(NetBuffer), ==, NULL);

        Mdl = NET_BUFFER_FIRST_MDL(NetBuffer);

        ReceiverReleaseNetBufferList(Receiver, NetBufferList, Cache);

        Packet = CONTAINING_RECORD(Mdl, XENVIF_RECEIVER_PACKET, Mdl);

        VIF(ReturnPacket,
            Adapter->VifInterface,
            Packet);

        Count++;
        NetBufferList = Next;
    }

    return Count;
}

VOID
ReceiverReturnNetBufferLists(
    IN  PRECEIVER           Receiver,
    IN  PNET_BUFFER_LIST    HeadNetBufferList,
    IN  ULONG               Flags
    )
{
    ULONG                   Count;

    UNREFERENCED_PARAMETER(Flags);

    Count = __ReceiverReturnNetBufferLists(Receiver, HeadNetBufferList, TRUE);
    (VOID) __InterlockedSubtract(&Receiver->InNDIS, Count);
}

static PNET_BUFFER_LIST
ReceiverReceivePacket(
    IN  PRECEIVER                               Receiver,
    IN  PMDL                                    Mdl,
    IN  ULONG                                   Offset,
    IN  ULONG                                   Length,
    IN  XENVIF_CHECKSUM_FLAGS                   Flags,
    IN  USHORT                                  TagControlInformation
    )
{
    PADAPTER                                    Adapter;
    PNET_BUFFER_LIST                            NetBufferList;
    NDIS_TCP_IP_CHECKSUM_NET_BUFFER_LIST_INFO   csumInfo;

    Adapter = CONTAINING_RECORD(Receiver, ADAPTER, Receiver);

    NetBufferList = ReceiverAllocateNetBufferList(Receiver,
                                                  Mdl,
                                                  Offset,
                                                  Length);
    if (NetBufferList == NULL)
        goto fail1;

    NetBufferList->SourceHandle = Adapter->NdisAdapterHandle;

    csumInfo.Value = 0;

    csumInfo.Receive.IpChecksumSucceeded = Flags.IpChecksumSucceeded;
    csumInfo.Receive.IpChecksumFailed = Flags.IpChecksumFailed;

    csumInfo.Receive.TcpChecksumSucceeded = Flags.TcpChecksumSucceeded;
    csumInfo.Receive.TcpChecksumFailed = Flags.TcpChecksumFailed;

    csumInfo.Receive.UdpChecksumSucceeded = Flags.UdpChecksumSucceeded;
    csumInfo.Receive.UdpChecksumFailed = Flags.UdpChecksumFailed;

    NET_BUFFER_LIST_INFO(NetBufferList, TcpIpChecksumNetBufferListInfo) = (PVOID)(ULONG_PTR)csumInfo.Value;

    if (TagControlInformation != 0) {
        NDIS_NET_BUFFER_LIST_8021Q_INFO Ieee8021QInfo;

        UNPACK_TAG_CONTROL_INFORMATION(TagControlInformation,
                                       Ieee8021QInfo.TagHeader.UserPriority,
                                       Ieee8021QInfo.TagHeader.CanonicalFormatId,
                                       Ieee8021QInfo.TagHeader.VlanId);

        if (Ieee8021QInfo.TagHeader.VlanId != 0)
            goto fail2;

        NET_BUFFER_LIST_INFO(NetBufferList, Ieee8021QNetBufferListInfo) = Ieee8021QInfo.Value;
    }

    return NetBufferList;

fail2:
    ReceiverReleaseNetBufferList(Receiver, NetBufferList, TRUE);

fail1:
    return NULL;
}

static VOID
ReceiverPushPackets(
    IN  PRECEIVER           Receiver,
    IN  PNET_BUFFER_LIST    NetBufferList,
    IN  ULONG               Count,
    IN  BOOLEAN             LowResources
    )
{
    PADAPTER                Adapter;
    ULONG                   Flags;
    LONG                    InNDIS;

    Adapter = CONTAINING_RECORD(Receiver, ADAPTER, Receiver);

    InNDIS = Receiver->InNDIS;

    Flags = NDIS_RECEIVE_FLAGS_DISPATCH_LEVEL;
    if (LowResources) {
        Flags |= NDIS_RECEIVE_FLAGS_RESOURCES;
    } else {
        InNDIS = __InterlockedAdd(&Receiver->InNDIS, Count);
    }

    for (;;) {
        LONG    InNDISMax;

        InNDISMax = Receiver->InNDISMax;
        KeMemoryBarrier();

        if (InNDIS <= InNDISMax)
            break;

        if (InterlockedCompareExchange(&Receiver->InNDISMax, InNDIS, InNDISMax) == InNDISMax)
            break;
    }

    NdisMIndicateReceiveNetBufferLists(Adapter->NdisAdapterHandle,
                                       NetBufferList,
                                       NDIS_DEFAULT_PORT_NUMBER,
                                       Count,
                                       Flags);

    if (LowResources)
        (VOID) __ReceiverReturnNetBufferLists(Receiver, NetBufferList, FALSE);
}

#define IN_NDIS_MAX 1024

VOID
ReceiverReceivePackets(
    IN  PRECEIVER       Receiver,
    IN  PLIST_ENTRY     List
    )
{
    PADAPTER            Adapter;
    PNET_BUFFER_LIST    HeadNetBufferList;
    PNET_BUFFER_LIST    *TailNetBufferList;
    ULONG               Count;
    BOOLEAN             LowResources;

    Adapter = CONTAINING_RECORD(Receiver, ADAPTER, Receiver);
    LowResources = FALSE;

again:
    HeadNetBufferList = NULL;
    TailNetBufferList = &HeadNetBufferList;
    Count = 0;

    while (!IsListEmpty(List)) {
        PLIST_ENTRY                     ListEntry;
        PXENVIF_RECEIVER_PACKET         Packet;
        PMDL                            Mdl;
        ULONG                           Offset;
        ULONG                           Length;
        XENVIF_CHECKSUM_FLAGS           Flags;
        USHORT                          TagControlInformation;
        PNET_BUFFER_LIST                NetBufferList;

        if (!LowResources &&
            Receiver->InNDIS + Count > IN_NDIS_MAX)
            break;

        ListEntry = RemoveHeadList(List);
        ASSERT(ListEntry != List);

        RtlZeroMemory(ListEntry, sizeof (LIST_ENTRY));

        Packet = CONTAINING_RECORD(ListEntry, XENVIF_RECEIVER_PACKET, ListEntry);
        Mdl = &Packet->Mdl;
        Offset = Packet->Offset;
        Length = Packet->Length;
        Flags = Packet->Flags;
        TagControlInformation = Packet->TagControlInformation;

        NetBufferList = ReceiverReceivePacket(Receiver, Mdl, Offset, Length, Flags, TagControlInformation);

        if (NetBufferList != NULL) {
            *TailNetBufferList = NetBufferList;
            TailNetBufferList = &NET_BUFFER_LIST_NEXT_NBL(NetBufferList);
            Count++;
        } else {
            VIF(ReturnPacket,
                Adapter->VifInterface,
                Packet);
        }
    }

    if (Count != 0) {
        ASSERT(HeadNetBufferList != NULL);

        ReceiverPushPackets(Receiver, HeadNetBufferList, Count, LowResources);
    }

    if (!IsListEmpty(List)) {
        ASSERT(!LowResources);
        LowResources = TRUE;
        goto again;
    }
}
