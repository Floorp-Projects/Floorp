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

#include "nsSupportsPrimitives.h"
#include "nsCRT.h"
#include "nsMemory.h"
#include "prprf.h"
#include "nsIInterfaceInfoManager.h"
#include "nsDependentString.h"
#include "nsReadableUtils.h"
#include "nsPromiseFlatString.h"

/***************************************************************************/

NS_IMPL_ISUPPORTS2(nsSupportsIDImpl, nsISupportsID, nsISupportsPrimitive)

nsSupportsIDImpl::nsSupportsIDImpl()
    : mData(nsnull)
{
}

NS_IMETHODIMP nsSupportsIDImpl::GetType(PRUint16 *aType)
{
    NS_ASSERTION(aType, "Bad pointer");
    *aType = TYPE_ID;

    return NS_OK;
}

NS_IMETHODIMP nsSupportsIDImpl::GetData(nsID **aData)
{
    NS_ASSERTION(aData, "Bad pointer");
    if(mData)
    {
        *aData = (nsID*) nsMemory::Clone(mData, sizeof(nsID));
        return *aData ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
    }
    *aData = nsnull;
    return NS_OK;
}

NS_IMETHODIMP nsSupportsIDImpl::SetData(const nsID *aData)
{
    if(mData)
      nsMemory::Free(mData);
    if(aData)
        mData = (nsID*) nsMemory::Clone(aData, sizeof(nsID));
    else
        mData = nsnull;
    return NS_OK;
}

NS_IMETHODIMP nsSupportsIDImpl::ToString(char **_retval)
{
    char* result;
    NS_ASSERTION(_retval, "Bad pointer");
    if(mData)
    {
        result = mData->ToString();
    }
    else
    {
        static const char nullStr[] = "null";
        result = (char*) nsMemory::Clone(nullStr, sizeof(nullStr));
    }

    *_retval = result;
    return result ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}  

/*****************************************************************************
 * nsSupportsCStringImpl
 *****************************************************************************/

NS_IMPL_ISUPPORTS2(nsSupportsCStringImpl, nsISupportsCString,
                   nsISupportsPrimitive)

NS_IMETHODIMP nsSupportsCStringImpl::GetType(PRUint16 *aType)
{
    NS_ASSERTION(aType, "Bad pointer");

    *aType = TYPE_CSTRING;
    return NS_OK;
}

NS_IMETHODIMP nsSupportsCStringImpl::GetData(nsACString& aData)
{
    aData = mData;
    return NS_OK;
}

NS_IMETHODIMP nsSupportsCStringImpl::ToString(char **_retval)
{
    *_retval = ToNewCString(mData);

    if (!*_retval)
        return NS_ERROR_OUT_OF_MEMORY;
    
    return NS_OK;
}

NS_IMETHODIMP nsSupportsCStringImpl::SetData(const nsACString& aData)
{
    mData = aData;
    return NS_OK;
}

/*****************************************************************************
 * nsSupportsStringImpl
 *****************************************************************************/

NS_IMPL_ISUPPORTS2(nsSupportsStringImpl, nsISupportsString,
                   nsISupportsPrimitive)

NS_IMETHODIMP nsSupportsStringImpl::GetType(PRUint16 *aType)
{
    NS_ASSERTION(aType, "Bad pointer");

    *aType = TYPE_STRING;
    return NS_OK;
}

NS_IMETHODIMP nsSupportsStringImpl::GetData(nsAString& aData)
{
    aData = mData;
    return NS_OK;
}

NS_IMETHODIMP nsSupportsStringImpl::ToString(PRUnichar **_retval)
{
    *_retval = ToNewUnicode(mData);
    
    if (!*_retval)
        return NS_ERROR_OUT_OF_MEMORY;
    
    return NS_OK;
}

NS_IMETHODIMP nsSupportsStringImpl::SetData(const nsAString& aData)
{
    mData = aData;
    return NS_OK;
}

/***************************************************************************/

