/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TelemetryScalar.h"

#include "geckoview/streaming/GeckoViewStreamingTelemetry.h"
#include "ipc/TelemetryIPCAccumulator.h"
#include "js/Array.h"               // JS::GetArrayLength, JS::IsArrayObject
#include "js/PropertyAndElement.h"  // JS_DefineProperty, JS_DefineUCProperty, JS_Enumerate, JS_GetElement, JS_GetProperty, JS_GetPropertyById, JS_HasProperty
#include "mozilla/dom/ContentParent.h"
#include "mozilla/JSONWriter.h"
#include "mozilla/Preferences.h"
#include "mozilla/StaticMutex.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/TelemetryComms.h"
#include "mozilla/Unused.h"
#include "nsBaseHashtable.h"
#include "nsClassHashtable.h"
#include "nsContentUtils.h"
#include "nsHashKeys.h"
#include "nsITelemetry.h"
#include "nsIVariant.h"
#include "nsIXPConnect.h"
#include "nsJSUtils.h"
#include "nsPrintfCString.h"
#include "nsVariant.h"
#include "TelemetryScalarData.h"

using mozilla::MakeUnique;
using mozilla::Nothing;
using mozilla::Preferences;
using mozilla::Some;
using mozilla::StaticAutoPtr;
using mozilla::StaticMutex;
using mozilla::StaticMutexAutoLock;
using mozilla::UniquePtr;
using mozilla::Telemetry::DynamicScalarDefinition;
using mozilla::Telemetry::KeyedScalarAction;
using mozilla::Telemetry::ProcessID;
using mozilla::Telemetry::ScalarAction;
using mozilla::Telemetry::ScalarActionType;
using mozilla::Telemetry::ScalarID;
using mozilla::Telemetry::ScalarVariant;
using mozilla::Telemetry::Common::AutoHashtable;
using mozilla::Telemetry::Common::CanRecordDataset;
using mozilla::Telemetry::Common::CanRecordProduct;
using mozilla::Telemetry::Common::GetCurrentProduct;
using mozilla::Telemetry::Common::GetIDForProcessName;
using mozilla::Telemetry::Common::GetNameForProcessID;
using mozilla::Telemetry::Common::IsExpiredVersion;
using mozilla::Telemetry::Common::IsInDataset;
using mozilla::Telemetry::Common::IsValidIdentifierString;
using mozilla::Telemetry::Common::LogToBrowserConsole;
using mozilla::Telemetry::Common::RecordedProcessType;
using mozilla::Telemetry::Common::StringHashSet;
using mozilla::Telemetry::Common::SupportedProduct;

namespace TelemetryIPCAccumulator = mozilla::TelemetryIPCAccumulator;

////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
//
// Naming: there are two kinds of functions in this file:
//
// * Functions named internal_*: these can only be reached via an
//   interface function (TelemetryScalar::*). If they access shared
//   state, they require the interface function to have acquired
//   |gTelemetryScalarMutex| to ensure thread safety.
//
// * Functions named TelemetryScalar::*. This is the external interface.
//   Entries and exits to these functions are serialised using
//   |gTelemetryScalarsMutex|.
//
// Avoiding races and deadlocks:
//
// All functions in the external interface (TelemetryScalar::*) are
// serialised using the mutex |gTelemetryScalarsMutex|. This means
// that the external interface is thread-safe. But it also brings
// a danger of deadlock if any function in the external interface can
// get back to that interface. That is, we will deadlock on any call
// chain like this
//
// TelemetryScalar::* -> .. any functions .. -> TelemetryScalar::*
//
// To reduce the danger of that happening, observe the following rules:
//
// * No function in TelemetryScalar::* may directly call, nor take the
//   address of, any other function in TelemetryScalar::*.
//
// * No internal function internal_* may call, nor take the address
//   of, any function in TelemetryScalar::*.

////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
//
// PRIVATE TYPES

namespace {

const uint32_t kMaximumNumberOfKeys = 100;
const uint32_t kMaxEventSummaryKeys = 500;
const uint32_t kMaximumKeyStringLength = 72;
const uint32_t kMaximumStringValueLength = 50;
// The category and scalar name maximum lengths are used by the dynamic
// scalar registration function and must match the constants used by
// the 'parse_scalars.py' script for static scalars.
const uint32_t kMaximumCategoryNameLength = 40;
const uint32_t kMaximumScalarNameLength = 40;
const uint32_t kScalarCount =
    static_cast<uint32_t>(mozilla::Telemetry::ScalarID::ScalarCount);

// To stop growing unbounded in memory while waiting for scalar deserialization
// to finish, we immediately apply pending operations if the array reaches
// a certain high water mark of elements.
const size_t kScalarActionsArrayHighWaterMark = 10000;

const char* TEST_SCALAR_PREFIX = "telemetry.test.";

// The max offset supported by gScalarStoresTable for static scalars' stores.
// Also the sentinel value (with store_count == 0) for just the sole "main"
// store.
const uint32_t kMaxStaticStoreOffset = UINT16_MAX;

enum class ScalarResult : uint8_t {
  // Nothing went wrong.
  Ok,
  // General Scalar Errors
  NotInitialized,
  CannotUnpackVariant,
  CannotRecordInProcess,
  CannotRecordDataset,
  KeyedTypeMismatch,
  UnknownScalar,
  OperationNotSupported,
  InvalidType,
  InvalidValue,
  // Keyed Scalar Errors
  KeyIsEmpty,
  KeyTooLong,
  TooManyKeys,
  KeyNotAllowed,
  // String Scalar Errors
  StringTooLong,
  // Unsigned Scalar Errors
  UnsignedNegativeValue,
  UnsignedTruncatedValue,
};

// A common identifier for both built-in and dynamic scalars.
struct ScalarKey {
  uint32_t id;
  bool dynamic;
};

// Dynamic scalar store names.
StaticAutoPtr<nsTArray<RefPtr<nsAtom>>> gDynamicStoreNames;

/**
 * Scalar information for dynamic definitions.
 */
struct DynamicScalarInfo : BaseScalarInfo {
  nsCString mDynamicName;
  bool mDynamicExpiration;
  uint32_t store_count;
  uint32_t store_offset;

  DynamicScalarInfo(uint32_t aKind, bool aRecordOnRelease, bool aExpired,
                    const nsACString& aName, bool aKeyed, bool aBuiltin,
                    const nsTArray<nsCString>& aStores)
      : BaseScalarInfo(aKind,
                       aRecordOnRelease
                           ? nsITelemetry::DATASET_ALL_CHANNELS
                           : nsITelemetry::DATASET_PRERELEASE_CHANNELS,
                       RecordedProcessType::All, aKeyed, 0, 0,
                       GetCurrentProduct(), aBuiltin),
        mDynamicName(aName),
        mDynamicExpiration(aExpired) {
    store_count = aStores.Length();
    if (store_count == 0) {
      store_count = 1;
      store_offset = kMaxStaticStoreOffset;
    } else {
      store_offset = kMaxStaticStoreOffset + 1 + gDynamicStoreNames->Length();
      for (const auto& storeName : aStores) {
        gDynamicStoreNames->AppendElement(NS_Atomize(storeName));
      }
      MOZ_ASSERT(
          gDynamicStoreNames->Length() < UINT32_MAX - kMaxStaticStoreOffset - 1,
          "Too many dynamic scalar store names. Overflow.");
    }
  };

  // The following functions will read the stored text
  // instead of looking it up in the statically generated
  // tables.
  const char* name() const override;
  const char* expiration() const override;

