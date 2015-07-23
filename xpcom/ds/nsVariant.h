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

  void Cleanup();

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

private:
  nsresult ToManageableNumber(nsDiscriminatedUnion* aOutData) const;
  void FreeArray();
  bool String2ID(nsID* aPid) const;

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

  static nsresult ConvertToAString(const nsDiscriminatedUnion& aData,
                                   nsAString& aResult);
  static nsresult ConvertToAUTF8String(const nsDiscriminatedUnion& aData,
                                       nsAUTF8String& aResult);
  static nsresult ConvertToACString(const nsDiscriminatedUnion& aData,
                                    nsACString& aResult);
  static nsresult ConvertToString(const nsDiscriminatedUnion& aData,
                                  char** aResult);
  static nsresult ConvertToWString(const nsDiscriminatedUnion& aData,
                                   char16_t** aResult);
  static nsresult ConvertToISupports(const nsDiscriminatedUnion& aData,
                                     nsISupports** aResult);
  static nsresult ConvertToInterface(const nsDiscriminatedUnion& aData,
                                     nsIID** aIID, void** aInterface);
  static nsresult ConvertToArray(const nsDiscriminatedUnion& aData,
                                 uint16_t* aType, nsIID* aIID,
                                 uint32_t* aCount, void** aPtr);
  static nsresult ConvertToStringWithSize(const nsDiscriminatedUnion& aData,
                                          uint32_t* aSize, char** aStr);
  static nsresult ConvertToWStringWithSize(const nsDiscriminatedUnion& aData,
                                           uint32_t* aSize, char16_t** aStr);

  static nsresult SetFromVariant(nsDiscriminatedUnion* aData,
                                 nsIVariant* aValue);

  static nsresult SetFromInt8(nsDiscriminatedUnion* aData, uint8_t aValue);
  static nsresult SetFromInt16(nsDiscriminatedUnion* aData, int16_t aValue);
  static nsresult SetFromInt32(nsDiscriminatedUnion* aData, int32_t aValue);
  static nsresult SetFromInt64(nsDiscriminatedUnion* aData, int64_t aValue);
  static nsresult SetFromUint8(nsDiscriminatedUnion* aData, uint8_t aValue);
  static nsresult SetFromUint16(nsDiscriminatedUnion* aData, uint16_t aValue);
  static nsresult SetFromUint32(nsDiscriminatedUnion* aData, uint32_t aValue);
  static nsresult SetFromUint64(nsDiscriminatedUnion* aData, uint64_t aValue);
  static nsresult SetFromFloat(nsDiscriminatedUnion* aData, float aValue);
  static nsresult SetFromDouble(nsDiscriminatedUnion* aData, double aValue);
  static nsresult SetFromBool(nsDiscriminatedUnion* aData, bool aValue);
  static nsresult SetFromChar(nsDiscriminatedUnion* aData, char aValue);
  static nsresult SetFromWChar(nsDiscriminatedUnion* aData, char16_t aValue);
  static nsresult SetFromID(nsDiscriminatedUnion* aData, const nsID& aValue);
  static nsresult SetFromAString(nsDiscriminatedUnion* aData,
                                 const nsAString& aValue);
  static nsresult SetFromAUTF8String(nsDiscriminatedUnion* aData,
                                     const nsAUTF8String& aValue);
  static nsresult SetFromACString(nsDiscriminatedUnion* aData,
                                  const nsACString& aValue);
  static nsresult SetFromString(nsDiscriminatedUnion* aData,
                                const char* aValue);
  static nsresult SetFromWString(nsDiscriminatedUnion* aData,
                                 const char16_t* aValue);
  static nsresult SetFromISupports(nsDiscriminatedUnion* aData,
                                   nsISupports* aValue);
  static nsresult SetFromInterface(nsDiscriminatedUnion* aData,
                                   const nsIID& aIID, nsISupports* aValue);
  static nsresult SetFromArray(nsDiscriminatedUnion* aData, uint16_t aType,
                               const nsIID* aIID, uint32_t aCount,
                               void* aValue);
  static nsresult SetFromStringWithSize(nsDiscriminatedUnion* aData,
                                        uint32_t aSize, const char* aValue);
  static nsresult SetFromWStringWithSize(nsDiscriminatedUnion* aData,
                                         uint32_t aSize,
                                         const char16_t* aValue);

  // Like SetFromWStringWithSize, but leaves the string uninitialized. It does
  // does write the null-terminator.
  static nsresult AllocateWStringWithSize(nsDiscriminatedUnion* aData,
                                          uint32_t aSize);

  static nsresult SetToVoid(nsDiscriminatedUnion* aData);
  static nsresult SetToEmpty(nsDiscriminatedUnion* aData);
  static nsresult SetToEmptyArray(nsDiscriminatedUnion* aData);

  static void Traverse(const nsDiscriminatedUnion& aData,
                       nsCycleCollectionTraversalCallback& aCb);

private:
  ~nsVariant();

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
