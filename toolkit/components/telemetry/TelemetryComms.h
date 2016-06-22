/* -*-  Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 */

#ifndef Telemetry_Comms_h__
#define Telemetry_Comms_h__

#include "ipc/IPCMessageUtils.h"

namespace mozilla {
namespace Telemetry {

enum ID : uint32_t;

struct Accumulation
{
  mozilla::Telemetry::ID mId;
  uint32_t mSample;
};

struct KeyedAccumulation
{
  mozilla::Telemetry::ID mId;
  uint32_t mSample;
  nsCString mKey;
};

} // namespace Telemetry
} // namespace mozilla

namespace IPC {

template<>
struct
ParamTraits<mozilla::Telemetry::Accumulation>
{
  typedef mozilla::Telemetry::Accumulation paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    aMsg->WriteUInt32(aParam.mId);
    WriteParam(aMsg, aParam.mSample);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter, paramType* aResult)
  {
    if (!aMsg->ReadUInt32(aIter, reinterpret_cast<uint32_t*>(&(aResult->mId))) ||
        !ReadParam(aMsg, aIter, &(aResult->mSample))) {
      return false;
    }

    return true;
  }
};

template<>
struct
ParamTraits<mozilla::Telemetry::KeyedAccumulation>
{
  typedef mozilla::Telemetry::KeyedAccumulation paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    aMsg->WriteUInt32(aParam.mId);
    WriteParam(aMsg, aParam.mSample);
    WriteParam(aMsg, aParam.mKey);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter, paramType* aResult)
  {
    if (!aMsg->ReadUInt32(aIter, reinterpret_cast<uint32_t*>(&(aResult->mId))) ||
        !ReadParam(aMsg, aIter, &(aResult->mSample)) ||
        !ReadParam(aMsg, aIter, &(aResult->mKey))) {
      return false;
    }

    return true;
  }
};

} // namespace IPC

#endif // Telemetry_Comms_h__