  uint32_t storeCount() const override;
  uint32_t storeOffset() const override;
};

const char* DynamicScalarInfo::name() const { return mDynamicName.get(); }

const char* DynamicScalarInfo::expiration() const {
  // Dynamic scalars can either be expired or not (boolean flag).
  // Return an appropriate version string to leverage the scalar expiration
  // logic.
  return mDynamicExpiration ? "1.0" : "never";
}

uint32_t DynamicScalarInfo::storeOffset() const { return store_offset; }
uint32_t DynamicScalarInfo::storeCount() const { return store_count; }

typedef nsBaseHashtableET<nsDepCharHashKey, ScalarKey> CharPtrEntryType;
typedef AutoHashtable<CharPtrEntryType> ScalarMapType;

// Dynamic scalar definitions.
StaticAutoPtr<nsTArray<DynamicScalarInfo>> gDynamicScalarInfo;

const BaseScalarInfo& internal_GetScalarInfo(const StaticMutexAutoLock& lock,
                                             const ScalarKey& aId) {
  if (!aId.dynamic) {
    return gScalars[aId.id];
  }

  return (*gDynamicScalarInfo)[aId.id];
}

bool IsValidEnumId(mozilla::Telemetry::ScalarID aID) {
  return aID < mozilla::Telemetry::ScalarID::ScalarCount;
}

bool internal_IsValidId(const StaticMutexAutoLock& lock, const ScalarKey& aId) {
  // Please note that this function needs to be called with the scalar
  // mutex being acquired: other functions might be messing with
  // |gDynamicScalarInfo|.
  return aId.dynamic
             ? (aId.id < gDynamicScalarInfo->Length())
             : IsValidEnumId(static_cast<mozilla::Telemetry::ScalarID>(aId.id));
}

/**
 * Convert a nsIVariant to a mozilla::Variant, which is used for
 * accumulating child process scalars.
 */
ScalarResult GetVariantFromIVariant(nsIVariant* aInput, uint32_t aScalarKind,
                                    mozilla::Maybe<ScalarVariant>& aOutput) {
  switch (aScalarKind) {
    case nsITelemetry::SCALAR_TYPE_COUNT: {
      uint32_t val = 0;
      nsresult rv = aInput->GetAsUint32(&val);
      if (NS_FAILED(rv)) {
        return ScalarResult::CannotUnpackVariant;
      }
      aOutput = mozilla::Some(mozilla::AsVariant(val));
      break;
    }
    case nsITelemetry::SCALAR_TYPE_STRING: {
      nsString val;
      nsresult rv = aInput->GetAsAString(val);
      if (NS_FAILED(rv)) {
        return ScalarResult::CannotUnpackVariant;
      }
      aOutput = mozilla::Some(mozilla::AsVariant(val));
      break;
    }
    case nsITelemetry::SCALAR_TYPE_BOOLEAN: {
      bool val = false;
      nsresult rv = aInput->GetAsBool(&val);
      if (NS_FAILED(rv)) {
        return ScalarResult::CannotUnpackVariant;
      }
      aOutput = mozilla::Some(mozilla::AsVariant(val));
      break;
    }
    default:
      MOZ_ASSERT(false, "Unknown scalar kind.");
      return ScalarResult::UnknownScalar;
  }
  return ScalarResult::Ok;
}

/**
 * Write a nsIVariant with a JSONWriter, used for GeckoView persistence.
 */
nsresult WriteVariantToJSONWriter(
    uint32_t aScalarType, nsIVariant* aInputValue,
    const mozilla::Span<const char>& aPropertyName,
    mozilla::JSONWriter& aWriter) {
  MOZ_ASSERT(aInputValue);

  switch (aScalarType) {
    case nsITelemetry::SCALAR_TYPE_COUNT: {
      uint32_t val = 0;
      nsresult rv = aInputValue->GetAsUint32(&val);
      NS_ENSURE_SUCCESS(rv, rv);
      aWriter.IntProperty(aPropertyName, val);
      break;
    }
    case nsITelemetry::SCALAR_TYPE_STRING: {
      nsCString val;
      nsresult rv = aInputValue->GetAsACString(val);
      NS_ENSURE_SUCCESS(rv, rv);
      aWriter.StringProperty(aPropertyName, val);
      break;
    }
    case nsITelemetry::SCALAR_TYPE_BOOLEAN: {
      bool val = false;
      nsresult rv = aInputValue->GetAsBool(&val);
      NS_ENSURE_SUCCESS(rv, rv);
      aWriter.BoolProperty(aPropertyName, val);
      break;
    }
    default:
      MOZ_ASSERT(false, "Unknown scalar kind.");
      return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

// Implements the methods for ScalarInfo.
const char* ScalarInfo::name() const {
  return &gScalarsStringTable[this->name_offset];
}

const char* ScalarInfo::expiration() const {
  return &gScalarsStringTable[this->expiration_offset];
}

/**
 * The base scalar object, that serves as a common ancestor for storage
 * purposes.
 */
class ScalarBase {
 public:
  explicit ScalarBase(const BaseScalarInfo& aInfo)
      : mStoreCount(aInfo.storeCount()),
        mStoreOffset(aInfo.storeOffset()),
        mStoreHasValue(mStoreCount),
        mName(aInfo.name()) {
    mStoreHasValue.SetLength(mStoreCount);
    for (auto& val : mStoreHasValue) {
      val = false;
    }
  };
  virtual ~ScalarBase() = default;

  // Set, Add and SetMaximum functions as described in the Telemetry IDL.
  virtual ScalarResult SetValue(nsIVariant* aValue) = 0;
  virtual ScalarResult AddValue(nsIVariant* aValue) {
    return ScalarResult::OperationNotSupported;
  }
  virtual ScalarResult SetMaximum(nsIVariant* aValue) {
    return ScalarResult::OperationNotSupported;
  }

  // Convenience methods used by the C++ API.
  virtual void SetValue(uint32_t aValue) {
    mozilla::Unused << HandleUnsupported();
  }
  virtual ScalarResult SetValue(const nsAString& aValue) {
    return HandleUnsupported();
  }
  virtual void SetValue(bool aValue) { mozilla::Unused << HandleUnsupported(); }
  virtual void AddValue(uint32_t aValue) {
    mozilla::Unused << HandleUnsupported();
  }
  virtual void SetMaximum(uint32_t aValue) {
    mozilla::Unused << HandleUnsupported();
  }

  // GetValue is used to get the value of the scalar when persisting it to JS.
  virtual nsresult GetValue(const nsACString& aStoreName, bool aClearStore,
                            nsCOMPtr<nsIVariant>& aResult) = 0;

  // To measure the memory stats.
  size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;
  virtual size_t SizeOfIncludingThis(
      mozilla::MallocSizeOf aMallocSizeOf) const = 0;

 protected:
  bool HasValueInStore(size_t aStoreIndex) const;
  void ClearValueInStore(size_t aStoreIndex);
  void SetValueInStores();
  nsresult StoreIndex(const nsACString& aStoreName, size_t* aStoreIndex) const;

 private:
  ScalarResult HandleUnsupported() const;

  const uint32_t mStoreCount;
  const uint32_t mStoreOffset;
  nsTArray<bool> mStoreHasValue;

 protected:
  const nsCString mName;
};

ScalarResult ScalarBase::HandleUnsupported() const {
  MOZ_ASSERT(false, "This operation is not support for this scalar type.");
  return ScalarResult::OperationNotSupported;
}

bool ScalarBase::HasValueInStore(size_t aStoreIndex) const {
  MOZ_ASSERT(aStoreIndex < mStoreHasValue.Length(),
             "Invalid scalar store index.");
  return mStoreHasValue[aStoreIndex];
}

void ScalarBase::ClearValueInStore(size_t aStoreIndex) {
  MOZ_ASSERT(aStoreIndex < mStoreHasValue.Length(),
             "Invalid scalar store index to clear.");
  mStoreHasValue[aStoreIndex] = false;
}

void ScalarBase::SetValueInStores() {
  for (auto& val : mStoreHasValue) {
    val = true;
  }
}

nsresult ScalarBase::StoreIndex(const nsACString& aStoreName,
                                size_t* aStoreIndex) const {
  if (mStoreCount == 1 && mStoreOffset == kMaxStaticStoreOffset) {
    // This Scalar is only in the "main" store.
    if (aStoreName.EqualsLiteral("main")) {
      *aStoreIndex = 0;
      return NS_OK;
    }
    return NS_ERROR_NO_CONTENT;
  }

  // Multiple stores. Linear scan to find one that matches aStoreName.
  // Dynamic Scalars start at kMaxStaticStoreOffset + 1
  if (mStoreOffset > kMaxStaticStoreOffset) {
    auto dynamicOffset = mStoreOffset - kMaxStaticStoreOffset - 1;
    for (uint32_t i = 0; i < mStoreCount; ++i) {
      auto scalarStore = (*gDynamicStoreNames)[dynamicOffset + i];
      if (nsAtomCString(scalarStore).Equals(aStoreName)) {
        *aStoreIndex = i;
        return NS_OK;
      }
    }
    return NS_ERROR_NO_CONTENT;
  }

  // Static Scalars are similar.
  for (uint32_t i = 0; i < mStoreCount; ++i) {
    uint32_t stringIndex = gScalarStoresTable[mStoreOffset + i];
    if (aStoreName.EqualsASCII(&gScalarsStringTable[stringIndex])) {
      *aStoreIndex = i;
      return NS_OK;
    }
  }
  return NS_ERROR_NO_CONTENT;
}

size_t ScalarBase::SizeOfExcludingThis(
    mozilla::MallocSizeOf aMallocSizeOf) const {
  return mStoreHasValue.ShallowSizeOfExcludingThis(aMallocSizeOf);
}

/**
 * The implementation for the unsigned int scalar type.
 */
class ScalarUnsigned : public ScalarBase {
 public:
  using ScalarBase::SetValue;

  explicit ScalarUnsigned(const BaseScalarInfo& aInfo)
      : ScalarBase(aInfo), mStorage(aInfo.storeCount()) {
    mStorage.SetLength(aInfo.storeCount());
    for (auto& val : mStorage) {
      val = 0;
    }
  };

  ~ScalarUnsigned() override = default;

  ScalarResult SetValue(nsIVariant* aValue) final;
  void SetValue(uint32_t aValue) final;
  ScalarResult AddValue(nsIVariant* aValue) final;
  void AddValue(uint32_t aValue) final;
  ScalarResult SetMaximum(nsIVariant* aValue) final;
  void SetMaximum(uint32_t aValue) final;
  nsresult GetValue(const nsACString& aStoreName, bool aClearStore,
                    nsCOMPtr<nsIVariant>& aResult) final;
  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const final;

 private:
  nsTArray<uint32_t> mStorage;

  ScalarResult CheckInput(nsIVariant* aValue);

  // Prevent copying.
  ScalarUnsigned(const ScalarUnsigned& aOther) = delete;
  void operator=(const ScalarUnsigned& aOther) = delete;
};

ScalarResult ScalarUnsigned::SetValue(nsIVariant* aValue) {
  ScalarResult sr = CheckInput(aValue);
  if (sr == ScalarResult::UnsignedNegativeValue) {
    return sr;
  }

  uint32_t value = 0;
  if (NS_FAILED(aValue->GetAsUint32(&value))) {
    return ScalarResult::InvalidValue;
  }

  SetValue(value);
  return sr;
}

void ScalarUnsigned::SetValue(uint32_t aValue) {
  if (GetCurrentProduct() == SupportedProduct::GeckoviewStreaming) {
    GeckoViewStreamingTelemetry::UintScalarSet(mName, aValue);
    return;
  }
  for (auto& val : mStorage) {
    val = aValue;
  }
  SetValueInStores();
}

ScalarResult ScalarUnsigned::AddValue(nsIVariant* aValue) {
  ScalarResult sr = CheckInput(aValue);
  if (sr == ScalarResult::UnsignedNegativeValue) {
    return sr;
  }

  uint32_t newAddend = 0;
  nsresult rv = aValue->GetAsUint32(&newAddend);
  if (NS_FAILED(rv)) {
    return ScalarResult::InvalidValue;
  }

  AddValue(newAddend);
  return sr;
}

void ScalarUnsigned::AddValue(uint32_t aValue) {
  MOZ_ASSERT(GetCurrentProduct() != SupportedProduct::GeckoviewStreaming);
  for (auto& val : mStorage) {
    val += aValue;
  }
  SetValueInStores();
}

ScalarResult ScalarUnsigned::SetMaximum(nsIVariant* aValue) {
  MOZ_ASSERT(GetCurrentProduct() != SupportedProduct::GeckoviewStreaming);
  ScalarResult sr = CheckInput(aValue);
  if (sr == ScalarResult::UnsignedNegativeValue) {
    return sr;
  }

  uint32_t newValue = 0;
  nsresult rv = aValue->GetAsUint32(&newValue);
  if (NS_FAILED(rv)) {
    return ScalarResult::InvalidValue;
  }

  SetMaximum(newValue);
  return sr;
}

void ScalarUnsigned::SetMaximum(uint32_t aValue) {
  for (auto& val : mStorage) {
    if (aValue > val) {
      val = aValue;
    }
  }
  SetValueInStores();
}

nsresult ScalarUnsigned::GetValue(const nsACString& aStoreName,
                                  bool aClearStore,
                                  nsCOMPtr<nsIVariant>& aResult) {
  size_t storeIndex = 0;
  nsresult rv = StoreIndex(aStoreName, &storeIndex);
  if (NS_FAILED(rv)) {
    return rv;
  }
  if (!HasValueInStore(storeIndex)) {
    return NS_ERROR_NO_CONTENT;
  }
  nsCOMPtr<nsIWritableVariant> outVar(new nsVariant());
  rv = outVar->SetAsUint32(mStorage[storeIndex]);
  if (NS_FAILED(rv)) {
    return rv;
  }
  aResult = std::move(outVar);
  if (aClearStore) {
    mStorage[storeIndex] = 0;
    ClearValueInStore(storeIndex);
  }
  return NS_OK;
}

size_t ScalarUnsigned::SizeOfIncludingThis(
    mozilla::MallocSizeOf aMallocSizeOf) const {
  size_t n = aMallocSizeOf(this);
  n += ScalarBase::SizeOfExcludingThis(aMallocSizeOf);
  n += mStorage.ShallowSizeOfExcludingThis(aMallocSizeOf);
  return n;
}

ScalarResult ScalarUnsigned::CheckInput(nsIVariant* aValue) {
  // If this is a floating point value/double, we will probably get truncated.
  uint16_t type = aValue->GetDataType();
  if (type == nsIDataType::VTYPE_FLOAT || type == nsIDataType::VTYPE_DOUBLE) {
    return ScalarResult::UnsignedTruncatedValue;
  }

  int32_t signedTest;
  // If we're able to cast the number to an int, check its sign.
  // Warn the user if he's trying to set the unsigned scalar to a negative
  // number.
  if (NS_SUCCEEDED(aValue->GetAsInt32(&signedTest)) && signedTest < 0) {
    return ScalarResult::UnsignedNegativeValue;
  }
  return ScalarResult::Ok;
}

/**
 * The implementation for the string scalar type.
 */
class ScalarString : public ScalarBase {
 public:
  using ScalarBase::SetValue;

  explicit ScalarString(const BaseScalarInfo& aInfo)
      : ScalarBase(aInfo), mStorage(aInfo.storeCount()) {
    mStorage.SetLength(aInfo.storeCount());
  };

  ~ScalarString() override = default;

  ScalarResult SetValue(nsIVariant* aValue) final;
  ScalarResult SetValue(const nsAString& aValue) final;
  nsresult GetValue(const nsACString& aStoreName, bool aClearStore,
                    nsCOMPtr<nsIVariant>& aResult) final;
  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const final;

 private:
  nsTArray<nsString> mStorage;

  // Prevent copying.
  ScalarString(const ScalarString& aOther) = delete;
  void operator=(const ScalarString& aOther) = delete;
};

ScalarResult ScalarString::SetValue(nsIVariant* aValue) {
  // Check that we got the correct data type.
  uint16_t type = aValue->GetDataType();
  if (type != nsIDataType::VTYPE_CHAR && type != nsIDataType::VTYPE_WCHAR &&
      type != nsIDataType::VTYPE_CHAR_STR &&
      type != nsIDataType::VTYPE_WCHAR_STR &&
      type != nsIDataType::VTYPE_STRING_SIZE_IS &&
      type != nsIDataType::VTYPE_WSTRING_SIZE_IS &&
      type != nsIDataType::VTYPE_UTF8STRING &&
      type != nsIDataType::VTYPE_CSTRING &&
      type != nsIDataType::VTYPE_ASTRING) {
    return ScalarResult::InvalidType;
  }

  nsAutoString convertedString;
  nsresult rv = aValue->GetAsAString(convertedString);
  if (NS_FAILED(rv)) {
    return ScalarResult::InvalidValue;
  }
  return SetValue(convertedString);
};

ScalarResult ScalarString::SetValue(const nsAString& aValue) {
  auto str = Substring(aValue, 0, kMaximumStringValueLength);
  if (GetCurrentProduct() == SupportedProduct::GeckoviewStreaming) {
    GeckoViewStreamingTelemetry::StringScalarSet(mName,
                                                 NS_ConvertUTF16toUTF8(str));
    return aValue.Length() > kMaximumStringValueLength
               ? ScalarResult::StringTooLong
               : ScalarResult::Ok;
  }
  for (auto& val : mStorage) {
    val.Assign(str);
  }
  SetValueInStores();
  if (aValue.Length() > kMaximumStringValueLength) {
    return ScalarResult::StringTooLong;
  }
  return ScalarResult::Ok;
}

nsresult ScalarString::GetValue(const nsACString& aStoreName, bool aClearStore,
                                nsCOMPtr<nsIVariant>& aResult) {
  nsCOMPtr<nsIWritableVariant> outVar(new nsVariant());
  size_t storeIndex = 0;
  nsresult rv = StoreIndex(aStoreName, &storeIndex);
  if (NS_FAILED(rv)) {
    return rv;
  }
  if (!HasValueInStore(storeIndex)) {
    return NS_ERROR_NO_CONTENT;
  }
  rv = outVar->SetAsAString(mStorage[storeIndex]);
  if (NS_FAILED(rv)) {
    return rv;
  }
  if (aClearStore) {
    ClearValueInStore(storeIndex);
  }
  aResult = std::move(outVar);
  return NS_OK;
}

size_t ScalarString::SizeOfIncludingThis(
    mozilla::MallocSizeOf aMallocSizeOf) const {
  size_t n = aMallocSizeOf(this);
  n += ScalarBase::SizeOfExcludingThis(aMallocSizeOf);
  n += mStorage.ShallowSizeOfExcludingThis(aMallocSizeOf);
  for (auto& val : mStorage) {
    n += val.SizeOfExcludingThisIfUnshared(aMallocSizeOf);
  }
  return n;
}

/**
 * The implementation for the boolean scalar type.
 */
class ScalarBoolean : public ScalarBase {
 public:
  using ScalarBase::SetValue;

  explicit ScalarBoolean(const BaseScalarInfo& aInfo)
      : ScalarBase(aInfo), mStorage(aInfo.storeCount()) {
    mStorage.SetLength(aInfo.storeCount());
    for (auto& val : mStorage) {
      val = false;
    }
  };

  ~ScalarBoolean() override = default;

  ScalarResult SetValue(nsIVariant* aValue) final;
  void SetValue(bool aValue) final;
  nsresult GetValue(const nsACString& aStoreName, bool aClearStore,
                    nsCOMPtr<nsIVariant>& aResult) final;
  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const final;

 private:
  nsTArray<bool> mStorage;

  // Prevent copying.
  ScalarBoolean(const ScalarBoolean& aOther) = delete;
  void operator=(const ScalarBoolean& aOther) = delete;
};

ScalarResult ScalarBoolean::SetValue(nsIVariant* aValue) {
  // Check that we got the correct data type.
  uint16_t type = aValue->GetDataType();
  if (type != nsIDataType::VTYPE_BOOL && type != nsIDataType::VTYPE_INT8 &&
      type != nsIDataType::VTYPE_INT16 && type != nsIDataType::VTYPE_INT32 &&
      type != nsIDataType::VTYPE_INT64 && type != nsIDataType::VTYPE_UINT8 &&
      type != nsIDataType::VTYPE_UINT16 && type != nsIDataType::VTYPE_UINT32 &&
      type != nsIDataType::VTYPE_UINT64) {
    return ScalarResult::InvalidType;
  }

  bool value = false;
  if (NS_FAILED(aValue->GetAsBool(&value))) {
    return ScalarResult::InvalidValue;
  }
  SetValue(value);
  return ScalarResult::Ok;
};

void ScalarBoolean::SetValue(bool aValue) {
  if (GetCurrentProduct() == SupportedProduct::GeckoviewStreaming) {
    GeckoViewStreamingTelemetry::BoolScalarSet(mName, aValue);
    return;
  }
  for (auto& val : mStorage) {
    val = aValue;
  }
  SetValueInStores();
}

nsresult ScalarBoolean::GetValue(const nsACString& aStoreName, bool aClearStore,
                                 nsCOMPtr<nsIVariant>& aResult) {
  nsCOMPtr<nsIWritableVariant> outVar(new nsVariant());
  size_t storeIndex = 0;
  nsresult rv = StoreIndex(aStoreName, &storeIndex);
  if (NS_FAILED(rv)) {
    return rv;
  }
  if (!HasValueInStore(storeIndex)) {
    return NS_ERROR_NO_CONTENT;
  }
  if (aClearStore) {
    ClearValueInStore(storeIndex);
  }
  rv = outVar->SetAsBool(mStorage[storeIndex]);
  if (NS_FAILED(rv)) {
    return rv;
  }
  aResult = std::move(outVar);
  return NS_OK;
}

size_t ScalarBoolean::SizeOfIncludingThis(
    mozilla::MallocSizeOf aMallocSizeOf) const {
  size_t n = aMallocSizeOf(this);
  n += ScalarBase::SizeOfExcludingThis(aMallocSizeOf);
  n += mStorage.ShallowSizeOfExcludingThis(aMallocSizeOf);
  return n;
}

/**
 * Allocate a scalar class given the scalar info.
 *
 * @param aInfo The informations for the scalar coming from the definition file.
 * @return nullptr if the scalar type is unknown, otherwise a valid pointer to
 * the scalar type.
 */
ScalarBase* internal_ScalarAllocate(const BaseScalarInfo& aInfo) {
  ScalarBase* scalar = nullptr;
  switch (aInfo.kind) {
    case nsITelemetry::SCALAR_TYPE_COUNT:
      scalar = new ScalarUnsigned(aInfo);
      break;
    case nsITelemetry::SCALAR_TYPE_STRING:
      scalar = new ScalarString(aInfo);
      break;
    case nsITelemetry::SCALAR_TYPE_BOOLEAN:
      scalar = new ScalarBoolean(aInfo);
      break;
    default:
      MOZ_ASSERT(false, "Invalid scalar type");
  }
  return scalar;
}

/**
 * The implementation for the keyed scalar type.
 */
class KeyedScalar {
 public:
  typedef std::pair<nsCString, nsCOMPtr<nsIVariant>> KeyValuePair;

  // We store the name instead of a reference to the BaseScalarInfo because
  // the BaseScalarInfo can move if it's from a dynamic scalar.
  explicit KeyedScalar(const BaseScalarInfo& info)
      : mScalarName(info.name()),
        mScalarKeyCount(info.key_count),
        mScalarKeyOffset(info.key_offset),
        mMaximumNumberOfKeys(kMaximumNumberOfKeys){};
  ~KeyedScalar() = default;

  // Set, Add and SetMaximum functions as described in the Telemetry IDL.
  // These methods implicitly instantiate a Scalar[*] for each key.
  ScalarResult SetValue(const StaticMutexAutoLock& locker,
                        const nsAString& aKey, nsIVariant* aValue);
  ScalarResult AddValue(const StaticMutexAutoLock& locker,
                        const nsAString& aKey, nsIVariant* aValue);
  ScalarResult SetMaximum(const StaticMutexAutoLock& locker,
                          const nsAString& aKey, nsIVariant* aValue);

  // Convenience methods used by the C++ API.
  void SetValue(const StaticMutexAutoLock& locker, const nsAString& aKey,
                uint32_t aValue);
  void SetValue(const StaticMutexAutoLock& locker, const nsAString& aKey,
                bool aValue);
  void AddValue(const StaticMutexAutoLock& locker, const nsAString& aKey,
                uint32_t aValue);
  void SetMaximum(const StaticMutexAutoLock& locker, const nsAString& aKey,
                  uint32_t aValue);

  // GetValue is used to get the key-value pairs stored in the keyed scalar
  // when persisting it to JS.
  nsresult GetValue(const nsACString& aStoreName, bool aClearStorage,
                    nsTArray<KeyValuePair>& aValues);

  // To measure the memory stats.
  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf);

  // To permit more keys than normal.
  void SetMaximumNumberOfKeys(uint32_t aMaximumNumberOfKeys) {
    mMaximumNumberOfKeys = aMaximumNumberOfKeys;
  };

 private:
  typedef nsClassHashtable<nsCStringHashKey, ScalarBase> ScalarKeysMapType;

  const nsCString mScalarName;
  ScalarKeysMapType mScalarKeys;
  uint32_t mScalarKeyCount;
  uint32_t mScalarKeyOffset;
  uint32_t mMaximumNumberOfKeys;

  ScalarResult GetScalarForKey(const StaticMutexAutoLock& locker,
                               const nsAString& aKey, ScalarBase** aRet);

  bool AllowsKey(const nsAString& aKey) const;
};

ScalarResult KeyedScalar::SetValue(const StaticMutexAutoLock& locker,
                                   const nsAString& aKey, nsIVariant* aValue) {
  ScalarBase* scalar = nullptr;
  ScalarResult sr = GetScalarForKey(locker, aKey, &scalar);
  if (sr != ScalarResult::Ok) {
    return sr;
  }

  return scalar->SetValue(aValue);
}

ScalarResult KeyedScalar::AddValue(const StaticMutexAutoLock& locker,
                                   const nsAString& aKey, nsIVariant* aValue) {
  ScalarBase* scalar = nullptr;
  ScalarResult sr = GetScalarForKey(locker, aKey, &scalar);
  if (sr != ScalarResult::Ok) {
    return sr;
  }

  return scalar->AddValue(aValue);
}

ScalarResult KeyedScalar::SetMaximum(const StaticMutexAutoLock& locker,
                                     const nsAString& aKey,
                                     nsIVariant* aValue) {
  ScalarBase* scalar = nullptr;
  ScalarResult sr = GetScalarForKey(locker, aKey, &scalar);
  if (sr != ScalarResult::Ok) {
    return sr;
  }

  return scalar->SetMaximum(aValue);
}

void KeyedScalar::SetValue(const StaticMutexAutoLock& locker,
                           const nsAString& aKey, uint32_t aValue) {
  ScalarBase* scalar = nullptr;
  ScalarResult sr = GetScalarForKey(locker, aKey, &scalar);

  if (sr != ScalarResult::Ok) {
    // Bug 1451813 - We now report which scalars exceed the key limit in
    // telemetry.keyed_scalars_exceed_limit.
    if (sr == ScalarResult::KeyTooLong) {
      MOZ_ASSERT(false, "Key too long to be recorded in the scalar.");
    }
    return;
  }

  return scalar->SetValue(aValue);
}

void KeyedScalar::SetValue(const StaticMutexAutoLock& locker,
                           const nsAString& aKey, bool aValue) {
  ScalarBase* scalar = nullptr;
  ScalarResult sr = GetScalarForKey(locker, aKey, &scalar);

  if (sr != ScalarResult::Ok) {
    // Bug 1451813 - We now report which scalars exceed the key limit in
    // telemetry.keyed_scalars_exceed_limit.
    if (sr == ScalarResult::KeyTooLong) {
      MOZ_ASSERT(false, "Key too long to be recorded in the scalar.");
    }
    return;
  }

  return scalar->SetValue(aValue);
}

void KeyedScalar::AddValue(const StaticMutexAutoLock& locker,
                           const nsAString& aKey, uint32_t aValue) {
  ScalarBase* scalar = nullptr;
  ScalarResult sr = GetScalarForKey(locker, aKey, &scalar);

  if (sr != ScalarResult::Ok) {
    // Bug 1451813 - We now report which scalars exceed the key limit in
    // telemetry.keyed_scalars_exceed_limit.
    if (sr == ScalarResult::KeyTooLong) {
      MOZ_ASSERT(false, "Key too long to be recorded in the scalar.");
    }
    return;
  }

  return scalar->AddValue(aValue);
}

void KeyedScalar::SetMaximum(const StaticMutexAutoLock& locker,
                             const nsAString& aKey, uint32_t aValue) {
  ScalarBase* scalar = nullptr;
  ScalarResult sr = GetScalarForKey(locker, aKey, &scalar);

  if (sr != ScalarResult::Ok) {
    // Bug 1451813 - We now report which scalars exceed the key limit in
    // telemetry.keyed_scalars_exceed_limit.
    if (sr == ScalarResult::KeyTooLong) {
      MOZ_ASSERT(false, "Key too long to be recorded in the scalar.");
    }

    return;
  }

  return scalar->SetMaximum(aValue);
}

/**
 * Get a key-value array with the values for the Keyed Scalar.
 * @param aValue The array that will hold the key-value pairs.
 * @return {nsresult} NS_OK or an error value as reported by the
 *         the specific scalar objects implementations (e.g.
 *         ScalarUnsigned).
 */
nsresult KeyedScalar::GetValue(const nsACString& aStoreName, bool aClearStorage,
                               nsTArray<KeyValuePair>& aValues) {
  for (const auto& entry : mScalarKeys) {
    ScalarBase* scalar = entry.GetWeak();

    // Get the scalar value.
    nsCOMPtr<nsIVariant> scalarValue;
    nsresult rv = scalar->GetValue(aStoreName, aClearStorage, scalarValue);
    if (rv == NS_ERROR_NO_CONTENT) {
      // No value for this store.
      continue;
    }
    if (NS_FAILED(rv)) {
      return rv;
    }

    // Append it to value list.
    aValues.AppendElement(
        std::make_pair(nsCString(entry.GetKey()), scalarValue));
  }

  return NS_OK;
}

// Forward declaration
nsresult internal_GetKeyedScalarByEnum(const StaticMutexAutoLock& lock,
                                       const ScalarKey& aId,
                                       ProcessID aProcessStorage,
                                       KeyedScalar** aRet);

// Forward declaration
nsresult internal_GetEnumByScalarName(const StaticMutexAutoLock& lock,
                                      const nsACString& aName, ScalarKey* aId);

/**
 * Get the scalar for the referenced key.
 * If there's no such key, instantiate a new Scalar object with the
 * same type of the Keyed scalar and create the key.
 */
ScalarResult KeyedScalar::GetScalarForKey(const StaticMutexAutoLock& locker,
                                          const nsAString& aKey,
                                          ScalarBase** aRet) {
  if (aKey.IsEmpty()) {
    return ScalarResult::KeyIsEmpty;
  }

  if (!AllowsKey(aKey)) {
    KeyedScalar* scalarUnknown = nullptr;
    ScalarKey scalarUnknownUniqueId{
        static_cast<uint32_t>(
            mozilla::Telemetry::ScalarID::TELEMETRY_KEYED_SCALARS_UNKNOWN_KEYS),
        false};
    ProcessID process = ProcessID::Parent;
    nsresult rv = internal_GetKeyedScalarByEnum(locker, scalarUnknownUniqueId,
                                                process, &scalarUnknown);
    if (NS_FAILED(rv)) {
      return ScalarResult::TooManyKeys;
    }
    scalarUnknown->AddValue(locker, NS_ConvertUTF8toUTF16(mScalarName), 1);

    return ScalarResult::KeyNotAllowed;
  }

  if (aKey.Length() > kMaximumKeyStringLength) {
    return ScalarResult::KeyTooLong;
  }

  NS_ConvertUTF16toUTF8 utf8Key(aKey);

  ScalarBase* scalar = nullptr;
  if (mScalarKeys.Get(utf8Key, &scalar)) {
    *aRet = scalar;
    return ScalarResult::Ok;
  }

  ScalarKey uniqueId;
  nsresult rv = internal_GetEnumByScalarName(locker, mScalarName, &uniqueId);
  if (NS_FAILED(rv)) {
    return (rv == NS_ERROR_FAILURE) ? ScalarResult::NotInitialized
                                    : ScalarResult::UnknownScalar;
  }

  const BaseScalarInfo& info = internal_GetScalarInfo(locker, uniqueId);
  if (mScalarKeys.Count() >= mMaximumNumberOfKeys) {
    if (aKey.EqualsLiteral("telemetry.keyed_scalars_exceed_limit")) {
      return ScalarResult::TooManyKeys;
    }

    KeyedScalar* scalarExceed = nullptr;

    ScalarKey uniqueId{
        static_cast<uint32_t>(
            mozilla::Telemetry::ScalarID::TELEMETRY_KEYED_SCALARS_EXCEED_LIMIT),
        false};

    ProcessID process = ProcessID::Parent;
    nsresult rv =
        internal_GetKeyedScalarByEnum(locker, uniqueId, process, &scalarExceed);

    if (NS_FAILED(rv)) {
      return ScalarResult::TooManyKeys;
    }

    scalarExceed->AddValue(locker, NS_ConvertUTF8toUTF16(info.name()), 1);

    return ScalarResult::TooManyKeys;
  }

  scalar = internal_ScalarAllocate(info);
  if (!scalar) {
    return ScalarResult::InvalidType;
  }

  mScalarKeys.InsertOrUpdate(utf8Key, UniquePtr<ScalarBase>(scalar));

  *aRet = scalar;
  return ScalarResult::Ok;
}

size_t KeyedScalar::SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) {
  size_t n = aMallocSizeOf(this);
  for (const auto& scalar : mScalarKeys.Values()) {
    n += scalar->SizeOfIncludingThis(aMallocSizeOf);
  }
  return n;
}

bool KeyedScalar::AllowsKey(const nsAString& aKey) const {
  // If we didn't specify a list of allowed keys, just return true.
  if (mScalarKeyCount == 0) {
    return true;
  }

  for (uint32_t i = 0; i < mScalarKeyCount; ++i) {
    uint32_t stringIndex = gScalarKeysTable[mScalarKeyOffset + i];
    if (aKey.EqualsASCII(&gScalarsStringTable[stringIndex])) {
      return true;
    }
  }

  return false;
}

typedef nsUint32HashKey ScalarIDHashKey;
typedef nsUint32HashKey ProcessIDHashKey;
typedef nsClassHashtable<ScalarIDHashKey, ScalarBase> ScalarStorageMapType;
typedef nsClassHashtable<ScalarIDHashKey, KeyedScalar>
    KeyedScalarStorageMapType;
typedef nsClassHashtable<ProcessIDHashKey, ScalarStorageMapType>
    ProcessesScalarsMapType;
typedef nsClassHashtable<ProcessIDHashKey, KeyedScalarStorageMapType>
    ProcessesKeyedScalarsMapType;

typedef mozilla::Tuple<const char*, nsCOMPtr<nsIVariant>, uint32_t>
    ScalarDataTuple;
typedef nsTArray<ScalarDataTuple> ScalarTupleArray;
typedef nsTHashMap<ProcessIDHashKey, ScalarTupleArray> ScalarSnapshotTable;

typedef mozilla::Tuple<const char*, nsTArray<KeyedScalar::KeyValuePair>,
                       uint32_t>
    KeyedScalarDataTuple;
typedef nsTArray<KeyedScalarDataTuple> KeyedScalarTupleArray;
typedef nsTHashMap<ProcessIDHashKey, KeyedScalarTupleArray>
    KeyedScalarSnapshotTable;

}  // namespace

