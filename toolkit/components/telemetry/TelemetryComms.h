/* -*-  Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 */

#ifndef Telemetry_Comms_h__
#define Telemetry_Comms_h__

#include "ipc/IPCMessageUtils.h"
#include "nsITelemetry.h"
#include "nsVariant.h"

namespace mozilla {
namespace Telemetry {

// Histogram accumulation types.
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

// Scalar accumulation types.
enum class ScalarID : uint32_t;

enum class ScalarActionType : uint32_t {
  eSet = 0,
  eAdd = 1,
  eSetMaximum = 2
};

struct ScalarAction
{
  ScalarID mId;
  uint32_t mScalarType;
  ScalarActionType mActionType;
  nsCOMPtr<nsIVariant> mData;
};

struct KeyedScalarAction
{
  ScalarID mId;
  uint32_t mScalarType;
  ScalarActionType mActionType;
  nsCString mKey;
  nsCOMPtr<nsIVariant> mData;
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

/**
 * IPC scalar data message serialization and de-serialization.
 */
template<>
struct
ParamTraits<mozilla::Telemetry::ScalarAction>
{
  typedef mozilla::Telemetry::ScalarAction paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    // Write the message type
    aMsg->WriteUInt32(static_cast<uint32_t>(aParam.mId));
    WriteParam(aMsg, aParam.mScalarType);
    WriteParam(aMsg, static_cast<uint32_t>(aParam.mActionType));

    switch(aParam.mScalarType) {
      case nsITelemetry::SCALAR_COUNT:
        {
          uint32_t val = 0;
          nsresult rv = aParam.mData->GetAsUint32(&val);
          if (NS_FAILED(rv)) {
            MOZ_ASSERT(false, "Count Scalar unable to convert variant to bool from child process.");
            return;
          }
          WriteParam(aMsg, val);
          break;
        }
      case nsITelemetry::SCALAR_STRING:
        {
          nsAutoString val;
          nsresult rv = aParam.mData->GetAsAString(val);
          if (NS_FAILED(rv)) {
            MOZ_ASSERT(false, "Conversion failed.");
            return;
          }
          WriteParam(aMsg, val);
          break;
        }
      case nsITelemetry::SCALAR_BOOLEAN:
        {
          bool val = 0;
          nsresult rv = aParam.mData->GetAsBool(&val);
          if (NS_FAILED(rv)) {
            MOZ_ASSERT(false, "Boolean Scalar unable to convert variant to bool from child process.");
            return;
          }
          WriteParam(aMsg, val);
          break;
        }
      default:
        MOZ_ASSERT(false, "Unknown scalar type.");
    }
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter, paramType* aResult)
  {
    // Read the scalar ID and the scalar type.
    if (!aMsg->ReadUInt32(aIter, reinterpret_cast<uint32_t*>(&(aResult->mId))) ||
        !ReadParam(aMsg, aIter, &(aResult->mScalarType)) ||
        !ReadParam(aMsg, aIter, reinterpret_cast<uint32_t*>(&(aResult->mActionType)))) {
      return false;
    }

    // De-serialize the data based on the scalar type.
    nsCOMPtr<nsIWritableVariant> outVar(new nsVariant());

    switch (aResult->mScalarType)
    {
      case nsITelemetry::SCALAR_COUNT:
        {
          uint32_t data = 0;
          // De-serialize the data.
          if (!ReadParam(aMsg, aIter, &data) ||
              NS_FAILED(outVar->SetAsUint32(data))) {
            return false;
          }
          break;
        }
      case nsITelemetry::SCALAR_STRING:
        {
          nsAutoString data;
          // De-serialize the data.
          if (!ReadParam(aMsg, aIter, &data) ||
              NS_FAILED(outVar->SetAsAString(data))) {
            return false;
          }
          break;
        }
      case nsITelemetry::SCALAR_BOOLEAN:
        {
          bool data = false;
          // De-serialize the data.
          if (!ReadParam(aMsg, aIter, &data) ||
              NS_FAILED(outVar->SetAsBool(data))) {
            return false;
          }
          break;
        }
      default:
        MOZ_ASSERT(false, "Unknown scalar type.");
        return false;
    }

    aResult->mData = outVar.forget();
    return true;
  }
};

/**
 * IPC keyed scalar data message serialization and de-serialization.
 */
template<>
struct
ParamTraits<mozilla::Telemetry::KeyedScalarAction>
{
  typedef mozilla::Telemetry::KeyedScalarAction paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    // Write the message type
    aMsg->WriteUInt32(static_cast<uint32_t>(aParam.mId));
    WriteParam(aMsg, aParam.mScalarType);
    WriteParam(aMsg, static_cast<uint32_t>(aParam.mActionType));
    WriteParam(aMsg, aParam.mKey);

    switch(aParam.mScalarType) {
      case nsITelemetry::SCALAR_COUNT:
        {
          uint32_t val = 0;
          nsresult rv = aParam.mData->GetAsUint32(&val);
          if (NS_FAILED(rv)) {
            MOZ_ASSERT(false, "Keyed Count Scalar unable to convert variant to uint from child process.");
            return;
          }
          WriteParam(aMsg, val);
          break;
        }
      case nsITelemetry::SCALAR_STRING:
        {
          // Keyed string scalars are not supported.
          MOZ_ASSERT(false, "Keyed String Scalar unable to be write from child process. Not supported.");
          break;
        }
      case nsITelemetry::SCALAR_BOOLEAN:
        {
          bool val = 0;
          nsresult rv = aParam.mData->GetAsBool(&val);
          if (NS_FAILED(rv)) {
            MOZ_ASSERT(false, "Keyed Boolean Scalar unable to convert variant to bool from child process.");
            return;
          }
          WriteParam(aMsg, val);
          break;
        }
      default:
        MOZ_ASSERT(false, "Unknown keyed scalar type.");
    }
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter, paramType* aResult)
  {
    // Read the scalar ID and the scalar type.
    if (!aMsg->ReadUInt32(aIter, reinterpret_cast<uint32_t*>(&(aResult->mId))) ||
        !ReadParam(aMsg, aIter, &(aResult->mScalarType)) ||
        !ReadParam(aMsg, aIter, reinterpret_cast<uint32_t*>(&(aResult->mActionType))) ||
        !ReadParam(aMsg, aIter, &(aResult->mKey))) {
      return false;
    }

    // De-serialize the data based on the scalar type.
    nsCOMPtr<nsIWritableVariant> outVar(new nsVariant());

    switch (aResult->mScalarType)
    {
      case nsITelemetry::SCALAR_COUNT:
        {
          uint32_t data = 0;
          // De-serialize the data.
          if (!ReadParam(aMsg, aIter, &data) ||
              NS_FAILED(outVar->SetAsUint32(data))) {
            return false;
          }
          break;
        }
      case nsITelemetry::SCALAR_STRING:
        {
          // Keyed string scalars are not supported.
          MOZ_ASSERT(false, "Keyed String Scalar unable to be read from child process. Not supported.");
          return false;
        }
      case nsITelemetry::SCALAR_BOOLEAN:
        {
          bool data = false;
          // De-serialize the data.
          if (!ReadParam(aMsg, aIter, &data) ||
              NS_FAILED(outVar->SetAsBool(data))) {
            return false;
          }
          break;
        }
      default:
        MOZ_ASSERT(false, "Unknown keyed scalar type.");
        return false;
    }

    aResult->mData = outVar.forget();
    return true;
  }
};

} // namespace IPC

#endif // Telemetry_Comms_h__
