/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef nsSupportsPrimitives_h__
#define nsSupportsPrimitives_h__

#include "nsISupportsPrimitives.h"

class nsSupportsIDImpl : public nsISupportsID
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSISUPPORTSID

    nsSupportsIDImpl();
    virtual ~nsSupportsIDImpl();

private:
    nsID *mData;
};

/***************************************************************************/

class nsSupportsStringImpl : public nsISupportsString
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSISUPPORTSSTRING

    nsSupportsStringImpl();
    virtual ~nsSupportsStringImpl();

private:
    char *mData;
};

/***************************************************************************/

class nsSupportsWStringImpl : public nsISupportsWString
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSISUPPORTSWSTRING

    nsSupportsWStringImpl();
    virtual ~nsSupportsWStringImpl();

private:
    PRUnichar *mData;
};

/***************************************************************************/

class nsSupportsPRBoolImpl : public nsISupportsPRBool
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSISUPPORTSPRBOOL

    nsSupportsPRBoolImpl();
    virtual ~nsSupportsPRBoolImpl();

private:
    PRBool mData;
};

/***************************************************************************/

class nsSupportsPRUint8Impl : public nsISupportsPRUint8
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSISUPPORTSPRUINT8

    nsSupportsPRUint8Impl();
    virtual ~nsSupportsPRUint8Impl();

private:
    PRUint8 mData;
};

/***************************************************************************/

class nsSupportsPRUint16Impl : public nsISupportsPRUint16
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSISUPPORTSPRUINT16

    nsSupportsPRUint16Impl();
    virtual ~nsSupportsPRUint16Impl();

private:
    PRUint16 mData;
};

/***************************************************************************/

class nsSupportsPRUint32Impl : public nsISupportsPRUint32
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSISUPPORTSPRUINT32

    nsSupportsPRUint32Impl();
    virtual ~nsSupportsPRUint32Impl();

private:
    PRUint32 mData;
};

/***************************************************************************/

class nsSupportsPRUint64Impl : public nsISupportsPRUint64
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSISUPPORTSPRUINT64

    nsSupportsPRUint64Impl();
    virtual ~nsSupportsPRUint64Impl();

private:
    PRUint64 mData;
};

/***************************************************************************/

class nsSupportsPRTimeImpl : public nsISupportsPRTime
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSISUPPORTSPRTIME

    nsSupportsPRTimeImpl();
    virtual ~nsSupportsPRTimeImpl();

private:
    PRTime mData;
};

/***************************************************************************/

class nsSupportsCharImpl : public nsISupportsChar
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSISUPPORTSCHAR

    nsSupportsCharImpl();
    virtual ~nsSupportsCharImpl();

private:
    char mData;
};

/***************************************************************************/

class nsSupportsPRInt16Impl : public nsISupportsPRInt16
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSISUPPORTSPRINT16

    nsSupportsPRInt16Impl();
    virtual ~nsSupportsPRInt16Impl();

private:
    PRInt16 mData;
};

/***************************************************************************/

class nsSupportsPRInt32Impl : public nsISupportsPRInt32
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSISUPPORTSPRINT32

    nsSupportsPRInt32Impl();
    virtual ~nsSupportsPRInt32Impl();

private:
    PRInt32 mData;
};

/***************************************************************************/

class nsSupportsPRInt64Impl : public nsISupportsPRInt64
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSISUPPORTSPRINT64

    nsSupportsPRInt64Impl();
    virtual ~nsSupportsPRInt64Impl();

private:
    PRInt64 mData;
};

/***************************************************************************/

class nsSupportsFloatImpl : public nsISupportsFloat
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSISUPPORTSFLOAT

    nsSupportsFloatImpl();
    virtual ~nsSupportsFloatImpl();

private:
    float mData;
};

/***************************************************************************/

class nsSupportsDoubleImpl : public nsISupportsDouble
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSISUPPORTSDOUBLE

    nsSupportsDoubleImpl();
    virtual ~nsSupportsDoubleImpl();

private:
    double mData;
};

/***************************************************************************/

class nsSupportsVoidImpl : public nsISupportsVoid
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSISUPPORTSVOID

    nsSupportsVoidImpl();
    virtual ~nsSupportsVoidImpl();

private:
    void* mData;
};

/***************************************************************************/

#endif /* nsSupportsPrimitives_h__ */