////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
//
// PRIVATE STATE, SHARED BY ALL THREADS

namespace {

// Set to true once this global state has been initialized.
bool gInitDone = false;

bool gCanRecordBase;
bool gCanRecordExtended;

// The Name -> ID cache map.
ScalarMapType gScalarNameIDMap(kScalarCount);

// The (Process Id -> (Scalar ID -> Scalar Object)) map. This is a
// nsClassHashtable, it owns the scalar instances and takes care of deallocating
// them when they are removed from the map.
ProcessesScalarsMapType gScalarStorageMap;
// As above, for the keyed scalars.
ProcessesKeyedScalarsMapType gKeyedScalarStorageMap;
// Provide separate storage for "dynamic builtin" plain and keyed scalars,
// needed to support "build faster" in local developer builds.
ProcessesScalarsMapType gDynamicBuiltinScalarStorageMap;
ProcessesKeyedScalarsMapType gDynamicBuiltinKeyedScalarStorageMap;

// Whether or not the deserialization of persisted scalars is still in progress.
// This is never the case on Desktop or Fennec.
// Only GeckoView restores persisted scalars.
bool gIsDeserializing = false;
// This batches scalar accumulations that should be applied once loading
// finished.
StaticAutoPtr<nsTArray<ScalarAction>> gScalarsActions;
StaticAutoPtr<nsTArray<KeyedScalarAction>> gKeyedScalarsActions;

bool internal_IsScalarDeserializing(const StaticMutexAutoLock& lock) {
  return gIsDeserializing;
}

}  // namespace

////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
//
// PRIVATE: Function that may call JS code.

// NOTE: the functions in this section all run without protection from
// |gTelemetryScalarsMutex|. If they held the mutex, there would be the
// possibility of deadlock because the JS_ calls that they make may call
// back into the TelemetryScalar interface, hence trying to re-acquire the
// mutex.
//
// This means that these functions potentially race against threads, but
// that seems preferable to risking deadlock.

namespace {

/**
 * Converts the error code to a human readable error message and prints it to
 * the browser console.
 *
 * @param aScalarName The name of the scalar that raised the error.
 * @param aSr The error code.
 */
void internal_LogScalarError(const nsACString& aScalarName, ScalarResult aSr) {
  nsAutoString errorMessage;
  AppendUTF8toUTF16(aScalarName, errorMessage);

  switch (aSr) {
    case ScalarResult::NotInitialized:
      errorMessage.AppendLiteral(u" - Telemetry was not yet initialized.");
      break;
    case ScalarResult::CannotUnpackVariant:
      errorMessage.AppendLiteral(
          u" - Cannot convert the provided JS value to nsIVariant.");
      break;
    case ScalarResult::CannotRecordInProcess:
      errorMessage.AppendLiteral(
          u" - Cannot record the scalar in the current process.");
      break;
    case ScalarResult::KeyedTypeMismatch:
      errorMessage.AppendLiteral(
          u" - Attempting to manage a keyed scalar as a scalar (or "
          u"vice-versa).");
      break;
    case ScalarResult::UnknownScalar:
      errorMessage.AppendLiteral(u" - Unknown scalar.");
      break;
    case ScalarResult::OperationNotSupported:
      errorMessage.AppendLiteral(
          u" - The requested operation is not supported on this scalar.");
      break;
    case ScalarResult::InvalidType:
      errorMessage.AppendLiteral(
          u" - Attempted to set the scalar to an invalid data type.");
      break;
    case ScalarResult::InvalidValue:
      errorMessage.AppendLiteral(
          u" - Attempted to set the scalar to an incompatible value.");
      break;
    case ScalarResult::StringTooLong:
      AppendUTF8toUTF16(
          nsPrintfCString(" - Truncating scalar value to %d characters.",
                          kMaximumStringValueLength),
          errorMessage);
      break;
    case ScalarResult::KeyIsEmpty:
      errorMessage.AppendLiteral(u" - The key must not be empty.");
      break;
    case ScalarResult::KeyTooLong:
      AppendUTF8toUTF16(
          nsPrintfCString(" - The key length must be limited to %d characters.",
                          kMaximumKeyStringLength),
          errorMessage);
      break;
    case ScalarResult::KeyNotAllowed:
      AppendUTF8toUTF16(
          nsPrintfCString(" - The key is not allowed for this scalar."),
          errorMessage);
      break;
    case ScalarResult::TooManyKeys:
      AppendUTF8toUTF16(
          nsPrintfCString(" - Keyed scalars cannot have more than %d keys.",
                          kMaximumNumberOfKeys),
          errorMessage);
      break;
    case ScalarResult::UnsignedNegativeValue:
      errorMessage.AppendLiteral(
          u" - Trying to set an unsigned scalar to a negative number.");
      break;
    case ScalarResult::UnsignedTruncatedValue:
      errorMessage.AppendLiteral(u" - Truncating float/double number.");
      break;
    default:
      // Nothing.
      return;
  }

  LogToBrowserConsole(nsIScriptError::warningFlag, errorMessage);
}

}  // namespace

////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
//
// PRIVATE: helpers for the external interface

namespace {

bool internal_CanRecordBase(const StaticMutexAutoLock& lock) {
  return gCanRecordBase;
}

bool internal_CanRecordExtended(const StaticMutexAutoLock& lock) {
  return gCanRecordExtended;
}

/**
 * Check if the given scalar is a keyed scalar.
 *
 * @param lock Instance of a lock locking gTelemetryHistogramMutex
 * @param aId The scalar identifier.
 * @return true if aId refers to a keyed scalar, false otherwise.
 */
bool internal_IsKeyedScalar(const StaticMutexAutoLock& lock,
                            const ScalarKey& aId) {
  return internal_GetScalarInfo(lock, aId).keyed;
}

/**
 * Check if we're allowed to record the given scalar in the current
 * process.
 *
 * @param lock Instance of a lock locking gTelemetryHistogramMutex
 * @param aId The scalar identifier.
 * @return true if the scalar is allowed to be recorded in the current process,
 * false otherwise.
 */
bool internal_CanRecordProcess(const StaticMutexAutoLock& lock,
                               const ScalarKey& aId) {
  const BaseScalarInfo& info = internal_GetScalarInfo(lock, aId);
  return CanRecordInProcess(info.record_in_processes, XRE_GetProcessType());
}

bool internal_CanRecordProduct(const StaticMutexAutoLock& lock,
                               const ScalarKey& aId) {
  const BaseScalarInfo& info = internal_GetScalarInfo(lock, aId);
  return CanRecordProduct(info.products);
}

bool internal_CanRecordForScalarID(const StaticMutexAutoLock& lock,
                                   const ScalarKey& aId) {
  // Get the scalar info from the id.
  const BaseScalarInfo& info = internal_GetScalarInfo(lock, aId);

  // Can we record at all?
  bool canRecordBase = internal_CanRecordBase(lock);
  if (!canRecordBase) {
    return false;
  }

  bool canRecordDataset = CanRecordDataset(info.dataset, canRecordBase,
                                           internal_CanRecordExtended(lock));
  if (!canRecordDataset) {
    return false;
  }

  return true;
}

/**
 * Check if we are allowed to record the provided scalar.
 *
 * @param lock Instance of a lock locking gTelemetryHistogramMutex
 * @param aId The scalar identifier.
 * @param aKeyed Are we attempting to write a keyed scalar?
 * @param aForce Whether to allow recording even if the probe is not allowed on
 *        the current process.
 *        This must only be true for GeckoView persistence and recorded
 *        actions.
 * @return ScalarResult::Ok if we can record, an error code otherwise.
 */
ScalarResult internal_CanRecordScalar(const StaticMutexAutoLock& lock,
                                      const ScalarKey& aId, bool aKeyed,
                                      bool aForce = false) {
  // Make sure that we have a keyed scalar if we are trying to change one.
  if (internal_IsKeyedScalar(lock, aId) != aKeyed) {
    return ScalarResult::KeyedTypeMismatch;
  }

  // Are we allowed to record this scalar based on the current Telemetry
  // settings?
  if (!internal_CanRecordForScalarID(lock, aId)) {
    return ScalarResult::CannotRecordDataset;
  }

  // Can we record in this process?
  if (!aForce && !internal_CanRecordProcess(lock, aId)) {
    return ScalarResult::CannotRecordInProcess;
  }

  // Can we record on this product?
  if (!internal_CanRecordProduct(lock, aId)) {
    return ScalarResult::CannotRecordDataset;
  }

  return ScalarResult::Ok;
}

/**
 * Get the scalar enum id from the scalar name.
 *
 * @param lock Instance of a lock locking gTelemetryHistogramMutex
 * @param aName The scalar name.
 * @param aId The output variable to contain the enum.
 * @return
 *   NS_ERROR_FAILURE if this was called before init is completed.
 *   NS_ERROR_INVALID_ARG if the name can't be found in the scalar definitions.
 *   NS_OK if the scalar was found and aId contains a valid enum id.
 */
nsresult internal_GetEnumByScalarName(const StaticMutexAutoLock& lock,
                                      const nsACString& aName, ScalarKey* aId) {
  if (!gInitDone) {
    return NS_ERROR_FAILURE;
  }

  CharPtrEntryType* entry =
      gScalarNameIDMap.GetEntry(PromiseFlatCString(aName).get());
  if (!entry) {
    return NS_ERROR_INVALID_ARG;
  }
  *aId = entry->GetData();
  return NS_OK;
}

/**
 * Get a scalar object by its enum id. This implicitly allocates the scalar
 * object in the storage if it wasn't previously allocated.
 *
 * @param lock Instance of a lock locking gTelemetryHistogramMutex
 * @param aId The scalar identifier.
 * @param aProcessStorage This drives the selection of the map to use to store
 *        the scalar data coming from child processes. This is only meaningful
 *        when this function is called in parent process. If that's the case,
 *        if this is not |GeckoProcessType_Default|, the process id is used to
 *        allocate and store the scalars.
 * @param aRes The output variable that stores scalar object.
 * @return
 *   NS_ERROR_INVALID_ARG if the scalar id is unknown.
 *   NS_ERROR_NOT_AVAILABLE if the scalar is expired.
 *   NS_OK if the scalar was found. If that's the case, aResult contains a
 *   valid pointer to a scalar type.
 */
nsresult internal_GetScalarByEnum(const StaticMutexAutoLock& lock,
                                  const ScalarKey& aId,
                                  ProcessID aProcessStorage,
                                  ScalarBase** aRet) {
  if (!internal_IsValidId(lock, aId)) {
    MOZ_ASSERT(false, "Requested a scalar with an invalid id.");
    return NS_ERROR_INVALID_ARG;
  }

  const BaseScalarInfo& info = internal_GetScalarInfo(lock, aId);

  // Dynamic scalars fixup: they are always stored in the "dynamic" process,
  // unless they are part of the "builtin" Firefox probes. Please note that
  // "dynamic builtin" probes are meant to support "artifact" and "build faster"
  // builds.
  if (aId.dynamic && !info.builtin) {
    aProcessStorage = ProcessID::Dynamic;
  }

  ScalarBase* scalar = nullptr;
  // Initialize the scalar storage to the parent storage. This will get
  // set to the child storage if needed.
  uint32_t storageId = static_cast<uint32_t>(aProcessStorage);

  // Put dynamic-builtin scalars (used to support "build faster") in a
  // separate storage.
  ProcessesScalarsMapType& processStorage =
      (aId.dynamic && info.builtin) ? gDynamicBuiltinScalarStorageMap
                                    : gScalarStorageMap;

  // Get the process-specific storage or create one if it's not
  // available.
  ScalarStorageMapType* const scalarStorage =
      processStorage.GetOrInsertNew(storageId);

  // Check if the scalar is already allocated in the parent or in the child
  // storage.
  if (scalarStorage->Get(aId.id, &scalar)) {
    // Dynamic scalars can expire at any time during the session (e.g. an
    // add-on was updated). Check if it expired.
    if (aId.dynamic) {
      const DynamicScalarInfo& dynInfo =
          static_cast<const DynamicScalarInfo&>(info);
      if (dynInfo.mDynamicExpiration) {
        // The Dynamic scalar is expired.
        return NS_ERROR_NOT_AVAILABLE;
      }
    }
    // This was not a dynamic scalar or was not expired.
    *aRet = scalar;
    return NS_OK;
  }

  // The scalar storage wasn't already allocated. Check if the scalar is expired
  // and then allocate the storage, if needed.
  if (IsExpiredVersion(info.expiration())) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  scalar = internal_ScalarAllocate(info);
  if (!scalar) {
    return NS_ERROR_INVALID_ARG;
  }

  scalarStorage->InsertOrUpdate(aId.id, UniquePtr<ScalarBase>(scalar));
  *aRet = scalar;
  return NS_OK;
}

void internal_ApplyPendingOperations(const StaticMutexAutoLock& lock);

/**
 * Record the given action on a scalar into the pending actions list.
 *
 * If the pending actions list overflows the high water mark length
 * all operations are immediately applied, including the passed action.
 *
 * @param aScalarAction The action to record.
 */
void internal_RecordScalarAction(const StaticMutexAutoLock& lock,
                                 const ScalarAction& aScalarAction) {
  // Make sure to have the storage.
  if (!gScalarsActions) {
    gScalarsActions = new nsTArray<ScalarAction>();
  }

  // Store the action.
  gScalarsActions->AppendElement(aScalarAction);

  // If this action overflows the pending actions array, we immediately apply
  // pending operations and assume loading is over. If loading still happens
  // afterwards, some scalar values might be overwritten and inconsistent, but
  // we won't lose operations on otherwise untouched probes.
  if (gScalarsActions->Length() > kScalarActionsArrayHighWaterMark) {
    internal_ApplyPendingOperations(lock);
    return;
  }
}

/**
 * Record the given action on a scalar on the main process into the pending
 * actions list.
 *
 * If the pending actions list overflows the high water mark length
 * all operations are immediately applied, including the passed action.
 *
 * @param aId The scalar's ID this action applies to
 * @param aDynamic Determines if the scalar is dynamic
 * @param aAction The action to record
 * @param aValue The additional data for the recorded action
 */
void internal_RecordScalarAction(const StaticMutexAutoLock& lock, uint32_t aId,
                                 bool aDynamic, ScalarActionType aAction,
                                 const ScalarVariant& aValue) {
  internal_RecordScalarAction(
      lock,
      ScalarAction{aId, aDynamic, aAction, Some(aValue), ProcessID::Parent});
}

/**
 * Record the given action on a keyed scalar into the pending actions list.
 *
 * If the pending actions list overflows the high water mark length
 * all operations are immediately applied, including the passed action.
 *
 * @param aScalarAction The action to record.
 */
void internal_RecordKeyedScalarAction(const StaticMutexAutoLock& lock,
                                      const KeyedScalarAction& aScalarAction) {
  // Make sure to have the storage.
  if (!gKeyedScalarsActions) {
    gKeyedScalarsActions = new nsTArray<KeyedScalarAction>();
  }

  // Store the action.
  gKeyedScalarsActions->AppendElement(aScalarAction);

  // If this action overflows the pending actions array, we immediately apply
  // pending operations and assume loading is over. If loading still happens
  // afterwards, some scalar values might be overwritten and inconsistent, but
  // we won't lose operations on otherwise untouched probes.
  if (gKeyedScalarsActions->Length() > kScalarActionsArrayHighWaterMark) {
    internal_ApplyPendingOperations(lock);
    return;
  }
}

/**
 * Record the given action on a keyed scalar on the main process into the
 * pending actions list.
 *
 * If the pending actions list overflows the high water mark length
 * all operations are immediately applied, including the passed action.
 *
 * @param aId The scalar's ID this action applies to
 * @param aDynamic Determines if the scalar is dynamic
 * @param aKey The scalar's key
 * @param aAction The action to record
 * @param aValue The additional data for the recorded action
 */
void internal_RecordKeyedScalarAction(const StaticMutexAutoLock& lock,
                                      uint32_t aId, bool aDynamic,
                                      const nsAString& aKey,
                                      ScalarActionType aAction,
                                      const ScalarVariant& aValue) {
  internal_RecordKeyedScalarAction(
      lock,
      KeyedScalarAction{aId, aDynamic, aAction, NS_ConvertUTF16toUTF8(aKey),
                        Some(aValue), ProcessID::Parent});
}

/**
 * Update the scalar with the provided value. This is used by the JS API.
 *
 * @param lock Instance of a lock locking gTelemetryHistogramMutex
 * @param aName The scalar name.
 * @param aType The action type for updating the scalar.
 * @param aValue The value to use for updating the scalar.
 * @param aProcessOverride The process for which the scalar must be updated.
 *        This must only be used for GeckoView persistence. It must be
 *        set to the ProcessID::Parent for all the other cases.
 * @param aForce Whether to force updating even if load is in progress.
 * @return a ScalarResult error value.
 */
ScalarResult internal_UpdateScalar(
    const StaticMutexAutoLock& lock, const nsACString& aName,
    ScalarActionType aType, nsIVariant* aValue,
    ProcessID aProcessOverride = ProcessID::Parent, bool aForce = false) {
  ScalarKey uniqueId;
  nsresult rv = internal_GetEnumByScalarName(lock, aName, &uniqueId);
  if (NS_FAILED(rv)) {
    return (rv == NS_ERROR_FAILURE) ? ScalarResult::NotInitialized
                                    : ScalarResult::UnknownScalar;
  }

  ScalarResult sr = internal_CanRecordScalar(lock, uniqueId, false, aForce);
  if (sr != ScalarResult::Ok) {
    if (sr == ScalarResult::CannotRecordDataset) {
      return ScalarResult::Ok;
    }
    return sr;
  }

  // Accumulate in the child process if needed.
  if (!XRE_IsParentProcess()) {
    const BaseScalarInfo& info = internal_GetScalarInfo(lock, uniqueId);
    // Convert the nsIVariant to a Variant.
    mozilla::Maybe<ScalarVariant> variantValue;
    sr = GetVariantFromIVariant(aValue, info.kind, variantValue);
    if (sr != ScalarResult::Ok) {
      MOZ_ASSERT(false, "Unable to convert nsIVariant to mozilla::Variant.");
      return sr;
    }
    TelemetryIPCAccumulator::RecordChildScalarAction(
        uniqueId.id, uniqueId.dynamic, aType, variantValue.ref());
    return ScalarResult::Ok;
  }

  if (!aForce && internal_IsScalarDeserializing(lock)) {
    const BaseScalarInfo& info = internal_GetScalarInfo(lock, uniqueId);
    // Convert the nsIVariant to a Variant.
    mozilla::Maybe<ScalarVariant> variantValue;
    sr = GetVariantFromIVariant(aValue, info.kind, variantValue);
    if (sr != ScalarResult::Ok) {
      MOZ_ASSERT(false, "Unable to convert nsIVariant to mozilla::Variant.");
      return sr;
    }
    internal_RecordScalarAction(lock, uniqueId.id, uniqueId.dynamic, aType,
                                variantValue.ref());
    return ScalarResult::Ok;
  }

  // Finally get the scalar.
  ScalarBase* scalar = nullptr;
  rv = internal_GetScalarByEnum(lock, uniqueId, aProcessOverride, &scalar);
  if (NS_FAILED(rv)) {
    // Don't throw on expired scalars.
    if (rv == NS_ERROR_NOT_AVAILABLE) {
      return ScalarResult::Ok;
    }
    return ScalarResult::UnknownScalar;
  }

  if (aType == ScalarActionType::eAdd) {
    return scalar->AddValue(aValue);
  }
  if (aType == ScalarActionType::eSet) {
    return scalar->SetValue(aValue);
  }

  return scalar->SetMaximum(aValue);
}

}  // namespace

