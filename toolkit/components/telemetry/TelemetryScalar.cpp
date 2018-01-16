/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsITelemetry.h"
#include "nsIVariant.h"
#include "nsVariant.h"
#include "nsHashKeys.h"
#include "nsBaseHashtable.h"
#include "nsClassHashtable.h"
#include "nsDataHashtable.h"
#include "nsIXPConnect.h"
#include "nsContentUtils.h"
#include "nsThreadUtils.h"
#include "nsJSUtils.h"
#include "nsPrintfCString.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/PContent.h"
#include "mozilla/StaticMutex.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/Unused.h"

#include "TelemetryCommon.h"
#include "TelemetryScalar.h"
#include "TelemetryScalarData.h"
#include "ipc/TelemetryComms.h"
#include "ipc/TelemetryIPCAccumulator.h"

using mozilla::StaticAutoPtr;
using mozilla::StaticMutex;
using mozilla::StaticMutexAutoLock;
using mozilla::Telemetry::Common::AutoHashtable;
using mozilla::Telemetry::Common::IsExpiredVersion;
using mozilla::Telemetry::Common::CanRecordDataset;
using mozilla::Telemetry::Common::IsInDataset;
using mozilla::Telemetry::Common::LogToBrowserConsole;
using mozilla::Telemetry::Common::GetNameForProcessID;
using mozilla::Telemetry::Common::RecordedProcessType;
using mozilla::Telemetry::Common::IsValidIdentifierString;
using mozilla::Telemetry::ScalarActionType;
using mozilla::Telemetry::ScalarID;
using mozilla::Telemetry::DynamicScalarDefinition;
using mozilla::Telemetry::ScalarVariant;
using mozilla::Telemetry::ProcessID;

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
const uint32_t kMaximumKeyStringLength = 70;
const uint32_t kMaximumStringValueLength = 50;
// The category and scalar name maximum lengths are used by the dynamic
// scalar registration function and must match the constants used by
// the 'parse_scalars.py' script for static scalars.
const uint32_t kMaximumCategoryNameLength = 40;
const uint32_t kMaximumScalarNameLength = 40;
const uint32_t kScalarCount =
  static_cast<uint32_t>(mozilla::Telemetry::ScalarID::ScalarCount);

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

/**
 * Scalar information for dynamic definitions.
 */
struct DynamicScalarInfo : BaseScalarInfo {
  nsCString mDynamicName;
  bool mDynamicExpiration;

  DynamicScalarInfo(uint32_t aKind, bool aRecordOnRelease,
                    bool aExpired, const nsACString& aName,
                    bool aKeyed)
    : BaseScalarInfo(aKind,
                     aRecordOnRelease ?
                     nsITelemetry::DATASET_RELEASE_CHANNEL_OPTOUT :
                     nsITelemetry::DATASET_RELEASE_CHANNEL_OPTIN,
                     RecordedProcessType::All,
                     aKeyed)
    , mDynamicName(aName)
    , mDynamicExpiration(aExpired)
  {}

  // The following functions will read the stored text
  // instead of looking it up in the statically generated
  // tables.
  const char *name() const override;
  const char *expiration() const override;
};

const char *
DynamicScalarInfo::name() const
{
  return mDynamicName.get();
}

const char *
DynamicScalarInfo::expiration() const
{
  // Dynamic scalars can either be expired or not (boolean flag).
  // Return an appropriate version string to leverage the scalar expiration
  // logic.
  return mDynamicExpiration ? "1.0" : "never";
}

typedef nsBaseHashtableET<nsDepCharHashKey, ScalarKey> CharPtrEntryType;
typedef AutoHashtable<CharPtrEntryType> ScalarMapType;

// Dynamic scalar definitions.
StaticAutoPtr<nsTArray<DynamicScalarInfo>> gDynamicScalarInfo;

const BaseScalarInfo&
internal_GetScalarInfo(const StaticMutexAutoLock& lock, const ScalarKey& aId)
{
  if (!aId.dynamic) {
    return gScalars[aId.id];
  }

  return (*gDynamicScalarInfo)[aId.id];
}

bool
IsValidEnumId(mozilla::Telemetry::ScalarID aID)
{
  return aID < mozilla::Telemetry::ScalarID::ScalarCount;
}

bool
internal_IsValidId(const StaticMutexAutoLock& lock, const ScalarKey& aId)
{
  // Please note that this function needs to be called with the scalar
  // mutex being acquired: other functions might be messing with
  // |gDynamicScalarInfo|.
  return aId.dynamic ? (aId.id < gDynamicScalarInfo->Length()) :
    IsValidEnumId(static_cast<mozilla::Telemetry::ScalarID>(aId.id));
}

/**
 * Convert a nsIVariant to a mozilla::Variant, which is used for
 * accumulating child process scalars.
 */
