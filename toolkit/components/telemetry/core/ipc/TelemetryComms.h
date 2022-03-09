/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef Telemetry_Comms_h__
#define Telemetry_Comms_h__

#include "ipc/IPCMessageUtils.h"
#include "ipc/IPCMessageUtilsSpecializations.h"
#include "mozilla/Telemetry.h"
#include "mozilla/TelemetryProcessEnums.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/Variant.h"
#include "nsITelemetry.h"

namespace mozilla {
namespace Telemetry {

// Histogram accumulation types.
enum HistogramID : uint32_t;

struct HistogramAccumulation {
  mozilla::Telemetry::HistogramID mId;
  uint32_t mSample;
};

struct KeyedHistogramAccumulation {
  mozilla::Telemetry::HistogramID mId;
  uint32_t mSample;
  nsCString mKey;
};

// Scalar accumulation types.
enum class ScalarID : uint32_t;

enum class ScalarActionType : uint32_t { eSet = 0, eAdd = 1, eSetMaximum = 2 };

typedef mozilla::Variant<uint32_t, bool, nsString> ScalarVariant;

struct ScalarAction {
  uint32_t mId;
  bool mDynamic;
  ScalarActionType mActionType;
  // We need to wrap mData in a Maybe otherwise the IPC system
  // is unable to instantiate a ScalarAction.
  Maybe<ScalarVariant> mData;
  // The process type this scalar should be recorded for.
  // The IPC system will determine the process this action was coming from
  // later.
  mozilla::Telemetry::ProcessID mProcessType;
};

struct KeyedScalarAction {
  uint32_t mId;
  bool mDynamic;
  ScalarActionType mActionType;
  nsCString mKey;
  // We need to wrap mData in a Maybe otherwise the IPC system
  // is unable to instantiate a ScalarAction.
  Maybe<ScalarVariant> mData;
  // The process type this scalar should be recorded for.
  // The IPC system will determine the process this action was coming from
  // later.
  mozilla::Telemetry::ProcessID mProcessType;
};

// Dynamic scalars support.
struct DynamicScalarDefinition {
  uint32_t type;
  uint32_t dataset;
  bool expired;
  bool keyed;
  bool builtin;
  nsCString name;

  bool operator==(const DynamicScalarDefinition& rhs) const {
    return type == rhs.type && dataset == rhs.dataset &&
           expired == rhs.expired && keyed == rhs.keyed &&
           builtin == rhs.builtin && name.Equals(rhs.name);
  }
};

struct ChildEventData {
  mozilla::TimeStamp timestamp;
  nsCString category;
  nsCString method;
  nsCString object;
  mozilla::Maybe<nsCString> value;
  CopyableTArray<EventExtraEntry> extra;
};

struct DiscardedData {
  uint32_t mDiscardedHistogramAccumulations;
  uint32_t mDiscardedKeyedHistogramAccumulations;
  uint32_t mDiscardedScalarActions;
  uint32_t mDiscardedKeyedScalarActions;
  uint32_t mDiscardedChildEvents;
};

}  // namespace Telemetry
}  // namespace mozilla

namespace IPC {

template <>
struct ParamTraits<mozilla::Telemetry::HistogramAccumulation> {
  typedef mozilla::Telemetry::HistogramAccumulation paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    aWriter->WriteUInt32(aParam.mId);
    WriteParam(aWriter, aParam.mSample);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    if (!aReader->ReadUInt32(reinterpret_cast<uint32_t*>(&(aResult->mId))) ||
        !ReadParam(aReader, &(aResult->mSample))) {
      return false;
    }

    return true;
  }
};

template <>
struct ParamTraits<mozilla::Telemetry::KeyedHistogramAccumulation> {
  typedef mozilla::Telemetry::KeyedHistogramAccumulation paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    aWriter->WriteUInt32(aParam.mId);
    WriteParam(aWriter, aParam.mSample);
    WriteParam(aWriter, aParam.mKey);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    if (!aReader->ReadUInt32(reinterpret_cast<uint32_t*>(&(aResult->mId))) ||
        !ReadParam(aReader, &(aResult->mSample)) ||
        !ReadParam(aReader, &(aResult->mKey))) {
      return false;
    }

    return true;
  }
};