NS_IMPL_THREADSAFE_ISUPPORTS2(nsSupportsPRBoolImpl, nsISupportsPRBool,
                              nsISupportsPrimitive)

nsSupportsPRBoolImpl::nsSupportsPRBoolImpl()
    : mData(PR_FALSE)
{
}

NS_IMETHODIMP nsSupportsPRBoolImpl::GetType(PRUint16 *aType)
{
    NS_ASSERTION(aType, "Bad pointer");
    *aType = TYPE_PRBOOL;

    return NS_OK;
}

NS_IMETHODIMP nsSupportsPRBoolImpl::GetData(PRBool *aData)
{
    NS_ASSERTION(aData, "Bad pointer");
    *aData = mData;
    return NS_OK;
}

NS_IMETHODIMP nsSupportsPRBoolImpl::SetData(PRBool aData)
{
    mData = aData;
    return NS_OK;
}

NS_IMETHODIMP nsSupportsPRBoolImpl::ToString(char **_retval)
{
    NS_ASSERTION(_retval, "Bad pointer");
    const char * str = mData ? "true" : "false";
    char* result = (char*) nsMemory::Clone(str,
                                (strlen(str)+1)*sizeof(char));
    *_retval = result;
    return result ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}  

/***************************************************************************/

NS_IMPL_ISUPPORTS2(nsSupportsPRUint8Impl, nsISupportsPRUint8,
                   nsISupportsPrimitive)

nsSupportsPRUint8Impl::nsSupportsPRUint8Impl()
    : mData(0)
{
}

NS_IMETHODIMP nsSupportsPRUint8Impl::GetType(PRUint16 *aType)
{
    NS_ASSERTION(aType, "Bad pointer");
    *aType = TYPE_PRUINT8;

    return NS_OK;
}

NS_IMETHODIMP nsSupportsPRUint8Impl::GetData(PRUint8 *aData)
{
    NS_ASSERTION(aData, "Bad pointer");
    *aData = mData;
    return NS_OK;
}

NS_IMETHODIMP nsSupportsPRUint8Impl::SetData(PRUint8 aData)
{
    mData = aData;
    return NS_OK;
}

NS_IMETHODIMP nsSupportsPRUint8Impl::ToString(char **_retval)
{
    NS_ASSERTION(_retval, "Bad pointer");
    static const int size = 8;
    char buf[size];

    PR_snprintf(buf, size, "%u", (PRUint16) mData);

    char* result = (char*) nsMemory::Clone(buf,
                                (strlen(buf)+1)*sizeof(char));
    *_retval = result;
    return result ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}  

/***************************************************************************/

NS_IMPL_ISUPPORTS2(nsSupportsPRUint16Impl, nsISupportsPRUint16,
                   nsISupportsPrimitive)

nsSupportsPRUint16Impl::nsSupportsPRUint16Impl()
    : mData(0)
{
}

NS_IMETHODIMP nsSupportsPRUint16Impl::GetType(PRUint16 *aType)
{
    NS_ASSERTION(aType, "Bad pointer");
    *aType = TYPE_PRUINT16;

    return NS_OK;
}

NS_IMETHODIMP nsSupportsPRUint16Impl::GetData(PRUint16 *aData)
{
    NS_ASSERTION(aData, "Bad pointer");
    *aData = mData;
    return NS_OK;
}

NS_IMETHODIMP nsSupportsPRUint16Impl::SetData(PRUint16 aData)
{
    mData = aData;
    return NS_OK;
}

NS_IMETHODIMP nsSupportsPRUint16Impl::ToString(char **_retval)
{
    NS_ASSERTION(_retval, "Bad pointer");
    static const int size = 8;
    char buf[size];

    PR_snprintf(buf, size, "%u", (int) mData);

    char* result = (char*) nsMemory::Clone(buf,
                                (strlen(buf)+1)*sizeof(char));
    *_retval = result;
    return result ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}  

/***************************************************************************/

NS_IMPL_ISUPPORTS2(nsSupportsPRUint32Impl, nsISupportsPRUint32,
                   nsISupportsPrimitive)

