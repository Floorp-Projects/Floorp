/* -*-  Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 */

#ifndef Telemetry_Comms_h__
#define Telemetry_Comms_h__

#include "ipc/IPCMessageUtils.h"
#include "nsITelemetry.h"
#include "nsVariant.h"
#include "mozilla/TimeStamp.h"

namespace mozilla {
namespace Telemetry {

// Histogram accumulation types.
enum HistogramID : uint32_t;

struct Accumulation
{
  mozilla::Telemetry::HistogramID mId;
  uint32_t mSample;
};

struct KeyedAccumulation
{
  mozilla::Telemetry::HistogramID mId;
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

typedef mozilla::Variant<uint32_t, bool, nsString> ScalarVariant;

struct ScalarAction
{
  ScalarID mId;
  ScalarActionType mActionType;
  // We need to wrap mData in a Maybe otherwise the IPC system
  // is unable to instantiate a ScalarAction.
  Maybe<ScalarVariant> mData;
};

struct KeyedScalarAction
{
  ScalarID mId;
  ScalarActionType mActionType;
  nsCString mKey;
  // We need to wrap mData in a Maybe otherwise the IPC system
  // is unable to instantiate a ScalarAction.
  Maybe<ScalarVariant> mData;
};

struct EventExtraEntry {
  nsCString key;
  nsCString value;
};

struct ChildEventData {
  mozilla::TimeStamp timestamp;
  nsCString category;
  nsCString method;
  nsCString object;
  mozilla::Maybe<nsCString> value;
  nsTArray<EventExtraEntry> extra;
};

struct DiscardedData {
  uint32_t mDiscardedHistogramAccumulations;
  uint32_t mDiscardedKeyedHistogramAccumulations;
  uint32_t mDiscardedScalarActions;
  uint32_t mDiscardedKeyedScalarActions;
  uint32_t mDiscardedChildEvents;
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
    WriteParam(aMsg, static_cast<uint32_t>(aParam.mActionType));

    if (aParam.mData.isNothing()) {
      MOZ_CRASH("There is no data in the ScalarAction.");
      return;
    }