/**
 * IPC scalar data message serialization and de-serialization.
 */
template <>
struct ParamTraits<mozilla::Telemetry::ScalarAction> {
  typedef mozilla::Telemetry::ScalarAction paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    // Write the message type
    aWriter->WriteUInt32(aParam.mId);
    WriteParam(aWriter, aParam.mDynamic);
    WriteParam(aWriter, static_cast<uint32_t>(aParam.mActionType));

    if (aParam.mData.isNothing()) {
      MOZ_CRASH("There is no data in the ScalarAction.");
      return;
    }

    if (aParam.mData->is<uint32_t>()) {
      // That's a nsITelemetry::SCALAR_TYPE_COUNT.
      WriteParam(aWriter,
                 static_cast<uint32_t>(nsITelemetry::SCALAR_TYPE_COUNT));
      WriteParam(aWriter, aParam.mData->as<uint32_t>());
    } else if (aParam.mData->is<nsString>()) {
      // That's a nsITelemetry::SCALAR_TYPE_STRING.
      WriteParam(aWriter,
                 static_cast<uint32_t>(nsITelemetry::SCALAR_TYPE_STRING));
      WriteParam(aWriter, aParam.mData->as<nsString>());
    } else if (aParam.mData->is<bool>()) {
      // That's a nsITelemetry::SCALAR_TYPE_BOOLEAN.
      WriteParam(aWriter,
                 static_cast<uint32_t>(nsITelemetry::SCALAR_TYPE_BOOLEAN));
      WriteParam(aWriter, aParam.mData->as<bool>());
    } else {
      MOZ_CRASH("Unknown scalar type.");
    }
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    // Read the scalar ID and the scalar type.
    uint32_t scalarType = 0;
    if (!aReader->ReadUInt32(reinterpret_cast<uint32_t*>(&(aResult->mId))) ||
        !ReadParam(aReader, reinterpret_cast<bool*>(&(aResult->mDynamic))) ||
        !ReadParam(aReader,
                   reinterpret_cast<uint32_t*>(&(aResult->mActionType))) ||
        !ReadParam(aReader, &scalarType)) {
      return false;
    }

