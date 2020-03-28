/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsVariant_h
#define nsVariant_h

#include "nsIVariant.h"
#include "nsStringFwd.h"
#include "mozilla/Attributes.h"
#include "nsCycleCollectionParticipant.h"

/**
 * Map the nsAUTF8String, nsUTF8String classes to the nsACString and
 * nsCString classes respectively for now.  These defines need to be removed
 * once Jag lands his nsUTF8String implementation.
 */
#define nsAUTF8String nsACString
#define nsUTF8String nsCString
#define PromiseFlatUTF8String PromiseFlatCString

/**
 * nsDiscriminatedUnion is a class that nsIVariant implementors can use
 * to hold the underlying data.
 */

class nsDiscriminatedUnion {
 public:
  nsDiscriminatedUnion() : mType(nsIDataType::VTYPE_EMPTY) {
    u.mInt8Value = '\0';
  }
  nsDiscriminatedUnion(const nsDiscriminatedUnion&) = delete;
  nsDiscriminatedUnion(nsDiscriminatedUnion&&) = delete;

  ~nsDiscriminatedUnion() { Cleanup(); }

  nsDiscriminatedUnion& operator=(const nsDiscriminatedUnion&) = delete;
  nsDiscriminatedUnion& operator=(nsDiscriminatedUnion&&) = delete;

  void Cleanup();

  uint16_t GetType() const { return mType; }

  [[nodiscard]] nsresult ConvertToInt8(uint8_t* aResult) const;
  [[nodiscard]] nsresult ConvertToInt16(int16_t* aResult) const;
  [[nodiscard]] nsresult ConvertToInt32(int32_t* aResult) const;
  [[nodiscard]] nsresult ConvertToInt64(int64_t* aResult) const;
  [[nodiscard]] nsresult ConvertToUint8(uint8_t* aResult) const;
  [[nodiscard]] nsresult ConvertToUint16(uint16_t* aResult) const;
  [[nodiscard]] nsresult ConvertToUint32(uint32_t* aResult) const;
  [[nodiscard]] nsresult ConvertToUint64(uint64_t* aResult) const;
  [[nodiscard]] nsresult ConvertToFloat(float* aResult) const;
  [[nodiscard]] nsresult ConvertToDouble(double* aResult) const;
  [[nodiscard]] nsresult ConvertToBool(bool* aResult) const;
  [[nodiscard]] nsresult ConvertToChar(char* aResult) const;
  [[nodiscard]] nsresult ConvertToWChar(char16_t* aResult) const;

  [[nodiscard]] nsresult ConvertToID(nsID* aResult) const;

  [[nodiscard]] nsresult ConvertToAString(nsAString& aResult) const;
  [[nodiscard]] nsresult ConvertToAUTF8String(nsAUTF8String& aResult) const;
  [[nodiscard]] nsresult ConvertToACString(nsACString& aResult) const;
  [[nodiscard]] nsresult ConvertToString(char** aResult) const;
  [[nodiscard]] nsresult ConvertToWString(char16_t** aResult) const;
  [[nodiscard]] nsresult ConvertToStringWithSize(uint32_t* aSize,
                                                 char** aStr) const;
  [[nodiscard]] nsresult ConvertToWStringWithSize(uint32_t* aSize,
                                                  char16_t** aStr) const;

  [[nodiscard]] nsresult ConvertToISupports(nsISupports** aResult) const;
  [[nodiscard]] nsresult ConvertToInterface(nsIID** aIID,
                                            void** aInterface) const;
  [[nodiscard]] nsresult ConvertToArray(uint16_t* aType, nsIID* aIID,
                                        uint32_t* aCount, void** aPtr) const;

  [[nodiscard]] nsresult SetFromVariant(nsIVariant* aValue);

