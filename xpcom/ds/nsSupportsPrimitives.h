/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsSupportsPrimitives_h__
#define nsSupportsPrimitives_h__

#include "mozilla/Attributes.h"

#include "nsISupportsPrimitives.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsDependentString.h"

class nsSupportsIDImpl MOZ_FINAL : public nsISupportsID
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSISUPPORTSPRIMITIVE
    NS_DECL_NSISUPPORTSID

    nsSupportsIDImpl();

private:
    ~nsSupportsIDImpl() { }

    nsID *mData;
};

/***************************************************************************/

class nsSupportsCStringImpl MOZ_FINAL : public nsISupportsCString
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSISUPPORTSPRIMITIVE
    NS_DECL_NSISUPPORTSCSTRING

    nsSupportsCStringImpl() {}

private:
    ~nsSupportsCStringImpl() {}
    
    nsCString mData;
};

/***************************************************************************/

class nsSupportsStringImpl MOZ_FINAL : public nsISupportsString
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSISUPPORTSPRIMITIVE
    NS_DECL_NSISUPPORTSSTRING

    nsSupportsStringImpl() {}

private:
    ~nsSupportsStringImpl() {}
    
    nsString mData;
};

/***************************************************************************/

class nsSupportsPRBoolImpl MOZ_FINAL : public nsISupportsPRBool
{
public:
    NS_DECL_THREADSAFE_ISUPPORTS
    NS_DECL_NSISUPPORTSPRIMITIVE
    NS_DECL_NSISUPPORTSPRBOOL

    nsSupportsPRBoolImpl();

private:
    ~nsSupportsPRBoolImpl() {}

    bool mData;
};

/***************************************************************************/

class nsSupportsPRUint8Impl MOZ_FINAL : public nsISupportsPRUint8
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSISUPPORTSPRIMITIVE
    NS_DECL_NSISUPPORTSPRUINT8

    nsSupportsPRUint8Impl();

private:
    ~nsSupportsPRUint8Impl() {}

    uint8_t mData;
};

/***************************************************************************/

class nsSupportsPRUint16Impl MOZ_FINAL : public nsISupportsPRUint16
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSISUPPORTSPRIMITIVE
    NS_DECL_NSISUPPORTSPRUINT16

    nsSupportsPRUint16Impl();

private:
    ~nsSupportsPRUint16Impl() {}

    uint16_t mData;
};

/***************************************************************************/

class nsSupportsPRUint32Impl MOZ_FINAL : public nsISupportsPRUint32
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSISUPPORTSPRIMITIVE
    NS_DECL_NSISUPPORTSPRUINT32

    nsSupportsPRUint32Impl();

private:
    ~nsSupportsPRUint32Impl() {}

    uint32_t mData;
};

/***************************************************************************/

class nsSupportsPRUint64Impl MOZ_FINAL : public nsISupportsPRUint64
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSISUPPORTSPRIMITIVE
    NS_DECL_NSISUPPORTSPRUINT64

    nsSupportsPRUint64Impl();

private:
    ~nsSupportsPRUint64Impl() {}

    uint64_t mData;
};

/***************************************************************************/

class nsSupportsPRTimeImpl MOZ_FINAL : public nsISupportsPRTime
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSISUPPORTSPRIMITIVE
    NS_DECL_NSISUPPORTSPRTIME

    nsSupportsPRTimeImpl();

private:
    ~nsSupportsPRTimeImpl() {}

    PRTime mData;
};

/***************************************************************************/

class nsSupportsCharImpl MOZ_FINAL : public nsISupportsChar
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSISUPPORTSPRIMITIVE
    NS_DECL_NSISUPPORTSCHAR

    nsSupportsCharImpl();

private:
    ~nsSupportsCharImpl() {}

    char mData;
};

/***************************************************************************/

class nsSupportsPRInt16Impl MOZ_FINAL : public nsISupportsPRInt16
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSISUPPORTSPRIMITIVE
    NS_DECL_NSISUPPORTSPRINT16

    nsSupportsPRInt16Impl();

private:
    ~nsSupportsPRInt16Impl() {}

    int16_t mData;
};

/***************************************************************************/

class nsSupportsPRInt32Impl MOZ_FINAL : public nsISupportsPRInt32
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSISUPPORTSPRIMITIVE
    NS_DECL_NSISUPPORTSPRINT32

    nsSupportsPRInt32Impl();

private:
    ~nsSupportsPRInt32Impl() {}

    int32_t mData;
};

/***************************************************************************/

class nsSupportsPRInt64Impl MOZ_FINAL : public nsISupportsPRInt64
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSISUPPORTSPRIMITIVE
    NS_DECL_NSISUPPORTSPRINT64

    nsSupportsPRInt64Impl();

private:
    ~nsSupportsPRInt64Impl() {}

    int64_t mData;
};

/***************************************************************************/

class nsSupportsFloatImpl MOZ_FINAL : public nsISupportsFloat
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSISUPPORTSPRIMITIVE
    NS_DECL_NSISUPPORTSFLOAT

    nsSupportsFloatImpl();

private:
    ~nsSupportsFloatImpl() {}

    float mData;
};

/***************************************************************************/

class nsSupportsDoubleImpl MOZ_FINAL : public nsISupportsDouble
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSISUPPORTSPRIMITIVE
    NS_DECL_NSISUPPORTSDOUBLE

    nsSupportsDoubleImpl();

private:
    ~nsSupportsDoubleImpl() {}

    double mData;
};

/***************************************************************************/

class nsSupportsVoidImpl MOZ_FINAL : public nsISupportsVoid
{
public:
    NS_DECL_THREADSAFE_ISUPPORTS
    NS_DECL_NSISUPPORTSPRIMITIVE
    NS_DECL_NSISUPPORTSVOID

    nsSupportsVoidImpl();

private:
    ~nsSupportsVoidImpl() {}

    void* mData;
};

/***************************************************************************/

class nsSupportsInterfacePointerImpl MOZ_FINAL : public nsISupportsInterfacePointer
{
public:
    NS_DECL_THREADSAFE_ISUPPORTS
    NS_DECL_NSISUPPORTSPRIMITIVE
    NS_DECL_NSISUPPORTSINTERFACEPOINTER

    nsSupportsInterfacePointerImpl();

private:
    ~nsSupportsInterfacePointerImpl();

    nsCOMPtr<nsISupports> mData;
    nsID *mIID;
};

/***************************************************************************/

/**
 * Wraps a static const char* buffer for use with nsISupportsCString
 *
 * Only use this class with static buffers, or arena-allocated buffers of
 * permanent lifetime!
 */
class nsSupportsDependentCString MOZ_FINAL : public nsISupportsCString
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISUPPORTSPRIMITIVE
  NS_DECL_NSISUPPORTSCSTRING

  nsSupportsDependentCString(const char* aStr);

private:
  ~nsSupportsDependentCString() {}

  nsDependentCString mData;
};

#endif /* nsSupportsPrimitives_h__ */
