/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtp_player.h"

#include <cstdlib>
#ifdef WIN32
#include <windows.h>
#include <Winsock2.h>
#else
#include <arpa/inet.h>
#endif

#include "../source/internal_defines.h"
#include "gtest/gtest.h"
#include "modules/video_coding/main/source/tick_time_base.h"
#include "rtp_rtcp.h"

using namespace webrtc;

RawRtpPacket::RawRtpPacket(uint8_t* rtp_data, uint16_t rtp_length)
    : data(rtp_data),
      length(rtp_length),
      resend_time_ms(-1) {
  data = new uint8_t[length];
  memcpy(data, rtp_data, length);
}

RawRtpPacket::~RawRtpPacket() {
  delete [] data;
}

LostPackets::LostPackets()
    : crit_sect_(CriticalSectionWrapper::CreateCriticalSection()),
      loss_count_(0),
      debug_file_(NULL),
      packets_() {
  debug_file_ = fopen("PacketLossDebug.txt", "w");
}

LostPackets::~LostPackets() {
  if (debug_file_) {
      fclose(debug_file_);
  }
  while (!packets_.empty()) {
    delete packets_.front();
    packets_.pop_front();
  }
  delete crit_sect_;
}

void LostPackets::AddPacket(RawRtpPacket* packet) {
  CriticalSectionScoped cs(crit_sect_);
  packets_.push_back(packet);
  uint16_t seq_num = (packet->data[2] << 8) + packet->data[3];
  if (debug_file_ != NULL) {
    fprintf(debug_file_, "%u Lost packet: %u\n", loss_count_, seq_num);
  }
  ++loss_count_;
}

void LostPackets::SetResendTime(uint16_t resend_seq_num,
                                int64_t resend_time_ms,
                                int64_t now_ms) {
  CriticalSectionScoped cs(crit_sect_);
  for (RtpPacketIterator it = packets_.begin(); it != packets_.end(); ++it) {
    const uint16_t seq_num = ((*it)->data[2] << 8) +
        (*it)->data[3];
    if (resend_seq_num == seq_num) {
      if ((*it)->resend_time_ms + 10 < now_ms) {
        if (debug_file_ != NULL) {
          fprintf(debug_file_, "Resend %u at %u\n", seq_num,
                  MaskWord64ToUWord32(resend_time_ms));
        }
        (*it)->resend_time_ms = resend_time_ms;
      }
      return;
    }
  }
  assert(false);
}

RawRtpPacket* LostPackets::NextPacketToResend(int64_t timeNow) {
  CriticalSectionScoped cs(crit_sect_);
  for (RtpPacketIterator it = packets_.begin(); it != packets_.end(); ++it) {
    if (timeNow >= (*it)->resend_time_ms && (*it)->resend_time_ms != -1) {
      RawRtpPacket* packet = *it;
      it = packets_.erase(it);
      return packet;
    }
  }
  return NULL;
}

int LostPackets::NumberOfPacketsToResend() const {
  CriticalSectionScoped cs(crit_sect_);
  int count = 0;
  for (ConstRtpPacketIterator it = packets_.begin(); it != packets_.end();
      ++it) {
    if ((*it)->resend_time_ms >= 0) {
        count++;
    }
  }
  return count;
}

void LostPackets::SetPacketResent(uint16_t seq_num, int64_t now_ms) {
  CriticalSectionScoped cs(crit_sect_);
  if (debug_file_ != NULL) {
    fprintf(debug_file_, "Resent %u at %u\n", seq_num,
            MaskWord64ToUWord32(now_ms));
  }
}

void LostPackets::Print() const {
  CriticalSectionScoped cs(crit_sect_);
  printf("Lost packets: %u\n", loss_count_);
  printf("Packets waiting to be resent: %u\n",
         NumberOfPacketsToResend());
  printf("Packets still lost: %u\n",
         static_cast<unsigned int>(packets_.size()));
  printf("Sequence numbers:\n");
  for (ConstRtpPacketIterator it = packets_.begin(); it != packets_.end();
      ++it) {
    uint16_t seq_num = ((*it)->data[2] << 8) + (*it)->data[3];
    printf("%u, ", seq_num);
  }
  printf("\n");
}

RTPPlayer::RTPPlayer(const char* filename,
                     RtpData* callback,
                     TickTimeBase* clock)
:
_clock(clock),
_rtpModule(*RtpRtcp::CreateRtpRtcp(1, false)),
_nextRtpTime(0),
_dataCallback(callback),
_firstPacket(true),
_lossRate(0.0f),
_nackEnabled(false),
_resendPacketCount(0),
_noLossStartup(100),
_endOfFile(false),
_rttMs(0),
_firstPacketRtpTime(0),
_firstPacketTimeMs(0),
_reorderBuffer(NULL),
_reordering(false),
_nextPacket(),
_nextPacketLength(0),
_randVec(),
_randVecPos(0)
{
    _rtpFile = fopen(filename, "rb");
    memset(_nextPacket, 0, sizeof(_nextPacket));
}