  void SetFromInt8(uint8_t aValue);
  void SetFromInt16(int16_t aValue);
  void SetFromInt32(int32_t aValue);
  void SetFromInt64(int64_t aValue);
  void SetFromUint8(uint8_t aValue);
  void SetFromUint16(uint16_t aValue);
  void SetFromUint32(uint32_t aValue);
  void SetFromUint64(uint64_t aValue);
  void SetFromFloat(float aValue);
  void SetFromDouble(double aValue);
  void SetFromBool(bool aValue);
  void SetFromChar(char aValue);
  void SetFromWChar(char16_t aValue);
  void SetFromID(const nsID& aValue);
  void SetFromAString(const nsAString& aValue);
  void SetFromAUTF8String(const nsAUTF8String& aValue);
  void SetFromACString(const nsACString& aValue);
  [[nodiscard]] nsresult SetFromString(const char* aValue);
  [[nodiscard]] nsresult SetFromWString(const char16_t* aValue);
  void SetFromISupports(nsISupports* aValue);
  void SetFromInterface(const nsIID& aIID, nsISupports* aValue);
  [[nodiscard]] nsresult SetFromArray(uint16_t aType, const nsIID* aIID,
                                      uint32_t aCount, void* aValue);
  [[nodiscard]] nsresult SetFromStringWithSize(uint32_t aSize,
                                               const char* aValue);
  [[nodiscard]] nsresult SetFromWStringWithSize(uint32_t aSize,
                                                const char16_t* aValue);

  // Like SetFromWStringWithSize, but leaves the string uninitialized. It does
  // does write the null-terminator.
  void AllocateWStringWithSize(uint32_t aSize);

  void SetToVoid();
  void SetToEmpty();
  void SetToEmptyArray();

  void Traverse(nsCycleCollectionTraversalCallback& aCb) const;

 private:
  [[nodiscard]] nsresult ToManageableNumber(
      nsDiscriminatedUnion* aOutData) const;
  void FreeArray();
  [[nodiscard]] bool String2ID(nsID* aPid) const;
  [[nodiscard]] nsresult ToString(nsACString& aOutString) const;

 public:
  union {
    int8_t mInt8Value;
    int16_t mInt16Value;
    int32_t mInt32Value;
    int64_t mInt64Value;
    uint8_t mUint8Value;
    uint16_t mUint16Value;
    uint32_t mUint32Value;
    uint64_t mUint64Value;
    float mFloatValue;
    double mDoubleValue;
    bool mBoolValue;
    char mCharValue;
    char16_t mWCharValue;
    nsIID mIDValue;
    nsAString* mAStringValue;
    nsAUTF8String* mUTF8StringValue;
    nsACString* mCStringValue;
    struct {
      // This is an owning reference that cannot be an nsCOMPtr because
      // nsDiscriminatedUnion needs to be POD.  AddRef/Release are manually
      // called on this.
      nsISupports* MOZ_OWNING_REF mInterfaceValue;
      nsIID mInterfaceID;
    } iface;
    struct {
      nsIID mArrayInterfaceID;
      void* mArrayValue;
      uint32_t mArrayCount;
      uint16_t mArrayType;
    } array;
    struct {
      char* mStringValue;
      uint32_t mStringLength;
    } str;
    struct {
      char16_t* mWStringValue;
      uint32_t mWStringLength;
    } wstr;
  } u;
  uint16_t mType;
};

/**
 * nsVariant implements the generic variant support. The xpcom module registers
 * a factory (see NS_VARIANT_CONTRACTID in nsIVariant.idl) that will create
 * these objects. They are created 'empty' and 'writable'.
 *
 * nsIVariant users won't usually need to see this class.
 */
class nsVariantBase : public nsIWritableVariant {
 public:
  NS_DECL_NSIVARIANT
  NS_DECL_NSIWRITABLEVARIANT

  nsVariantBase();

 protected:
  ~nsVariantBase() = default;

  nsDiscriminatedUnion mData;
  bool mWritable;
};

class nsVariant final : public nsVariantBase {
 public:
  NS_DECL_ISUPPORTS

  nsVariant() = default;

 private:
  ~nsVariant() = default;
};

class nsVariantCC final : public nsVariantBase {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(nsVariantCC)

  nsVariantCC() = default;

 private:
  ~nsVariantCC() = default;
};

/**
 * Users of nsIVariant should be using the contractID and not this CID.
 *  - see NS_VARIANT_CONTRACTID in nsIVariant.idl.
 */

#define NS_VARIANT_CID                              \
  { /* 0D6EA1D0-879C-11d5-90EF-0010A4E73D9A */      \
    0xd6ea1d0, 0x879c, 0x11d5, {                    \
      0x90, 0xef, 0x0, 0x10, 0xa4, 0xe7, 0x3d, 0x9a \
    }                                               \
  }

#endif  // nsVariant_h