////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
//
// PRIVATE: thread-unsafe helpers for the keyed scalars

namespace {

/**
 * Get a keyed scalar object by its enum id. This implicitly allocates the keyed
 * scalar object in the storage if it wasn't previously allocated.
 *
 * @param lock Instance of a lock locking gTelemetryHistogramMutex
 * @param aId The scalar identifier.
 * @param aProcessStorage This drives the selection of the map to use to store
 *        the scalar data coming from child processes. This is only meaningful
 *        when this function is called in parent process. If that's the case,
 *        if this is not |GeckoProcessType_Default|, the process id is used to
 *        allocate and store the scalars.
 * @param aRet The output variable that stores scalar object.
 * @return
 *   NS_ERROR_INVALID_ARG if the scalar id is unknown or a this is a keyed
 *                        string scalar.
 *   NS_ERROR_NOT_AVAILABLE if the scalar is expired.
 *   NS_OK if the scalar was found. If that's the case, aResult contains a
 *   valid pointer to a scalar type.
 */
nsresult internal_GetKeyedScalarByEnum(const StaticMutexAutoLock& lock,
                                       const ScalarKey& aId,
                                       ProcessID aProcessStorage,
                                       KeyedScalar** aRet) {
  if (!internal_IsValidId(lock, aId)) {
    MOZ_ASSERT(false, "Requested a keyed scalar with an invalid id.");
    return NS_ERROR_INVALID_ARG;
  }

  const BaseScalarInfo& info = internal_GetScalarInfo(lock, aId);

  // Dynamic scalars fixup: they are always stored in the "dynamic" process,
  // unless they are part of the "builtin" Firefox probes. Please note that
  // "dynamic builtin" probes are meant to support "artifact" and "build faster"
  // builds.
  if (aId.dynamic && !info.builtin) {
    aProcessStorage = ProcessID::Dynamic;
  }

  KeyedScalar* scalar = nullptr;
  // Initialize the scalar storage to the parent storage. This will get
  // set to the child storage if needed.
  uint32_t storageId = static_cast<uint32_t>(aProcessStorage);

  // Put dynamic-builtin scalars (used to support "build faster") in a
  // separate storage.
  ProcessesKeyedScalarsMapType& processStorage =
      (aId.dynamic && info.builtin) ? gDynamicBuiltinKeyedScalarStorageMap
                                    : gKeyedScalarStorageMap;

  // Get the process-specific storage or create one if it's not
  // available.
  KeyedScalarStorageMapType* const scalarStorage =
      processStorage.GetOrInsertNew(storageId);

  if (scalarStorage->Get(aId.id, &scalar)) {
    *aRet = scalar;
    return NS_OK;
  }

  if (IsExpiredVersion(info.expiration())) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // We don't currently support keyed string scalars. Disable them.
  if (info.kind == nsITelemetry::SCALAR_TYPE_STRING) {
    MOZ_ASSERT(false, "Keyed string scalars are not currently supported.");
    return NS_ERROR_INVALID_ARG;
  }

  scalar = new KeyedScalar(info);

  scalarStorage->InsertOrUpdate(aId.id, UniquePtr<KeyedScalar>(scalar));
  *aRet = scalar;
  return NS_OK;
}

/**
 * Update the keyed scalar with the provided value. This is used by the JS API.
 *
 * @param lock Instance of a lock locking gTelemetryHistogramMutex
 * @param aName The scalar name.
 * @param aKey The key name.
 * @param aType The action type for updating the scalar.
 * @param aValue The value to use for updating the scalar.
 * @param aProcessOverride The process for which the scalar must be updated.
 *        This must only be used for GeckoView persistence. It must be
 *        set to the ProcessID::Parent for all the other cases.
 * @return a ScalarResult error value.
 */
ScalarResult internal_UpdateKeyedScalar(
    const StaticMutexAutoLock& lock, const nsACString& aName,
    const nsAString& aKey, ScalarActionType aType, nsIVariant* aValue,
    ProcessID aProcessOverride = ProcessID::Parent, bool aForce = false) {
  ScalarKey uniqueId;
  nsresult rv = internal_GetEnumByScalarName(lock, aName, &uniqueId);
  if (NS_FAILED(rv)) {
    return (rv == NS_ERROR_FAILURE) ? ScalarResult::NotInitialized
                                    : ScalarResult::UnknownScalar;
  }

  ScalarResult sr = internal_CanRecordScalar(lock, uniqueId, true, aForce);
  if (sr != ScalarResult::Ok) {
    if (sr == ScalarResult::CannotRecordDataset) {
      return ScalarResult::Ok;
    }
    return sr;
  }

  // Accumulate in the child process if needed.
  if (!XRE_IsParentProcess()) {
    const BaseScalarInfo& info = internal_GetScalarInfo(lock, uniqueId);
    // Convert the nsIVariant to a Variant.
    mozilla::Maybe<ScalarVariant> variantValue;
    sr = GetVariantFromIVariant(aValue, info.kind, variantValue);
    if (sr != ScalarResult::Ok) {
      MOZ_ASSERT(false, "Unable to convert nsIVariant to mozilla::Variant.");
      return sr;
    }
    TelemetryIPCAccumulator::RecordChildKeyedScalarAction(
        uniqueId.id, uniqueId.dynamic, aKey, aType, variantValue.ref());
    return ScalarResult::Ok;
  }

  if (!aForce && internal_IsScalarDeserializing(lock)) {
    const BaseScalarInfo& info = internal_GetScalarInfo(lock, uniqueId);
    // Convert the nsIVariant to a Variant.
    mozilla::Maybe<ScalarVariant> variantValue;
    sr = GetVariantFromIVariant(aValue, info.kind, variantValue);
    if (sr != ScalarResult::Ok) {
      MOZ_ASSERT(false, "Unable to convert nsIVariant to mozilla::Variant.");
      return sr;
    }
    internal_RecordKeyedScalarAction(lock, uniqueId.id, uniqueId.dynamic, aKey,
                                     aType, variantValue.ref());
    return ScalarResult::Ok;
  }

  // Finally get the scalar.
  KeyedScalar* scalar = nullptr;
  rv = internal_GetKeyedScalarByEnum(lock, uniqueId, aProcessOverride, &scalar);
  if (NS_FAILED(rv)) {
    // Don't throw on expired scalars.
    if (rv == NS_ERROR_NOT_AVAILABLE) {
      return ScalarResult::Ok;
    }
    return ScalarResult::UnknownScalar;
  }

  if (aType == ScalarActionType::eAdd) {
    return scalar->AddValue(lock, aKey, aValue);
  }
  if (aType == ScalarActionType::eSet) {
    return scalar->SetValue(lock, aKey, aValue);
  }

  return scalar->SetMaximum(lock, aKey, aValue);
}

/**
 * Helper function to convert an array of |DynamicScalarInfo|
 * to |DynamicScalarDefinition| used by the IPC calls.
 */
void internal_DynamicScalarToIPC(
    const StaticMutexAutoLock& lock,
    const nsTArray<DynamicScalarInfo>& aDynamicScalarInfos,
    nsTArray<DynamicScalarDefinition>& aIPCDefs) {
  for (auto& info : aDynamicScalarInfos) {
    DynamicScalarDefinition stubDefinition;
    stubDefinition.type = info.kind;
    stubDefinition.dataset = info.dataset;
    stubDefinition.expired = info.mDynamicExpiration;
    stubDefinition.keyed = info.keyed;
    stubDefinition.name = info.mDynamicName;
    stubDefinition.builtin = info.builtin;
    aIPCDefs.AppendElement(stubDefinition);
  }
}

/**
 * Broadcasts the dynamic scalar definitions to all the other
 * content processes.
 */
void internal_BroadcastDefinitions(
    const nsTArray<DynamicScalarDefinition>& scalarDefs) {
  nsTArray<mozilla::dom::ContentParent*> parents;
  mozilla::dom::ContentParent::GetAll(parents);
  if (!parents.Length()) {
    return;
  }

  // Broadcast the definitions to the other content processes.
  for (auto parent : parents) {
    mozilla::Unused << parent->SendAddDynamicScalars(scalarDefs);
  }
}

void internal_RegisterScalars(const StaticMutexAutoLock& lock,
                              const nsTArray<DynamicScalarInfo>& scalarInfos) {
  // Register the new scalars.
  if (!gDynamicScalarInfo) {
    gDynamicScalarInfo = new nsTArray<DynamicScalarInfo>();
  }
  if (!gDynamicStoreNames) {
    gDynamicStoreNames = new nsTArray<RefPtr<nsAtom>>();
  }

  for (auto& scalarInfo : scalarInfos) {
    // Allow expiring scalars that were already registered.
    CharPtrEntryType* existingKey =
        gScalarNameIDMap.GetEntry(scalarInfo.name());
    if (existingKey) {
      // Change the scalar to expired if needed.
      if (scalarInfo.mDynamicExpiration && !scalarInfo.builtin) {
        DynamicScalarInfo& scalarData =
            (*gDynamicScalarInfo)[existingKey->GetData().id];
        scalarData.mDynamicExpiration = true;
      }
      continue;
    }

    gDynamicScalarInfo->AppendElement(scalarInfo);
    uint32_t scalarId = gDynamicScalarInfo->Length() - 1;
    CharPtrEntryType* entry = gScalarNameIDMap.PutEntry(scalarInfo.name());
    entry->SetData(ScalarKey{scalarId, true});
  }
}

/**
 * Creates a snapshot of the desired scalar storage.
 * @param {aLock} The proof of lock to access scalar data.
 * @param {aScalarsToReflect} The table that will contain the snapshot.
 * @param {aDataset} The dataset we're asking the snapshot for.
 * @param {aProcessStorage} The scalar storage to take a snapshot of.
 * @param {aIsBuiltinDynamic} Whether or not the storage is for dynamic builtin
 *                            scalars.
 * @return NS_OK or the error code describing the failure reason.
 */
nsresult internal_ScalarSnapshotter(const StaticMutexAutoLock& aLock,
                                    ScalarSnapshotTable& aScalarsToReflect,
                                    unsigned int aDataset,
                                    ProcessesScalarsMapType& aProcessStorage,
                                    bool aIsBuiltinDynamic, bool aClearScalars,
                                    const nsACString& aStoreName) {
  // Iterate the scalars in aProcessStorage. The storage may contain empty or
  // yet to be initialized scalars from all the supported processes.
  for (const auto& entry : aProcessStorage) {
    ScalarStorageMapType* scalarStorage = entry.GetWeak();
    ScalarTupleArray& processScalars =
        aScalarsToReflect.LookupOrInsert(entry.GetKey());

    // Are we in the "Dynamic" process?
    bool isDynamicProcess =
        ProcessID::Dynamic == static_cast<ProcessID>(entry.GetKey());

    // Iterate each available child storage.
    for (const auto& childEntry : *scalarStorage) {
      ScalarBase* scalar = childEntry.GetWeak();

      // Get the informations for this scalar.
      const BaseScalarInfo& info = internal_GetScalarInfo(
          aLock, ScalarKey{childEntry.GetKey(),
                           aIsBuiltinDynamic ? true : isDynamicProcess});

      // Serialize the scalar if it's in the desired dataset.
      if (IsInDataset(info.dataset, aDataset)) {
        // Get the scalar value.
        nsCOMPtr<nsIVariant> scalarValue;
        nsresult rv = scalar->GetValue(aStoreName, aClearScalars, scalarValue);
        if (rv == NS_ERROR_NO_CONTENT) {
          // No value for this store. Proceed.
          continue;
        }
        if (NS_FAILED(rv)) {
          return rv;
        }
        // Append it to our list.
        processScalars.AppendElement(
            mozilla::MakeTuple(info.name(), scalarValue, info.kind));
      }
    }
    if (processScalars.Length() == 0) {
      aScalarsToReflect.Remove(entry.GetKey());
    }
  }
  return NS_OK;
}

/**
 * Creates a snapshot of the desired keyed scalar storage.
 * @param {aLock} The proof of lock to access scalar data.
 * @param {aScalarsToReflect} The table that will contain the snapshot.
 * @param {aDataset} The dataset we're asking the snapshot for.
 * @param {aProcessStorage} The scalar storage to take a snapshot of.
 * @param {aIsBuiltinDynamic} Whether or not the storage is for dynamic builtin
 *                            scalars.
 * @return NS_OK or the error code describing the failure reason.
 */
nsresult internal_KeyedScalarSnapshotter(
    const StaticMutexAutoLock& aLock,
    KeyedScalarSnapshotTable& aScalarsToReflect, unsigned int aDataset,
    ProcessesKeyedScalarsMapType& aProcessStorage, bool aIsBuiltinDynamic,
    bool aClearScalars, const nsACString& aStoreName) {
  // Iterate the scalars in aProcessStorage. The storage may contain empty or
  // yet to be initialized scalars from all the supported processes.
  for (const auto& entry : aProcessStorage) {
    KeyedScalarStorageMapType* scalarStorage = entry.GetWeak();
    KeyedScalarTupleArray& processScalars =
        aScalarsToReflect.LookupOrInsert(entry.GetKey());

    // Are we in the "Dynamic" process?
    bool isDynamicProcess =
        ProcessID::Dynamic == static_cast<ProcessID>(entry.GetKey());

    for (const auto& childEntry : *scalarStorage) {
      KeyedScalar* scalar = childEntry.GetWeak();

      // Get the informations for this scalar.
      const BaseScalarInfo& info = internal_GetScalarInfo(
          aLock, ScalarKey{childEntry.GetKey(),
                           aIsBuiltinDynamic ? true : isDynamicProcess});

      // Serialize the scalar if it's in the desired dataset.
      if (IsInDataset(info.dataset, aDataset)) {
        // Get the keys for this scalar.
        nsTArray<KeyedScalar::KeyValuePair> scalarKeyedData;
        nsresult rv =
            scalar->GetValue(aStoreName, aClearScalars, scalarKeyedData);
        if (NS_FAILED(rv)) {
          return rv;
        }
        if (scalarKeyedData.Length() == 0) {
          // Don't bother with empty keyed scalars.
          continue;
        }
        // Append it to our list.
        processScalars.AppendElement(mozilla::MakeTuple(
            info.name(), std::move(scalarKeyedData), info.kind));
      }
    }
    if (processScalars.Length() == 0) {
      aScalarsToReflect.Remove(entry.GetKey());
    }
  }
  return NS_OK;
}

/**
 * Helper function to get a snapshot of the scalars.
 *
 * @param {aLock} The proof of lock to access scalar data.
 * @param {aScalarsToReflect} The table that will contain the snapshot.
 * @param {aDataset} The dataset we're asking the snapshot for.
 * @param {aClearScalars} Whether or not to clear the scalar storage.
 * @param {aStoreName} The name of the store to snapshot.
 * @return NS_OK or the error code describing the failure reason.
 */
nsresult internal_GetScalarSnapshot(const StaticMutexAutoLock& aLock,
                                    ScalarSnapshotTable& aScalarsToReflect,
                                    unsigned int aDataset, bool aClearScalars,
                                    const nsACString& aStoreName) {
  // Take a snapshot of the scalars.
  nsresult rv =
      internal_ScalarSnapshotter(aLock, aScalarsToReflect, aDataset,
                                 gScalarStorageMap, false, /*aIsBuiltinDynamic*/
                                 aClearScalars, aStoreName);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // And a snapshot of the dynamic builtin ones.
  rv = internal_ScalarSnapshotter(aLock, aScalarsToReflect, aDataset,
                                  gDynamicBuiltinScalarStorageMap,
                                  true, /*aIsBuiltinDynamic*/
                                  aClearScalars, aStoreName);
  if (NS_FAILED(rv)) {
    return rv;
  }

  return NS_OK;
}

/**
 * Helper function to get a snapshot of the keyed scalars.
 *
 * @param {aLock} The proof of lock to access scalar data.
 * @param {aScalarsToReflect} The table that will contain the snapshot.
 * @param {aDataset} The dataset we're asking the snapshot for.
 * @param {aClearScalars} Whether or not to clear the scalar storage.
 * @param {aStoreName} The name of the store to snapshot.
 * @return NS_OK or the error code describing the failure reason.
 */
nsresult internal_GetKeyedScalarSnapshot(
    const StaticMutexAutoLock& aLock,
    KeyedScalarSnapshotTable& aScalarsToReflect, unsigned int aDataset,
    bool aClearScalars, const nsACString& aStoreName) {
  // Take a snapshot of the scalars.
  nsresult rv = internal_KeyedScalarSnapshotter(
      aLock, aScalarsToReflect, aDataset, gKeyedScalarStorageMap,
      false, /*aIsBuiltinDynamic*/
      aClearScalars, aStoreName);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // And a snapshot of the dynamic builtin ones.
  rv = internal_KeyedScalarSnapshotter(aLock, aScalarsToReflect, aDataset,
                                       gDynamicBuiltinKeyedScalarStorageMap,
                                       true, /*aIsBuiltinDynamic*/
                                       aClearScalars, aStoreName);
  if (NS_FAILED(rv)) {
    return rv;
  }

  return NS_OK;
}

}  // namespace