    // De-serialize the data based on the scalar type.
    switch (scalarType) {
      case nsITelemetry::SCALAR_TYPE_COUNT: {
        uint32_t data = 0;
        // De-serialize the data.
        if (!ReadParam(aReader, &data)) {
          return false;
        }
        aResult->mData = mozilla::Some(mozilla::AsVariant(data));
        break;
      }
      case nsITelemetry::SCALAR_TYPE_STRING: {
        nsString data;
        // De-serialize the data.
        if (!ReadParam(aReader, &data)) {
          return false;
        }
        aResult->mData = mozilla::Some(mozilla::AsVariant(data));
        break;
      }
      case nsITelemetry::SCALAR_TYPE_BOOLEAN: {
        bool data = false;
        // De-serialize the data.
        if (!ReadParam(aReader, &data)) {
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
template <>
struct ParamTraits<mozilla::Telemetry::KeyedScalarAction> {
  typedef mozilla::Telemetry::KeyedScalarAction paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    // Write the message type
    aWriter->WriteUInt32(static_cast<uint32_t>(aParam.mId));
    WriteParam(aWriter, aParam.mDynamic);
    WriteParam(aWriter, static_cast<uint32_t>(aParam.mActionType));
    WriteParam(aWriter, aParam.mKey);

    if (aParam.mData.isNothing()) {
      MOZ_CRASH("There is no data in the KeyedScalarAction.");
      return;
    }

    if (aParam.mData->is<uint32_t>()) {
      // That's a nsITelemetry::SCALAR_TYPE_COUNT.
      WriteParam(aWriter,
                 static_cast<uint32_t>(nsITelemetry::SCALAR_TYPE_COUNT));
      WriteParam(aWriter, aParam.mData->as<uint32_t>());
    } else if (aParam.mData->is<nsString>()) {
      // That's a nsITelemetry::SCALAR_TYPE_STRING.
      // Keyed string scalars are not supported.
      MOZ_ASSERT(false,
                 "Keyed String Scalar unable to be write from child process. "
                 "Not supported.");
    } else if (aParam.mData->is<bool>()) {
      // That's a nsITelemetry::SCALAR_TYPE_BOOLEAN.
      WriteParam(aWriter,
                 static_cast<uint32_t>(nsITelemetry::SCALAR_TYPE_BOOLEAN));
      WriteParam(aWriter, aParam.mData->as<bool>());
    } else {
      MOZ_CRASH("Unknown keyed scalar type.");
    }
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    // Read the scalar ID and the scalar type.
    uint32_t scalarType = 0;
    if (!aReader->ReadUInt32(reinterpret_cast<uint32_t*>(&(aResult->mId))) ||
        !ReadParam(aReader, reinterpret_cast<bool*>(&(aResult->mDynamic))) ||
        !ReadParam(aReader,
                   reinterpret_cast<uint32_t*>(&(aResult->mActionType))) ||
        !ReadParam(aReader, &(aResult->mKey)) ||
        !ReadParam(aReader, &scalarType)) {
      return false;
    }

    // De-serialize the data based on the scalar type.
    switch (scalarType) {
      case nsITelemetry::SCALAR_TYPE_COUNT: {
        uint32_t data = 0;
        // De-serialize the data.
        if (!ReadParam(aReader, &data)) {
          return false;
        }
        aResult->mData = mozilla::Some(mozilla::AsVariant(data));
        break;
      }
      case nsITelemetry::SCALAR_TYPE_STRING: {
        // Keyed string scalars are not supported.
        MOZ_ASSERT(false,
                   "Keyed String Scalar unable to be read from child process. "
                   "Not supported.");
        return false;
      }
      case nsITelemetry::SCALAR_TYPE_BOOLEAN: {
        bool data = false;
        // De-serialize the data.
        if (!ReadParam(aReader, &data)) {
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

template <>
struct ParamTraits<mozilla::Telemetry::DynamicScalarDefinition> {
  typedef mozilla::Telemetry::DynamicScalarDefinition paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    nsCString name;
    WriteParam(aWriter, aParam.type);
    WriteParam(aWriter, aParam.dataset);
    WriteParam(aWriter, aParam.expired);
    WriteParam(aWriter, aParam.keyed);
    WriteParam(aWriter, aParam.builtin);
    WriteParam(aWriter, aParam.name);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    if (!ReadParam(aReader, reinterpret_cast<uint32_t*>(&(aResult->type))) ||
        !ReadParam(aReader, reinterpret_cast<uint32_t*>(&(aResult->dataset))) ||
        !ReadParam(aReader, reinterpret_cast<bool*>(&(aResult->expired))) ||
        !ReadParam(aReader, reinterpret_cast<bool*>(&(aResult->keyed))) ||
        !ReadParam(aReader, reinterpret_cast<bool*>(&(aResult->builtin))) ||
        !ReadParam(aReader, &(aResult->name))) {
      return false;
    }
    return true;
  }
};

template <>
struct ParamTraits<mozilla::Telemetry::ChildEventData> {
  typedef mozilla::Telemetry::ChildEventData paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.timestamp);
    WriteParam(aWriter, aParam.category);
    WriteParam(aWriter, aParam.method);
    WriteParam(aWriter, aParam.object);
    WriteParam(aWriter, aParam.value);
    WriteParam(aWriter, aParam.extra);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    if (!ReadParam(aReader, &(aResult->timestamp)) ||
        !ReadParam(aReader, &(aResult->category)) ||
        !ReadParam(aReader, &(aResult->method)) ||
        !ReadParam(aReader, &(aResult->object)) ||
        !ReadParam(aReader, &(aResult->value)) ||
        !ReadParam(aReader, &(aResult->extra))) {
      return false;
    }

    return true;
  }
};

template <>
struct ParamTraits<mozilla::Telemetry::EventExtraEntry> {
  typedef mozilla::Telemetry::EventExtraEntry paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.key);
    WriteParam(aWriter, aParam.value);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    if (!ReadParam(aReader, &(aResult->key)) ||
        !ReadParam(aReader, &(aResult->value))) {
      return false;
    }

    return true;
  }
};

template <>
struct ParamTraits<mozilla::Telemetry::DiscardedData>
    : public PlainOldDataSerializer<mozilla::Telemetry::DiscardedData> {};

}  // namespace IPC

#endif  // Telemetry_Comms_h__