nsSupportsPRUint32Impl::nsSupportsPRUint32Impl()
    : mData(0)
{
}

NS_IMETHODIMP nsSupportsPRUint32Impl::GetType(PRUint16 *aType)
{
    NS_ASSERTION(aType, "Bad pointer");
    *aType = TYPE_PRUINT32;

    return NS_OK;
}

NS_IMETHODIMP nsSupportsPRUint32Impl::GetData(PRUint32 *aData)
{
    NS_ASSERTION(aData, "Bad pointer");
    *aData = mData;
    return NS_OK;
}

NS_IMETHODIMP nsSupportsPRUint32Impl::SetData(PRUint32 aData)
{
    mData = aData;
    return NS_OK;
}

NS_IMETHODIMP nsSupportsPRUint32Impl::ToString(char **_retval)
{
    NS_ASSERTION(_retval, "Bad pointer");
    static const int size = 16;
    char buf[size];

    PR_snprintf(buf, size, "%lu", mData);

    char* result = (char*) nsMemory::Clone(buf,
                                (strlen(buf)+1)*sizeof(char));
    *_retval = result;
    return result ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}  

/***************************************************************************/

NS_IMPL_ISUPPORTS2(nsSupportsPRUint64Impl, nsISupportsPRUint64,
                   nsISupportsPrimitive)

nsSupportsPRUint64Impl::nsSupportsPRUint64Impl()
    : mData(LL_ZERO)
{
}

NS_IMETHODIMP nsSupportsPRUint64Impl::GetType(PRUint16 *aType)
{
    NS_ASSERTION(aType, "Bad pointer");
    *aType = TYPE_PRUINT64;

    return NS_OK;
}

NS_IMETHODIMP nsSupportsPRUint64Impl::GetData(PRUint64 *aData)
{
    NS_ASSERTION(aData, "Bad pointer");
    *aData = mData;
    return NS_OK;
}

NS_IMETHODIMP nsSupportsPRUint64Impl::SetData(PRUint64 aData)
{
    mData = aData;
    return NS_OK;
}

NS_IMETHODIMP nsSupportsPRUint64Impl::ToString(char **_retval)
{
    NS_ASSERTION(_retval, "Bad pointer");
    static const int size = 32;
    char buf[size];

    PR_snprintf(buf, size, "%llu", mData);

    char* result = (char*) nsMemory::Clone(buf,
                                (strlen(buf)+1)*sizeof(char));
    *_retval = result;
    return result ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}  

/***************************************************************************/

NS_IMPL_ISUPPORTS2(nsSupportsPRTimeImpl, nsISupportsPRTime,
                   nsISupportsPrimitive)

nsSupportsPRTimeImpl::nsSupportsPRTimeImpl()
    : mData(LL_ZERO)
{
}

NS_IMETHODIMP nsSupportsPRTimeImpl::GetType(PRUint16 *aType)
{
    NS_ASSERTION(aType, "Bad pointer");
    *aType = TYPE_PRTIME;

    return NS_OK;
}

NS_IMETHODIMP nsSupportsPRTimeImpl::GetData(PRTime *aData)
{
    NS_ASSERTION(aData, "Bad pointer");
    *aData = mData;
    return NS_OK;
}

NS_IMETHODIMP nsSupportsPRTimeImpl::SetData(PRTime aData)
{
    mData = aData;
    return NS_OK;
}

NS_IMETHODIMP nsSupportsPRTimeImpl::ToString(char **_retval)
{
    NS_ASSERTION(_retval, "Bad pointer");
    static const int size = 32;
    char buf[size];

    PR_snprintf(buf, size, "%llu", mData);

    char* result = (char*) nsMemory::Clone(buf,
                                (strlen(buf)+1)*sizeof(char));
    *_retval = result;
    return result ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}  

/***************************************************************************/

NS_IMPL_ISUPPORTS2(nsSupportsCharImpl, nsISupportsChar,
                   nsISupportsPrimitive)

nsSupportsCharImpl::nsSupportsCharImpl()
    : mData(0)
{
}