ScalarResult
GetVariantFromIVariant(nsIVariant* aInput, uint32_t aScalarKind,
                       mozilla::Maybe<ScalarVariant>& aOutput)
{
  switch (aScalarKind) {
    case nsITelemetry::SCALAR_TYPE_COUNT:
      {
        uint32_t val = 0;
        nsresult rv = aInput->GetAsUint32(&val);
        if (NS_FAILED(rv)) {
          return ScalarResult::CannotUnpackVariant;
        }
        aOutput = mozilla::Some(mozilla::AsVariant(val));
        break;
      }
    case nsITelemetry::SCALAR_TYPE_STRING:
      {
        nsString val;
        nsresult rv = aInput->GetAsAString(val);
        if (NS_FAILED(rv)) {
          return ScalarResult::CannotUnpackVariant;
        }
        aOutput = mozilla::Some(mozilla::AsVariant(val));
        break;
      }
    case nsITelemetry::SCALAR_TYPE_BOOLEAN:
      {
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

// Implements the methods for ScalarInfo.
const char *
ScalarInfo::name() const
{
  return &gScalarsStringTable[this->name_offset];
}

const char *
ScalarInfo::expiration() const
{
  return &gScalarsStringTable[this->expiration_offset];
}

/**
 * The base scalar object, that serves as a common ancestor for storage
 * purposes.
 */
class ScalarBase
{
public:
  virtual ~ScalarBase() = default;

  // Set, Add and SetMaximum functions as described in the Telemetry IDL.
  virtual ScalarResult SetValue(nsIVariant* aValue) = 0;
  virtual ScalarResult AddValue(nsIVariant* aValue) { return ScalarResult::OperationNotSupported; }
  virtual ScalarResult SetMaximum(nsIVariant* aValue) { return ScalarResult::OperationNotSupported; }

  // Convenience methods used by the C++ API.
  virtual void SetValue(uint32_t aValue) { mozilla::Unused << HandleUnsupported(); }
  virtual ScalarResult SetValue(const nsAString& aValue) { return HandleUnsupported(); }
  virtual void SetValue(bool aValue) { mozilla::Unused << HandleUnsupported(); }
  virtual void AddValue(uint32_t aValue) { mozilla::Unused << HandleUnsupported(); }
  virtual void SetMaximum(uint32_t aValue) { mozilla::Unused << HandleUnsupported(); }

  // GetValue is used to get the value of the scalar when persisting it to JS.
  virtual nsresult GetValue(nsCOMPtr<nsIVariant>& aResult) const = 0;

  // To measure the memory stats.
  virtual size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const = 0;

private:
  ScalarResult HandleUnsupported() const;
};

ScalarResult
ScalarBase::HandleUnsupported() const
{
  MOZ_ASSERT(false, "This operation is not support for this scalar type.");
  return ScalarResult::OperationNotSupported;
}

/**
 * The implementation for the unsigned int scalar type.
 */
class ScalarUnsigned : public ScalarBase
{
public:
  using ScalarBase::SetValue;

  ScalarUnsigned() : mStorage(0) {};
  ~ScalarUnsigned() override = default;

  ScalarResult SetValue(nsIVariant* aValue) final override;
  void SetValue(uint32_t aValue) final override;
  ScalarResult AddValue(nsIVariant* aValue) final override;
  void AddValue(uint32_t aValue) final override;
  ScalarResult SetMaximum(nsIVariant* aValue) final override;
  void SetMaximum(uint32_t aValue) final override;
  nsresult GetValue(nsCOMPtr<nsIVariant>& aResult) const final override;
  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const final override;

private:
  uint32_t mStorage;

  ScalarResult CheckInput(nsIVariant* aValue);

  // Prevent copying.
  ScalarUnsigned(const ScalarUnsigned& aOther) = delete;
  void operator=(const ScalarUnsigned& aOther) = delete;
};

ScalarResult
ScalarUnsigned::SetValue(nsIVariant* aValue)
{
  ScalarResult sr = CheckInput(aValue);
  if (sr == ScalarResult::UnsignedNegativeValue) {
    return sr;
  }

  if (NS_FAILED(aValue->GetAsUint32(&mStorage))) {
    return ScalarResult::InvalidValue;
  }
  return sr;
}

void
ScalarUnsigned::SetValue(uint32_t aValue)
{
  mStorage = aValue;
}

ScalarResult
ScalarUnsigned::AddValue(nsIVariant* aValue)
{
  ScalarResult sr = CheckInput(aValue);
  if (sr == ScalarResult::UnsignedNegativeValue) {
    return sr;
  }

  uint32_t newAddend = 0;
  nsresult rv = aValue->GetAsUint32(&newAddend);
  if (NS_FAILED(rv)) {
    return ScalarResult::InvalidValue;
  }
  mStorage += newAddend;
  return sr;
}

void
ScalarUnsigned::AddValue(uint32_t aValue)
{
  mStorage += aValue;
}

ScalarResult
ScalarUnsigned::SetMaximum(nsIVariant* aValue)
{
  ScalarResult sr = CheckInput(aValue);
  if (sr == ScalarResult::UnsignedNegativeValue) {
    return sr;
  }

  uint32_t newValue = 0;
  nsresult rv = aValue->GetAsUint32(&newValue);
  if (NS_FAILED(rv)) {
    return ScalarResult::InvalidValue;
  }
  if (newValue > mStorage) {
    mStorage = newValue;
  }
  return sr;
}

void
ScalarUnsigned::SetMaximum(uint32_t aValue)
{
  if (aValue > mStorage) {
    mStorage = aValue;
  }
}

nsresult
ScalarUnsigned::GetValue(nsCOMPtr<nsIVariant>& aResult) const
{
  nsCOMPtr<nsIWritableVariant> outVar(new nsVariant());
  nsresult rv = outVar->SetAsUint32(mStorage);
  if (NS_FAILED(rv)) {
    return rv;
  }
  aResult = outVar.forget();
  return NS_OK;
}

size_t
ScalarUnsigned::SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
{
  return aMallocSizeOf(this);
}

ScalarResult
ScalarUnsigned::CheckInput(nsIVariant* aValue)
{
  // If this is a floating point value/double, we will probably get truncated.
  uint16_t type;
  aValue->GetDataType(&type);
  if (type == nsIDataType::VTYPE_FLOAT ||
      type == nsIDataType::VTYPE_DOUBLE) {
    return ScalarResult::UnsignedTruncatedValue;
  }

  int32_t signedTest;
  // If we're able to cast the number to an int, check its sign.
  // Warn the user if he's trying to set the unsigned scalar to a negative
  // number.
  if (NS_SUCCEEDED(aValue->GetAsInt32(&signedTest)) &&
      signedTest < 0) {
    return ScalarResult::UnsignedNegativeValue;
  }
  return ScalarResult::Ok;
}

/**
 * The implementation for the string scalar type.
 */
class ScalarString : public ScalarBase
{
public:
  using ScalarBase::SetValue;

  ScalarString() : mStorage(EmptyString()) {};
  ~ScalarString() override = default;

  ScalarResult SetValue(nsIVariant* aValue) final override;
  ScalarResult SetValue(const nsAString& aValue) final override;
  nsresult GetValue(nsCOMPtr<nsIVariant>& aResult) const final override;
  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const final override;

private:
  nsString mStorage;

  // Prevent copying.
  ScalarString(const ScalarString& aOther) = delete;
  void operator=(const ScalarString& aOther) = delete;
};

ScalarResult
ScalarString::SetValue(nsIVariant* aValue)
{
  // Check that we got the correct data type.
  uint16_t type;
  aValue->GetDataType(&type);
  if (type != nsIDataType::VTYPE_CHAR &&
      type != nsIDataType::VTYPE_WCHAR &&
      type != nsIDataType::VTYPE_DOMSTRING &&
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

ScalarResult
ScalarString::SetValue(const nsAString& aValue)
{
  mStorage = Substring(aValue, 0, kMaximumStringValueLength);
  if (aValue.Length() > kMaximumStringValueLength) {
    return ScalarResult::StringTooLong;
  }
  return ScalarResult::Ok;
}

nsresult
ScalarString::GetValue(nsCOMPtr<nsIVariant>& aResult) const
{
  nsCOMPtr<nsIWritableVariant> outVar(new nsVariant());
  nsresult rv = outVar->SetAsAString(mStorage);
  if (NS_FAILED(rv)) {
    return rv;
  }
  aResult = outVar.forget();
  return NS_OK;
}

size_t
ScalarString::SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
{
  size_t n = aMallocSizeOf(this);
  n+= mStorage.SizeOfExcludingThisIfUnshared(aMallocSizeOf);
  return n;
}

/**
 * The implementation for the boolean scalar type.
 */
class ScalarBoolean : public ScalarBase
{
public:
  using ScalarBase::SetValue;

  ScalarBoolean() : mStorage(false) {};
  ~ScalarBoolean() override = default;

  ScalarResult SetValue(nsIVariant* aValue) final override;
  void SetValue(bool aValue) final override;
  nsresult GetValue(nsCOMPtr<nsIVariant>& aResult) const final override;
  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const final override;

private:
  bool mStorage;

  // Prevent copying.
  ScalarBoolean(const ScalarBoolean& aOther) = delete;
  void operator=(const ScalarBoolean& aOther) = delete;
};

ScalarResult
ScalarBoolean::SetValue(nsIVariant* aValue)
{
  // Check that we got the correct data type.
  uint16_t type;
  aValue->GetDataType(&type);
  if (type != nsIDataType::VTYPE_BOOL &&
      type != nsIDataType::VTYPE_INT8 &&
      type != nsIDataType::VTYPE_INT16 &&
      type != nsIDataType::VTYPE_INT32 &&
      type != nsIDataType::VTYPE_INT64 &&
      type != nsIDataType::VTYPE_UINT8 &&
      type != nsIDataType::VTYPE_UINT16 &&
      type != nsIDataType::VTYPE_UINT32 &&
      type != nsIDataType::VTYPE_UINT64) {
    return ScalarResult::InvalidType;
  }

  if (NS_FAILED(aValue->GetAsBool(&mStorage))) {
    return ScalarResult::InvalidValue;
  }
  return ScalarResult::Ok;
};

void
ScalarBoolean::SetValue(bool aValue)
{
  mStorage = aValue;
}

nsresult
ScalarBoolean::GetValue(nsCOMPtr<nsIVariant>& aResult) const
{
  nsCOMPtr<nsIWritableVariant> outVar(new nsVariant());
  nsresult rv = outVar->SetAsBool(mStorage);
  if (NS_FAILED(rv)) {
    return rv;
  }
  aResult = outVar.forget();
  return NS_OK;
}

size_t
ScalarBoolean::SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
{
  return aMallocSizeOf(this);
}

/**
 * Allocate a scalar class given the scalar info.
 *
 * @param aInfo The informations for the scalar coming from the definition file.
 * @return nullptr if the scalar type is unknown, otherwise a valid pointer to the
 *         scalar type.
 */
ScalarBase*
internal_ScalarAllocate(uint32_t aScalarKind)
{
  ScalarBase* scalar = nullptr;
  switch (aScalarKind) {
  case nsITelemetry::SCALAR_TYPE_COUNT:
    scalar = new ScalarUnsigned();
    break;
  case nsITelemetry::SCALAR_TYPE_STRING:
    scalar = new ScalarString();
    break;
  case nsITelemetry::SCALAR_TYPE_BOOLEAN:
    scalar = new ScalarBoolean();
    break;
  default:
    MOZ_ASSERT(false, "Invalid scalar type");
  }
  return scalar;
}

/**
 * The implementation for the keyed scalar type.
 */
class KeyedScalar
{
public:
  typedef mozilla::Pair<nsCString, nsCOMPtr<nsIVariant>> KeyValuePair;

  explicit KeyedScalar(uint32_t aScalarKind) : mScalarKind(aScalarKind) {};
  ~KeyedScalar() = default;

  // Set, Add and SetMaximum functions as described in the Telemetry IDL.
  // These methods implicitly instantiate a Scalar[*] for each key.
  ScalarResult SetValue(const nsAString& aKey, nsIVariant* aValue);
  ScalarResult AddValue(const nsAString& aKey, nsIVariant* aValue);
  ScalarResult SetMaximum(const nsAString& aKey, nsIVariant* aValue);

  // Convenience methods used by the C++ API.
  void SetValue(const nsAString& aKey, uint32_t aValue);
  void SetValue(const nsAString& aKey, bool aValue);
  void AddValue(const nsAString& aKey, uint32_t aValue);
  void SetMaximum(const nsAString& aKey, uint32_t aValue);

  // GetValue is used to get the key-value pairs stored in the keyed scalar
  // when persisting it to JS.
  nsresult GetValue(nsTArray<KeyValuePair>& aValues) const;

  // To measure the memory stats.
  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf);

private:
  typedef nsClassHashtable<nsCStringHashKey, ScalarBase> ScalarKeysMapType;

  ScalarKeysMapType mScalarKeys;
  const uint32_t mScalarKind;

  ScalarResult GetScalarForKey(const nsAString& aKey, ScalarBase** aRet);
};

ScalarResult
KeyedScalar::SetValue(const nsAString& aKey, nsIVariant* aValue)
{
  ScalarBase* scalar = nullptr;
  ScalarResult sr = GetScalarForKey(aKey, &scalar);
  if (sr != ScalarResult::Ok) {
    return sr;
  }

  return scalar->SetValue(aValue);
}

ScalarResult
KeyedScalar::AddValue(const nsAString& aKey, nsIVariant* aValue)
{
  ScalarBase* scalar = nullptr;
  ScalarResult sr = GetScalarForKey(aKey, &scalar);
  if (sr != ScalarResult::Ok) {
    return sr;
  }

  return scalar->AddValue(aValue);
}

ScalarResult
KeyedScalar::SetMaximum(const nsAString& aKey, nsIVariant* aValue)
{
  ScalarBase* scalar = nullptr;
  ScalarResult sr = GetScalarForKey(aKey, &scalar);
  if (sr != ScalarResult::Ok) {
    return sr;
  }

  return scalar->SetMaximum(aValue);
}

void
KeyedScalar::SetValue(const nsAString& aKey, uint32_t aValue)
{
  ScalarBase* scalar = nullptr;
  ScalarResult sr = GetScalarForKey(aKey, &scalar);
  if (sr != ScalarResult::Ok) {
    MOZ_ASSERT(false, "Key too long or too many keys are recorded in the scalar.");
    return;
  }

  return scalar->SetValue(aValue);
}

void
KeyedScalar::SetValue(const nsAString& aKey, bool aValue)
{
  ScalarBase* scalar = nullptr;
  ScalarResult sr = GetScalarForKey(aKey, &scalar);
  if (sr != ScalarResult::Ok) {
    MOZ_ASSERT(false, "Key too long or too many keys are recorded in the scalar.");
    return;
  }

  return scalar->SetValue(aValue);
}

void
KeyedScalar::AddValue(const nsAString& aKey, uint32_t aValue)
{
  ScalarBase* scalar = nullptr;
  ScalarResult sr = GetScalarForKey(aKey, &scalar);
  if (sr != ScalarResult::Ok) {
    MOZ_ASSERT(false, "Key too long or too many keys are recorded in the scalar.");
    return;
  }

  return scalar->AddValue(aValue);
}

void
KeyedScalar::SetMaximum(const nsAString& aKey, uint32_t aValue)
{
  ScalarBase* scalar = nullptr;
  ScalarResult sr = GetScalarForKey(aKey, &scalar);
  if (sr != ScalarResult::Ok) {
    MOZ_ASSERT(false, "Key too long or too many keys are recorded in the scalar.");
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
nsresult
KeyedScalar::GetValue(nsTArray<KeyValuePair>& aValues) const
{
  for (auto iter = mScalarKeys.ConstIter(); !iter.Done(); iter.Next()) {
    ScalarBase* scalar = static_cast<ScalarBase*>(iter.Data());

    // Get the scalar value.
    nsCOMPtr<nsIVariant> scalarValue;
    nsresult rv = scalar->GetValue(scalarValue);
    if (NS_FAILED(rv)) {
      return rv;
    }

    // Append it to value list.
    aValues.AppendElement(mozilla::MakePair(nsCString(iter.Key()), scalarValue));
  }

  return NS_OK;
}

/**
 * Get the scalar for the referenced key.
 * If there's no such key, instantiate a new Scalar object with the
 * same type of the Keyed scalar and create the key.
 */
ScalarResult
KeyedScalar::GetScalarForKey(const nsAString& aKey, ScalarBase** aRet)
{
  if (aKey.IsEmpty()) {
    return ScalarResult::KeyIsEmpty;
  }

  if (aKey.Length() >= kMaximumKeyStringLength) {
    return ScalarResult::KeyTooLong;
  }

  if (mScalarKeys.Count() >= kMaximumNumberOfKeys) {
    return ScalarResult::TooManyKeys;
  }

  NS_ConvertUTF16toUTF8 utf8Key(aKey);

  ScalarBase* scalar = nullptr;
  if (mScalarKeys.Get(utf8Key, &scalar)) {
    *aRet = scalar;
    return ScalarResult::Ok;
  }

  scalar = internal_ScalarAllocate(mScalarKind);
  if (!scalar) {
    return ScalarResult::InvalidType;
  }

  mScalarKeys.Put(utf8Key, scalar);

  *aRet = scalar;
  return ScalarResult::Ok;
}

size_t
KeyedScalar::SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf)
{
  size_t n = aMallocSizeOf(this);
  for (auto iter = mScalarKeys.Iter(); !iter.Done(); iter.Next()) {
    ScalarBase* scalar = static_cast<ScalarBase*>(iter.Data());
    n += scalar->SizeOfIncludingThis(aMallocSizeOf);
  }
  return n;
}

typedef nsUint32HashKey ScalarIDHashKey;
typedef nsUint32HashKey ProcessIDHashKey;
typedef nsClassHashtable<ScalarIDHashKey, ScalarBase> ScalarStorageMapType;
typedef nsClassHashtable<ScalarIDHashKey, KeyedScalar> KeyedScalarStorageMapType;
typedef nsClassHashtable<ProcessIDHashKey, ScalarStorageMapType> ProcessesScalarsMapType;
typedef nsClassHashtable<ProcessIDHashKey, KeyedScalarStorageMapType> ProcessesKeyedScalarsMapType;

} // namespace

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

// The (Process Id -> (Scalar ID -> Scalar Object)) map. This is a nsClassHashtable,
// it owns the scalar instances and takes care of deallocating them when they are
// removed from the map.
ProcessesScalarsMapType gScalarStorageMap;
// As above, for the keyed scalars.
ProcessesKeyedScalarsMapType gKeyedScalarStorageMap;

} // namespace

////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
//
// PRIVATE: Function that may call JS code.

// NOTE: the functions in this section all run without protection from
// |gTelemetryScalarsMutex|. If they held the mutex, there would be the
// possibility of deadlock because the JS_ calls that they make may call
// back into the TelemetryScalar interface, hence trying to re-acquire the mutex.
//
// This means that these functions potentially race against threads, but
// that seems preferable to risking deadlock.

namespace {

/**
 * Converts the error code to a human readable error message and prints it to the
 * browser console.
 *
 * @param aScalarName The name of the scalar that raised the error.
 * @param aSr The error code.
 */
void
internal_LogScalarError(const nsACString& aScalarName, ScalarResult aSr)
{
  nsAutoString errorMessage;
  AppendUTF8toUTF16(aScalarName, errorMessage);

  switch (aSr) {
    case ScalarResult::NotInitialized:
      errorMessage.AppendLiteral(u" - Telemetry was not yet initialized.");
      break;
    case ScalarResult::CannotUnpackVariant:
      errorMessage.AppendLiteral(u" - Cannot convert the provided JS value to nsIVariant.");
      break;
    case ScalarResult::CannotRecordInProcess:
      errorMessage.AppendLiteral(u" - Cannot record the scalar in the current process.");
      break;
    case ScalarResult::KeyedTypeMismatch:
      errorMessage.AppendLiteral(u" - Attempting to manage a keyed scalar as a scalar (or vice-versa).");
      break;
    case ScalarResult::UnknownScalar:
      errorMessage.AppendLiteral(u" - Unknown scalar.");
      break;
    case ScalarResult::OperationNotSupported:
      errorMessage.AppendLiteral(u" - The requested operation is not supported on this scalar.");
      break;
    case ScalarResult::InvalidType:
      errorMessage.AppendLiteral(u" - Attempted to set the scalar to an invalid data type.");
      break;
    case ScalarResult::InvalidValue:
      errorMessage.AppendLiteral(u" - Attempted to set the scalar to an incompatible value.");
      break;
    case ScalarResult::StringTooLong:
      errorMessage.AppendLiteral(u" - Truncating scalar value to 50 characters.");
      break;
    case ScalarResult::KeyIsEmpty:
      errorMessage.AppendLiteral(u" - The key must not be empty.");
      break;
    case ScalarResult::KeyTooLong:
      errorMessage.AppendLiteral(u" - The key length must be limited to 70 characters.");
      break;
    case ScalarResult::TooManyKeys:
      errorMessage.AppendLiteral(u" - Keyed scalars cannot have more than 100 keys.");
      break;
    case ScalarResult::UnsignedNegativeValue:
      errorMessage.AppendLiteral(u" - Trying to set an unsigned scalar to a negative number.");
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

} // namespace

////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
//
// PRIVATE: helpers for the external interface

namespace {

bool
internal_CanRecordBase(const StaticMutexAutoLock& lock)
{
  return gCanRecordBase;
}

bool
internal_CanRecordExtended(const StaticMutexAutoLock& lock)
{
  return gCanRecordExtended;
}

/**
 * Check if the given scalar is a keyed scalar.
 *
 * @param lock Instance of a lock locking gTelemetryHistogramMutex
 * @param aId The scalar identifier.
 * @return true if aId refers to a keyed scalar, false otherwise.
 */
bool
internal_IsKeyedScalar(const StaticMutexAutoLock& lock, const ScalarKey& aId)
{
  return internal_GetScalarInfo(lock, aId).keyed;
}

/**
 * Check if we're allowed to record the given scalar in the current
 * process.
 *
 * @param lock Instance of a lock locking gTelemetryHistogramMutex
 * @param aId The scalar identifier.
 * @return true if the scalar is allowed to be recorded in the current process, false
 *         otherwise.
 */
bool
internal_CanRecordProcess(const StaticMutexAutoLock& lock,
                          const ScalarKey& aId)
{
  const BaseScalarInfo &info = internal_GetScalarInfo(lock, aId);
  return CanRecordInProcess(info.record_in_processes, XRE_GetProcessType());
}

bool
internal_CanRecordForScalarID(const StaticMutexAutoLock& lock,
                              const ScalarKey& aId)
{
  // Get the scalar info from the id.
  const BaseScalarInfo &info = internal_GetScalarInfo(lock, aId);

  // Can we record at all?
  bool canRecordBase = internal_CanRecordBase(lock);
  if (!canRecordBase) {
    return false;
  }

  bool canRecordDataset = CanRecordDataset(info.dataset,
                                           canRecordBase,
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
 * @return ScalarResult::Ok if we can record, an error code otherwise.
 */
ScalarResult
internal_CanRecordScalar(const StaticMutexAutoLock& lock, const ScalarKey& aId,
                         bool aKeyed)
{
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
  if (!internal_CanRecordProcess(lock, aId)) {
    return ScalarResult::CannotRecordInProcess;
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
nsresult
internal_GetEnumByScalarName(const StaticMutexAutoLock& lock,
                             const nsACString& aName,
                             ScalarKey* aId)
{
  if (!gInitDone) {
    return NS_ERROR_FAILURE;
  }

  CharPtrEntryType *entry = gScalarNameIDMap.GetEntry(PromiseFlatCString(aName).get());
  if (!entry) {
    return NS_ERROR_INVALID_ARG;
  }
  *aId = entry->mData;
  return NS_OK;
}

/**
 * Get a scalar object by its enum id. This implicitly allocates the scalar
 * object in the storage if it wasn't previously allocated.
 *
 * @param lock Instance of a lock locking gTelemetryHistogramMutex
 * @param aId The scalar identifier.
 * @param aProcessStorage This drives the selection of the map to use to store
 *        the scalar data coming from child processes. This is only meaningful when
 *        this function is called in parent process. If that's the case, if
 *        this is not |GeckoProcessType_Default|, the process id is used to
 *        allocate and store the scalars.
 * @param aRes The output variable that stores scalar object.
 * @return
 *   NS_ERROR_INVALID_ARG if the scalar id is unknown.
 *   NS_ERROR_NOT_AVAILABLE if the scalar is expired.
 *   NS_OK if the scalar was found. If that's the case, aResult contains a
 *   valid pointer to a scalar type.
 */
nsresult
internal_GetScalarByEnum(const StaticMutexAutoLock& lock,
                         const ScalarKey& aId,
                         ProcessID aProcessStorage,
                         ScalarBase** aRet)
{
  if (!internal_IsValidId(lock, aId)) {
    MOZ_ASSERT(false, "Requested a scalar with an invalid id.");
    return NS_ERROR_INVALID_ARG;
  }

  const BaseScalarInfo &info = internal_GetScalarInfo(lock, aId);

  // Dynamic scalars fixup: they are always stored in the "dynamic" process.
  if (aId.dynamic) {
    aProcessStorage = ProcessID::Dynamic;
  }

  ScalarBase* scalar = nullptr;
  ScalarStorageMapType* scalarStorage = nullptr;
  // Initialize the scalar storage to the parent storage. This will get
  // set to the child storage if needed.
  uint32_t storageId = static_cast<uint32_t>(aProcessStorage);

  // Get the process-specific storage or create one if it's not
  // available.
  if (!gScalarStorageMap.Get(storageId, &scalarStorage)) {
    scalarStorage = new ScalarStorageMapType();
    gScalarStorageMap.Put(storageId, scalarStorage);
  }

  // Check if the scalar is already allocated in the parent or in the child storage.
  if (scalarStorage->Get(aId.id, &scalar)) {
    // Dynamic scalars can expire at any time during the session (e.g. an
    // add-on was updated). Check if it expired.
    if (aId.dynamic) {
      const DynamicScalarInfo& dynInfo = static_cast<const DynamicScalarInfo&>(info);
      if (dynInfo.mDynamicExpiration) {
        // The Dynamic scalar is expired.
        return NS_ERROR_NOT_AVAILABLE;
      }
    }
    // This was not a dynamic scalar or was not expired.
    *aRet = scalar;
    return NS_OK;
  }

  // The scalar storage wasn't already allocated. Check if the scalar is expired and
  // then allocate the storage, if needed.
  if (IsExpiredVersion(info.expiration())) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  scalar = internal_ScalarAllocate(info.kind);
  if (!scalar) {
    return NS_ERROR_INVALID_ARG;
  }

  scalarStorage->Put(aId.id, scalar);
  *aRet = scalar;
  return NS_OK;
}

/**
 * Update the scalar with the provided value. This is used by the JS API.
 *
 * @param lock Instance of a lock locking gTelemetryHistogramMutex
 * @param aName The scalar name.
 * @param aType The action type for updating the scalar.
 * @param aValue The value to use for updating the scalar.
 * @return a ScalarResult error value.
 */
ScalarResult
internal_UpdateScalar(const StaticMutexAutoLock& lock, const nsACString& aName,
                      ScalarActionType aType, nsIVariant* aValue)
{
  ScalarKey uniqueId;
  nsresult rv = internal_GetEnumByScalarName(lock, aName, &uniqueId);
  if (NS_FAILED(rv)) {
    return (rv == NS_ERROR_FAILURE) ?
           ScalarResult::NotInitialized : ScalarResult::UnknownScalar;
  }

  ScalarResult sr = internal_CanRecordScalar(lock, uniqueId, false);
  if (sr != ScalarResult::Ok) {
    if (sr == ScalarResult::CannotRecordDataset) {
      return ScalarResult::Ok;
    }
    return sr;
  }

  // Accumulate in the child process if needed.
  if (!XRE_IsParentProcess()) {
    const BaseScalarInfo &info = internal_GetScalarInfo(lock, uniqueId);
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

  // Finally get the scalar.
  ScalarBase* scalar = nullptr;
  rv = internal_GetScalarByEnum(lock, uniqueId, ProcessID::Parent, &scalar);
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

} // namespace



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
 *        the scalar data coming from child processes. This is only meaningful when
 *        this function is called in parent process. If that's the case, if
 *        this is not |GeckoProcessType_Default|, the process id is used to
 *        allocate and store the scalars.
 * @param aRet The output variable that stores scalar object.
 * @return
 *   NS_ERROR_INVALID_ARG if the scalar id is unknown or a this is a keyed string
 *                        scalar.
 *   NS_ERROR_NOT_AVAILABLE if the scalar is expired.
 *   NS_OK if the scalar was found. If that's the case, aResult contains a
 *   valid pointer to a scalar type.
 */
nsresult
internal_GetKeyedScalarByEnum(const StaticMutexAutoLock& lock,
                              const ScalarKey& aId,
                              ProcessID aProcessStorage,
                              KeyedScalar** aRet)
{
  if (!internal_IsValidId(lock, aId)) {
    MOZ_ASSERT(false, "Requested a keyed scalar with an invalid id.");
    return NS_ERROR_INVALID_ARG;
  }

  const BaseScalarInfo &info = internal_GetScalarInfo(lock, aId);

  // Dynamic scalars fixup: they are always stored in the "dynamic" process.
  if (aId.dynamic) {
    aProcessStorage = ProcessID::Dynamic;
  }

  KeyedScalar* scalar = nullptr;
  KeyedScalarStorageMapType* scalarStorage = nullptr;
  // Initialize the scalar storage to the parent storage. This will get
  // set to the child storage if needed.
  uint32_t storageId = static_cast<uint32_t>(aProcessStorage);

  // Get the process-specific storage or create one if it's not
  // available.
  if (!gKeyedScalarStorageMap.Get(storageId, &scalarStorage)) {
    scalarStorage = new KeyedScalarStorageMapType();
    gKeyedScalarStorageMap.Put(storageId, scalarStorage);
  }

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

  scalar = new KeyedScalar(info.kind);
  if (!scalar) {
    return NS_ERROR_INVALID_ARG;
  }

  scalarStorage->Put(aId.id, scalar);
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
 * @return a ScalarResult error value.
 */
ScalarResult
internal_UpdateKeyedScalar(const StaticMutexAutoLock& lock,
                           const nsACString& aName, const nsAString& aKey,
                           ScalarActionType aType, nsIVariant* aValue)
{
  ScalarKey uniqueId;
  nsresult rv = internal_GetEnumByScalarName(lock, aName, &uniqueId);
  if (NS_FAILED(rv)) {
    return (rv == NS_ERROR_FAILURE) ?
           ScalarResult::NotInitialized : ScalarResult::UnknownScalar;
  }

  ScalarResult sr = internal_CanRecordScalar(lock, uniqueId, true);
  if (sr != ScalarResult::Ok) {
    if (sr == ScalarResult::CannotRecordDataset) {
      return ScalarResult::Ok;
    }
    return sr;
  }

  // Accumulate in the child process if needed.
  if (!XRE_IsParentProcess()) {
    const BaseScalarInfo &info = internal_GetScalarInfo(lock, uniqueId);
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

  // Finally get the scalar.
  KeyedScalar* scalar = nullptr;
  rv = internal_GetKeyedScalarByEnum(lock, uniqueId, ProcessID::Parent,
                                     &scalar);
  if (NS_FAILED(rv)) {
    // Don't throw on expired scalars.
    if (rv == NS_ERROR_NOT_AVAILABLE) {
      return ScalarResult::Ok;
    }
    return ScalarResult::UnknownScalar;
  }

  if (aType == ScalarActionType::eAdd) {
    return scalar->AddValue(aKey, aValue);
  }
  if (aType == ScalarActionType::eSet) {
    return scalar->SetValue(aKey, aValue);
  }

  return scalar->SetMaximum(aKey, aValue);
}

/**
 * Helper function to convert an array of |DynamicScalarInfo|
 * to |DynamicScalarDefinition| used by the IPC calls.
 */
void
internal_DynamicScalarToIPC(const StaticMutexAutoLock& lock,
                            const nsTArray<DynamicScalarInfo>& aDynamicScalarInfos,
                            nsTArray<DynamicScalarDefinition>& aIPCDefs)
{
  for (auto info : aDynamicScalarInfos) {
    DynamicScalarDefinition stubDefinition;
    stubDefinition.type = info.kind;
    stubDefinition.dataset = info.dataset;
    stubDefinition.expired = info.mDynamicExpiration;
    stubDefinition.keyed = info.keyed;
    stubDefinition.name = info.mDynamicName;
    aIPCDefs.AppendElement(stubDefinition);
  }
}

/**
 * Broadcasts the dynamic scalar definitions to all the other
 * content processes.
 */
void
internal_BroadcastDefinitions(const StaticMutexAutoLock& lock,
                              const nsTArray<DynamicScalarInfo>& scalarInfos)
{
  nsTArray<mozilla::dom::ContentParent*> parents;
  mozilla::dom::ContentParent::GetAll(parents);
  if (!parents.Length()) {
     return;
  }

  // Convert the internal scalar representation to a stripped down IPC one.
  nsTArray<DynamicScalarDefinition> ipcDefinitions;
  internal_DynamicScalarToIPC(lock, scalarInfos, ipcDefinitions);

  // Broadcast the definitions to the other content processes.
  for (auto parent : parents) {
      mozilla::Unused << parent->SendAddDynamicScalars(ipcDefinitions);
  }
}

void
internal_RegisterScalars(const StaticMutexAutoLock& lock,
                         const nsTArray<DynamicScalarInfo>& scalarInfos)
{
  // Register the new scalars.
  if (!gDynamicScalarInfo) {
    gDynamicScalarInfo = new nsTArray<DynamicScalarInfo>();
  }

  for (auto scalarInfo : scalarInfos) {
    // Allow expiring scalars that were already registered.
    CharPtrEntryType *existingKey = gScalarNameIDMap.GetEntry(scalarInfo.name());
    if (existingKey) {
      // Change the scalar to expired if needed.
      if (scalarInfo.mDynamicExpiration) {
        DynamicScalarInfo& scalarData = (*gDynamicScalarInfo)[existingKey->mData.id];
        scalarData.mDynamicExpiration = true;
      }
      continue;
    }

    gDynamicScalarInfo->AppendElement(scalarInfo);
    uint32_t scalarId = gDynamicScalarInfo->Length() - 1;
    CharPtrEntryType *entry = gScalarNameIDMap.PutEntry(scalarInfo.name());
    entry->mData = ScalarKey{scalarId, true};
  }
}

} // namespace

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
static StaticMutex gTelemetryScalarsMutex;

void
TelemetryScalar::InitializeGlobalState(bool aCanRecordBase, bool aCanRecordExtended)
{
  StaticMutexAutoLock locker(gTelemetryScalarsMutex);
  MOZ_ASSERT(!gInitDone, "TelemetryScalar::InitializeGlobalState "
             "may only be called once");

  gCanRecordBase = aCanRecordBase;
  gCanRecordExtended = aCanRecordExtended;

  // Populate the static scalar name->id cache. Note that the scalar names are
  // statically allocated and come from the automatically generated TelemetryScalarData.h.
  uint32_t scalarCount = static_cast<uint32_t>(mozilla::Telemetry::ScalarID::ScalarCount);
  for (uint32_t i = 0; i < scalarCount; i++) {
    CharPtrEntryType *entry = gScalarNameIDMap.PutEntry(gScalars[i].name());
    entry->mData = ScalarKey{i, false};
  }

  gInitDone = true;
}

void
TelemetryScalar::DeInitializeGlobalState()
{
  StaticMutexAutoLock locker(gTelemetryScalarsMutex);
  gCanRecordBase = false;
  gCanRecordExtended = false;
  gScalarNameIDMap.Clear();
  gScalarStorageMap.Clear();
  gKeyedScalarStorageMap.Clear();
  gDynamicScalarInfo = nullptr;
  gInitDone = false;
}

void
TelemetryScalar::SetCanRecordBase(bool b)
{
  StaticMutexAutoLock locker(gTelemetryScalarsMutex);
  gCanRecordBase = b;
}

void
TelemetryScalar::SetCanRecordExtended(bool b) {
  StaticMutexAutoLock locker(gTelemetryScalarsMutex);
  gCanRecordExtended = b;
}

/**
 * Adds the value to the given scalar.
 *
 * @param aName The scalar name.
 * @param aVal The numeric value to add to the scalar.
 * @param aCx The JS context.
 * @return NS_OK (always) so that the JS API call doesn't throw. In case of errors,
 *         a warning level message is printed in the browser console.
 */
nsresult
TelemetryScalar::Add(const nsACString& aName, JS::HandleValue aVal, JSContext* aCx)
{
  // Unpack the aVal to nsIVariant. This uses the JS context.
  nsCOMPtr<nsIVariant> unpackedVal;
  nsresult rv =
    nsContentUtils::XPConnect()->JSToVariant(aCx, aVal,  getter_AddRefs(unpackedVal));
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
 * @return NS_OK (always) so that the JS API call doesn't throw. In case of errors,
 *         a warning level message is printed in the browser console.
 */
nsresult
TelemetryScalar::Add(const nsACString& aName, const nsAString& aKey, JS::HandleValue aVal,
                     JSContext* aCx)
{
  // Unpack the aVal to nsIVariant. This uses the JS context.
  nsCOMPtr<nsIVariant> unpackedVal;
  nsresult rv =
    nsContentUtils::XPConnect()->JSToVariant(aCx, aVal,  getter_AddRefs(unpackedVal));
  if (NS_FAILED(rv)) {
    internal_LogScalarError(aName, ScalarResult::CannotUnpackVariant);
    return NS_OK;
  }

  ScalarResult sr;
  {
    StaticMutexAutoLock locker(gTelemetryScalarsMutex);
    sr = internal_UpdateKeyedScalar(locker, aName, aKey,
                                    ScalarActionType::eAdd,
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
void
TelemetryScalar::Add(mozilla::Telemetry::ScalarID aId, uint32_t aValue)
{
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
    TelemetryIPCAccumulator::RecordChildScalarAction(uniqueId.id, uniqueId.dynamic,
                                                     ScalarActionType::eAdd,
                                                     ScalarVariant(aValue));
    return;
  }

  ScalarBase* scalar = nullptr;
  nsresult rv = internal_GetScalarByEnum(locker, uniqueId, ProcessID::Parent,
                                         &scalar);
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
void
TelemetryScalar::Add(mozilla::Telemetry::ScalarID aId, const nsAString& aKey,
                     uint32_t aValue)
{
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
    TelemetryIPCAccumulator::RecordChildKeyedScalarAction(uniqueId.id, uniqueId.dynamic,
      aKey, ScalarActionType::eAdd, ScalarVariant(aValue));
    return;
  }

  KeyedScalar* scalar = nullptr;
  nsresult rv = internal_GetKeyedScalarByEnum(locker, uniqueId,
                                              ProcessID::Parent,
                                              &scalar);
  if (NS_FAILED(rv)) {
    return;
  }

  scalar->AddValue(aKey, aValue);
}

/**
 * Sets the scalar to the given value.
 *
 * @param aName The scalar name.
 * @param aVal The value to set the scalar to.
 * @param aCx The JS context.
 * @return NS_OK (always) so that the JS API call doesn't throw. In case of errors,
 *         a warning level message is printed in the browser console.
 */
nsresult
TelemetryScalar::Set(const nsACString& aName, JS::HandleValue aVal, JSContext* aCx)
{
  // Unpack the aVal to nsIVariant. This uses the JS context.
  nsCOMPtr<nsIVariant> unpackedVal;
  nsresult rv =
    nsContentUtils::XPConnect()->JSToVariant(aCx, aVal,  getter_AddRefs(unpackedVal));
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
 * @return NS_OK (always) so that the JS API call doesn't throw. In case of errors,
 *         a warning level message is printed in the browser console.
 */
nsresult
TelemetryScalar::Set(const nsACString& aName, const nsAString& aKey, JS::HandleValue aVal,
                     JSContext* aCx)
{
  // Unpack the aVal to nsIVariant. This uses the JS context.
  nsCOMPtr<nsIVariant> unpackedVal;
  nsresult rv =
    nsContentUtils::XPConnect()->JSToVariant(aCx, aVal,  getter_AddRefs(unpackedVal));
  if (NS_FAILED(rv)) {
    internal_LogScalarError(aName, ScalarResult::CannotUnpackVariant);
    return NS_OK;
  }

  ScalarResult sr;
  {
    StaticMutexAutoLock locker(gTelemetryScalarsMutex);
    sr = internal_UpdateKeyedScalar(locker, aName, aKey,
                                    ScalarActionType::eSet,
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
void
TelemetryScalar::Set(mozilla::Telemetry::ScalarID aId, uint32_t aValue)
{
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
    TelemetryIPCAccumulator::RecordChildScalarAction(uniqueId.id, uniqueId.dynamic,
                                                     ScalarActionType::eSet,
                                                     ScalarVariant(aValue));
    return;
  }

  ScalarBase* scalar = nullptr;
  nsresult rv = internal_GetScalarByEnum(locker, uniqueId, ProcessID::Parent,
                                         &scalar);
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
void
TelemetryScalar::Set(mozilla::Telemetry::ScalarID aId, const nsAString& aValue)
{
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
    TelemetryIPCAccumulator::RecordChildScalarAction(uniqueId.id, uniqueId.dynamic,
                                                     ScalarActionType::eSet,
                                                     ScalarVariant(nsString(aValue)));
    return;
  }

  ScalarBase* scalar = nullptr;
  nsresult rv = internal_GetScalarByEnum(locker, uniqueId, ProcessID::Parent,
                                         &scalar);
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
void
TelemetryScalar::Set(mozilla::Telemetry::ScalarID aId, bool aValue)
{
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
    TelemetryIPCAccumulator::RecordChildScalarAction(uniqueId.id, uniqueId.dynamic,
                                                     ScalarActionType::eSet,
                                                     ScalarVariant(aValue));
    return;
  }

  ScalarBase* scalar = nullptr;
  nsresult rv = internal_GetScalarByEnum(locker, uniqueId, ProcessID::Parent,
                                         &scalar);
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
void
TelemetryScalar::Set(mozilla::Telemetry::ScalarID aId, const nsAString& aKey,
                     uint32_t aValue)
{
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
    TelemetryIPCAccumulator::RecordChildKeyedScalarAction(uniqueId.id, uniqueId.dynamic,
      aKey, ScalarActionType::eSet, ScalarVariant(aValue));
    return;
  }

  KeyedScalar* scalar = nullptr;
  nsresult rv = internal_GetKeyedScalarByEnum(locker, uniqueId,
                                              ProcessID::Parent,
                                              &scalar);
  if (NS_FAILED(rv)) {
    return;
  }

  scalar->SetValue(aKey, aValue);
}

/**
 * Sets the scalar to the given boolean value.
 *
 * @param aId The scalar enum id.
 * @param aKey The scalar key.
 * @param aValue The boolean value to set the scalar to.
 */
void
TelemetryScalar::Set(mozilla::Telemetry::ScalarID aId, const nsAString& aKey,
                     bool aValue)
{
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
    TelemetryIPCAccumulator::RecordChildKeyedScalarAction(uniqueId.id, uniqueId.dynamic,
      aKey, ScalarActionType::eSet, ScalarVariant(aValue));
    return;
  }

  KeyedScalar* scalar = nullptr;
  nsresult rv = internal_GetKeyedScalarByEnum(locker, uniqueId,
                                              ProcessID::Parent,
                                              &scalar);
  if (NS_FAILED(rv)) {
    return;
  }

  scalar->SetValue(aKey, aValue);
}

/**
 * Sets the scalar to the maximum of the current and the passed value.
 *
 * @param aName The scalar name.
 * @param aVal The numeric value to set the scalar to.
 * @param aCx The JS context.
 * @return NS_OK (always) so that the JS API call doesn't throw. In case of errors,
 *         a warning level message is printed in the browser console.
 */
nsresult
TelemetryScalar::SetMaximum(const nsACString& aName, JS::HandleValue aVal, JSContext* aCx)
{
  // Unpack the aVal to nsIVariant. This uses the JS context.
  nsCOMPtr<nsIVariant> unpackedVal;
  nsresult rv =
    nsContentUtils::XPConnect()->JSToVariant(aCx, aVal,  getter_AddRefs(unpackedVal));
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
 * @return NS_OK (always) so that the JS API call doesn't throw. In case of errors,
 *         a warning level message is printed in the browser console.
 */
nsresult
TelemetryScalar::SetMaximum(const nsACString& aName, const nsAString& aKey, JS::HandleValue aVal,
                            JSContext* aCx)
{
  // Unpack the aVal to nsIVariant. This uses the JS context.
  nsCOMPtr<nsIVariant> unpackedVal;
  nsresult rv =
    nsContentUtils::XPConnect()->JSToVariant(aCx, aVal,  getter_AddRefs(unpackedVal));
  if (NS_FAILED(rv)) {
    internal_LogScalarError(aName, ScalarResult::CannotUnpackVariant);
    return NS_OK;
  }

  ScalarResult sr;
  {
    StaticMutexAutoLock locker(gTelemetryScalarsMutex);
    sr = internal_UpdateKeyedScalar(locker, aName, aKey,
                                    ScalarActionType::eSetMaximum,
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
 * @param aId The scalar enum id.
 * @param aValue The numeric value to set the scalar to.
 */
void
TelemetryScalar::SetMaximum(mozilla::Telemetry::ScalarID aId, uint32_t aValue)
{
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
    TelemetryIPCAccumulator::RecordChildScalarAction(uniqueId.id, uniqueId.dynamic,
                                                     ScalarActionType::eSetMaximum,
                                                     ScalarVariant(aValue));
    return;
  }

  ScalarBase* scalar = nullptr;
  nsresult rv = internal_GetScalarByEnum(locker, uniqueId, ProcessID::Parent,
                                         &scalar);
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
void
TelemetryScalar::SetMaximum(mozilla::Telemetry::ScalarID aId, const nsAString& aKey,
                            uint32_t aValue)
{
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
    TelemetryIPCAccumulator::RecordChildKeyedScalarAction(uniqueId.id, uniqueId.dynamic,
      aKey, ScalarActionType::eSetMaximum, ScalarVariant(aValue));
    return;
  }

  KeyedScalar* scalar = nullptr;
  nsresult rv = internal_GetKeyedScalarByEnum(locker, uniqueId, ProcessID::Parent,
                                              &scalar);
  if (NS_FAILED(rv)) {
    return;
  }

  scalar->SetMaximum(aKey, aValue);
}

/**
 * Serializes the scalars from the given dataset to a json-style object and resets them.
 * The returned structure looks like:
 *    {"process": {"category1.probe":1,"category1.other_probe":false,...}, ... }.
 *
 * @param aDataset DATASET_RELEASE_CHANNEL_OPTOUT or DATASET_RELEASE_CHANNEL_OPTIN.
 * @param aClear Whether to clear out the scalars after snapshotting.
 */
nsresult
TelemetryScalar::CreateSnapshots(unsigned int aDataset, bool aClearScalars, JSContext* aCx,
                                 uint8_t optional_argc, JS::MutableHandle<JS::Value> aResult)
{
  MOZ_ASSERT(XRE_IsParentProcess(),
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

  // Only lock the mutex while accessing our data, without locking any JS related code.
  typedef mozilla::Pair<const char*, nsCOMPtr<nsIVariant>> DataPair;
  typedef nsTArray<DataPair> ScalarArray;
  nsDataHashtable<ProcessIDHashKey, ScalarArray> scalarsToReflect;
  {
    StaticMutexAutoLock locker(gTelemetryScalarsMutex);
    // Iterate the scalars in gScalarStorageMap. The storage may contain empty or yet to be
    // initialized scalars from all the supported processes.
    for (auto iter = gScalarStorageMap.Iter(); !iter.Done(); iter.Next()) {
      ScalarStorageMapType* scalarStorage = static_cast<ScalarStorageMapType*>(iter.Data());
      ScalarArray& processScalars = scalarsToReflect.GetOrInsert(iter.Key());

      // Are we in the "Dynamic" process?
      bool isDynamicProcess = ProcessID::Dynamic == static_cast<ProcessID>(iter.Key());

      // Iterate each available child storage.
      for (auto childIter = scalarStorage->Iter(); !childIter.Done(); childIter.Next()) {
        ScalarBase* scalar = static_cast<ScalarBase*>(childIter.Data());

        // Get the informations for this scalar.
        const BaseScalarInfo& info =
          internal_GetScalarInfo(locker, ScalarKey{childIter.Key(),
                                 isDynamicProcess});

        // Serialize the scalar if it's in the desired dataset.
        if (IsInDataset(info.dataset, aDataset)) {
          // Get the scalar value.
          nsCOMPtr<nsIVariant> scalarValue;
          nsresult rv = scalar->GetValue(scalarValue);
          if (NS_FAILED(rv)) {
            return rv;
          }
          // Append it to our list.
          processScalars.AppendElement(mozilla::MakePair(info.name(), scalarValue));
        }
      }
    }

    if (aClearScalars) {
      // The map already takes care of freeing the allocated memory.
      gScalarStorageMap.Clear();
    }
  }

  // Reflect it to JS.
  for (auto iter = scalarsToReflect.Iter(); !iter.Done(); iter.Next()) {
    ScalarArray& processScalars = iter.Data();
    const char* processName = GetNameForProcessID(ProcessID(iter.Key()));

    // Create the object that will hold the scalars for this process and add it
    // to the returned root object.
    JS::RootedObject processObj(aCx, JS_NewPlainObject(aCx));
    if (!processObj ||
        !JS_DefineProperty(aCx, root_obj, processName, processObj, JSPROP_ENUMERATE)) {
      return NS_ERROR_FAILURE;
    }

    for (nsTArray<DataPair>::size_type i = 0; i < processScalars.Length(); i++) {
      const DataPair& scalar = processScalars[i];

      // Convert it to a JS Val.
      JS::Rooted<JS::Value> scalarJsValue(aCx);
      nsresult rv =
        nsContentUtils::XPConnect()->VariantToJS(aCx, processObj, scalar.second(), &scalarJsValue);
      if (NS_FAILED(rv)) {
        return rv;
      }

      // Add it to the scalar object.
      if (!JS_DefineProperty(aCx, processObj, scalar.first(), scalarJsValue, JSPROP_ENUMERATE)) {
        return NS_ERROR_FAILURE;
      }
    }
  }

  return NS_OK;
}

/**
 * Serializes the scalars from the given dataset to a json-style object and resets them.
 * The returned structure looks like:
 *   { "process": { "category1.probe": { "key_1": 2, "key_2": 1, ... }, ... }, ... }
 *
 * @param aDataset DATASET_RELEASE_CHANNEL_OPTOUT or DATASET_RELEASE_CHANNEL_OPTIN.
 * @param aClear Whether to clear out the keyed scalars after snapshotting.
 */
nsresult
TelemetryScalar::CreateKeyedSnapshots(unsigned int aDataset, bool aClearScalars, JSContext* aCx,
                                      uint8_t optional_argc, JS::MutableHandle<JS::Value> aResult)
{
  MOZ_ASSERT(XRE_IsParentProcess(),
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

  // Only lock the mutex while accessing our data, without locking any JS related code.
  typedef mozilla::Pair<const char*, nsTArray<KeyedScalar::KeyValuePair>> DataPair;
  typedef nsTArray<DataPair> ScalarArray;
  nsDataHashtable<ProcessIDHashKey, ScalarArray> scalarsToReflect;
  {
    StaticMutexAutoLock locker(gTelemetryScalarsMutex);
    // Iterate the scalars in gKeyedScalarStorageMap. The storage may contain empty or yet
    // to be initialized scalars from all the supported processes.
    for (auto iter = gKeyedScalarStorageMap.Iter(); !iter.Done(); iter.Next()) {
      KeyedScalarStorageMapType* scalarStorage =
        static_cast<KeyedScalarStorageMapType*>(iter.Data());
      ScalarArray& processScalars = scalarsToReflect.GetOrInsert(iter.Key());

      // Are we in the "Dynamic" process?
      bool isDynamicProcess = ProcessID::Dynamic == static_cast<ProcessID>(iter.Key());

      for (auto childIter = scalarStorage->Iter(); !childIter.Done(); childIter.Next()) {
        KeyedScalar* scalar = static_cast<KeyedScalar*>(childIter.Data());

        // Get the informations for this scalar.
        const BaseScalarInfo& info =
          internal_GetScalarInfo(locker, ScalarKey{childIter.Key(),
                                 isDynamicProcess});

        // Serialize the scalar if it's in the desired dataset.
        if (IsInDataset(info.dataset, aDataset)) {
          // Get the keys for this scalar.
          nsTArray<KeyedScalar::KeyValuePair> scalarKeyedData;
          nsresult rv = scalar->GetValue(scalarKeyedData);
          if (NS_FAILED(rv)) {
            return rv;
          }
          // Append it to our list.
          processScalars.AppendElement(mozilla::MakePair(info.name(), scalarKeyedData));
        }
      }
    }

    if (aClearScalars) {
      // The map already takes care of freeing the allocated memory.
      gKeyedScalarStorageMap.Clear();
    }
  }

  // Reflect it to JS.
  for (auto iter = scalarsToReflect.Iter(); !iter.Done(); iter.Next()) {
    ScalarArray& processScalars = iter.Data();
    const char* processName = GetNameForProcessID(ProcessID(iter.Key()));

    // Create the object that will hold the scalars for this process and add it
    // to the returned root object.
    JS::RootedObject processObj(aCx, JS_NewPlainObject(aCx));
    if (!processObj ||
        !JS_DefineProperty(aCx, root_obj, processName, processObj, JSPROP_ENUMERATE)) {
      return NS_ERROR_FAILURE;
    }

    for (nsTArray<DataPair>::size_type i = 0; i < processScalars.Length(); i++) {
      const DataPair& keyedScalarData = processScalars[i];

      // Go through each keyed scalar and create a keyed scalar object.
      // This object will hold the values for all the keyed scalar keys.
      JS::RootedObject keyedScalarObj(aCx, JS_NewPlainObject(aCx));

      // Define a property for each scalar key, then add it to the keyed scalar
      // object.
      const nsTArray<KeyedScalar::KeyValuePair>& keyProps = keyedScalarData.second();
      for (uint32_t i = 0; i < keyProps.Length(); i++) {
        const KeyedScalar::KeyValuePair& keyData = keyProps[i];

        // Convert the value for the key to a JSValue.
        JS::Rooted<JS::Value> keyJsValue(aCx);
        nsresult rv =
          nsContentUtils::XPConnect()->VariantToJS(aCx, keyedScalarObj, keyData.second(), &keyJsValue);
        if (NS_FAILED(rv)) {
          return rv;
        }

        // Add the key to the scalar representation.
        const NS_ConvertUTF8toUTF16 key(keyData.first());
        if (!JS_DefineUCProperty(aCx, keyedScalarObj, key.Data(), key.Length(), keyJsValue, JSPROP_ENUMERATE)) {
          return NS_ERROR_FAILURE;
        }
      }

      // Add the scalar to the root object.
      if (!JS_DefineProperty(aCx, processObj, keyedScalarData.first(), keyedScalarObj, JSPROP_ENUMERATE)) {
        return NS_ERROR_FAILURE;
      }
    }
  }

  return NS_OK;
}

nsresult
TelemetryScalar::RegisterScalars(const nsACString& aCategoryName,
                                 JS::Handle<JS::Value> aScalarData,
                                 JSContext* cx)
{
  MOZ_ASSERT(XRE_IsParentProcess(),
             "Dynamic scalars should only be created in the parent process.");

  if (!IsValidIdentifierString(aCategoryName, kMaximumCategoryNameLength, true, false)) {
    JS_ReportErrorASCII(cx, "Invalid category name %s.",
                        PromiseFlatCString(aCategoryName).get());
    return NS_ERROR_INVALID_ARG;
  }

  if (!aScalarData.isObject()) {
    JS_ReportErrorASCII(cx, "Scalar data parameter should be an object");
    return NS_ERROR_INVALID_ARG;
  }

  JS::RootedObject obj(cx, &aScalarData.toObject());
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

    if (!IsValidIdentifierString(NS_ConvertUTF16toUTF8(scalarName), kMaximumScalarNameLength,
                                 false, true)) {
      JS_ReportErrorASCII(cx, "Invalid scalar name %s.",
                          PromiseFlatCString(NS_ConvertUTF16toUTF8(scalarName)).get());
      return NS_ERROR_INVALID_ARG;
    }

    // Join the category and the probe names.
    nsPrintfCString fullName("%s.%s",
                             PromiseFlatCString(aCategoryName).get(),
                             NS_ConvertUTF16toUTF8(scalarName).get());

    JS::RootedValue value(cx);
    if (!JS_GetPropertyById(cx, obj, scalarPropertyIds[i], &value) || !value.isObject()) {
      return NS_ERROR_FAILURE;
    }
    JS::RootedObject scalarDef(cx, &value.toObject());

    // Get the scalar's kind.
    if (!JS_GetProperty(cx, scalarDef, "kind", &value)
      || !value.isInt32()) {
      JS_ReportErrorASCII(cx, "Invalid or missing 'kind' for scalar %s.",
                          PromiseFlatCString(fullName).get());
      return NS_ERROR_FAILURE;
    }
    uint32_t kind = static_cast<uint32_t>(value.toInt32());

    // Get the optional scalar's recording policy (default to false).
    bool hasProperty = false;
    bool recordOnRelease = false;
    if (JS_HasProperty(cx, scalarDef, "record_on_release", &hasProperty) && hasProperty) {
      if (!JS_GetProperty(cx, scalarDef, "record_on_release", &value) || !value.isBoolean()) {
        JS_ReportErrorASCII(cx, "Invalid 'record_on_release' for scalar %s.",
                            PromiseFlatCString(fullName).get());
        return NS_ERROR_FAILURE;
      }
      recordOnRelease = static_cast<bool>(value.toBoolean());
    }

    // Get the optional scalar's keyed (default to false).
    bool keyed = false;
    if (JS_HasProperty(cx, scalarDef, "keyed", &hasProperty) && hasProperty) {
      if (!JS_GetProperty(cx, scalarDef, "keyed", &value) || !value.isBoolean()) {
        JS_ReportErrorASCII(cx, "Invalid 'keyed' for scalar %s.",
                            PromiseFlatCString(fullName).get());
        return NS_ERROR_FAILURE;
      }
      keyed = static_cast<bool>(value.toBoolean());
    }


    // Get the optional scalar's expired state (default to false).
    bool expired = false;
    if (JS_HasProperty(cx, scalarDef, "expired", &hasProperty) && hasProperty) {
      if (!JS_GetProperty(cx, scalarDef, "expired", &value) || !value.isBoolean()) {
        JS_ReportErrorASCII(cx, "Invalid 'expired' for scalar %s.",
                            PromiseFlatCString(fullName).get());
        return NS_ERROR_FAILURE;
      }
      expired = static_cast<bool>(value.toBoolean());
    }

    // We defer the actual registration here in case any other event description is invalid.
    // In that case we don't need to roll back any partial registration.
    newScalarInfos.AppendElement(DynamicScalarInfo{
      kind, recordOnRelease, expired, fullName, keyed
    });
  }

  // Register the dynamic definition on the parent process.
  {
    StaticMutexAutoLock locker(gTelemetryScalarsMutex);
    ::internal_RegisterScalars(locker, newScalarInfos);

    // Propagate the registration to all the content-processes. Please note that
    // this does not require to hold the mutex.
    ::internal_BroadcastDefinitions(locker, newScalarInfos);
  }
  return NS_OK;
}

/**
 * Resets all the stored scalars. This is intended to be only used in tests.
 */
void
TelemetryScalar::ClearScalars()
{
  MOZ_ASSERT(XRE_IsParentProcess(), "Scalars should only be cleared in the parent process.");
  if (!XRE_IsParentProcess()) {
    return;
  }

  StaticMutexAutoLock locker(gTelemetryScalarsMutex);
  gScalarStorageMap.Clear();
  gKeyedScalarStorageMap.Clear();
}

size_t
TelemetryScalar::GetMapShallowSizesOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf)
{
  StaticMutexAutoLock locker(gTelemetryScalarsMutex);
  return gScalarNameIDMap.ShallowSizeOfExcludingThis(aMallocSizeOf);
}

size_t
TelemetryScalar::GetScalarSizesOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf)
{
  StaticMutexAutoLock locker(gTelemetryScalarsMutex);
  size_t n = 0;
  // Account for scalar data coming from parent and child processes.
  for (auto iter = gScalarStorageMap.Iter(); !iter.Done(); iter.Next()) {
    ScalarStorageMapType* scalarStorage = static_cast<ScalarStorageMapType*>(iter.Data());
    for (auto childIter = scalarStorage->Iter(); !childIter.Done(); childIter.Next()) {
      ScalarBase* scalar = static_cast<ScalarBase*>(childIter.Data());
      n += scalar->SizeOfIncludingThis(aMallocSizeOf);
    }
  }
  // Also account for keyed scalar data coming from parent and child processes.
  for (auto iter = gKeyedScalarStorageMap.Iter(); !iter.Done(); iter.Next()) {
    KeyedScalarStorageMapType* scalarStorage =
      static_cast<KeyedScalarStorageMapType*>(iter.Data());
    for (auto childIter = scalarStorage->Iter(); !childIter.Done(); childIter.Next()) {
      KeyedScalar* scalar = static_cast<KeyedScalar*>(childIter.Data());
      n += scalar->SizeOfIncludingThis(aMallocSizeOf);
    }
  }
  return n;
}

void
TelemetryScalar::UpdateChildData(ProcessID aProcessType,
                                 const nsTArray<mozilla::Telemetry::ScalarAction>& aScalarActions)
{
  MOZ_ASSERT(XRE_IsParentProcess(),
             "The stored child processes scalar data must be updated from the parent process.");
  StaticMutexAutoLock locker(gTelemetryScalarsMutex);
  if (!internal_CanRecordBase(locker)) {
    return;
  }

  for (auto& upd : aScalarActions) {
    ScalarKey uniqueId{upd.mId, upd.mDynamic};
    if (NS_WARN_IF(!internal_IsValidId(locker, uniqueId))) {
      MOZ_ASSERT_UNREACHABLE("Scalar usage requires valid ids.");
      continue;
    }

    if (internal_IsKeyedScalar(locker, uniqueId)) {
      continue;
    }

    // Are we allowed to record this scalar? We don't need to check for
    // allowed processes here, that's taken care of when recording
    // in child processes.
    if (!internal_CanRecordForScalarID(locker, uniqueId)) {
      continue;
    }

    // Refresh the data in the parent process with the data coming from the child
    // processes.
    ScalarBase* scalar = nullptr;
    nsresult rv = internal_GetScalarByEnum(locker, uniqueId, aProcessType,
                                           &scalar);
    if (NS_FAILED(rv)) {
      NS_WARNING("NS_FAILED internal_GetScalarByEnum for CHILD");
      continue;
    }

    if (upd.mData.isNothing()) {
      MOZ_ASSERT(false, "There is no data in the ScalarActionType.");
      continue;
    }

    // Get the type of this scalar from the scalar ID. We already checked
    // for its validity a few lines above.
    const uint32_t scalarType = internal_GetScalarInfo(locker, uniqueId).kind;

    // Extract the data from the mozilla::Variant.
    switch (upd.mActionType)
    {
      case ScalarActionType::eSet:
        {
          switch (scalarType)
          {
            case nsITelemetry::SCALAR_TYPE_COUNT:
              scalar->SetValue(upd.mData->as<uint32_t>());
              break;
            case nsITelemetry::SCALAR_TYPE_BOOLEAN:
              scalar->SetValue(upd.mData->as<bool>());
              break;
            case nsITelemetry::SCALAR_TYPE_STRING:
              scalar->SetValue(upd.mData->as<nsString>());
              break;
          }
          break;
        }
      case ScalarActionType::eAdd:
        {
          if (scalarType != nsITelemetry::SCALAR_TYPE_COUNT) {
            NS_WARNING("Attempting to add on a non count scalar.");
            continue;
          }
          // We only support adding uint32_t.
          scalar->AddValue(upd.mData->as<uint32_t>());
          break;
        }
      case ScalarActionType::eSetMaximum:
        {
          if (scalarType != nsITelemetry::SCALAR_TYPE_COUNT) {
            NS_WARNING("Attempting to add on a non count scalar.");
            continue;
          }
          // We only support SetMaximum on uint32_t.
          scalar->SetMaximum(upd.mData->as<uint32_t>());
          break;
        }
      default:
        NS_WARNING("Unsupported action coming from scalar child updates.");
    }
  }
}

void
TelemetryScalar::UpdateChildKeyedData(ProcessID aProcessType,
                                      const nsTArray<mozilla::Telemetry::KeyedScalarAction>& aScalarActions)
{
  MOZ_ASSERT(XRE_IsParentProcess(),
             "The stored child processes keyed scalar data must be updated from the parent process.");
  StaticMutexAutoLock locker(gTelemetryScalarsMutex);
  if (!internal_CanRecordBase(locker)) {
    return;
  }

  for (auto& upd : aScalarActions) {
    ScalarKey uniqueId{upd.mId, upd.mDynamic};
    if (NS_WARN_IF(!internal_IsValidId(locker, uniqueId))) {
      MOZ_ASSERT_UNREACHABLE("Scalar usage requires valid ids.");
      continue;
    }

    if (!internal_IsKeyedScalar(locker, uniqueId)) {
      continue;
    }

    // Are we allowed to record this scalar? We don't need to check for
    // allowed processes here, that's taken care of when recording
    // in child processes.
    if (!internal_CanRecordForScalarID(locker, uniqueId)) {
      continue;
    }

    // Refresh the data in the parent process with the data coming from the child
    // processes.
    KeyedScalar* scalar = nullptr;
    nsresult rv = internal_GetKeyedScalarByEnum(locker, uniqueId, aProcessType,
                                                &scalar);
    if (NS_FAILED(rv)) {
      NS_WARNING("NS_FAILED internal_GetScalarByEnum for CHILD");
      continue;
    }

    if (upd.mData.isNothing()) {
      MOZ_ASSERT(false, "There is no data in the KeyedScalarAction.");
      continue;
    }

    // Get the type of this scalar from the scalar ID. We already checked
    // for its validity a few lines above.
    const uint32_t scalarType = internal_GetScalarInfo(locker, uniqueId).kind;

    // Extract the data from the mozilla::Variant.
    switch (upd.mActionType)
    {
      case ScalarActionType::eSet:
        {
          switch (scalarType)
          {
            case nsITelemetry::SCALAR_TYPE_COUNT:
              scalar->SetValue(NS_ConvertUTF8toUTF16(upd.mKey), upd.mData->as<uint32_t>());
              break;
            case nsITelemetry::SCALAR_TYPE_BOOLEAN:
              scalar->SetValue(NS_ConvertUTF8toUTF16(upd.mKey), upd.mData->as<bool>());
              break;
            default:
              NS_WARNING("Unsupported type coming from scalar child updates.");
          }
          break;
        }
      case ScalarActionType::eAdd:
        {
          if (scalarType != nsITelemetry::SCALAR_TYPE_COUNT) {
            NS_WARNING("Attempting to add on a non count scalar.");
            continue;
          }
          // We only support adding on uint32_t.
          scalar->AddValue(NS_ConvertUTF8toUTF16(upd.mKey), upd.mData->as<uint32_t>());
          break;
        }
      case ScalarActionType::eSetMaximum:
        {
          if (scalarType != nsITelemetry::SCALAR_TYPE_COUNT) {
            NS_WARNING("Attempting to add on a non count scalar.");
            continue;
          }
          // We only support SetMaximum on uint32_t.
          scalar->SetMaximum(NS_ConvertUTF8toUTF16(upd.mKey), upd.mData->as<uint32_t>());
          break;
        }
      default:
        NS_WARNING("Unsupported action coming from keyed scalar child updates.");
    }
  }
}

void
TelemetryScalar::RecordDiscardedData(ProcessID aProcessType,
                                     const mozilla::Telemetry::DiscardedData& aDiscardedData)
{
  MOZ_ASSERT(XRE_IsParentProcess(),
             "Discarded Data must be updated from the parent process.");
  StaticMutexAutoLock locker(gTelemetryScalarsMutex);
  if (!internal_CanRecordBase(locker)) {
    return;
  }

  ScalarBase* scalar = nullptr;
  mozilla::DebugOnly<nsresult> rv;

  rv = internal_GetScalarByEnum(locker,
    ScalarKey{static_cast<uint32_t>(ScalarID::TELEMETRY_DISCARDED_ACCUMULATIONS), false},
    aProcessType, &scalar);
  MOZ_ASSERT(NS_SUCCEEDED(rv));
  scalar->AddValue(aDiscardedData.mDiscardedHistogramAccumulations);

  rv = internal_GetScalarByEnum(locker,
    ScalarKey{static_cast<uint32_t>(ScalarID::TELEMETRY_DISCARDED_KEYED_ACCUMULATIONS), false},
    aProcessType, &scalar);
  MOZ_ASSERT(NS_SUCCEEDED(rv));
  scalar->AddValue(aDiscardedData.mDiscardedKeyedHistogramAccumulations);

  rv = internal_GetScalarByEnum(locker,
    ScalarKey{static_cast<uint32_t>(ScalarID::TELEMETRY_DISCARDED_SCALAR_ACTIONS), false},
    aProcessType, &scalar);
  MOZ_ASSERT(NS_SUCCEEDED(rv));
  scalar->AddValue(aDiscardedData.mDiscardedScalarActions);

  rv = internal_GetScalarByEnum(locker,
    ScalarKey{static_cast<uint32_t>(ScalarID::TELEMETRY_DISCARDED_KEYED_SCALAR_ACTIONS), false},
    aProcessType, &scalar);
  MOZ_ASSERT(NS_SUCCEEDED(rv));
  scalar->AddValue(aDiscardedData.mDiscardedKeyedScalarActions);

  rv = internal_GetScalarByEnum(locker,
    ScalarKey{static_cast<uint32_t>(ScalarID::TELEMETRY_DISCARDED_CHILD_EVENTS), false},
    aProcessType, &scalar);
  MOZ_ASSERT(NS_SUCCEEDED(rv));
  scalar->AddValue(aDiscardedData.mDiscardedChildEvents);
}

/**
 * Get the dynamic scalar definitions in an IPC-friendly
 * structure.
 */
void
TelemetryScalar::GetDynamicScalarDefinitions(
  nsTArray<DynamicScalarDefinition> &aDefArray)
{
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
void
TelemetryScalar::AddDynamicScalarDefinitions(
  const nsTArray<DynamicScalarDefinition>& aDefs)
{
  MOZ_ASSERT(!XRE_IsParentProcess());

  nsTArray<DynamicScalarInfo> dynamicStubs;

  // Populate the definitions array before acquiring the lock.
  for (auto def : aDefs) {
    bool recordOnRelease = def.dataset == nsITelemetry::DATASET_RELEASE_CHANNEL_OPTOUT;
    dynamicStubs.AppendElement(DynamicScalarInfo{
      def.type,
      recordOnRelease,
      def.expired,
      def.name,
      def.keyed});
  }

  {
    StaticMutexAutoLock locker(gTelemetryScalarsMutex);
    internal_RegisterScalars(locker, dynamicStubs);
  }
}