    if (aParam.mData->is<uint32_t>()) {
      // That's a nsITelemetry::SCALAR_COUNT.
      WriteParam(aMsg, static_cast<uint32_t>(nsITelemetry::SCALAR_COUNT));
      WriteParam(aMsg, aParam.mData->as<uint32_t>());
    } else if (aParam.mData->is<nsString>()) {
      // That's a nsITelemetry::SCALAR_STRING.
      WriteParam(aMsg, static_cast<uint32_t>(nsITelemetry::SCALAR_STRING));
      WriteParam(aMsg, aParam.mData->as<nsString>());
    } else if (aParam.mData->is<bool>()) {
      // That's a nsITelemetry::SCALAR_BOOLEAN.
      WriteParam(aMsg, static_cast<uint32_t>(nsITelemetry::SCALAR_BOOLEAN));
      WriteParam(aMsg, aParam.mData->as<bool>());
    } else {
      MOZ_CRASH("Unknown scalar type.");
    }
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter, paramType* aResult)
  {
    // Read the scalar ID and the scalar type.
    uint32_t scalarType = 0;
    if (!aMsg->ReadUInt32(aIter, reinterpret_cast<uint32_t*>(&(aResult->mId))) ||
        !ReadParam(aMsg, aIter, reinterpret_cast<uint32_t*>(&(aResult->mActionType))) ||
        !ReadParam(aMsg, aIter, &scalarType)) {
      return false;
    }

    // De-serialize the data based on the scalar type.
    switch (scalarType)
    {
      case nsITelemetry::SCALAR_COUNT:
        {
          uint32_t data = 0;
          // De-serialize the data.
          if (!ReadParam(aMsg, aIter, &data)) {
            return false;
          }
          aResult->mData = mozilla::Some(mozilla::AsVariant(data));
          break;
        }
      case nsITelemetry::SCALAR_STRING:
        {
          nsString data;
          // De-serialize the data.
          if (!ReadParam(aMsg, aIter, &data)) {
            return false;
          }
          aResult->mData = mozilla::Some(mozilla::AsVariant(data));
          break;
        }
      case nsITelemetry::SCALAR_BOOLEAN:
        {
          bool data = false;
          // De-serialize the data.
          if (!ReadParam(aMsg, aIter, &data)) {
            return false;
          }
          aResult->mData = mozilla::Some(mozilla::AsVariant(data));
          break;
        }
      default:
        MOZ_ASSERT(false, "Unknown scalar type.");
        return false;
    }

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
    WriteParam(aMsg, static_cast<uint32_t>(aParam.mActionType));
    WriteParam(aMsg, aParam.mKey);

    if (aParam.mData.isNothing()) {
      MOZ_CRASH("There is no data in the KeyedScalarAction.");
      return;
    }

    if (aParam.mData->is<uint32_t>()) {
      // That's a nsITelemetry::SCALAR_COUNT.
      WriteParam(aMsg, static_cast<uint32_t>(nsITelemetry::SCALAR_COUNT));
      WriteParam(aMsg, aParam.mData->as<uint32_t>());
    } else if (aParam.mData->is<nsString>()) {
      // That's a nsITelemetry::SCALAR_STRING.
      // Keyed string scalars are not supported.
      MOZ_ASSERT(false, "Keyed String Scalar unable to be write from child process. Not supported.");
    } else if (aParam.mData->is<bool>()) {
      // That's a nsITelemetry::SCALAR_BOOLEAN.
      WriteParam(aMsg, static_cast<uint32_t>(nsITelemetry::SCALAR_BOOLEAN));
      WriteParam(aMsg, aParam.mData->as<bool>());
    } else {
      MOZ_CRASH("Unknown keyed scalar type.");
    }
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter, paramType* aResult)
  {
    // Read the scalar ID and the scalar type.
    uint32_t scalarType = 0;
    if (!aMsg->ReadUInt32(aIter, reinterpret_cast<uint32_t*>(&(aResult->mId))) ||
        !ReadParam(aMsg, aIter, reinterpret_cast<uint32_t*>(&(aResult->mActionType))) ||
        !ReadParam(aMsg, aIter, &(aResult->mKey)) ||
        !ReadParam(aMsg, aIter, &scalarType)) {
      return false;
    }

    // De-serialize the data based on the scalar type.
    switch (scalarType)
    {
      case nsITelemetry::SCALAR_COUNT:
        {
          uint32_t data = 0;
          // De-serialize the data.
          if (!ReadParam(aMsg, aIter, &data)) {
            return false;
          }
          aResult->mData = mozilla::Some(mozilla::AsVariant(data));
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
          if (!ReadParam(aMsg, aIter, &data)) {
            return false;
          }
          aResult->mData = mozilla::Some(mozilla::AsVariant(data));
          break;
        }
      default:
        MOZ_ASSERT(false, "Unknown keyed scalar type.");
        return false;
    }

    return true;
  }
};

template<>
struct
ParamTraits<mozilla::Telemetry::ChildEventData>
{
  typedef mozilla::Telemetry::ChildEventData paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.timestamp);
    WriteParam(aMsg, aParam.category);
    WriteParam(aMsg, aParam.method);
    WriteParam(aMsg, aParam.object);
    WriteParam(aMsg, aParam.value);
    WriteParam(aMsg, aParam.extra);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter, paramType* aResult)
  {
    if (!ReadParam(aMsg, aIter, &(aResult->timestamp)) ||
        !ReadParam(aMsg, aIter, &(aResult->category)) ||
        !ReadParam(aMsg, aIter, &(aResult->method)) ||
        !ReadParam(aMsg, aIter, &(aResult->object)) ||
        !ReadParam(aMsg, aIter, &(aResult->value)) ||
        !ReadParam(aMsg, aIter, &(aResult->extra))) {
      return false;
    }

    return true;
  }
};

template<>
struct
ParamTraits<mozilla::Telemetry::EventExtraEntry>
{
  typedef mozilla::Telemetry::EventExtraEntry paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.key);
    WriteParam(aMsg, aParam.value);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter, paramType* aResult)
  {
    if (!ReadParam(aMsg, aIter, &(aResult->key)) ||
        !ReadParam(aMsg, aIter, &(aResult->value))) {
      return false;
    }

    return true;
  }
};

template<>
struct
ParamTraits<mozilla::Telemetry::DiscardedData>
  : public PlainOldDataSerializer<mozilla::Telemetry::DiscardedData>
{ };

} // namespace IPC

#endif // Telemetry_Comms_h__
