/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/video_coding/main/test/rtp_file_reader.h"

#ifdef WIN32
#include <windows.h>
#include <Winsock2.h>
#else
#include <arpa/inet.h>
#endif
#include <assert.h>
#include <stdio.h>

#include "webrtc/modules/video_coding/main/test/rtp_player.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"

namespace webrtc {
namespace rtpplayer {

enum {
  kResultFail = -1,
  kResultSuccess = 0,

  kFirstLineLength = 40,    // More than needed to read the ID line.
  kPacketHeaderSize = 8     // Rtpplay packet header size in bytes.
};

#if 1
# define DEBUG_LOG(text)
# define DEBUG_LOG1(text, arg)
#else
# define DEBUG_LOG(text) (printf(text "\n"))
# define DEBUG_LOG1(text, arg) (printf(text "\n", arg))
#endif

#define TRY(expr)                                      \
  do {                                                 \
    if ((expr) < 0) {                                  \
      DEBUG_LOG1("FAIL at " __FILE__ ":%d", __LINE__);       \
      return kResultFail;                                       \
    }                                                  \
  } while (0)

// Read RTP packets from file in rtpdump format, as documented at:
// http://www.cs.columbia.edu/irt/software/rtptools/
class RtpFileReaderImpl : public RtpPacketSourceInterface {
 public:
  RtpFileReaderImpl() : file_(NULL) {}
  virtual ~RtpFileReaderImpl() {
    if (file_ != NULL) {
      fclose(file_);
      file_ = NULL;
    }
  }

  int Initialize(const std::string& filename) {
    file_ = fopen(filename.c_str(), "rb");
    if (file_ == NULL) {
      printf("ERROR: Can't open file: %s\n", filename.c_str());
      return kResultFail;
    }

    char firstline[kFirstLineLength + 1] = {0};
    if (fgets(firstline, kFirstLineLength, file_) == NULL) {
      DEBUG_LOG("ERROR: Can't read from file\n");
      return kResultFail;
    }
    if (strncmp(firstline, "#!rtpplay", 9) == 0) {
      if (strncmp(firstline, "#!rtpplay1.0", 12) != 0) {
        DEBUG_LOG("ERROR: wrong rtpplay version, must be 1.0\n");
        return kResultFail;
      }
    } else if (strncmp(firstline, "#!RTPencode", 11) == 0) {
      if (strncmp(firstline, "#!RTPencode1.0", 14) != 0) {
        DEBUG_LOG("ERROR: wrong RTPencode version, must be 1.0\n");
        return kResultFail;
      }
    } else {
      DEBUG_LOG("ERROR: wrong file format of input file\n");
      return kResultFail;
    }

    uint32_t start_sec;
    uint32_t start_usec;
    uint32_t source;
    uint16_t port;
    uint16_t padding;
    TRY(Read(&start_sec));
    TRY(Read(&start_usec));
    TRY(Read(&source));
    TRY(Read(&port));
    TRY(Read(&padding));

    return kResultSuccess;
  }

  virtual int NextPacket(uint8_t* rtp_data, uint32_t* length,
                         uint32_t* time_ms) {
    assert(rtp_data);
    assert(length);
    assert(time_ms);

    uint16_t len;
    uint16_t plen;
    uint32_t offset;
    TRY(Read(&len));
    TRY(Read(&plen));
    TRY(Read(&offset));

    // Use 'len' here because a 'plen' of 0 specifies rtcp.
    len -= kPacketHeaderSize;
    if (*length < len) {
      return kResultFail;
    }
    if (fread(rtp_data, 1, len, file_) != len) {
      return kResultFail;
    }

    *length = len;
    *time_ms = offset;
    return kResultSuccess;
  }

 private:
  int Read(uint32_t* out) {
    assert(out);
    uint32_t tmp = 0;
    if (fread(&tmp, 1, sizeof(uint32_t), file_) != sizeof(uint32_t)) {
      return kResultFail;
    }
    *out = ntohl(tmp);
    return kResultSuccess;
  }

  int Read(uint16_t* out) {
    assert(out);
    uint16_t tmp = 0;
    if (fread(&tmp, 1, sizeof(uint16_t), file_) != sizeof(uint16_t)) {
      return kResultFail;
    }
    *out = ntohs(tmp);
    return kResultSuccess;
  }

  FILE* file_;

  DISALLOW_COPY_AND_ASSIGN(RtpFileReaderImpl);
};

RtpPacketSourceInterface* CreateRtpFileReader(const std::string& filename) {
  scoped_ptr<RtpFileReaderImpl> impl(new RtpFileReaderImpl());
  if (impl->Initialize(filename) != 0) {
    return NULL;
  }
  return impl.release();
}
}  // namespace rtpplayer
}  // namespace webrtc