NS_IMETHODIMP nsSupportsCharImpl::GetType(PRUint16 *aType)
{
    NS_ASSERTION(aType, "Bad pointer");
    *aType = TYPE_CHAR;

    return NS_OK;
}

NS_IMETHODIMP nsSupportsCharImpl::GetData(char *aData)
{
    NS_ASSERTION(aData, "Bad pointer");
    *aData = mData;
    return NS_OK;
}

NS_IMETHODIMP nsSupportsCharImpl::SetData(char aData)
{
    mData = aData;
    return NS_OK;
}

NS_IMETHODIMP nsSupportsCharImpl::ToString(char **_retval)
{
    char* result;
    NS_ASSERTION(_retval, "Bad pointer");

    if(nsnull != (result = (char*) nsMemory::Alloc(2*sizeof(char))))
    {
        result[0] = mData;
        result[1] = '\0';
    }
    *_retval = result;
    return result ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}  

/***************************************************************************/

NS_IMPL_ISUPPORTS2(nsSupportsPRInt16Impl, nsISupportsPRInt16,
                   nsISupportsPrimitive)

nsSupportsPRInt16Impl::nsSupportsPRInt16Impl()
    : mData(0)
{
}

NS_IMETHODIMP nsSupportsPRInt16Impl::GetType(PRUint16 *aType)
{
    NS_ASSERTION(aType, "Bad pointer");
    *aType = TYPE_PRINT16;

    return NS_OK;
}

NS_IMETHODIMP nsSupportsPRInt16Impl::GetData(PRInt16 *aData)
{
    NS_ASSERTION(aData, "Bad pointer");
    *aData = mData;
    return NS_OK;
}

NS_IMETHODIMP nsSupportsPRInt16Impl::SetData(PRInt16 aData)
{
    mData = aData;
    return NS_OK;
}

NS_IMETHODIMP nsSupportsPRInt16Impl::ToString(char **_retval)
{
    NS_ASSERTION(_retval, "Bad pointer");
    static const int size = 8;
    char buf[size];

    PR_snprintf(buf, size, "%d", mData);

    char* result = (char*) nsMemory::Clone(buf,
                                (strlen(buf)+1)*sizeof(char));
    *_retval = result;
    return result ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}  

/***************************************************************************/

NS_IMPL_ISUPPORTS2(nsSupportsPRInt32Impl, nsISupportsPRInt32,
                   nsISupportsPrimitive)

nsSupportsPRInt32Impl::nsSupportsPRInt32Impl()
    : mData(0)
{
}

NS_IMETHODIMP nsSupportsPRInt32Impl::GetType(PRUint16 *aType)
{
    NS_ASSERTION(aType, "Bad pointer");
    *aType = TYPE_PRINT32;

    return NS_OK;
}

NS_IMETHODIMP nsSupportsPRInt32Impl::GetData(PRInt32 *aData)
{
    NS_ASSERTION(aData, "Bad pointer");
    *aData = mData;
    return NS_OK;
}

NS_IMETHODIMP nsSupportsPRInt32Impl::SetData(PRInt32 aData)
{
    mData = aData;
    return NS_OK;
}

NS_IMETHODIMP nsSupportsPRInt32Impl::ToString(char **_retval)
{
    NS_ASSERTION(_retval, "Bad pointer");
    static const int size = 16;
    char buf[size];

    PR_snprintf(buf, size, "%ld", mData);

    char* result = (char*) nsMemory::Clone(buf,
                                (strlen(buf)+1)*sizeof(char));
    *_retval = result;
    return result ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}  

/***************************************************************************/

NS_IMPL_ISUPPORTS2(nsSupportsPRInt64Impl, nsISupportsPRInt64,
                   nsISupportsPrimitive)

nsSupportsPRInt64Impl::nsSupportsPRInt64Impl()
    : mData(LL_ZERO)
{
}

NS_IMETHODIMP nsSupportsPRInt64Impl::GetType(PRUint16 *aType)
{
    NS_ASSERTION(aType, "Bad pointer");
    *aType = TYPE_PRINT64;

    return NS_OK;
}

