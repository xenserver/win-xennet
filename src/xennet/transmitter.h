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

typedef struct _TRANSMITTER {
    PADAPTER                Adapter;
    XENVIF_OFFLOAD_OPTIONS  OffloadOptions;
} TRANSMITTER, *PTRANSMITTER;

VOID 
TransmitterCleanup (
    IN OUT PTRANSMITTER* Transmitter
    );

NDIS_STATUS
TransmitterInitialize (
    IN  PTRANSMITTER    Transmitter,
    IN  PADAPTER        Adapter
    );

VOID
TransmitterEnable (
    IN  PTRANSMITTER    Transmitter
    );

VOID 
TransmitterDelete (
    IN OUT PTRANSMITTER* Transmitter
    );

VOID
TransmitterSendNetBufferLists (
    IN  PTRANSMITTER        Transmitter,
    IN  PNET_BUFFER_LIST    NetBufferList,
    IN  NDIS_PORT_NUMBER    PortNumber,
    IN  ULONG               SendFlags
    );

VOID
TransmitterCompletePackets(
    IN  PTRANSMITTER                Transmitter,
    IN  PXENVIF_TRANSMITTER_PACKET  Packet
    );

void TransmitterPause(PTRANSMITTER Transmitter);
void TransmitterUnpause(PTRANSMITTER Transmitter);