RTPPlayer::~RTPPlayer()
{
    RtpRtcp::DestroyRtpRtcp(&_rtpModule);
    if (_rtpFile != NULL)
    {
        fclose(_rtpFile);
    }
    if (_reorderBuffer != NULL)
    {
        delete _reorderBuffer;
        _reorderBuffer = NULL;
    }
}

WebRtc_Word32 RTPPlayer::Initialize(const PayloadTypeList* payloadList)
{
    std::srand(321);
    for (int i=0; i < RAND_VEC_LENGTH; i++)
    {
        _randVec[i] = rand();
    }
    _randVecPos = 0;
    WebRtc_Word32 ret = _rtpModule.SetNACKStatus(kNackOff);
    if (ret < 0)
    {
        return -1;
    }
    ret = _rtpModule.InitReceiver();
    if (ret < 0)
    {
        return -1;
    }

    _rtpModule.InitSender();
    _rtpModule.SetRTCPStatus(kRtcpNonCompound);
    _rtpModule.SetTMMBRStatus(true);

    ret = _rtpModule.RegisterIncomingDataCallback(_dataCallback);
    if (ret < 0)
    {
        return -1;
    }
    // Register payload types
    for (PayloadTypeList::const_iterator it = payloadList->begin();
        it != payloadList->end(); ++it) {
        PayloadCodecTuple* payloadType = *it;
        if (payloadType != NULL)
        {
            VideoCodec videoCodec;
            strncpy(videoCodec.plName, payloadType->name.c_str(), 32);
            videoCodec.plType = payloadType->payloadType;
            if (_rtpModule.RegisterReceivePayload(videoCodec) < 0)
            {
                return -1;
            }
        }
    }
    if (ReadHeader() < 0)
    {
        return -1;
    }
    memset(_nextPacket, 0, sizeof(_nextPacket));
    _nextPacketLength = ReadPacket(_nextPacket, &_nextRtpTime);
    return 0;
}

WebRtc_Word32 RTPPlayer::ReadHeader()
{
    char firstline[FIRSTLINELEN];
    if (_rtpFile == NULL)
    {
        return -1;
    }
    EXPECT_TRUE(fgets(firstline, FIRSTLINELEN, _rtpFile) != NULL);
    if(strncmp(firstline,"#!rtpplay",9) == 0) {
        if(strncmp(firstline,"#!rtpplay1.0",12) != 0){
            printf("ERROR: wrong rtpplay version, must be 1.0\n");
            return -1;
        }
    }
    else if (strncmp(firstline,"#!RTPencode",11) == 0) {
        if(strncmp(firstline,"#!RTPencode1.0",14) != 0){
            printf("ERROR: wrong RTPencode version, must be 1.0\n");
            return -1;
        }
    }
    else {
        printf("ERROR: wrong file format of input file\n");
        return -1;
    }

    WebRtc_UWord32 start_sec;
    WebRtc_UWord32 start_usec;
    WebRtc_UWord32 source;
    WebRtc_UWord16 port;
    WebRtc_UWord16 padding;

    EXPECT_GT(fread(&start_sec, 4, 1, _rtpFile), 0u);
    start_sec=ntohl(start_sec);
    EXPECT_GT(fread(&start_usec, 4, 1, _rtpFile), 0u);
    start_usec=ntohl(start_usec);
    EXPECT_GT(fread(&source, 4, 1, _rtpFile), 0u);
    source=ntohl(source);
    EXPECT_GT(fread(&port, 2, 1, _rtpFile), 0u);
    port=ntohs(port);
    EXPECT_GT(fread(&padding, 2, 1, _rtpFile), 0u);
    padding=ntohs(padding);
    return 0;
}

WebRtc_UWord32 RTPPlayer::TimeUntilNextPacket() const
{
    WebRtc_Word64 timeLeft = (_nextRtpTime - _firstPacketRtpTime) - (_clock->MillisecondTimestamp() - _firstPacketTimeMs);
    if (timeLeft < 0)
    {
        return 0;
    }
    return static_cast<WebRtc_UWord32>(timeLeft);
}