NS_IMETHODIMP nsSupportsPRInt64Impl::GetData(PRInt64 *aData)
{
    NS_ASSERTION(aData, "Bad pointer");
    *aData = mData;
    return NS_OK;
}

NS_IMETHODIMP nsSupportsPRInt64Impl::SetData(PRInt64 aData)
{
    mData = aData;
    return NS_OK;
}

NS_IMETHODIMP nsSupportsPRInt64Impl::ToString(char **_retval)
{
    NS_ASSERTION(_retval, "Bad pointer");
    static const int size = 32;
    char buf[size];

    PR_snprintf(buf, size, "%lld", mData);

    char* result = (char*) nsMemory::Clone(buf,
                                (strlen(buf)+1)*sizeof(char));
    *_retval = result;
    return result ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}  

/***************************************************************************/

NS_IMPL_ISUPPORTS2(nsSupportsFloatImpl, nsISupportsFloat,
                   nsISupportsPrimitive)

nsSupportsFloatImpl::nsSupportsFloatImpl()
    : mData(float(0.0))
{
}

NS_IMETHODIMP nsSupportsFloatImpl::GetType(PRUint16 *aType)
{
    NS_ASSERTION(aType, "Bad pointer");
    *aType = TYPE_FLOAT;

    return NS_OK;
}

NS_IMETHODIMP nsSupportsFloatImpl::GetData(float *aData)
{
    NS_ASSERTION(aData, "Bad pointer");
    *aData = mData;
    return NS_OK;
}

NS_IMETHODIMP nsSupportsFloatImpl::SetData(float aData)
{
    mData = aData;
    return NS_OK;
}

NS_IMETHODIMP nsSupportsFloatImpl::ToString(char **_retval)
{
    NS_ASSERTION(_retval, "Bad pointer");
    static const int size = 32;
    char buf[size];

    PR_snprintf(buf, size, "%f", (double) mData);

    char* result = (char*) nsMemory::Clone(buf,
                                (strlen(buf)+1)*sizeof(char));
    *_retval = result;
    return result ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}  

/***************************************************************************/

NS_IMPL_ISUPPORTS2(nsSupportsDoubleImpl, nsISupportsDouble,
                   nsISupportsPrimitive)

nsSupportsDoubleImpl::nsSupportsDoubleImpl()
    : mData(double(0.0))
{
}

NS_IMETHODIMP nsSupportsDoubleImpl::GetType(PRUint16 *aType)
{
    NS_ASSERTION(aType, "Bad pointer");
    *aType = TYPE_DOUBLE;

    return NS_OK;
}

NS_IMETHODIMP nsSupportsDoubleImpl::GetData(double *aData)
{
    NS_ASSERTION(aData, "Bad pointer");
    *aData = mData;
    return NS_OK;
}

NS_IMETHODIMP nsSupportsDoubleImpl::SetData(double aData)
{
    mData = aData;
    return NS_OK;
}

NS_IMETHODIMP nsSupportsDoubleImpl::ToString(char **_retval)
{
    NS_ASSERTION(_retval, "Bad pointer");
    static const int size = 32;
    char buf[size];

    PR_snprintf(buf, size, "%f", mData);

    char* result = (char*) nsMemory::Clone(buf,
                                (strlen(buf)+1)*sizeof(char));
    *_retval = result;
    return  result ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}  

/***************************************************************************/


NS_IMPL_THREADSAFE_ISUPPORTS2(nsSupportsVoidImpl, nsISupportsVoid,
                              nsISupportsPrimitive)

nsSupportsVoidImpl::nsSupportsVoidImpl()
    : mData(nsnull)
{
}

NS_IMETHODIMP nsSupportsVoidImpl::GetType(PRUint16 *aType)
{
    NS_ASSERTION(aType, "Bad pointer");
    *aType = TYPE_VOID;

    return NS_OK;
}

