#ifndef RTP_AUDIO_LEVEL_OBSERVER_H
#define RTP_AUDIO_LEVEL_OBSERVER_H

namespace webrtc {

struct RTPHeader;

class RtpPacketObserver {
  public:

  virtual void
  OnRtpPacket(const RTPHeader& aRtpHeader,
              const int64_t aTimestamp,
              const uint32_t aJitter) = 0;
};

}

#endif // RTP_AUDIO_LEVEL_OBSERVER_H