WebRtc_Word32 RTPPlayer::NextPacket(const WebRtc_Word64 timeNow)
{
    // Send any packets ready to be resent,
    RawRtpPacket* resend_packet = _lostPackets.NextPacketToResend(timeNow);
    while (resend_packet != NULL) {
      const uint16_t seqNo = (resend_packet->data[2] << 8) +
          resend_packet->data[3];
      printf("Resend: %u\n", seqNo);
      int ret = SendPacket(resend_packet->data, resend_packet->length);
      delete resend_packet;
      _resendPacketCount++;
      if (ret > 0) {
        _lostPackets.SetPacketResent(seqNo, _clock->MillisecondTimestamp());
      } else if (ret < 0) {
        return ret;
      }
      resend_packet = _lostPackets.NextPacketToResend(timeNow);
    }

    // Send any packets from rtp file
    if (!_endOfFile && (TimeUntilNextPacket() == 0 || _firstPacket))
    {
        _rtpModule.Process();
        if (_firstPacket)
        {
            _firstPacketRtpTime = static_cast<WebRtc_Word64>(_nextRtpTime);
            _firstPacketTimeMs = _clock->MillisecondTimestamp();
        }
        if (_reordering && _reorderBuffer == NULL)
        {
            _reorderBuffer = new RawRtpPacket(reinterpret_cast<WebRtc_UWord8*>(_nextPacket), static_cast<WebRtc_UWord16>(_nextPacketLength));
            return 0;
        }
        WebRtc_Word32 ret = SendPacket(reinterpret_cast<WebRtc_UWord8*>(_nextPacket), static_cast<WebRtc_UWord16>(_nextPacketLength));
        if (_reordering && _reorderBuffer != NULL)
        {
            RawRtpPacket* rtpPacket = _reorderBuffer;
            _reorderBuffer = NULL;
            SendPacket(rtpPacket->data, rtpPacket->length);
            delete rtpPacket;
        }
        _firstPacket = false;
        if (ret < 0)
        {
            return ret;
        }
        _nextPacketLength = ReadPacket(_nextPacket, &_nextRtpTime);
        if (_nextPacketLength < 0)
        {
            _endOfFile = true;
            return 0;
        }
        else if (_nextPacketLength == 0)
        {
            return 0;
        }
    }
    if (_endOfFile && _lostPackets.NumberOfPacketsToResend() == 0)
    {
        return 1;
    }
    return 0;
}

WebRtc_Word32 RTPPlayer::SendPacket(WebRtc_UWord8* rtpData, WebRtc_UWord16 rtpLen)
{
    if ((_randVec[(_randVecPos++) % RAND_VEC_LENGTH] + 1.0)/(RAND_MAX + 1.0) < _lossRate &&
        _noLossStartup < 0)
    {
        if (_nackEnabled)
        {
            const WebRtc_UWord16 seqNo = (rtpData[2] << 8) + rtpData[3];
            printf("Throw: %u\n", seqNo);
            _lostPackets.AddPacket(new RawRtpPacket(rtpData, rtpLen));
            return 0;
        }
    }
    else if (rtpLen > 0)
    {
        WebRtc_Word32 ret = _rtpModule.IncomingPacket(rtpData, rtpLen);
        if (ret < 0)
        {
            return -1;
        }
    }
    if (_noLossStartup >= 0)
    {
        _noLossStartup--;
    }
    return 1;
}

WebRtc_Word32 RTPPlayer::ReadPacket(WebRtc_Word16* rtpdata, WebRtc_UWord32* offset)
{
    WebRtc_UWord16 length, plen;

    if (fread(&length,2,1,_rtpFile)==0)
        return(-1);
    length=ntohs(length);

    if (fread(&plen,2,1,_rtpFile)==0)
        return(-1);
    plen=ntohs(plen);

    if (fread(offset,4,1,_rtpFile)==0)
        return(-1);
    *offset=ntohl(*offset);

    // Use length here because a plen of 0 specifies rtcp
    length = (WebRtc_UWord16) (length - HDR_SIZE);
    if (fread((unsigned short *) rtpdata,1,length,_rtpFile) != length)
        return(-1);

#ifdef JUNK_DATA
    // destroy the RTP payload with random data
    if (plen > 12) { // ensure that we have more than just a header
        for ( int ix = 12; ix < plen; ix=ix+2 ) {
            rtpdata[ix>>1] = (short) (rtpdata[ix>>1] + (short) rand());
        }
    }
#endif
    return plen;
}

WebRtc_Word32 RTPPlayer::SimulatePacketLoss(float lossRate, bool enableNack, WebRtc_UWord32 rttMs)
{
    _nackEnabled = enableNack;
    _lossRate = lossRate;
    _rttMs = rttMs;
    return 0;
}

WebRtc_Word32 RTPPlayer::SetReordering(bool enabled)
{
    _reordering = enabled;
    return 0;
}

WebRtc_Word32 RTPPlayer::ResendPackets(const WebRtc_UWord16* sequenceNumbers, WebRtc_UWord16 length)
{
    if (sequenceNumbers == NULL)
    {
        return 0;
    }
    for (int i=0; i < length; i++)
    {
        _lostPackets.SetResendTime(sequenceNumbers[i],
                                   _clock->MillisecondTimestamp() + _rttMs,
                                   _clock->MillisecondTimestamp());
    }
    return 0;
}

void RTPPlayer::Print() const
{
    printf("Resent packets: %u\n", _resendPacketCount);
    _lostPackets.Print();
}
