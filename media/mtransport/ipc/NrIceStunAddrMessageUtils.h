/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_NrIceStunAddrMessageUtils_h
#define mozilla_net_NrIceStunAddrMessageUtils_h

// forward declare NrIceStunAddr for --disable-webrtc builds where
// the header will not be available.
namespace mozilla {
  class NrIceStunAddr;
} // namespace mozilla

#include "ipc/IPCMessageUtils.h"
#ifdef MOZ_WEBRTC
#include "mtransport/nricestunaddr.h"
#endif

namespace IPC {

template<>
struct ParamTraits<mozilla::NrIceStunAddr>
{
  static void Write(Message* aMsg, const mozilla::NrIceStunAddr &aParam)
  {
#ifdef MOZ_WEBRTC
    const size_t bufSize = aParam.SerializationBufferSize();
    char* buffer = new char[bufSize];
    aParam.Serialize(buffer, bufSize);
    aMsg->WriteBytes((void*)buffer, bufSize);
    delete[] buffer;
#endif
  }

  static bool Read(const Message* aMsg,
                   PickleIterator* aIter,
                   mozilla::NrIceStunAddr* aResult)
  {
#ifdef MOZ_WEBRTC
    const size_t bufSize = aResult->SerializationBufferSize();
    char* buffer = new char[bufSize];
    bool result =
      aMsg->ReadBytesInto(aIter, (void*)buffer, bufSize);

    if (result) {
      result = result &&
               (NS_OK == aResult->Deserialize(buffer, bufSize));
    }
    delete[] buffer;

    return result;
#else
    return false;
#endif
  }
};

} // namespace IPC

#endif // mozilla_net_NrIceStunAddrMessageUtils_h
