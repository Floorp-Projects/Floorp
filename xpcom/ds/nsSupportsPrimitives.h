/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Dan Rosen <dr@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsSupportsPrimitives_h__
#define nsSupportsPrimitives_h__

#include "nsISupportsPrimitives.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsDependentString.h"

class nsSupportsIDImpl : public nsISupportsID
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

class nsSupportsCStringImpl : public nsISupportsCString
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

class nsSupportsStringImpl : public nsISupportsString
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

class nsSupportsPRBoolImpl : public nsISupportsPRBool
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSISUPPORTSPRIMITIVE
    NS_DECL_NSISUPPORTSPRBOOL

    nsSupportsPRBoolImpl();

private:
    ~nsSupportsPRBoolImpl() {}

    PRBool mData;
};

/***************************************************************************/

class nsSupportsPRUint8Impl : public nsISupportsPRUint8
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSISUPPORTSPRIMITIVE
    NS_DECL_NSISUPPORTSPRUINT8

    nsSupportsPRUint8Impl();

private:
    ~nsSupportsPRUint8Impl() {}

    PRUint8 mData;
};

/***************************************************************************/

class nsSupportsPRUint16Impl : public nsISupportsPRUint16
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSISUPPORTSPRIMITIVE
    NS_DECL_NSISUPPORTSPRUINT16

    nsSupportsPRUint16Impl();

private:
    ~nsSupportsPRUint16Impl() {}

    PRUint16 mData;
};

/***************************************************************************/

class nsSupportsPRUint32Impl : public nsISupportsPRUint32
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSISUPPORTSPRIMITIVE
    NS_DECL_NSISUPPORTSPRUINT32

    nsSupportsPRUint32Impl();

private:
    ~nsSupportsPRUint32Impl() {}

    PRUint32 mData;
};

/***************************************************************************/

class nsSupportsPRUint64Impl : public nsISupportsPRUint64
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSISUPPORTSPRIMITIVE
    NS_DECL_NSISUPPORTSPRUINT64

    nsSupportsPRUint64Impl();

private:
    ~nsSupportsPRUint64Impl() {}

    PRUint64 mData;
};

/***************************************************************************/

class nsSupportsPRTimeImpl : public nsISupportsPRTime
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

class nsSupportsCharImpl : public nsISupportsChar
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

class nsSupportsPRInt16Impl : public nsISupportsPRInt16
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSISUPPORTSPRIMITIVE
    NS_DECL_NSISUPPORTSPRINT16

    nsSupportsPRInt16Impl();

private:
    ~nsSupportsPRInt16Impl() {}

    PRInt16 mData;
};

/***************************************************************************/

class nsSupportsPRInt32Impl : public nsISupportsPRInt32
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSISUPPORTSPRIMITIVE
    NS_DECL_NSISUPPORTSPRINT32

    nsSupportsPRInt32Impl();

private:
    ~nsSupportsPRInt32Impl() {}

    PRInt32 mData;
};

/***************************************************************************/

class nsSupportsPRInt64Impl : public nsISupportsPRInt64
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSISUPPORTSPRIMITIVE
    NS_DECL_NSISUPPORTSPRINT64

    nsSupportsPRInt64Impl();

private:
    ~nsSupportsPRInt64Impl() {}

    PRInt64 mData;
};

/***************************************************************************/

class nsSupportsFloatImpl : public nsISupportsFloat
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

class nsSupportsDoubleImpl : public nsISupportsDouble
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

class nsSupportsVoidImpl : public nsISupportsVoid
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSISUPPORTSPRIMITIVE
    NS_DECL_NSISUPPORTSVOID

    nsSupportsVoidImpl();

private:
    ~nsSupportsVoidImpl() {}

    void* mData;
};

/***************************************************************************/

class nsSupportsInterfacePointerImpl : public nsISupportsInterfacePointer
{
public:
    NS_DECL_ISUPPORTS
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
class nsSupportsDependentCString : public nsISupportsCString
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
