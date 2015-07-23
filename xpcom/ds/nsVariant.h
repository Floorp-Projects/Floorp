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

class nsCycleCollectionTraversalCallback;

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

class nsDiscriminatedUnion
{
public:

  nsDiscriminatedUnion() : mType(nsIDataType::VTYPE_EMPTY) {}
  ~nsDiscriminatedUnion() { Cleanup(); }

  void Cleanup();

  uint16_t GetType() const { return mType; }

  nsresult ConvertToInt8(uint8_t* aResult) const;
  nsresult ConvertToInt16(int16_t* aResult) const;
  nsresult ConvertToInt32(int32_t* aResult) const;
  nsresult ConvertToInt64(int64_t* aResult) const;
  nsresult ConvertToUint8(uint8_t* aResult) const;
  nsresult ConvertToUint16(uint16_t* aResult) const;
  nsresult ConvertToUint32(uint32_t* aResult) const;
  nsresult ConvertToUint64(uint64_t* aResult) const;
  nsresult ConvertToFloat(float* aResult) const;
  nsresult ConvertToDouble(double* aResult) const;
  nsresult ConvertToBool(bool* aResult) const;
  nsresult ConvertToChar(char* aResult) const;
  nsresult ConvertToWChar(char16_t* aResult) const;

  nsresult ConvertToID(nsID* aResult) const;

  nsresult ConvertToAString(nsAString& aResult) const;
  nsresult ConvertToAUTF8String(nsAUTF8String& aResult) const;
  nsresult ConvertToACString(nsACString& aResult) const;
  nsresult ConvertToString(char** aResult) const;
  nsresult ConvertToWString(char16_t** aResult) const;
  nsresult ConvertToStringWithSize(uint32_t* aSize, char** aStr) const;
  nsresult ConvertToWStringWithSize(uint32_t* aSize, char16_t** aStr) const;

  nsresult ConvertToISupports(nsISupports** aResult) const;
  nsresult ConvertToInterface(nsIID** aIID, void** aInterface) const;
  nsresult ConvertToArray(uint16_t* aType, nsIID* aIID,
                          uint32_t* aCount, void** aPtr) const;

  nsresult SetFromVariant(nsIVariant* aValue);

  nsresult SetFromInt8(uint8_t aValue);
  nsresult SetFromInt16(int16_t aValue);
  nsresult SetFromInt32(int32_t aValue);
  nsresult SetFromInt64(int64_t aValue);
  nsresult SetFromUint8(uint8_t aValue);
  nsresult SetFromUint16(uint16_t aValue);
  nsresult SetFromUint32(uint32_t aValue);
  nsresult SetFromUint64(uint64_t aValue);
  nsresult SetFromFloat(float aValue);
  nsresult SetFromDouble(double aValue);
  nsresult SetFromBool(bool aValue);
  nsresult SetFromChar(char aValue);
  nsresult SetFromWChar(char16_t aValue);
  nsresult SetFromID(const nsID& aValue);
  nsresult SetFromAString(const nsAString& aValue);
  nsresult SetFromDOMString(const nsAString& aValue);
  nsresult SetFromAUTF8String(const nsAUTF8String& aValue);
  nsresult SetFromACString(const nsACString& aValue);
  nsresult SetFromString(const char* aValue);
  nsresult SetFromWString(const char16_t* aValue);
  nsresult SetFromISupports(nsISupports* aValue);
  nsresult SetFromInterface(const nsIID& aIID, nsISupports* aValue);
  nsresult SetFromArray(uint16_t aType, const nsIID* aIID, uint32_t aCount,
                        void* aValue);
  nsresult SetFromStringWithSize(uint32_t aSize, const char* aValue);
  nsresult SetFromWStringWithSize(uint32_t aSize, const char16_t* aValue);

  // Like SetFromWStringWithSize, but leaves the string uninitialized. It does
  // does write the null-terminator.
  nsresult AllocateWStringWithSize(uint32_t aSize);

  nsresult SetToVoid();
  nsresult SetToEmpty();
  nsresult SetToEmptyArray();

  void Traverse(nsCycleCollectionTraversalCallback& aCb) const;

private:
  nsresult ToManageableNumber(nsDiscriminatedUnion* aOutData) const;
  void FreeArray();
  bool String2ID(nsID* aPid) const;
  nsresult ToString(nsACString& aOutString) const;

public:
  union
  {
    int8_t         mInt8Value;
    int16_t        mInt16Value;
    int32_t        mInt32Value;
    int64_t        mInt64Value;
    uint8_t        mUint8Value;
    uint16_t       mUint16Value;
    uint32_t       mUint32Value;
    uint64_t       mUint64Value;
    float          mFloatValue;
    double         mDoubleValue;
    bool           mBoolValue;
    char           mCharValue;
    char16_t       mWCharValue;
    nsIID          mIDValue;
    nsAString*     mAStringValue;
    nsAUTF8String* mUTF8StringValue;
    nsACString*    mCStringValue;
    struct
    {
      // This is an owning reference that cannot be an nsCOMPtr because
      // nsDiscriminatedUnion needs to be POD.  AddRef/Release are manually
      // called on this.
      nsISupports* MOZ_OWNING_REF mInterfaceValue;
      nsIID        mInterfaceID;
    } iface;
    struct
    {
      nsIID        mArrayInterfaceID;
      void*        mArrayValue;
      uint32_t     mArrayCount;
      uint16_t     mArrayType;
    } array;
    struct
    {
      char*        mStringValue;
      uint32_t     mStringLength;
    } str;
    struct
    {
      char16_t*    mWStringValue;
      uint32_t     mWStringLength;
    } wstr;
  } u;
  uint16_t       mType;
};

/**
 * nsVariant implements the generic variant support. The xpcom module registers
 * a factory (see NS_VARIANT_CONTRACTID in nsIVariant.idl) that will create
 * these objects. They are created 'empty' and 'writable'.
 *
 * nsIVariant users won't usually need to see this class.
 *
 * This class also has static helper methods that nsIVariant *implementors* can
 * use to help them do all the 'standard' nsIVariant data conversions.
 */

class nsVariant final : public nsIWritableVariant
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIVARIANT
  NS_DECL_NSIWRITABLEVARIANT

  nsVariant();

private:
  ~nsVariant() {};

protected:
  nsDiscriminatedUnion mData;
  bool                 mWritable;
};

/**
 * Users of nsIVariant should be using the contractID and not this CID.
 *  - see NS_VARIANT_CONTRACTID in nsIVariant.idl.
 */

#define NS_VARIANT_CID \
{ /* 0D6EA1D0-879C-11d5-90EF-0010A4E73D9A */ \
    0xd6ea1d0,                               \
    0x879c,                                  \
    0x11d5,                                  \
    {0x90, 0xef, 0x0, 0x10, 0xa4, 0xe7, 0x3d, 0x9a}}

#endif // nsVariant_h