// helpers for recording/applying scalar operations
namespace {

void internal_ApplyScalarActions(
    const StaticMutexAutoLock& lock,
    const nsTArray<mozilla::Telemetry::ScalarAction>& aScalarActions,
    const mozilla::Maybe<ProcessID>& aProcessType = Nothing()) {
  if (!internal_CanRecordBase(lock)) {
    return;
  }

  for (auto& upd : aScalarActions) {
    ScalarKey uniqueId{upd.mId, upd.mDynamic};
    if (NS_WARN_IF(!internal_IsValidId(lock, uniqueId))) {
      MOZ_ASSERT_UNREACHABLE("Scalar usage requires valid ids.");
      continue;
    }

    if (internal_IsKeyedScalar(lock, uniqueId)) {
      continue;
    }

    // Are we allowed to record this scalar? We don't need to check for
    // allowed processes here, that's taken care of when recording
    // in child processes.
    if (!internal_CanRecordForScalarID(lock, uniqueId)) {
      continue;
    }

    // Either we got passed a process type or it was explicitely set on the
    // recorded action. It should never happen that it is set to an invalid
    // value (such as ProcessID::Count)
    ProcessID processType = aProcessType.valueOr(upd.mProcessType);
    MOZ_ASSERT(processType != ProcessID::Count);

    // Refresh the data in the parent process with the data coming from the
    // child processes.
    ScalarBase* scalar = nullptr;
    nsresult rv =
        internal_GetScalarByEnum(lock, uniqueId, processType, &scalar);
    if (NS_FAILED(rv)) {
      // Bug 1513496 - We no longer log a warning if the scalar is expired.
      if (rv != NS_ERROR_NOT_AVAILABLE) {
        NS_WARNING("NS_FAILED internal_GetScalarByEnum for CHILD");
      }
      continue;
    }

    if (upd.mData.isNothing()) {
      MOZ_ASSERT(false, "There is no data in the ScalarActionType.");
      continue;
    }

    // Get the type of this scalar from the scalar ID. We already checked
    // for its validity a few lines above.
    const uint32_t scalarType = internal_GetScalarInfo(lock, uniqueId).kind;

    // Extract the data from the mozilla::Variant.
    switch (upd.mActionType) {
      case ScalarActionType::eSet: {
        switch (scalarType) {
          case nsITelemetry::SCALAR_TYPE_COUNT:
            if (!upd.mData->is<uint32_t>()) {
              NS_WARNING("Attempting to set a count scalar to a non-integer.");
              continue;
            }
            scalar->SetValue(upd.mData->as<uint32_t>());
            break;
          case nsITelemetry::SCALAR_TYPE_BOOLEAN:
            if (!upd.mData->is<bool>()) {
              NS_WARNING(
                  "Attempting to set a boolean scalar to a non-boolean.");
              continue;
            }
            scalar->SetValue(upd.mData->as<bool>());
            break;
          case nsITelemetry::SCALAR_TYPE_STRING:
            if (!upd.mData->is<nsString>()) {
              NS_WARNING("Attempting to set a string scalar to a non-string.");
              continue;
            }
            scalar->SetValue(upd.mData->as<nsString>());
            break;
        }
        break;
      }
      case ScalarActionType::eAdd: {
        if (scalarType != nsITelemetry::SCALAR_TYPE_COUNT) {
          NS_WARNING("Attempting to add on a non count scalar.");
          continue;
        }
        // We only support adding uint32_t.
        if (!upd.mData->is<uint32_t>()) {
          NS_WARNING("Attempting to add to a count scalar with a non-integer.");
          continue;
        }
        scalar->AddValue(upd.mData->as<uint32_t>());
        break;
      }
      case ScalarActionType::eSetMaximum: {
        if (scalarType != nsITelemetry::SCALAR_TYPE_COUNT) {
          NS_WARNING("Attempting to setMaximum on a non count scalar.");
          continue;
        }
        // We only support SetMaximum on uint32_t.
        if (!upd.mData->is<uint32_t>()) {
          NS_WARNING(
              "Attempting to setMaximum a count scalar to a non-integer.");
          continue;
        }
        scalar->SetMaximum(upd.mData->as<uint32_t>());
        break;
      }
      default:
        NS_WARNING("Unsupported action coming from scalar child updates.");
    }
  }
}

void internal_ApplyKeyedScalarActions(
    const StaticMutexAutoLock& lock,
    const nsTArray<mozilla::Telemetry::KeyedScalarAction>& aScalarActions,
    const mozilla::Maybe<ProcessID>& aProcessType = Nothing()) {
  if (!internal_CanRecordBase(lock)) {
    return;
  }

  for (auto& upd : aScalarActions) {
    ScalarKey uniqueId{upd.mId, upd.mDynamic};
    if (NS_WARN_IF(!internal_IsValidId(lock, uniqueId))) {
      MOZ_ASSERT_UNREACHABLE("Scalar usage requires valid ids.");
      continue;
    }

    if (!internal_IsKeyedScalar(lock, uniqueId)) {
      continue;
    }

    // Are we allowed to record this scalar? We don't need to check for
    // allowed processes here, that's taken care of when recording
    // in child processes.
    if (!internal_CanRecordForScalarID(lock, uniqueId)) {
      continue;
    }

    // Either we got passed a process type or it was explicitely set on the
    // recorded action. It should never happen that it is set to an invalid
    // value (such as ProcessID::Count)
    ProcessID processType = aProcessType.valueOr(upd.mProcessType);
    MOZ_ASSERT(processType != ProcessID::Count);

    // Refresh the data in the parent process with the data coming from the
    // child processes.
    KeyedScalar* scalar = nullptr;
    nsresult rv =
        internal_GetKeyedScalarByEnum(lock, uniqueId, processType, &scalar);
    if (NS_FAILED(rv)) {
      // Bug 1513496 - We no longer log a warning if the scalar is expired.
      if (rv != NS_ERROR_NOT_AVAILABLE) {
        NS_WARNING("NS_FAILED internal_GetKeyedScalarByEnum for CHILD");
      }
      continue;
    }

    if (upd.mData.isNothing()) {
      MOZ_ASSERT(false, "There is no data in the KeyedScalarAction.");
      continue;
    }

    // Get the type of this scalar from the scalar ID. We already checked
    // for its validity a few lines above.
    const uint32_t scalarType = internal_GetScalarInfo(lock, uniqueId).kind;

    // Extract the data from the mozilla::Variant.
    switch (upd.mActionType) {
      case ScalarActionType::eSet: {
        switch (scalarType) {
          case nsITelemetry::SCALAR_TYPE_COUNT:
            if (!upd.mData->is<uint32_t>()) {
              NS_WARNING("Attempting to set a count scalar to a non-integer.");
              continue;
            }
            scalar->SetValue(lock, NS_ConvertUTF8toUTF16(upd.mKey),
                             upd.mData->as<uint32_t>());
            break;
          case nsITelemetry::SCALAR_TYPE_BOOLEAN:
            if (!upd.mData->is<bool>()) {
              NS_WARNING(
                  "Attempting to set a boolean scalar to a non-boolean.");
              continue;
            }
            scalar->SetValue(lock, NS_ConvertUTF8toUTF16(upd.mKey),
                             upd.mData->as<bool>());
            break;
          default:
            NS_WARNING("Unsupported type coming from scalar child updates.");
        }
        break;
      }
      case ScalarActionType::eAdd: {
        if (scalarType != nsITelemetry::SCALAR_TYPE_COUNT) {
          NS_WARNING("Attempting to add on a non count scalar.");
          continue;
        }
        // We only support adding on uint32_t.
        if (!upd.mData->is<uint32_t>()) {
          NS_WARNING("Attempting to add to a count scalar with a non-integer.");
          continue;
        }
        scalar->AddValue(lock, NS_ConvertUTF8toUTF16(upd.mKey),
                         upd.mData->as<uint32_t>());
        break;
      }
      case ScalarActionType::eSetMaximum: {
        if (scalarType != nsITelemetry::SCALAR_TYPE_COUNT) {
          NS_WARNING("Attempting to setMaximum on a non count scalar.");
          continue;
        }
        // We only support SetMaximum on uint32_t.
        if (!upd.mData->is<uint32_t>()) {
          NS_WARNING(
              "Attempting to setMaximum a count scalar to a non-integer.");
          continue;
        }
        scalar->SetMaximum(lock, NS_ConvertUTF8toUTF16(upd.mKey),
                           upd.mData->as<uint32_t>());
        break;
      }
      default:
        NS_WARNING(
            "Unsupported action coming from keyed scalar child updates.");
    }
  }
}

void internal_ApplyPendingOperations(const StaticMutexAutoLock& lock) {
  if (gScalarsActions && gScalarsActions->Length() > 0) {
    internal_ApplyScalarActions(lock, *gScalarsActions);
    gScalarsActions->Clear();
  }

  if (gKeyedScalarsActions && gKeyedScalarsActions->Length() > 0) {
    internal_ApplyKeyedScalarActions(lock, *gKeyedScalarsActions);
    gKeyedScalarsActions->Clear();
  }

  // After all pending operations are applied deserialization is done
  gIsDeserializing = false;
}

}  // namespace

////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
//
// EXTERNALLY VISIBLE FUNCTIONS in namespace TelemetryScalars::

// This is a StaticMutex rather than a plain Mutex (1) so that
// it gets initialised in a thread-safe manner the first time
// it is used, and (2) because it is never de-initialised, and
// a normal Mutex would show up as a leak in BloatView.  StaticMutex
// also has the "OffTheBooks" property, so it won't show as a leak
// in BloatView.
// Another reason to use a StaticMutex instead of a plain Mutex is
// that, due to the nature of Telemetry, we cannot rely on having a
// mutex initialized in InitializeGlobalState. Unfortunately, we
// cannot make sure that no other function is called before this point.
static StaticMutex gTelemetryScalarsMutex MOZ_UNANNOTATED;

void TelemetryScalar::InitializeGlobalState(bool aCanRecordBase,
                                            bool aCanRecordExtended) {
  StaticMutexAutoLock locker(gTelemetryScalarsMutex);
  MOZ_ASSERT(!gInitDone,
             "TelemetryScalar::InitializeGlobalState "
             "may only be called once");

  gCanRecordBase = aCanRecordBase;
  gCanRecordExtended = aCanRecordExtended;

  // Populate the static scalar name->id cache. Note that the scalar names are
  // statically allocated and come from the automatically generated
  // TelemetryScalarData.h.
  uint32_t scalarCount =
      static_cast<uint32_t>(mozilla::Telemetry::ScalarID::ScalarCount);
  for (uint32_t i = 0; i < scalarCount; i++) {
    CharPtrEntryType* entry = gScalarNameIDMap.PutEntry(gScalars[i].name());
    entry->SetData(ScalarKey{i, false});
  }

  // To summarize dynamic events we need a dynamic scalar.
  const nsTArray<DynamicScalarInfo> initialDynamicScalars({
      DynamicScalarInfo{
          nsITelemetry::SCALAR_TYPE_COUNT,
          true /* recordOnRelease */,
          false /* expired */,
          nsAutoCString("telemetry.dynamic_event_counts"),
          true /* keyed */,
          false /* built-in */,
          {} /* stores */,
      },
  });
  internal_RegisterScalars(locker, initialDynamicScalars);

  gInitDone = true;
}

void TelemetryScalar::DeInitializeGlobalState() {
  StaticMutexAutoLock locker(gTelemetryScalarsMutex);
  gCanRecordBase = false;
  gCanRecordExtended = false;
  gScalarNameIDMap.Clear();
  gScalarStorageMap.Clear();
  gKeyedScalarStorageMap.Clear();
  gDynamicBuiltinScalarStorageMap.Clear();
  gDynamicBuiltinKeyedScalarStorageMap.Clear();
  gDynamicScalarInfo = nullptr;
  gDynamicStoreNames = nullptr;
  gInitDone = false;
}

void TelemetryScalar::DeserializationStarted() {
  StaticMutexAutoLock locker(gTelemetryScalarsMutex);
  gIsDeserializing = true;
}

void TelemetryScalar::ApplyPendingOperations() {
  StaticMutexAutoLock locker(gTelemetryScalarsMutex);
  internal_ApplyPendingOperations(locker);
}

void TelemetryScalar::SetCanRecordBase(bool b) {
  StaticMutexAutoLock locker(gTelemetryScalarsMutex);
  gCanRecordBase = b;
}

void TelemetryScalar::SetCanRecordExtended(bool b) {
  StaticMutexAutoLock locker(gTelemetryScalarsMutex);
  gCanRecordExtended = b;
}

/**
 * Adds the value to the given scalar.
 *
 * @param aName The scalar name.
 * @param aVal The numeric value to add to the scalar.
 * @param aCx The JS context.
 * @return NS_OK (always) so that the JS API call doesn't throw. In case of
 * errors, a warning level message is printed in the browser console.
 */
nsresult TelemetryScalar::Add(const nsACString& aName,
                              JS::Handle<JS::Value> aVal, JSContext* aCx) {
  // Unpack the aVal to nsIVariant. This uses the JS context.
  nsCOMPtr<nsIVariant> unpackedVal;
  nsresult rv = nsContentUtils::XPConnect()->JSToVariant(
      aCx, aVal, getter_AddRefs(unpackedVal));
  if (NS_FAILED(rv)) {
    internal_LogScalarError(aName, ScalarResult::CannotUnpackVariant);
    return NS_OK;
  }

  ScalarResult sr;
  {
    StaticMutexAutoLock locker(gTelemetryScalarsMutex);
    sr = internal_UpdateScalar(locker, aName, ScalarActionType::eAdd,
                               unpackedVal);
  }

  // Warn the user about the error if we need to.
  if (sr != ScalarResult::Ok) {
    internal_LogScalarError(aName, sr);
  }

  return NS_OK;
}

/**
 * Adds the value to the given scalar.
 *
 * @param aName The scalar name.
 * @param aKey The key name.
 * @param aVal The numeric value to add to the scalar.
 * @param aCx The JS context.
 * @return NS_OK (always) so that the JS API call doesn't throw. In case of
 * errors, a warning level message is printed in the browser console.
 */
nsresult TelemetryScalar::Add(const nsACString& aName, const nsAString& aKey,
                              JS::Handle<JS::Value> aVal, JSContext* aCx) {
  // Unpack the aVal to nsIVariant. This uses the JS context.
  nsCOMPtr<nsIVariant> unpackedVal;
  nsresult rv = nsContentUtils::XPConnect()->JSToVariant(
      aCx, aVal, getter_AddRefs(unpackedVal));
  if (NS_FAILED(rv)) {
    internal_LogScalarError(aName, ScalarResult::CannotUnpackVariant);
    return NS_OK;
  }

  ScalarResult sr;
  {
    StaticMutexAutoLock locker(gTelemetryScalarsMutex);
    sr = internal_UpdateKeyedScalar(locker, aName, aKey, ScalarActionType::eAdd,
                                    unpackedVal);
  }

  // Warn the user about the error if we need to.
  if (sr != ScalarResult::Ok) {
    internal_LogScalarError(aName, sr);
  }

  return NS_OK;
}

/**
 * Adds the value to the given scalar.
 *
 * @param aId The scalar enum id.
 * @param aVal The numeric value to add to the scalar.
 */
void TelemetryScalar::Add(mozilla::Telemetry::ScalarID aId, uint32_t aValue) {
  if (NS_WARN_IF(!IsValidEnumId(aId))) {
    MOZ_ASSERT_UNREACHABLE("Scalar usage requires valid ids.");
    return;
  }

  ScalarKey uniqueId{static_cast<uint32_t>(aId), false};
  StaticMutexAutoLock locker(gTelemetryScalarsMutex);

  if (internal_CanRecordScalar(locker, uniqueId, false) != ScalarResult::Ok) {
    // We can't record this scalar. Bail out.
    return;
  }

  // Accumulate in the child process if needed.
  if (!XRE_IsParentProcess()) {
    TelemetryIPCAccumulator::RecordChildScalarAction(
        uniqueId.id, uniqueId.dynamic, ScalarActionType::eAdd,
        ScalarVariant(aValue));
    return;
  }

  if (internal_IsScalarDeserializing(locker)) {
    internal_RecordScalarAction(locker, uniqueId.id, uniqueId.dynamic,
                                ScalarActionType::eAdd, ScalarVariant(aValue));
    return;
  }

  ScalarBase* scalar = nullptr;
  nsresult rv =
      internal_GetScalarByEnum(locker, uniqueId, ProcessID::Parent, &scalar);
  if (NS_FAILED(rv)) {
    return;
  }

  scalar->AddValue(aValue);
}

/**
 * Adds the value to the given keyed scalar.
 *
 * @param aId The scalar enum id.
 * @param aKey The key name.
 * @param aVal The numeric value to add to the scalar.
 */
