/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Some of this code is cut-and-pasted from nICEr. Copyright is:

/*
Copyright (c) 2007, Adobe Systems, Incorporated
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

* Redistributions of source code must retain the above copyright
  notice, this list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions and the following disclaimer in the
  documentation and/or other materials provided with the distribution.

* Neither the name of Adobe Systems, Network Resonance nor the names of its
  contributors may be used to endorse or promote products derived from
  this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Original author: bcampen@mozilla.com */

/*
   This file defines an r_dest_vlog that can be used to accumulate log messages
   for later inspection/filtering. The intent is to use this for interactive
   debug purposes on an about:webrtc page or similar.
*/

#ifndef rlogconnector_h__
#define rlogconnector_h__

#include <stdint.h>

#include <deque>
#include <string>
#include <vector>

#include "mozilla/Mutex.h"

#include "m_cpp_utils.h"

namespace mozilla {

class RLogConnector {
  public:
    /*
       NB: These are not threadsafe, nor are they safe to call during static
       init/deinit.
    */
    static RLogConnector* CreateInstance();
    static RLogConnector* GetInstance();
    static void DestroyInstance();

    /*
       Retrieves log statements that match a given substring, subject to a
       limit. |matching_logs| will be filled in chronological order (front()
       is oldest, back() is newest). |limit| == 0 will be interpreted as no
       limit.
    */
    void Filter(const std::string& substring,
                uint32_t limit,
                std::deque<std::string>* matching_logs);

    void FilterAny(const std::vector<std::string>& substrings,
                   uint32_t limit,
                   std::deque<std::string>* matching_logs);

    inline void GetAny(uint32_t limit,
                       std::deque<std::string>* matching_logs) {
      Filter("", limit, matching_logs);
    }

    void SetLogLimit(uint32_t new_limit);
    void Log(int level, std::string&& log);
    void Clear();

    // Methods to signal when a PeerConnection exists in a Private Window.
    void EnterPrivateMode();
    void ExitPrivateMode();

  private:
    RLogConnector();
    ~RLogConnector();
    void RemoveOld();
    void AddMsg(std::string&& msg);

    static RLogConnector* instance;

    /*
     * Might be worthwhile making this a circular buffer, but I think it is
     * preferable to take up as little space as possible if no logging is
     * happening/the ringbuffer is not being used.
    */
    std::deque<std::string> log_messages_;
    /* Max size of log buffer (should we use time-depth instead/also?) */
    uint32_t log_limit_;
    OffTheBooksMutex mutex_;
    uint32_t disableCount_;

    DISALLOW_COPY_ASSIGN(RLogConnector);
}; // class RLogConnector

} // namespace mozilla

#endif // rlogconnector_h__
