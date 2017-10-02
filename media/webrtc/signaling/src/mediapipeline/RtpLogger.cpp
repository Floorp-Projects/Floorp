/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Original author: nohlmeier@mozilla.com

#include "RtpLogger.h"

#include "CSFLog.h"

#include <ctime>
#include <iomanip>
#include <sstream>
#ifdef _WIN32
#include <time.h>
#include <sys/timeb.h>
#else
#include <sys/time.h>
#endif

// Logging context
using namespace mozilla;

static const char* rlLogTag = "RtpLogger";
#ifdef LOGTAG
#undef LOGTAG
#endif
#define LOGTAG rlLogTag

namespace mozilla {

bool RtpLogger::IsPacketLoggingOn() {
  return CSFLogTestLevel(CSF_LOG_DEBUG);
}

void RtpLogger::LogPacket(const unsigned char *data, int len, bool input,
                          bool isRtp, int headerLength, std::string desc) {
  if (CSFLogTestLevel(CSF_LOG_DEBUG)) {
    std::stringstream ss;
    /* This creates text2pcap compatible format, e.g.:
     *   O 10:36:26.864934  000000 80 c8 00 06 6d ... RTCP_PACKET
     */
    ss << (input ? "I " : "O ");
    std::time_t t = std::time(nullptr);
    std::tm tm = *std::localtime(&t);
    char buf[9];
    if (0 < strftime(buf, sizeof(buf), "%H:%M:%S", &tm)) {
      ss << buf;
    }
    ss << std::setfill('0');
#ifdef _WIN32
    struct timeb tb;
    ftime(&tb);
    ss << "." << (tb.millitm) << " ";
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    ss << "." << (tv.tv_usec) << " ";
#endif
    ss << " 000000";
    ss << std::hex << std::setfill('0');
    int offset_ = headerLength;
    if (isRtp && (offset_ + 5 < len)) {
      // Allow the first 5 bytes of the payload in clear
      offset_ += 5;
    }
    for (int i=0; i < len; ++i) {
      if (isRtp && i > offset_) {
        ss << " 00";
      }
      else {
        ss << " " << std::setw(2) << (int)data[i];
      }
    }
    CSFLogDebug(LOGTAG, "%s%s%s", ss.str().c_str(),
                (isRtp ? " RTP_PACKET " : " RTCP_PACKET "), desc.c_str());
  }
}

}  // end of namespace

