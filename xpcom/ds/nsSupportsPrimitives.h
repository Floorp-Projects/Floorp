/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsSupportsPrimitives_h__
#define nsSupportsPrimitives_h__

#include "mozilla/Attributes.h"

#include "nsISupportsPrimitives.h"
#include "nsCOMPtr.h"
#include "nsString.h"

/***************************************************************************/

class nsSupportsCString final : public nsISupportsCString {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISUPPORTSPRIMITIVE
  NS_DECL_NSISUPPORTSCSTRING

  nsSupportsCString() = default;

 private:
  ~nsSupportsCString() = default;

  nsCString mData;
};

/***************************************************************************/

class nsSupportsString final : public nsISupportsString {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISUPPORTSPRIMITIVE
  NS_DECL_NSISUPPORTSSTRING

  nsSupportsString() = default;

 private:
  ~nsSupportsString() = default;

  nsString mData;
};

/***************************************************************************/

class nsSupportsPRBool final : public nsISupportsPRBool {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSISUPPORTSPRIMITIVE
  NS_DECL_NSISUPPORTSPRBOOL

  nsSupportsPRBool();

 private:
  ~nsSupportsPRBool() = default;

  bool mData;
};

/***************************************************************************/

class nsSupportsPRUint8 final : public nsISupportsPRUint8 {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISUPPORTSPRIMITIVE
  NS_DECL_NSISUPPORTSPRUINT8

  nsSupportsPRUint8();

 private:
  ~nsSupportsPRUint8() = default;

  uint8_t mData;
};

/***************************************************************************/

class nsSupportsPRUint16 final : public nsISupportsPRUint16 {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISUPPORTSPRIMITIVE
  NS_DECL_NSISUPPORTSPRUINT16

  nsSupportsPRUint16();

 private:
  ~nsSupportsPRUint16() = default;

  uint16_t mData;
};

/***************************************************************************/

class nsSupportsPRUint32 final : public nsISupportsPRUint32 {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISUPPORTSPRIMITIVE
  NS_DECL_NSISUPPORTSPRUINT32

  nsSupportsPRUint32();

 private:
  ~nsSupportsPRUint32() = default;

  uint32_t mData;
};

/***************************************************************************/

class nsSupportsPRUint64 final : public nsISupportsPRUint64 {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISUPPORTSPRIMITIVE
  NS_DECL_NSISUPPORTSPRUINT64

  nsSupportsPRUint64();

 private:
  ~nsSupportsPRUint64() = default;

  uint64_t mData;
};

/***************************************************************************/

class nsSupportsPRTime final : public nsISupportsPRTime {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISUPPORTSPRIMITIVE
  NS_DECL_NSISUPPORTSPRTIME

  nsSupportsPRTime();

 private:
  ~nsSupportsPRTime() = default;

  PRTime mData;
};

/***************************************************************************/

class nsSupportsChar final : public nsISupportsChar {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISUPPORTSPRIMITIVE
  NS_DECL_NSISUPPORTSCHAR

  nsSupportsChar();

 private:
  ~nsSupportsChar() = default;

  char mData;
};

/***************************************************************************/

class nsSupportsPRInt16 final : public nsISupportsPRInt16 {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISUPPORTSPRIMITIVE
  NS_DECL_NSISUPPORTSPRINT16

  nsSupportsPRInt16();

 private:
  ~nsSupportsPRInt16() = default;

  int16_t mData;
};

/***************************************************************************/

class nsSupportsPRInt32 final : public nsISupportsPRInt32 {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISUPPORTSPRIMITIVE
  NS_DECL_NSISUPPORTSPRINT32

  nsSupportsPRInt32();

 private:
  ~nsSupportsPRInt32() = default;

  int32_t mData;
};

/***************************************************************************/

class nsSupportsPRInt64 final : public nsISupportsPRInt64 {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISUPPORTSPRIMITIVE
  NS_DECL_NSISUPPORTSPRINT64

  nsSupportsPRInt64();

 private:
  ~nsSupportsPRInt64() = default;

  int64_t mData;
};

/***************************************************************************/

class nsSupportsFloat final : public nsISupportsFloat {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISUPPORTSPRIMITIVE
  NS_DECL_NSISUPPORTSFLOAT

  nsSupportsFloat();

 private:
  ~nsSupportsFloat() = default;

  float mData;
};

/***************************************************************************/

class nsSupportsDouble final : public nsISupportsDouble {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISUPPORTSPRIMITIVE
  NS_DECL_NSISUPPORTSDOUBLE

  nsSupportsDouble();

 private:
  ~nsSupportsDouble() = default;

  double mData;
};

/***************************************************************************/

class nsSupportsInterfacePointer final : public nsISupportsInterfacePointer {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSISUPPORTSPRIMITIVE
  NS_DECL_NSISUPPORTSINTERFACEPOINTER

  nsSupportsInterfacePointer();

 private:
  ~nsSupportsInterfacePointer();

  nsCOMPtr<nsISupports> mData;
  nsID* mIID;
};

/***************************************************************************/

/**
 * Wraps a static const char* buffer for use with nsISupportsCString
 *
 * Only use this class with static buffers, or arena-allocated buffers of
 * permanent lifetime!
 */
class nsSupportsDependentCString final : public nsISupportsCString {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISUPPORTSPRIMITIVE
  NS_DECL_NSISUPPORTSCSTRING

  explicit nsSupportsDependentCString(const char* aStr);

 private:
  ~nsSupportsDependentCString() = default;

  nsDependentCString mData;
};

#endif /* nsSupportsPrimitives_h__ */
