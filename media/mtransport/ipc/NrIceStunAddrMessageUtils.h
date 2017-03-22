/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_NrIceStunAddrMessageUtils_h
#define mozilla_net_NrIceStunAddrMessageUtils_h

#include "ipc/IPCMessageUtils.h"
#include "mtransport/nricestunaddr.h"

namespace IPC {

template<>
struct ParamTraits<mozilla::NrIceStunAddr>
{
  static void Write(Message* aMsg, const mozilla::NrIceStunAddr &aParam)
  {
    const size_t bufSize = aParam.SerializationBufferSize();
    char* buffer = new char[bufSize];
    aParam.Serialize(buffer, bufSize);
    aMsg->WriteBytes((void*)buffer, bufSize);
    delete[] buffer;
  }

  static bool Read(const Message* aMsg,
                   PickleIterator* aIter,
                   mozilla::NrIceStunAddr* aResult)
  {
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
  }
};

} // namespace IPC

#endif // mozilla_net_NrIceStunAddrMessageUtils_h
