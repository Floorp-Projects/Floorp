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

    NS_IMETHOD GetData(nsID **aData);
    NS_IMETHOD SetData(nsID *aData);
    NS_IMETHOD toString(char **_retval);

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

    NS_IMETHOD GetData(char **aData);
    NS_IMETHOD SetData(char *aData);
    NS_IMETHOD toString(char **_retval);

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

    NS_IMETHOD GetData(PRUnichar **aData);
    NS_IMETHOD SetData(PRUnichar *aData);
    NS_IMETHOD toString(PRUnichar **_retval);

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

    NS_IMETHOD GetData(PRBool *aData);
    NS_IMETHOD SetData(PRBool aData);
    NS_IMETHOD toString(char **_retval);

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

    NS_IMETHOD GetData(PRUint8 *aData);
    NS_IMETHOD SetData(PRUint8 aData);
    NS_IMETHOD toString(char **_retval);

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

    NS_IMETHOD GetData(PRUint16 *aData);
    NS_IMETHOD SetData(PRUint16 aData);
    NS_IMETHOD toString(char **_retval);

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

    NS_IMETHOD GetData(PRUint32 *aData);
    NS_IMETHOD SetData(PRUint32 aData);
    NS_IMETHOD toString(char **_retval);

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

    NS_IMETHOD GetData(PRUint64 *aData);
    NS_IMETHOD SetData(PRUint64 aData);
    NS_IMETHOD toString(char **_retval);

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

    NS_IMETHOD GetData(PRTime *aData);
    NS_IMETHOD SetData(PRTime aData);
    NS_IMETHOD toString(char **_retval);

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

    NS_IMETHOD GetData(char *aData);
    NS_IMETHOD SetData(char aData);
    NS_IMETHOD toString(char **_retval);

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

    NS_IMETHOD GetData(PRInt16 *aData);
    NS_IMETHOD SetData(PRInt16 aData);
    NS_IMETHOD toString(char **_retval);

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

    NS_IMETHOD GetData(PRInt32 *aData);
    NS_IMETHOD SetData(PRInt32 aData);
    NS_IMETHOD toString(char **_retval);

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

    NS_IMETHOD GetData(PRInt64 *aData);
    NS_IMETHOD SetData(PRInt64 aData);
    NS_IMETHOD toString(char **_retval);

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

    NS_IMETHOD GetData(float *aData);
    NS_IMETHOD SetData(float aData);
    NS_IMETHOD toString(char **_retval);

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

    NS_IMETHOD GetData(double *aData);
    NS_IMETHOD SetData(double aData);
    NS_IMETHOD toString(char **_retval);

    nsSupportsDoubleImpl();
    virtual ~nsSupportsDoubleImpl();

private:
    double mData;
};

/***************************************************************************/



#endif /* nsSupportsPrimitives_h__ */