NS_IMETHODIMP nsSupportsVoidImpl::GetData(void * *aData)
{
    NS_ASSERTION(aData, "Bad pointer");
    *aData = mData;
    return NS_OK;
}

NS_IMETHODIMP nsSupportsVoidImpl::SetData(void * aData)
{
    mData = aData;
    return NS_OK;
}

NS_IMETHODIMP nsSupportsVoidImpl::ToString(char **_retval)
{
    NS_ASSERTION(_retval, "Bad pointer");

    static const char str[] = "[raw data]";
    char* result = (char*) nsMemory::Clone(str, sizeof(str));
    *_retval = result;
    return  result ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}  

/***************************************************************************/


NS_IMPL_THREADSAFE_ISUPPORTS2(nsSupportsInterfacePointerImpl,
                              nsISupportsInterfacePointer,
                              nsISupportsPrimitive)

nsSupportsInterfacePointerImpl::nsSupportsInterfacePointerImpl()
    : mIID(nsnull)
{
}

nsSupportsInterfacePointerImpl::~nsSupportsInterfacePointerImpl()
{
    if (mIID) {
        nsMemory::Free(mIID);
    }
}

NS_IMETHODIMP nsSupportsInterfacePointerImpl::GetType(PRUint16 *aType)
{
    NS_ASSERTION(aType, "Bad pointer");
    *aType = TYPE_INTERFACE_POINTER;

    return NS_OK;
}

NS_IMETHODIMP nsSupportsInterfacePointerImpl::GetData(nsISupports **aData)
{
    NS_ASSERTION(aData,"Bad pointer");

    *aData = mData;
    NS_IF_ADDREF(*aData);

    return NS_OK;
}

NS_IMETHODIMP nsSupportsInterfacePointerImpl::SetData(nsISupports * aData)
{
    mData = aData;

    return NS_OK;
}

NS_IMETHODIMP nsSupportsInterfacePointerImpl::GetDataIID(nsID **aIID)
{
    NS_ASSERTION(aIID,"Bad pointer");

    if(mIID)
    {
        *aIID = (nsID*) nsMemory::Clone(mIID, sizeof(nsID));
        return *aIID ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
    }
    *aIID = nsnull;
    return NS_OK;
}

NS_IMETHODIMP nsSupportsInterfacePointerImpl::SetDataIID(const nsID *aIID)
{
    if(mIID)
        nsMemory::Free(mIID);
    if(aIID)
        mIID = (nsID*) nsMemory::Clone(aIID, sizeof(nsID));
    else
        mIID = nsnull;

    return NS_OK;
}

NS_IMETHODIMP nsSupportsInterfacePointerImpl::ToString(char **_retval)
{
    NS_ASSERTION(_retval, "Bad pointer");

    static const char str[] = "[interface pointer]";

    // jband sez: think about asking nsIInterfaceInfoManager whether
    // the interface has a known human-readable name
    char* result = (char*) nsMemory::Clone(str, sizeof(str));
    *_retval = result;
    return  result ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

/***************************************************************************/

NS_IMPL_ISUPPORTS2(nsSupportsDependentCString,nsISupportsCString,nsISupportsPrimitive)

nsSupportsDependentCString::nsSupportsDependentCString(const char* aStr)
    : mData(aStr)
{ }

NS_IMETHODIMP
nsSupportsDependentCString::GetType(PRUint16 *aType)
{
    NS_ENSURE_ARG_POINTER(aType);

    *aType = TYPE_CSTRING;
    return NS_OK;
}

NS_IMETHODIMP
nsSupportsDependentCString::GetData(nsACString& aData)
{
    aData = mData;
    return NS_OK;
}

NS_IMETHODIMP
nsSupportsDependentCString::ToString(char **_retval)
{
    NS_ENSURE_ARG_POINTER(_retval);

    *_retval = ToNewCString(mData);
    if (!*_retval)
        return NS_ERROR_OUT_OF_MEMORY;
    
    return NS_OK;
}

NS_IMETHODIMP
nsSupportsDependentCString::SetData(const nsACString& aData)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
