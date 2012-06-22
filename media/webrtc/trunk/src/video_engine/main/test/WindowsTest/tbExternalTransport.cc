/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

//
// tbExternalTransport.cpp
//

#include "tbExternalTransport.h"

#include "critical_section_wrapper.h"
#include "event_wrapper.h"
#include "thread_wrapper.h"
#include "tick_util.h"
#include "vie_network.h"
#include "tick_util.h"

using namespace webrtc;

TbExternalTransport::TbExternalTransport(ViENetwork& vieNetwork)
    :
    _vieNetwork(vieNetwork),
    _thread(*ThreadWrapper::CreateThread(ViEExternalTransportRun, this, kHighPriority, "AutotestTransport")), 
    _event(*EventWrapper::Create()),
    _crit(*CriticalSectionWrapper::CreateCriticalSection()),
    _statCrit(*CriticalSectionWrapper::CreateCriticalSection()),
    _lossRate(0),
    _networkDelayMs(0),
    _rtpCount(0),
    _dropCount(0),
    _rtcpCount(0),
    _rtpPackets(),
    _rtcpPackets(),
    _checkSSRC(false),
    _lastSSRC(0),
    _checkSequenceNumber(0),
    _firstSequenceNumber(0),
    _lastSeq(0)
{
    srand((int)TickTime::MicrosecondTimestamp());
    unsigned int tId = 0;
    _thread.Start(tId);
}


TbExternalTransport::~TbExternalTransport()
{
    // TODO: stop thread
    _thread.SetNotAlive();
    _event.Set();
    if (_thread.Stop())
    {
        delete &_thread;
        delete &_event;
    }
    delete &_crit;
    delete &_statCrit;
}




    
int TbExternalTransport::SendPacket(int channel, const void *data, int len)
{
    _statCrit.Enter();
    _rtpCount++;
    _statCrit.Leave();


    unsigned short sequenceNumber =  (((unsigned char*) data)[2]) << 8;
    sequenceNumber +=  (((unsigned char*) data)[3]);

            
        // Packet loss
    int dropThis = rand() % 100;
    bool nacked=false;
    if(sequenceNumber<_lastSeq)
    {
        nacked=true;
    }
    else
    {
        _lastSeq=sequenceNumber;
    }

    if (dropThis < _lossRate)
    {
        _statCrit.Enter();
        _dropCount++;
        _statCrit.Leave();

      
      /*  char str[256];
        sprintf(str,"Dropping seq %d length %d m %d, ts %u\n", sequenceNumber,len,marker,timestamp) ;
        OutputDebugString(str);*/
        
        return len;
    }
    else
    {
        if(nacked)
        {
            /*char str[256];
            sprintf(str,"Resending seq %d length %d m %d, ts %u\n", sequenceNumber,len,marker,timestamp) ;
            OutputDebugString(str);*/
        }    
        else
        {
            /*char str[256];
            sprintf(str,"Sending seq %d length %d m %d, ts %u\n", sequenceNumber,len,marker,timestamp) ;
            OutputDebugString(str);*/
         
        }
    }    
    

    VideoPacket* newPacket = new VideoPacket();
    memcpy(newPacket->packetBuffer, data, len);
    newPacket->length = len;
    newPacket->channel = channel;

    _crit.Enter();
    newPacket->receiveTime = NowMs() + _networkDelayMs;
    _rtpPackets.push(newPacket);
    _event.Set();
    _crit.Leave();
    return len;
}

int TbExternalTransport::SendRTCPPacket(int channel, const void *data, int len)
{
    _statCrit.Enter();
    _rtcpCount++;
    _statCrit.Leave();

    VideoPacket* newPacket = new VideoPacket();
    memcpy(newPacket->packetBuffer, data, len);
    newPacket->length = len;
    newPacket->channel = channel;

    _crit.Enter();
    newPacket->receiveTime = NowMs() + _networkDelayMs;
    _rtcpPackets.push(newPacket);
    _event.Set();
    _crit.Leave();
    return len;
}

WebRtc_Word32 TbExternalTransport::SetPacketLoss(WebRtc_Word32 lossRate)
{
    CriticalSectionScoped cs(_statCrit);
    _lossRate = lossRate;
    return 0;
}

void TbExternalTransport::SetNetworkDelay(WebRtc_Word64 delayMs)
{
    CriticalSectionScoped cs(_crit);
    _networkDelayMs = delayMs;
    return;
}

void TbExternalTransport::ClearStats()
{
    CriticalSectionScoped cs(_statCrit);
    _rtpCount = 0;
    _dropCount = 0;
    _rtcpCount = 0;
    return;
}

