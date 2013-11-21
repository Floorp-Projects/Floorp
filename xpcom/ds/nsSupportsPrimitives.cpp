/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsSupportsPrimitives.h"
#include "nsMemory.h"
#include "prprf.h"

/***************************************************************************/

NS_IMPL_ISUPPORTS2(nsSupportsIDImpl, nsISupportsID, nsISupportsPrimitive)

nsSupportsIDImpl::nsSupportsIDImpl()
    : mData(nullptr)
{
}

NS_IMETHODIMP nsSupportsIDImpl::GetType(uint16_t *aType)
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
    *aData = nullptr;
    return NS_OK;
}

NS_IMETHODIMP nsSupportsIDImpl::SetData(const nsID *aData)
{
    if(mData)
      nsMemory::Free(mData);
    if(aData)
        mData = (nsID*) nsMemory::Clone(aData, sizeof(nsID));
    else
        mData = nullptr;
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

NS_IMETHODIMP nsSupportsCStringImpl::GetType(uint16_t *aType)
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

NS_IMETHODIMP nsSupportsStringImpl::GetType(uint16_t *aType)
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

NS_IMPL_ISUPPORTS2(nsSupportsPRBoolImpl, nsISupportsPRBool,
                   nsISupportsPrimitive)

nsSupportsPRBoolImpl::nsSupportsPRBoolImpl()
    : mData(false)
{
}

NS_IMETHODIMP nsSupportsPRBoolImpl::GetType(uint16_t *aType)
{
    NS_ASSERTION(aType, "Bad pointer");
    *aType = TYPE_PRBOOL;

    return NS_OK;
}

NS_IMETHODIMP nsSupportsPRBoolImpl::GetData(bool *aData)
{
    NS_ASSERTION(aData, "Bad pointer");
    *aData = mData;
    return NS_OK;
}

NS_IMETHODIMP nsSupportsPRBoolImpl::SetData(bool aData)
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

NS_IMETHODIMP nsSupportsPRUint8Impl::GetType(uint16_t *aType)
{
    NS_ASSERTION(aType, "Bad pointer");
    *aType = TYPE_PRUINT8;

    return NS_OK;
}

NS_IMETHODIMP nsSupportsPRUint8Impl::GetData(uint8_t *aData)
{
    NS_ASSERTION(aData, "Bad pointer");
    *aData = mData;
    return NS_OK;
}

NS_IMETHODIMP nsSupportsPRUint8Impl::SetData(uint8_t aData)
{
    mData = aData;
    return NS_OK;
}

NS_IMETHODIMP nsSupportsPRUint8Impl::ToString(char **_retval)
{
    NS_ASSERTION(_retval, "Bad pointer");
    static const int size = 8;
    char buf[size];

    PR_snprintf(buf, size, "%u", (uint16_t) mData);

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

NS_IMETHODIMP nsSupportsPRUint16Impl::GetType(uint16_t *aType)
{
    NS_ASSERTION(aType, "Bad pointer");
    *aType = TYPE_PRUINT16;

    return NS_OK;
}

NS_IMETHODIMP nsSupportsPRUint16Impl::GetData(uint16_t *aData)
{
    NS_ASSERTION(aData, "Bad pointer");
    *aData = mData;
    return NS_OK;
}

NS_IMETHODIMP nsSupportsPRUint16Impl::SetData(uint16_t aData)
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

NS_IMETHODIMP nsSupportsPRUint32Impl::GetType(uint16_t *aType)
{
    NS_ASSERTION(aType, "Bad pointer");
    *aType = TYPE_PRUINT32;

    return NS_OK;
}

NS_IMETHODIMP nsSupportsPRUint32Impl::GetData(uint32_t *aData)
{
    NS_ASSERTION(aData, "Bad pointer");
    *aData = mData;
    return NS_OK;
}

NS_IMETHODIMP nsSupportsPRUint32Impl::SetData(uint32_t aData)
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
    : mData(0)
{
}

NS_IMETHODIMP nsSupportsPRUint64Impl::GetType(uint16_t *aType)
{
    NS_ASSERTION(aType, "Bad pointer");
    *aType = TYPE_PRUINT64;

    return NS_OK;
}

NS_IMETHODIMP nsSupportsPRUint64Impl::GetData(uint64_t *aData)
{
    NS_ASSERTION(aData, "Bad pointer");
    *aData = mData;
    return NS_OK;
}

NS_IMETHODIMP nsSupportsPRUint64Impl::SetData(uint64_t aData)
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
    : mData(0)
{
}

NS_IMETHODIMP nsSupportsPRTimeImpl::GetType(uint16_t *aType)
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

NS_IMETHODIMP nsSupportsCharImpl::GetType(uint16_t *aType)
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

    if(nullptr != (result = (char*) nsMemory::Alloc(2*sizeof(char))))
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

NS_IMETHODIMP nsSupportsPRInt16Impl::GetType(uint16_t *aType)
{
    NS_ASSERTION(aType, "Bad pointer");
    *aType = TYPE_PRINT16;

    return NS_OK;
}

NS_IMETHODIMP nsSupportsPRInt16Impl::GetData(int16_t *aData)
{
    NS_ASSERTION(aData, "Bad pointer");
    *aData = mData;
    return NS_OK;
}

NS_IMETHODIMP nsSupportsPRInt16Impl::SetData(int16_t aData)
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

NS_IMETHODIMP nsSupportsPRInt32Impl::GetType(uint16_t *aType)
{
    NS_ASSERTION(aType, "Bad pointer");
    *aType = TYPE_PRINT32;

    return NS_OK;
}

NS_IMETHODIMP nsSupportsPRInt32Impl::GetData(int32_t *aData)
{
    NS_ASSERTION(aData, "Bad pointer");
    *aData = mData;
    return NS_OK;
}

NS_IMETHODIMP nsSupportsPRInt32Impl::SetData(int32_t aData)
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
    : mData(0)
{
}

NS_IMETHODIMP nsSupportsPRInt64Impl::GetType(uint16_t *aType)
{
    NS_ASSERTION(aType, "Bad pointer");
    *aType = TYPE_PRINT64;

    return NS_OK;
}

NS_IMETHODIMP nsSupportsPRInt64Impl::GetData(int64_t *aData)
{
    NS_ASSERTION(aData, "Bad pointer");
    *aData = mData;
    return NS_OK;
}

NS_IMETHODIMP nsSupportsPRInt64Impl::SetData(int64_t aData)
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

NS_IMETHODIMP nsSupportsFloatImpl::GetType(uint16_t *aType)
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

NS_IMETHODIMP nsSupportsDoubleImpl::GetType(uint16_t *aType)
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


NS_IMPL_ISUPPORTS2(nsSupportsVoidImpl, nsISupportsVoid,
                   nsISupportsPrimitive)

nsSupportsVoidImpl::nsSupportsVoidImpl()
    : mData(nullptr)
{
}

NS_IMETHODIMP nsSupportsVoidImpl::GetType(uint16_t *aType)
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


NS_IMPL_ISUPPORTS2(nsSupportsInterfacePointerImpl,
                   nsISupportsInterfacePointer,
                   nsISupportsPrimitive)

nsSupportsInterfacePointerImpl::nsSupportsInterfacePointerImpl()
    : mIID(nullptr)
{
}

nsSupportsInterfacePointerImpl::~nsSupportsInterfacePointerImpl()
{
    if (mIID) {
        nsMemory::Free(mIID);
    }
}

NS_IMETHODIMP nsSupportsInterfacePointerImpl::GetType(uint16_t *aType)
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
    *aIID = nullptr;
    return NS_OK;
}

NS_IMETHODIMP nsSupportsInterfacePointerImpl::SetDataIID(const nsID *aIID)
{
    if(mIID)
        nsMemory::Free(mIID);
    if(aIID)
        mIID = (nsID*) nsMemory::Clone(aIID, sizeof(nsID));
    else
        mIID = nullptr;

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
nsSupportsDependentCString::GetType(uint16_t *aType)
{
    if (NS_WARN_IF(!aType))
        return NS_ERROR_INVALID_ARG;

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
    if (NS_WARN_IF(!_retval))
        return NS_ERROR_INVALID_ARG;

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