void TelemetryScalar::Add(mozilla::Telemetry::ScalarID aId,
                          const nsAString& aKey, uint32_t aValue) {
  if (NS_WARN_IF(!IsValidEnumId(aId))) {
    MOZ_ASSERT_UNREACHABLE("Scalar usage requires valid ids.");
    return;
  }

  ScalarKey uniqueId{static_cast<uint32_t>(aId), false};
  StaticMutexAutoLock locker(gTelemetryScalarsMutex);

  if (internal_CanRecordScalar(locker, uniqueId, true) != ScalarResult::Ok) {
    // We can't record this scalar. Bail out.
    return;
  }

  // Accumulate in the child process if needed.
  if (!XRE_IsParentProcess()) {
    TelemetryIPCAccumulator::RecordChildKeyedScalarAction(
        uniqueId.id, uniqueId.dynamic, aKey, ScalarActionType::eAdd,
        ScalarVariant(aValue));
    return;
  }

  if (internal_IsScalarDeserializing(locker)) {
    internal_RecordKeyedScalarAction(locker, uniqueId.id, uniqueId.dynamic,
                                     aKey, ScalarActionType::eAdd,
                                     ScalarVariant(aValue));
    return;
  }

  KeyedScalar* scalar = nullptr;
  nsresult rv = internal_GetKeyedScalarByEnum(locker, uniqueId,
                                              ProcessID::Parent, &scalar);
  if (NS_FAILED(rv)) {
    return;
  }

  scalar->AddValue(locker, aKey, aValue);
}

/**
 * Sets the scalar to the given value.
 *
 * @param aName The scalar name.
 * @param aVal The value to set the scalar to.
 * @param aCx The JS context.
 * @return NS_OK (always) so that the JS API call doesn't throw. In case of
 * errors, a warning level message is printed in the browser console.
 */
nsresult TelemetryScalar::Set(const nsACString& aName,
                              JS::Handle<JS::Value> aVal, JSContext* aCx) {
  // Unpack the aVal to nsIVariant. This uses the JS context.
  nsCOMPtr<nsIVariant> unpackedVal;
  nsresult rv = nsContentUtils::XPConnect()->JSToVariant(
      aCx, aVal, getter_AddRefs(unpackedVal));
  if (NS_FAILED(rv)) {
    internal_LogScalarError(aName, ScalarResult::CannotUnpackVariant);
    return NS_OK;
  }

  ScalarResult sr;
  {
    StaticMutexAutoLock locker(gTelemetryScalarsMutex);
    sr = internal_UpdateScalar(locker, aName, ScalarActionType::eSet,
                               unpackedVal);
  }

  // Warn the user about the error if we need to.
  if (sr != ScalarResult::Ok) {
    internal_LogScalarError(aName, sr);
  }

  return NS_OK;
}

/**
 * Sets the keyed scalar to the given value.
 *
 * @param aName The scalar name.
 * @param aKey The key name.
 * @param aVal The value to set the scalar to.
 * @param aCx The JS context.
 * @return NS_OK (always) so that the JS API call doesn't throw. In case of
 * errors, a warning level message is printed in the browser console.
 */
nsresult TelemetryScalar::Set(const nsACString& aName, const nsAString& aKey,
                              JS::Handle<JS::Value> aVal, JSContext* aCx) {
  // Unpack the aVal to nsIVariant. This uses the JS context.
  nsCOMPtr<nsIVariant> unpackedVal;
  nsresult rv = nsContentUtils::XPConnect()->JSToVariant(
      aCx, aVal, getter_AddRefs(unpackedVal));
  if (NS_FAILED(rv)) {
    internal_LogScalarError(aName, ScalarResult::CannotUnpackVariant);
    return NS_OK;
  }

  ScalarResult sr;
  {
    StaticMutexAutoLock locker(gTelemetryScalarsMutex);
    sr = internal_UpdateKeyedScalar(locker, aName, aKey, ScalarActionType::eSet,
                                    unpackedVal);
  }

  // Warn the user about the error if we need to.
  if (sr != ScalarResult::Ok) {
    internal_LogScalarError(aName, sr);
  }

  return NS_OK;
}

/**
 * Sets the scalar to the given numeric value.
 *
 * @param aId The scalar enum id.
 * @param aValue The numeric, unsigned value to set the scalar to.
 */
void TelemetryScalar::Set(mozilla::Telemetry::ScalarID aId, uint32_t aValue) {
  if (NS_WARN_IF(!IsValidEnumId(aId))) {
    MOZ_ASSERT_UNREACHABLE("Scalar usage requires valid ids.");
    return;
  }

  ScalarKey uniqueId{static_cast<uint32_t>(aId), false};
  StaticMutexAutoLock locker(gTelemetryScalarsMutex);

  if (internal_CanRecordScalar(locker, uniqueId, false) != ScalarResult::Ok) {
    // We can't record this scalar. Bail out.
    return;
  }

  // Accumulate in the child process if needed.
  if (!XRE_IsParentProcess()) {
    TelemetryIPCAccumulator::RecordChildScalarAction(
        uniqueId.id, uniqueId.dynamic, ScalarActionType::eSet,
        ScalarVariant(aValue));
    return;
  }

  if (internal_IsScalarDeserializing(locker)) {
    internal_RecordScalarAction(locker, uniqueId.id, uniqueId.dynamic,
                                ScalarActionType::eSet, ScalarVariant(aValue));
    return;
  }

  ScalarBase* scalar = nullptr;
  nsresult rv =
      internal_GetScalarByEnum(locker, uniqueId, ProcessID::Parent, &scalar);
  if (NS_FAILED(rv)) {
    return;
  }

  scalar->SetValue(aValue);
}

/**
 * Sets the scalar to the given string value.
 *
 * @param aId The scalar enum id.
 * @param aValue The string value to set the scalar to.
 */
void TelemetryScalar::Set(mozilla::Telemetry::ScalarID aId,
                          const nsAString& aValue) {
  if (NS_WARN_IF(!IsValidEnumId(aId))) {
    MOZ_ASSERT_UNREACHABLE("Scalar usage requires valid ids.");
    return;
  }

  ScalarKey uniqueId{static_cast<uint32_t>(aId), false};
  StaticMutexAutoLock locker(gTelemetryScalarsMutex);

  if (internal_CanRecordScalar(locker, uniqueId, false) != ScalarResult::Ok) {
    // We can't record this scalar. Bail out.
    return;
  }

  // Accumulate in the child process if needed.
  if (!XRE_IsParentProcess()) {
    TelemetryIPCAccumulator::RecordChildScalarAction(
        uniqueId.id, uniqueId.dynamic, ScalarActionType::eSet,
        ScalarVariant(nsString(aValue)));
    return;
  }

  if (internal_IsScalarDeserializing(locker)) {
    internal_RecordScalarAction(locker, uniqueId.id, uniqueId.dynamic,
                                ScalarActionType::eSet,
                                ScalarVariant(nsString(aValue)));
    return;
  }

  ScalarBase* scalar = nullptr;
  nsresult rv =
      internal_GetScalarByEnum(locker, uniqueId, ProcessID::Parent, &scalar);
  if (NS_FAILED(rv)) {
    return;
  }

  scalar->SetValue(aValue);
}

/**
 * Sets the scalar to the given boolean value.
 *
 * @param aId The scalar enum id.
 * @param aValue The boolean value to set the scalar to.
 */
void TelemetryScalar::Set(mozilla::Telemetry::ScalarID aId, bool aValue) {
  if (NS_WARN_IF(!IsValidEnumId(aId))) {
    MOZ_ASSERT_UNREACHABLE("Scalar usage requires valid ids.");
    return;
  }

  ScalarKey uniqueId{static_cast<uint32_t>(aId), false};
  StaticMutexAutoLock locker(gTelemetryScalarsMutex);

  if (internal_CanRecordScalar(locker, uniqueId, false) != ScalarResult::Ok) {
    // We can't record this scalar. Bail out.
    return;
  }

  // Accumulate in the child process if needed.
  if (!XRE_IsParentProcess()) {
    TelemetryIPCAccumulator::RecordChildScalarAction(
        uniqueId.id, uniqueId.dynamic, ScalarActionType::eSet,
        ScalarVariant(aValue));
    return;
  }

  if (internal_IsScalarDeserializing(locker)) {
    internal_RecordScalarAction(locker, uniqueId.id, uniqueId.dynamic,
                                ScalarActionType::eSet, ScalarVariant(aValue));
    return;
  }

  ScalarBase* scalar = nullptr;
  nsresult rv =
      internal_GetScalarByEnum(locker, uniqueId, ProcessID::Parent, &scalar);
  if (NS_FAILED(rv)) {
    return;
  }

  scalar->SetValue(aValue);
}

/**
 * Sets the keyed scalar to the given numeric value.
 *
 * @param aId The scalar enum id.
 * @param aKey The scalar key.
 * @param aValue The numeric, unsigned value to set the scalar to.
 */
void TelemetryScalar::Set(mozilla::Telemetry::ScalarID aId,
                          const nsAString& aKey, uint32_t aValue) {
  if (NS_WARN_IF(!IsValidEnumId(aId))) {
    MOZ_ASSERT_UNREACHABLE("Scalar usage requires valid ids.");
    return;
  }

  ScalarKey uniqueId{static_cast<uint32_t>(aId), false};
  StaticMutexAutoLock locker(gTelemetryScalarsMutex);

  if (internal_CanRecordScalar(locker, uniqueId, true) != ScalarResult::Ok) {
    // We can't record this scalar. Bail out.
    return;
  }

  // Accumulate in the child process if needed.
  if (!XRE_IsParentProcess()) {
    TelemetryIPCAccumulator::RecordChildKeyedScalarAction(
        uniqueId.id, uniqueId.dynamic, aKey, ScalarActionType::eSet,
        ScalarVariant(aValue));
    return;
  }

  if (internal_IsScalarDeserializing(locker)) {
    internal_RecordKeyedScalarAction(locker, uniqueId.id, uniqueId.dynamic,
                                     aKey, ScalarActionType::eSet,
                                     ScalarVariant(aValue));
    return;
  }

  KeyedScalar* scalar = nullptr;
  nsresult rv = internal_GetKeyedScalarByEnum(locker, uniqueId,
                                              ProcessID::Parent, &scalar);
  if (NS_FAILED(rv)) {
    return;
  }

  scalar->SetValue(locker, aKey, aValue);
}

/**
 * Sets the scalar to the given boolean value.
 *
 * @param aId The scalar enum id.
 * @param aKey The scalar key.
 * @param aValue The boolean value to set the scalar to.
 */
void TelemetryScalar::Set(mozilla::Telemetry::ScalarID aId,
                          const nsAString& aKey, bool aValue) {
  if (NS_WARN_IF(!IsValidEnumId(aId))) {
    MOZ_ASSERT_UNREACHABLE("Scalar usage requires valid ids.");
    return;
  }

  ScalarKey uniqueId{static_cast<uint32_t>(aId), false};
  StaticMutexAutoLock locker(gTelemetryScalarsMutex);

  if (internal_CanRecordScalar(locker, uniqueId, true) != ScalarResult::Ok) {
    // We can't record this scalar. Bail out.
    return;
  }

  // Accumulate in the child process if needed.
  if (!XRE_IsParentProcess()) {
    TelemetryIPCAccumulator::RecordChildKeyedScalarAction(
        uniqueId.id, uniqueId.dynamic, aKey, ScalarActionType::eSet,
        ScalarVariant(aValue));
    return;
  }

  if (internal_IsScalarDeserializing(locker)) {
    internal_RecordKeyedScalarAction(locker, uniqueId.id, uniqueId.dynamic,
                                     aKey, ScalarActionType::eSet,
                                     ScalarVariant(aValue));
    return;
  }

  KeyedScalar* scalar = nullptr;
  nsresult rv = internal_GetKeyedScalarByEnum(locker, uniqueId,
                                              ProcessID::Parent, &scalar);
  if (NS_FAILED(rv)) {
    return;
  }

  scalar->SetValue(locker, aKey, aValue);
}

/**
 * Sets the scalar to the maximum of the current and the passed value.
 *
 * @param aName The scalar name.
 * @param aVal The numeric value to set the scalar to.
 * @param aCx The JS context.
 * @return NS_OK (always) so that the JS API call doesn't throw. In case of
 * errors, a warning level message is printed in the browser console.
 */
nsresult TelemetryScalar::SetMaximum(const nsACString& aName,
                                     JS::Handle<JS::Value> aVal,
                                     JSContext* aCx) {
  // Unpack the aVal to nsIVariant. This uses the JS context.
  nsCOMPtr<nsIVariant> unpackedVal;
  nsresult rv = nsContentUtils::XPConnect()->JSToVariant(
      aCx, aVal, getter_AddRefs(unpackedVal));
  if (NS_FAILED(rv)) {
    internal_LogScalarError(aName, ScalarResult::CannotUnpackVariant);
    return NS_OK;
  }

  ScalarResult sr;
  {
    StaticMutexAutoLock locker(gTelemetryScalarsMutex);
    sr = internal_UpdateScalar(locker, aName, ScalarActionType::eSetMaximum,
                               unpackedVal);
  }

  // Warn the user about the error if we need to.
  if (sr != ScalarResult::Ok) {
    internal_LogScalarError(aName, sr);
  }

  return NS_OK;
}

/**
 * Sets the scalar to the maximum of the current and the passed value.
 *
 * @param aName The scalar name.
 * @param aKey The key name.
 * @param aVal The numeric value to set the scalar to.
 * @param aCx The JS context.
 * @return NS_OK (always) so that the JS API call doesn't throw. In case of
 * errors, a warning level message is printed in the browser console.
 */
nsresult TelemetryScalar::SetMaximum(const nsACString& aName,
                                     const nsAString& aKey,
                                     JS::Handle<JS::Value> aVal,
                                     JSContext* aCx) {
  // Unpack the aVal to nsIVariant. This uses the JS context.
  nsCOMPtr<nsIVariant> unpackedVal;
  nsresult rv = nsContentUtils::XPConnect()->JSToVariant(
      aCx, aVal, getter_AddRefs(unpackedVal));
  if (NS_FAILED(rv)) {
    internal_LogScalarError(aName, ScalarResult::CannotUnpackVariant);
    return NS_OK;
  }

  ScalarResult sr;
  {
    StaticMutexAutoLock locker(gTelemetryScalarsMutex);
    sr = internal_UpdateKeyedScalar(locker, aName, aKey,
                                    ScalarActionType::eSetMaximum, unpackedVal);
  }

  // Warn the user about the error if we need to.
  if (sr != ScalarResult::Ok) {
    internal_LogScalarError(aName, sr);
  }

  return NS_OK;
}

/**
 * Sets the scalar to the maximum of the current and the passed value.
 *
 * @param aId The scalar enum id.
 * @param aValue The numeric value to set the scalar to.
 */
void TelemetryScalar::SetMaximum(mozilla::Telemetry::ScalarID aId,
                                 uint32_t aValue) {
  if (NS_WARN_IF(!IsValidEnumId(aId))) {
    MOZ_ASSERT_UNREACHABLE("Scalar usage requires valid ids.");
    return;
  }

  ScalarKey uniqueId{static_cast<uint32_t>(aId), false};
  StaticMutexAutoLock locker(gTelemetryScalarsMutex);

  if (internal_CanRecordScalar(locker, uniqueId, false) != ScalarResult::Ok) {
    // We can't record this scalar. Bail out.
    return;
  }

  // Accumulate in the child process if needed.
  if (!XRE_IsParentProcess()) {
    TelemetryIPCAccumulator::RecordChildScalarAction(
        uniqueId.id, uniqueId.dynamic, ScalarActionType::eSetMaximum,
        ScalarVariant(aValue));
    return;
  }

  if (internal_IsScalarDeserializing(locker)) {
    internal_RecordScalarAction(locker, uniqueId.id, uniqueId.dynamic,
                                ScalarActionType::eSetMaximum,
                                ScalarVariant(aValue));
    return;
  }

  ScalarBase* scalar = nullptr;
  nsresult rv =
      internal_GetScalarByEnum(locker, uniqueId, ProcessID::Parent, &scalar);
  if (NS_FAILED(rv)) {
    return;
  }

  scalar->SetMaximum(aValue);
}

/**
 * Sets the keyed scalar to the maximum of the current and the passed value.
 *
 * @param aId The scalar enum id.
 * @param aKey The key name.
 * @param aValue The numeric value to set the scalar to.
 */
void TelemetryScalar::SetMaximum(mozilla::Telemetry::ScalarID aId,
                                 const nsAString& aKey, uint32_t aValue) {
  if (NS_WARN_IF(!IsValidEnumId(aId))) {
    MOZ_ASSERT_UNREACHABLE("Scalar usage requires valid ids.");
    return;
  }

  ScalarKey uniqueId{static_cast<uint32_t>(aId), false};
  StaticMutexAutoLock locker(gTelemetryScalarsMutex);

  if (internal_CanRecordScalar(locker, uniqueId, true) != ScalarResult::Ok) {
    // We can't record this scalar. Bail out.
    return;
  }

  // Accumulate in the child process if needed.
  if (!XRE_IsParentProcess()) {
    TelemetryIPCAccumulator::RecordChildKeyedScalarAction(
        uniqueId.id, uniqueId.dynamic, aKey, ScalarActionType::eSetMaximum,
        ScalarVariant(aValue));
    return;
  }

  if (internal_IsScalarDeserializing(locker)) {
    internal_RecordKeyedScalarAction(locker, uniqueId.id, uniqueId.dynamic,
                                     aKey, ScalarActionType::eSetMaximum,
                                     ScalarVariant(aValue));
    return;
  }

  KeyedScalar* scalar = nullptr;
  nsresult rv = internal_GetKeyedScalarByEnum(locker, uniqueId,
                                              ProcessID::Parent, &scalar);
  if (NS_FAILED(rv)) {
    return;
  }

  scalar->SetMaximum(locker, aKey, aValue);
}

