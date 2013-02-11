/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "udp_socket_manager_wrapper.h"

#include <cassert>

#ifdef _WIN32
#include "fix_interlocked_exchange_pointer_win.h"
#include "udp_socket2_manager_windows.h"
#else
#include "udp_socket_manager_posix.h"
#endif

namespace webrtc {
UdpSocketManager* UdpSocketManager::CreateInstance()
{
#if defined(_WIN32)
  return static_cast<UdpSocketManager*>(new UdpSocket2ManagerWindows());
#else
    return new UdpSocketManagerPosix();
#endif
}

UdpSocketManager* UdpSocketManager::StaticInstance(
    CountOperation count_operation,
    const WebRtc_Word32 id,
    WebRtc_UWord8& numOfWorkThreads)
{
    UdpSocketManager* impl =
        GetStaticInstance<UdpSocketManager>(count_operation);
    if (count_operation == kAddRef && impl != NULL) {
        if (impl->Init(id, numOfWorkThreads)) {
            impl->Start();
        }
    }
    return impl;
}

UdpSocketManager* UdpSocketManager::Create(const WebRtc_Word32 id,
                                           WebRtc_UWord8& numOfWorkThreads)
{
    return UdpSocketManager::StaticInstance(kAddRef, id,
                                            numOfWorkThreads);
}

void UdpSocketManager::Return()
{
    WebRtc_UWord8 numOfWorkThreads = 0;
    UdpSocketManager::StaticInstance(kRelease, -1,
                                     numOfWorkThreads);
}

UdpSocketManager::UdpSocketManager() : _numOfWorkThreads(0)
{
}

WebRtc_UWord8 UdpSocketManager::WorkThreads() const
{
    return _numOfWorkThreads;
}
} // namespace webrtc