void TbExternalTransport::GetStats(WebRtc_Word32& numRtpPackets, WebRtc_Word32& numDroppedPackets, WebRtc_Word32& numRtcpPackets)
{
    CriticalSectionScoped cs(_statCrit);
    numRtpPackets = _rtpCount;
    numDroppedPackets = _dropCount;
    numRtcpPackets = _rtcpCount;
    return;
}

void TbExternalTransport::EnableSSRCCheck()
{
    CriticalSectionScoped cs(_statCrit);
    _checkSSRC = true;
}
unsigned int TbExternalTransport::ReceivedSSRC()
{
    CriticalSectionScoped cs(_statCrit);
    return _lastSSRC;
}

void TbExternalTransport::EnableSequenceNumberCheck()
{
    CriticalSectionScoped cs(_statCrit);
    _checkSequenceNumber = true;
}

unsigned short TbExternalTransport::GetFirstSequenceNumber()
{
    CriticalSectionScoped cs(_statCrit);
    return _firstSequenceNumber;
}


bool TbExternalTransport::ViEExternalTransportRun(void* object)
{
    return static_cast<TbExternalTransport*>(object)->ViEExternalTransportProcess();
}
bool TbExternalTransport::ViEExternalTransportProcess()
{
    unsigned int waitTime = KMaxWaitTimeMs;

    VideoPacket* packet = NULL;

    while (!_rtpPackets.empty())
    {
        // Take first packet in queue
        _crit.Enter();
        packet = _rtpPackets.front();
        WebRtc_Word64 timeToReceive = packet->receiveTime - NowMs();
        if (timeToReceive > 0)
        {
            // No packets to receive yet
            if (timeToReceive < waitTime &&
                timeToReceive > 0)
            {
                waitTime = (unsigned int) timeToReceive;
            }
            _crit.Leave();
            break;
        }
        _rtpPackets.pop();
        _crit.Leave();

        // Send to ViE
        if (packet)
        {
            {
                CriticalSectionScoped cs(_statCrit);
                if (_checkSSRC)
                {
                    _lastSSRC  = ((packet->packetBuffer[8]) << 24);
                    _lastSSRC += (packet->packetBuffer[9] << 16);
                    _lastSSRC += (packet->packetBuffer[10] << 8);
                    _lastSSRC += packet->packetBuffer[11];
                    _checkSSRC = false;
                }
                if (_checkSequenceNumber)
                {
                    _firstSequenceNumber = (unsigned char)packet->packetBuffer[2] << 8;
                    _firstSequenceNumber += (unsigned char)packet->packetBuffer[3];
                    _checkSequenceNumber = false;
                }
            }
            /*
            unsigned short sequenceNumber =  (unsigned char)packet->packetBuffer[2] << 8;
            sequenceNumber +=  (unsigned char)packet->packetBuffer[3];
            
            int marker=packet->packetBuffer[1] & 0x80;
            unsigned int timestamp=((((unsigned char*)packet->packetBuffer)[4]) << 24) + ((((unsigned char*)packet->packetBuffer)[5])<<16) +((((unsigned char*)packet->packetBuffer)[6])<<8)+(((unsigned char*)packet->packetBuffer)[7]);
            char str[256];
            sprintf(str,"Receiving seq %u length %d m %d, ts %u\n", sequenceNumber,packet->length,marker,timestamp) ;
            OutputDebugString(str);*/

            _vieNetwork.ReceivedRTPPacket(packet->channel, packet->packetBuffer, packet->length);
            delete packet;
            packet = NULL;
        }
    }
    while (!_rtcpPackets.empty())
    {
        // Take first packet in queue
        _crit.Enter();
        packet = _rtcpPackets.front();
        WebRtc_Word64 timeToReceive = packet->receiveTime - NowMs();
        if (timeToReceive > 0)
        {
            // No packets to receive yet
            if (timeToReceive < waitTime &&
                timeToReceive > 0)
            {
                waitTime = (unsigned int) timeToReceive;
            }
            _crit.Leave();
            break;
        }
        packet = _rtcpPackets.front();
        _rtcpPackets.pop();
        _crit.Leave();

        // Send to ViE
        if (packet)
        {
            _vieNetwork.ReceivedRTCPPacket(packet->channel, packet->packetBuffer, packet->length);
            delete packet;
            packet = NULL;
        }
    }
    _event.Wait(waitTime + 1); // Add 1 ms to not call to early...
    return true;
}

WebRtc_Word64 TbExternalTransport::NowMs()
{
    return TickTime::MillisecondTimestamp();
}