nsresult TelemetryScalar::CreateSnapshots(unsigned int aDataset,
                                          bool aClearScalars, JSContext* aCx,
                                          uint8_t optional_argc,
                                          JS::MutableHandle<JS::Value> aResult,
                                          bool aFilterTest,
                                          const nsACString& aStoreName) {
  MOZ_ASSERT(
      XRE_IsParentProcess(),
      "Snapshotting scalars should only happen in the parent processes.");
  // If no arguments were passed in, apply the default value.
  if (!optional_argc) {
    aClearScalars = false;
  }

  JS::Rooted<JSObject*> root_obj(aCx, JS_NewPlainObject(aCx));
  if (!root_obj) {
    return NS_ERROR_FAILURE;
  }
  aResult.setObject(*root_obj);

  // Return `{}` in child processes.
  if (!XRE_IsParentProcess()) {
    return NS_OK;
  }

  // Only lock the mutex while accessing our data, without locking any JS
  // related code.
  ScalarSnapshotTable scalarsToReflect;
  {
    StaticMutexAutoLock locker(gTelemetryScalarsMutex);

    nsresult rv = internal_GetScalarSnapshot(locker, scalarsToReflect, aDataset,
                                             aClearScalars, aStoreName);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  // Reflect it to JS.
  for (const auto& entry : scalarsToReflect) {
    const ScalarTupleArray& processScalars = entry.GetData();
    const char* processName = GetNameForProcessID(ProcessID(entry.GetKey()));

    // Create the object that will hold the scalars for this process and add it
    // to the returned root object.
    JS::Rooted<JSObject*> processObj(aCx, JS_NewPlainObject(aCx));
    if (!processObj || !JS_DefineProperty(aCx, root_obj, processName,
                                          processObj, JSPROP_ENUMERATE)) {
      return NS_ERROR_FAILURE;
    }

    for (ScalarTupleArray::size_type i = 0; i < processScalars.Length(); i++) {
      const ScalarDataTuple& scalar = processScalars[i];

      const char* scalarName = mozilla::Get<0>(scalar);
      if (aFilterTest && strncmp(TEST_SCALAR_PREFIX, scalarName,
                                 strlen(TEST_SCALAR_PREFIX)) == 0) {
        continue;
      }

      // Convert it to a JS Val.
      JS::Rooted<JS::Value> scalarJsValue(aCx);
      nsresult rv = nsContentUtils::XPConnect()->VariantToJS(
          aCx, processObj, mozilla::Get<1>(scalar), &scalarJsValue);
      if (NS_FAILED(rv)) {
        return rv;
      }

      // Add it to the scalar object.
      if (!JS_DefineProperty(aCx, processObj, scalarName, scalarJsValue,
                             JSPROP_ENUMERATE)) {
        return NS_ERROR_FAILURE;
      }
    }
  }

  return NS_OK;
}

nsresult TelemetryScalar::CreateKeyedSnapshots(
    unsigned int aDataset, bool aClearScalars, JSContext* aCx,
    uint8_t optional_argc, JS::MutableHandle<JS::Value> aResult,
    bool aFilterTest, const nsACString& aStoreName) {
  MOZ_ASSERT(
      XRE_IsParentProcess(),
      "Snapshotting scalars should only happen in the parent processes.");
  // If no arguments were passed in, apply the default value.
  if (!optional_argc) {
    aClearScalars = false;
  }

  JS::Rooted<JSObject*> root_obj(aCx, JS_NewPlainObject(aCx));
  if (!root_obj) {
    return NS_ERROR_FAILURE;
  }
  aResult.setObject(*root_obj);

  // Return `{}` in child processes.
  if (!XRE_IsParentProcess()) {
    return NS_OK;
  }

  // Only lock the mutex while accessing our data, without locking any JS
  // related code.
  KeyedScalarSnapshotTable scalarsToReflect;
  {
    StaticMutexAutoLock locker(gTelemetryScalarsMutex);

    nsresult rv = internal_GetKeyedScalarSnapshot(
        locker, scalarsToReflect, aDataset, aClearScalars, aStoreName);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  // Reflect it to JS.
  for (const auto& entry : scalarsToReflect) {
    const KeyedScalarTupleArray& processScalars = entry.GetData();
    const char* processName = GetNameForProcessID(ProcessID(entry.GetKey()));

    // Create the object that will hold the scalars for this process and add it
    // to the returned root object.
    JS::Rooted<JSObject*> processObj(aCx, JS_NewPlainObject(aCx));
    if (!processObj || !JS_DefineProperty(aCx, root_obj, processName,
                                          processObj, JSPROP_ENUMERATE)) {
      return NS_ERROR_FAILURE;
    }

    for (KeyedScalarTupleArray::size_type i = 0; i < processScalars.Length();
         i++) {
      const KeyedScalarDataTuple& keyedScalarData = processScalars[i];

      const char* scalarName = mozilla::Get<0>(keyedScalarData);
      if (aFilterTest && strncmp(TEST_SCALAR_PREFIX, scalarName,
                                 strlen(TEST_SCALAR_PREFIX)) == 0) {
        continue;
      }

      // Go through each keyed scalar and create a keyed scalar object.
      // This object will hold the values for all the keyed scalar keys.
      JS::Rooted<JSObject*> keyedScalarObj(aCx, JS_NewPlainObject(aCx));

      // Define a property for each scalar key, then add it to the keyed scalar
      // object.
      const nsTArray<KeyedScalar::KeyValuePair>& keyProps =
          mozilla::Get<1>(keyedScalarData);
      for (uint32_t i = 0; i < keyProps.Length(); i++) {
        const KeyedScalar::KeyValuePair& keyData = keyProps[i];

        // Convert the value for the key to a JSValue.
        JS::Rooted<JS::Value> keyJsValue(aCx);
        nsresult rv = nsContentUtils::XPConnect()->VariantToJS(
            aCx, keyedScalarObj, keyData.second, &keyJsValue);
        if (NS_FAILED(rv)) {
          return rv;
        }

        // Add the key to the scalar representation.
        const NS_ConvertUTF8toUTF16 key(keyData.first);
        if (!JS_DefineUCProperty(aCx, keyedScalarObj, key.Data(), key.Length(),
                                 keyJsValue, JSPROP_ENUMERATE)) {
          return NS_ERROR_FAILURE;
        }
      }

      // Add the scalar to the root object.
      if (!JS_DefineProperty(aCx, processObj, scalarName, keyedScalarObj,
                             JSPROP_ENUMERATE)) {
        return NS_ERROR_FAILURE;
      }
    }
  }

  return NS_OK;
}

nsresult TelemetryScalar::RegisterScalars(const nsACString& aCategoryName,
                                          JS::Handle<JS::Value> aScalarData,
                                          bool aBuiltin, JSContext* cx) {
  MOZ_ASSERT(XRE_IsParentProcess(),
             "Dynamic scalars should only be created in the parent process.");

  if (!IsValidIdentifierString(aCategoryName, kMaximumCategoryNameLength, true,
                               false)) {
    JS_ReportErrorASCII(cx, "Invalid category name %s.",
                        PromiseFlatCString(aCategoryName).get());
    return NS_ERROR_INVALID_ARG;
  }

  if (!aScalarData.isObject()) {
    JS_ReportErrorASCII(cx, "Scalar data parameter should be an object");
    return NS_ERROR_INVALID_ARG;
  }

  JS::Rooted<JSObject*> obj(cx, &aScalarData.toObject());
  JS::Rooted<JS::IdVector> scalarPropertyIds(cx, JS::IdVector(cx));
  if (!JS_Enumerate(cx, obj, &scalarPropertyIds)) {
    return NS_ERROR_FAILURE;
  }

  // Collect the scalar data into local storage first.
  // Only after successfully validating all contained scalars will we register
  // them into global storage.
  nsTArray<DynamicScalarInfo> newScalarInfos;

  for (size_t i = 0, n = scalarPropertyIds.length(); i < n; i++) {
    nsAutoJSString scalarName;
    if (!scalarName.init(cx, scalarPropertyIds[i])) {
      return NS_ERROR_FAILURE;
    }

    if (!IsValidIdentifierString(NS_ConvertUTF16toUTF8(scalarName),
                                 kMaximumScalarNameLength, false, true)) {
      JS_ReportErrorASCII(
          cx, "Invalid scalar name %s.",
          PromiseFlatCString(NS_ConvertUTF16toUTF8(scalarName)).get());
      return NS_ERROR_INVALID_ARG;
    }

    // Join the category and the probe names.
    nsPrintfCString fullName("%s.%s", PromiseFlatCString(aCategoryName).get(),
                             NS_ConvertUTF16toUTF8(scalarName).get());

    JS::Rooted<JS::Value> value(cx);
    if (!JS_GetPropertyById(cx, obj, scalarPropertyIds[i], &value) ||
        !value.isObject()) {
      return NS_ERROR_FAILURE;
    }
    JS::Rooted<JSObject*> scalarDef(cx, &value.toObject());

    // Get the scalar's kind.
    if (!JS_GetProperty(cx, scalarDef, "kind", &value) || !value.isInt32()) {
      JS_ReportErrorASCII(cx, "Invalid or missing 'kind' for scalar %s.",
                          PromiseFlatCString(fullName).get());
      return NS_ERROR_FAILURE;
    }
    uint32_t kind = static_cast<uint32_t>(value.toInt32());

    // Get the optional scalar's recording policy (default to false).
    bool hasProperty = false;
    bool recordOnRelease = false;
    if (JS_HasProperty(cx, scalarDef, "record_on_release", &hasProperty) &&
        hasProperty) {
      if (!JS_GetProperty(cx, scalarDef, "record_on_release", &value) ||
          !value.isBoolean()) {
        JS_ReportErrorASCII(cx, "Invalid 'record_on_release' for scalar %s.",
                            PromiseFlatCString(fullName).get());
        return NS_ERROR_FAILURE;
      }
      recordOnRelease = static_cast<bool>(value.toBoolean());
    }

    // Get the optional scalar's keyed (default to false).
    bool keyed = false;
    if (JS_HasProperty(cx, scalarDef, "keyed", &hasProperty) && hasProperty) {
      if (!JS_GetProperty(cx, scalarDef, "keyed", &value) ||
          !value.isBoolean()) {
        JS_ReportErrorASCII(cx, "Invalid 'keyed' for scalar %s.",
                            PromiseFlatCString(fullName).get());
        return NS_ERROR_FAILURE;
      }
      keyed = static_cast<bool>(value.toBoolean());
    }

    // Get the optional scalar's expired state (default to false).
    bool expired = false;
    if (JS_HasProperty(cx, scalarDef, "expired", &hasProperty) && hasProperty) {
      if (!JS_GetProperty(cx, scalarDef, "expired", &value) ||
          !value.isBoolean()) {
        JS_ReportErrorASCII(cx, "Invalid 'expired' for scalar %s.",
                            PromiseFlatCString(fullName).get());
        return NS_ERROR_FAILURE;
      }
      expired = static_cast<bool>(value.toBoolean());
    }

    // Get the scalar's optional stores list (default to ["main"]).
    nsTArray<nsCString> stores;
    if (JS_HasProperty(cx, scalarDef, "stores", &hasProperty) && hasProperty) {
      bool isArray = false;
      if (!JS_GetProperty(cx, scalarDef, "stores", &value) ||
          !JS::IsArrayObject(cx, value, &isArray) || !isArray) {
        JS_ReportErrorASCII(cx, "Invalid 'stores' for scalar %s.",
                            PromiseFlatCString(fullName).get());
        return NS_ERROR_FAILURE;
      }

      JS::Rooted<JSObject*> arrayObj(cx, &value.toObject());
      uint32_t storesLength = 0;
      if (!JS::GetArrayLength(cx, arrayObj, &storesLength)) {
        JS_ReportErrorASCII(cx,
                            "Can't get 'stores' array length for scalar %s.",
                            PromiseFlatCString(fullName).get());
        return NS_ERROR_FAILURE;
      }

      for (uint32_t i = 0; i < storesLength; ++i) {
        JS::Rooted<JS::Value> elt(cx);
        if (!JS_GetElement(cx, arrayObj, i, &elt)) {
          JS_ReportErrorASCII(
              cx, "Can't get element from scalar %s 'stores' array.",
              PromiseFlatCString(fullName).get());
          return NS_ERROR_FAILURE;
        }
        if (!elt.isString()) {
          JS_ReportErrorASCII(cx,
                              "Element in scalar %s 'stores' array isn't a "
                              "string.",
                              PromiseFlatCString(fullName).get());
          return NS_ERROR_FAILURE;
        }

        nsAutoJSString jsStr;
        if (!jsStr.init(cx, elt)) {
          return NS_ERROR_FAILURE;
        }

        stores.AppendElement(NS_ConvertUTF16toUTF8(jsStr));
      }
      // In the event of the usual case (just "main"), save the storage.
      if (stores.Length() == 1 && stores[0].EqualsLiteral("main")) {
        stores.TruncateLength(0);
      }
    }

    // We defer the actual registration here in case any other event description
    // is invalid. In that case we don't need to roll back any partial
    // registration.
    newScalarInfos.AppendElement(
        DynamicScalarInfo{kind, recordOnRelease, expired, fullName, keyed,
                          aBuiltin, std::move(stores)});
  }

  // Register the dynamic definition on the parent process.
  nsTArray<DynamicScalarDefinition> ipcDefinitions;
  {
    StaticMutexAutoLock locker(gTelemetryScalarsMutex);
    ::internal_RegisterScalars(locker, newScalarInfos);

    // Convert the internal scalar representation to a stripped down IPC one.
    ::internal_DynamicScalarToIPC(locker, newScalarInfos, ipcDefinitions);
  }

  // Propagate the registration to all the content-processes.
  // Do not hold the mutex while calling IPC.
  ::internal_BroadcastDefinitions(ipcDefinitions);

  return NS_OK;
}

/**
 * Count in Scalars how many of which events were recorded. See bug 1440673
 *
 * Event Telemetry unfortunately cannot use vanilla ScalarAdd because it needs
 * to summarize events recorded in different processes to the
 * telemetry.event_counts of the same process. Including "dynamic".
 *
 * @param aUniqueEventName - expected to be category#object#method
 * @param aProcessType - the process of the event being summarized
 * @param aDynamic - whether the event being summarized was dynamic
 */
void TelemetryScalar::SummarizeEvent(const nsCString& aUniqueEventName,
                                     ProcessID aProcessType, bool aDynamic) {
  MOZ_ASSERT(XRE_IsParentProcess(),
             "Only summarize events in the parent process");
  if (!XRE_IsParentProcess()) {
    return;
  }

  StaticMutexAutoLock lock(gTelemetryScalarsMutex);

  ScalarKey scalarKey{static_cast<uint32_t>(ScalarID::TELEMETRY_EVENT_COUNTS),
                      aDynamic};
  if (aDynamic) {
    nsresult rv = internal_GetEnumByScalarName(
        lock, nsAutoCString("telemetry.dynamic_event_counts"), &scalarKey);
    if (NS_FAILED(rv)) {
      NS_WARNING(
          "NS_FAILED getting ScalarKey for telemetry.dynamic_event_counts");
      return;
    }
  }

  KeyedScalar* scalar = nullptr;
  nsresult rv =
      internal_GetKeyedScalarByEnum(lock, scalarKey, aProcessType, &scalar);

  if (NS_FAILED(rv)) {
    NS_WARNING("NS_FAILED getting keyed scalar for event summary. Wut.");
    return;
  }

  // Set this each time as it may have been cleared and recreated between calls
  scalar->SetMaximumNumberOfKeys(kMaxEventSummaryKeys);

  scalar->AddValue(lock, NS_ConvertASCIItoUTF16(aUniqueEventName), 1);
}

/**
 * Resets all the stored scalars. This is intended to be only used in tests.
 */
void TelemetryScalar::ClearScalars() {
  MOZ_ASSERT(XRE_IsParentProcess(),
             "Scalars should only be cleared in the parent process.");
  if (!XRE_IsParentProcess()) {
    return;
  }

  StaticMutexAutoLock locker(gTelemetryScalarsMutex);
  gScalarStorageMap.Clear();
  gKeyedScalarStorageMap.Clear();
  gDynamicBuiltinScalarStorageMap.Clear();
  gDynamicBuiltinKeyedScalarStorageMap.Clear();
  gScalarsActions = nullptr;
  gKeyedScalarsActions = nullptr;
}

size_t TelemetryScalar::GetMapShallowSizesOfExcludingThis(
    mozilla::MallocSizeOf aMallocSizeOf) {
  StaticMutexAutoLock locker(gTelemetryScalarsMutex);
  return gScalarNameIDMap.ShallowSizeOfExcludingThis(aMallocSizeOf);
}

size_t TelemetryScalar::GetScalarSizesOfIncludingThis(
    mozilla::MallocSizeOf aMallocSizeOf) {
  StaticMutexAutoLock locker(gTelemetryScalarsMutex);
  size_t n = 0;

  auto getSizeOf = [aMallocSizeOf](auto& storageMap) {
    size_t partial = 0;
    for (const auto& scalarStorage : storageMap.Values()) {
      for (const auto& scalar : scalarStorage->Values()) {
        partial += scalar->SizeOfIncludingThis(aMallocSizeOf);
      }
    }
    return partial;
  };

  // Account for all the storage used for the different scalar types.
  n += getSizeOf(gScalarStorageMap);
  n += getSizeOf(gKeyedScalarStorageMap);
  n += getSizeOf(gDynamicBuiltinScalarStorageMap);
  n += getSizeOf(gDynamicBuiltinKeyedScalarStorageMap);

  return n;
}

void TelemetryScalar::UpdateChildData(
    ProcessID aProcessType,
    const nsTArray<mozilla::Telemetry::ScalarAction>& aScalarActions) {
  MOZ_ASSERT(XRE_IsParentProcess(),
             "The stored child processes scalar data must be updated from the "
             "parent process.");
  StaticMutexAutoLock locker(gTelemetryScalarsMutex);

  // If scalars are still being deserialized, we need to record the incoming
  // operations as well.
  if (internal_IsScalarDeserializing(locker)) {
    for (const ScalarAction& action : aScalarActions) {
      // We're only getting immutable access, so let's copy it
      ScalarAction copy = action;
      // Fix up the process type
      copy.mProcessType = aProcessType;
      internal_RecordScalarAction(locker, copy);
    }

    return;
  }

  internal_ApplyScalarActions(locker, aScalarActions, Some(aProcessType));
}

void TelemetryScalar::UpdateChildKeyedData(
    ProcessID aProcessType,
    const nsTArray<mozilla::Telemetry::KeyedScalarAction>& aScalarActions) {
  MOZ_ASSERT(XRE_IsParentProcess(),
             "The stored child processes keyed scalar data must be updated "
             "from the parent process.");
  StaticMutexAutoLock locker(gTelemetryScalarsMutex);

  // If scalars are still being deserialized, we need to record the incoming
  // operations as well.
  if (internal_IsScalarDeserializing(locker)) {
    for (const KeyedScalarAction& action : aScalarActions) {
      // We're only getting immutable access, so let's copy it
      KeyedScalarAction copy = action;
      // Fix up the process type
      copy.mProcessType = aProcessType;
      internal_RecordKeyedScalarAction(locker, copy);
    }

    return;
  }

  internal_ApplyKeyedScalarActions(locker, aScalarActions, Some(aProcessType));
}

void TelemetryScalar::RecordDiscardedData(
    ProcessID aProcessType,
    const mozilla::Telemetry::DiscardedData& aDiscardedData) {
  MOZ_ASSERT(XRE_IsParentProcess(),
             "Discarded Data must be updated from the parent process.");
  StaticMutexAutoLock locker(gTelemetryScalarsMutex);
  if (!internal_CanRecordBase(locker)) {
    return;
  }

  if (GetCurrentProduct() == SupportedProduct::GeckoviewStreaming) {
    return;
  }

  ScalarBase* scalar = nullptr;
  mozilla::DebugOnly<nsresult> rv;

  rv = internal_GetScalarByEnum(
      locker,
      ScalarKey{
          static_cast<uint32_t>(ScalarID::TELEMETRY_DISCARDED_ACCUMULATIONS),
          false},
      aProcessType, &scalar);
  MOZ_ASSERT(NS_SUCCEEDED(rv));
  scalar->AddValue(aDiscardedData.mDiscardedHistogramAccumulations);

  rv = internal_GetScalarByEnum(
      locker,
      ScalarKey{static_cast<uint32_t>(
                    ScalarID::TELEMETRY_DISCARDED_KEYED_ACCUMULATIONS),
                false},
      aProcessType, &scalar);
  MOZ_ASSERT(NS_SUCCEEDED(rv));
  scalar->AddValue(aDiscardedData.mDiscardedKeyedHistogramAccumulations);

  rv = internal_GetScalarByEnum(
      locker,
      ScalarKey{
          static_cast<uint32_t>(ScalarID::TELEMETRY_DISCARDED_SCALAR_ACTIONS),
          false},
      aProcessType, &scalar);
  MOZ_ASSERT(NS_SUCCEEDED(rv));
  scalar->AddValue(aDiscardedData.mDiscardedScalarActions);

  rv = internal_GetScalarByEnum(
      locker,
      ScalarKey{static_cast<uint32_t>(
                    ScalarID::TELEMETRY_DISCARDED_KEYED_SCALAR_ACTIONS),
                false},
      aProcessType, &scalar);
  MOZ_ASSERT(NS_SUCCEEDED(rv));
  scalar->AddValue(aDiscardedData.mDiscardedKeyedScalarActions);

  rv = internal_GetScalarByEnum(
      locker,
      ScalarKey{
          static_cast<uint32_t>(ScalarID::TELEMETRY_DISCARDED_CHILD_EVENTS),
          false},
      aProcessType, &scalar);
  MOZ_ASSERT(NS_SUCCEEDED(rv));
  scalar->AddValue(aDiscardedData.mDiscardedChildEvents);
}

/**
 * Get the dynamic scalar definitions in an IPC-friendly
 * structure.
 */
void TelemetryScalar::GetDynamicScalarDefinitions(
    nsTArray<DynamicScalarDefinition>& aDefArray) {
  MOZ_ASSERT(XRE_IsParentProcess());
  if (!gDynamicScalarInfo) {
    // Don't have dynamic scalar definitions. Bail out!
    return;
  }

  StaticMutexAutoLock locker(gTelemetryScalarsMutex);
  internal_DynamicScalarToIPC(locker, *gDynamicScalarInfo, aDefArray);
}

/**
 * This adds the dynamic scalar definitions coming from
 * the parent process to this child process. If a dynamic
 * scalar definition is already defined, check if the new definition
 * makes the scalar expired and eventually update the expiration
 * state.
 */
void TelemetryScalar::AddDynamicScalarDefinitions(
    const nsTArray<DynamicScalarDefinition>& aDefs) {
  MOZ_ASSERT(!XRE_IsParentProcess());

  nsTArray<DynamicScalarInfo> dynamicStubs;

  // Populate the definitions array before acquiring the lock.
  for (auto& def : aDefs) {
    bool recordOnRelease = def.dataset == nsITelemetry::DATASET_ALL_CHANNELS;
    dynamicStubs.AppendElement(DynamicScalarInfo{def.type,
                                                 recordOnRelease,
                                                 def.expired,
                                                 def.name,
                                                 def.keyed,
                                                 def.builtin,
                                                 {} /* stores */});
  }

  {
    StaticMutexAutoLock locker(gTelemetryScalarsMutex);
    internal_RegisterScalars(locker, dynamicStubs);
  }
}

nsresult TelemetryScalar::GetAllStores(StringHashSet& set) {
  // Static stores
  for (uint32_t storeIdx : gScalarStoresTable) {
    const char* name = &gScalarsStringTable[storeIdx];
    nsAutoCString store;
    store.AssignASCII(name);
    if (!set.Insert(store, mozilla::fallible)) {
      return NS_ERROR_FAILURE;
    }
  }

  // Dynamic stores
  for (auto& ptr : *gDynamicStoreNames) {
    nsAutoCString store;
    ptr->ToUTF8String(store);
    if (!set.Insert(store, mozilla::fallible)) {
      return NS_ERROR_FAILURE;
    }
  }

  return NS_OK;
}

////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
//
// PUBLIC: GeckoView serialization/deserialization functions.

/**
 * Write the scalar data to the provided Json object, for
 * GeckoView measurement persistence. The output format is the same one used
 * for snapshotting the scalars.
 *
 * @param {aWriter} The JSON object to write to.
 * @returns NS_OK or a failure value explaining why persistence failed.
 */
nsresult TelemetryScalar::SerializeScalars(mozilla::JSONWriter& aWriter) {
  // Get a copy of the data, without clearing.
  ScalarSnapshotTable scalarsToReflect;
  {
    StaticMutexAutoLock locker(gTelemetryScalarsMutex);
    // For persistence, we care about all the datasets. Worst case, they
    // will be empty.
    nsresult rv = internal_GetScalarSnapshot(
        locker, scalarsToReflect, nsITelemetry::DATASET_PRERELEASE_CHANNELS,
        false, /*aClearScalars*/
        "main"_ns);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  // Persist the scalars to the JSON object.
  for (const auto& entry : scalarsToReflect) {
    const ScalarTupleArray& processScalars = entry.GetData();
    const char* processName = GetNameForProcessID(ProcessID(entry.GetKey()));

    aWriter.StartObjectProperty(mozilla::MakeStringSpan(processName));

    for (const ScalarDataTuple& scalar : processScalars) {
      nsresult rv = WriteVariantToJSONWriter(
          mozilla::Get<2>(scalar) /*aScalarType*/,
          mozilla::Get<1>(scalar) /*aInputValue*/,
          mozilla::MakeStringSpan(mozilla::Get<0>(scalar)) /*aPropertyName*/,
          aWriter /*aWriter*/);
      if (NS_FAILED(rv)) {
        // Skip this scalar if we failed to write it. We don't bail out just
        // yet as we may salvage other scalars. We eventually need to call
        // EndObject.
        continue;
      }
    }

    aWriter.EndObject();
  }

  return NS_OK;
}

/**
 * Write the keyed scalar data to the provided Json object, for
 * GeckoView measurement persistence. The output format is the same
 * one used for snapshotting the keyed scalars.
 *
 * @param {aWriter} The JSON object to write to.
 * @returns NS_OK or a failure value explaining why persistence failed.
 */
nsresult TelemetryScalar::SerializeKeyedScalars(mozilla::JSONWriter& aWriter) {
  // Get a copy of the data, without clearing.
  KeyedScalarSnapshotTable keyedScalarsToReflect;
  {
    StaticMutexAutoLock locker(gTelemetryScalarsMutex);
    // For persistence, we care about all the datasets. Worst case, they
    // will be empty.
    nsresult rv = internal_GetKeyedScalarSnapshot(
        locker, keyedScalarsToReflect,
        nsITelemetry::DATASET_PRERELEASE_CHANNELS, false, /*aClearScalars*/
        "main"_ns);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  // Persist the scalars to the JSON object.
  for (const auto& entry : keyedScalarsToReflect) {
    const KeyedScalarTupleArray& processScalars = entry.GetData();
    const char* processName = GetNameForProcessID(ProcessID(entry.GetKey()));

    aWriter.StartObjectProperty(mozilla::MakeStringSpan(processName));

    for (const KeyedScalarDataTuple& keyedScalarData : processScalars) {
      aWriter.StartObjectProperty(
          mozilla::MakeStringSpan(mozilla::Get<0>(keyedScalarData)));

      // Define a property for each scalar key, then add it to the keyed scalar
      // object.
      const nsTArray<KeyedScalar::KeyValuePair>& keyProps =
          mozilla::Get<1>(keyedScalarData);
      for (const KeyedScalar::KeyValuePair& keyData : keyProps) {
        nsresult rv = WriteVariantToJSONWriter(
            mozilla::Get<2>(keyedScalarData) /*aScalarType*/,
            keyData.second /*aInputValue*/,
            PromiseFlatCString(keyData.first) /*aOutKey*/, aWriter /*aWriter*/);
        if (NS_FAILED(rv)) {
          // Skip this scalar if we failed to write it. We don't bail out just
          // yet as we may salvage other scalars. We eventually need to call
          // EndObject.
          continue;
        }
      }
      aWriter.EndObject();
    }
    aWriter.EndObject();
  }

  return NS_OK;
}

/**
 * Load the persisted measurements from a Json object and inject them
 * in the relevant process storage.
 *
 * @param {aData} The input Json object.
 * @returns NS_OK if loading was performed, an error code explaining the
 *          failure reason otherwise.
 */
nsresult TelemetryScalar::DeserializePersistedScalars(
    JSContext* aCx, JS::Handle<JS::Value> aData) {
  MOZ_ASSERT(XRE_IsParentProcess(), "Only load scalars in the parent process");
  if (!XRE_IsParentProcess()) {
    return NS_ERROR_FAILURE;
  }

  typedef std::pair<nsCString, nsCOMPtr<nsIVariant>> PersistedScalarPair;
  typedef nsTArray<PersistedScalarPair> PersistedScalarArray;
  typedef nsTHashMap<ProcessIDHashKey, PersistedScalarArray>
      PeristedScalarStorage;

  PeristedScalarStorage scalarsToUpdate;

  // Before updating the scalars, we need to get the data out of the JS
  // wrappers. We can't hold the scalars mutex while handling JS stuff.
  // Build a <scalar name, value> map.
  JS::Rooted<JSObject*> scalarDataObj(aCx, &aData.toObject());
  JS::Rooted<JS::IdVector> processes(aCx, JS::IdVector(aCx));
  if (!JS_Enumerate(aCx, scalarDataObj, &processes)) {
    // We can't even enumerate the processes in the loaded data, so
    // there is nothing we could recover from the persistence file. Bail out.
    JS_ClearPendingException(aCx);
    return NS_ERROR_FAILURE;
  }

  // The following block of code attempts to extract as much data as possible
  // from the serialized JSON, even in case of light data corruptions: if, for
  // example, the data for a single process is corrupted or is in an unexpected
  // form, we press on and attempt to load the data for the other processes.
  JS::Rooted<JS::PropertyKey> process(aCx);
  for (auto& processVal : processes) {
    // This is required as JS API calls require an Handle<jsid> and not a
    // plain jsid.
    process = processVal;
    // Get the process name.
    nsAutoJSString processNameJS;
    if (!processNameJS.init(aCx, process)) {
      JS_ClearPendingException(aCx);
      continue;
    }

    // Make sure it's valid. Note that this is safe to call outside
    // of a locked section.
    NS_ConvertUTF16toUTF8 processName(processNameJS);
    ProcessID processID = GetIDForProcessName(processName.get());
    if (processID == ProcessID::Count) {
      NS_WARNING(
          nsPrintfCString("Failed to get process ID for %s", processName.get())
              .get());
      continue;
    }

    // And its probes.
    JS::Rooted<JS::Value> processData(aCx);
    if (!JS_GetPropertyById(aCx, scalarDataObj, process, &processData)) {
      JS_ClearPendingException(aCx);
      continue;
    }

    if (!processData.isObject()) {
      // |processData| should be an object containing scalars. If this is
      // not the case, silently skip and try to load the data for the other
      // processes.
      continue;
    }

    // Iterate through each scalar.
    JS::Rooted<JSObject*> processDataObj(aCx, &processData.toObject());
    JS::Rooted<JS::IdVector> scalars(aCx, JS::IdVector(aCx));
    if (!JS_Enumerate(aCx, processDataObj, &scalars)) {
      JS_ClearPendingException(aCx);
      continue;
    }

    JS::Rooted<JS::PropertyKey> scalar(aCx);
    for (auto& scalarVal : scalars) {
      scalar = scalarVal;
      // Get the scalar name.
      nsAutoJSString scalarName;
      if (!scalarName.init(aCx, scalar)) {
        JS_ClearPendingException(aCx);
        continue;
      }

      // Get the scalar value as a JS value.
      JS::Rooted<JS::Value> scalarValue(aCx);
      if (!JS_GetPropertyById(aCx, processDataObj, scalar, &scalarValue)) {
        JS_ClearPendingException(aCx);
        continue;
      }

      if (scalarValue.isNullOrUndefined()) {
        // We can't set scalars to null or undefined values, skip this
        // and try to load other scalars.
        continue;
      }

      // Unpack the aVal to nsIVariant.
      nsCOMPtr<nsIVariant> unpackedVal;
      nsresult rv = nsContentUtils::XPConnect()->JSToVariant(
          aCx, scalarValue, getter_AddRefs(unpackedVal));
      if (NS_FAILED(rv)) {
        JS_ClearPendingException(aCx);
        continue;
      }

      // Add the scalar to the map.
      PersistedScalarArray& processScalars =
          scalarsToUpdate.LookupOrInsert(static_cast<uint32_t>(processID));
      processScalars.AppendElement(std::make_pair(
          nsCString(NS_ConvertUTF16toUTF8(scalarName)), unpackedVal));
    }
  }

  // Now that all the JS specific operations are finished, update the scalars.
  {
    StaticMutexAutoLock lock(gTelemetryScalarsMutex);

    for (const auto& entry : scalarsToUpdate) {
      const PersistedScalarArray& processScalars = entry.GetData();
      for (PersistedScalarArray::size_type i = 0; i < processScalars.Length();
           i++) {
        mozilla::Unused << internal_UpdateScalar(
            lock, processScalars[i].first, ScalarActionType::eSet,
            processScalars[i].second, ProcessID(entry.GetKey()),
            true /* aForce */);
      }
    }
  }

  return NS_OK;
}

/**
 * Load the persisted measurements from a Json object and injects them
 * in the relevant process storage.
 *
 * @param {aData} The input Json object.
 * @returns NS_OK if loading was performed, an error code explaining the
 *          failure reason otherwise.
 */
nsresult TelemetryScalar::DeserializePersistedKeyedScalars(
    JSContext* aCx, JS::Handle<JS::Value> aData) {
  MOZ_ASSERT(XRE_IsParentProcess(), "Only load scalars in the parent process");
  if (!XRE_IsParentProcess()) {
    return NS_ERROR_FAILURE;
  }

  typedef mozilla::Tuple<nsCString, nsString, nsCOMPtr<nsIVariant>>
      PersistedKeyedScalarTuple;
  typedef nsTArray<PersistedKeyedScalarTuple> PersistedKeyedScalarArray;
  typedef nsTHashMap<ProcessIDHashKey, PersistedKeyedScalarArray>
      PeristedKeyedScalarStorage;

  PeristedKeyedScalarStorage scalarsToUpdate;

  // Before updating the keyed scalars, we need to get the data out of the JS
  // wrappers. We can't hold the scalars mutex while handling JS stuff.
  // Build a <scalar name, value> map.
  JS::Rooted<JSObject*> scalarDataObj(aCx, &aData.toObject());
  JS::Rooted<JS::IdVector> processes(aCx, JS::IdVector(aCx));
  if (!JS_Enumerate(aCx, scalarDataObj, &processes)) {
    // We can't even enumerate the processes in the loaded data, so
    // there is nothing we could recover from the persistence file. Bail out.
    JS_ClearPendingException(aCx);
    return NS_ERROR_FAILURE;
  }

  // The following block of code attempts to extract as much data as possible
  // from the serialized JSON, even in case of light data corruptions: if, for
  // example, the data for a single process is corrupted or is in an unexpected
  // form, we press on and attempt to load the data for the other processes.
  JS::Rooted<JS::PropertyKey> process(aCx);
  for (auto& processVal : processes) {
    process = processVal;
    // Get the process name.
    nsAutoJSString processNameJS;
    if (!processNameJS.init(aCx, process)) {
      JS_ClearPendingException(aCx);
      continue;
    }

    // Make sure it's valid. Note that this is safe to call outside
    // of a locked section.
    NS_ConvertUTF16toUTF8 processName(processNameJS);
    ProcessID processID = GetIDForProcessName(processName.get());
    if (processID == ProcessID::Count) {
      NS_WARNING(
          nsPrintfCString("Failed to get process ID for %s", processName.get())
              .get());
      continue;
    }

    // And its probes.
    JS::Rooted<JS::Value> processData(aCx);
    if (!JS_GetPropertyById(aCx, scalarDataObj, process, &processData)) {
      JS_ClearPendingException(aCx);
      continue;
    }

    if (!processData.isObject()) {
      // |processData| should be an object containing scalars. If this is
      // not the case, silently skip and try to load the data for the other
      // processes.
      continue;
    }

    // Iterate through each keyed scalar.
    JS::Rooted<JSObject*> processDataObj(aCx, &processData.toObject());
    JS::Rooted<JS::IdVector> keyedScalars(aCx, JS::IdVector(aCx));
    if (!JS_Enumerate(aCx, processDataObj, &keyedScalars)) {
      JS_ClearPendingException(aCx);
      continue;
    }

    JS::Rooted<JS::PropertyKey> keyedScalar(aCx);
    for (auto& keyedScalarVal : keyedScalars) {
      keyedScalar = keyedScalarVal;
      // Get the scalar name.
      nsAutoJSString scalarName;
      if (!scalarName.init(aCx, keyedScalar)) {
        JS_ClearPendingException(aCx);
        continue;
      }

      // Get the data for this keyed scalar.
      JS::Rooted<JS::Value> keyedScalarData(aCx);
      if (!JS_GetPropertyById(aCx, processDataObj, keyedScalar,
                              &keyedScalarData)) {
        JS_ClearPendingException(aCx);
        continue;
      }

      if (!keyedScalarData.isObject()) {
        // Keyed scalar data need to be an object. If that's not the case, skip
        // it and try to load the rest of the data.
        continue;
      }

      // Get the keys in the keyed scalar.
      JS::Rooted<JSObject*> keyedScalarDataObj(aCx,
                                               &keyedScalarData.toObject());
      JS::Rooted<JS::IdVector> keys(aCx, JS::IdVector(aCx));
      if (!JS_Enumerate(aCx, keyedScalarDataObj, &keys)) {
        JS_ClearPendingException(aCx);
        continue;
      }

      JS::Rooted<JS::PropertyKey> key(aCx);
      for (auto keyVal : keys) {
        key = keyVal;
        // Get the process name.
        nsAutoJSString keyName;
        if (!keyName.init(aCx, key)) {
          JS_ClearPendingException(aCx);
          continue;
        }

        // Get the scalar value as a JS value.
        JS::Rooted<JS::Value> scalarValue(aCx);
        if (!JS_GetPropertyById(aCx, keyedScalarDataObj, key, &scalarValue)) {
          JS_ClearPendingException(aCx);
          continue;
        }

        if (scalarValue.isNullOrUndefined()) {
          // We can't set scalars to null or undefined values, skip this
          // and try to load other scalars.
          continue;
        }

        // Unpack the aVal to nsIVariant.
        nsCOMPtr<nsIVariant> unpackedVal;
        nsresult rv = nsContentUtils::XPConnect()->JSToVariant(
            aCx, scalarValue, getter_AddRefs(unpackedVal));
        if (NS_FAILED(rv)) {
          JS_ClearPendingException(aCx);
          continue;
        }

        // Add the scalar to the map.
        PersistedKeyedScalarArray& processScalars =
            scalarsToUpdate.LookupOrInsert(static_cast<uint32_t>(processID));
        processScalars.AppendElement(
            mozilla::MakeTuple(nsCString(NS_ConvertUTF16toUTF8(scalarName)),
                               nsString(keyName), unpackedVal));
      }
    }
  }

  // Now that all the JS specific operations are finished, update the scalars.
  {
    StaticMutexAutoLock lock(gTelemetryScalarsMutex);

    for (const auto& entry : scalarsToUpdate) {
      const PersistedKeyedScalarArray& processScalars = entry.GetData();
      for (PersistedKeyedScalarArray::size_type i = 0;
           i < processScalars.Length(); i++) {
        mozilla::Unused << internal_UpdateKeyedScalar(
            lock, mozilla::Get<0>(processScalars[i]),
            mozilla::Get<1>(processScalars[i]), ScalarActionType::eSet,
            mozilla::Get<2>(processScalars[i]), ProcessID(entry.GetKey()),
            true /* aForce */);
      }
    }
  }

  return NS_OK;
}
