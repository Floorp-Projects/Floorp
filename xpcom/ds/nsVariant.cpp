/* -*- Mode: IDL; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *   John Bandhauer <jband@netscape.com>
 */

/* The long avoided variant support for xpcom. */

#include "nsVariant.h"
#include "nsReadableUtils.h"
#include "prprf.h"
#include "prdtoa.h"
#include <math.h>

/***************************************************************************/
// Helpers for static convert functions...

static nsresult String2Double(const char* aString, double* retval)
{
    char* next;
    double value = PR_strtod(aString, &next);
    if(next == aString)
        return NS_ERROR_CANNOT_CONVERT_DATA;
    *retval = value;
    return NS_OK;
}

static nsresult AString2Double(const nsAString& aString, double* retval)
{
    char* pChars = ToNewCString(aString);
    if(!pChars)
        return NS_ERROR_OUT_OF_MEMORY;
    nsresult rv = String2Double(pChars, retval);
    nsMemory::Free(pChars);
    return rv;
}    

// Fills outVariant with double, PRUint32, or PRInt32.
// Returns NS_OK, an error code, or a non-NS_OK success code
static nsresult ToManageableNumber(const nsDiscriminatedUnion& inData, 
                                   nsDiscriminatedUnion* outData)
{
    nsAutoString tempString;
    nsresult rv;

    switch(inData.mType)
    {
    // This group results in a PRInt32...

#define CASE__NUMBER_INT32(type_, member_)                                    \
    case nsIDataType :: type_ :                                               \
        outData->u.mInt32Value = inData.u. member_ ;                          \
        outData->mType = nsIDataType::VTYPE_INT32;                            \
        return NS_OK;

    CASE__NUMBER_INT32(VTYPE_INT8,   mInt8Value)
    CASE__NUMBER_INT32(VTYPE_INT16,  mInt16Value)
    CASE__NUMBER_INT32(VTYPE_INT32,  mInt32Value)
    CASE__NUMBER_INT32(VTYPE_UINT8,  mUint8Value)
    CASE__NUMBER_INT32(VTYPE_UINT16, mUint16Value)
    CASE__NUMBER_INT32(VTYPE_BOOL,   mBoolValue)
    CASE__NUMBER_INT32(VTYPE_CHAR,   mCharValue)
    CASE__NUMBER_INT32(VTYPE_WCHAR,  mWCharValue)
      
#undef CASE__NUMBER_INT32
            
    // This group results in a PRUint32...
    
    case nsIDataType::VTYPE_UINT32:
        outData->u.mInt32Value = inData.u.mUint32Value;
        outData->mType = nsIDataType::VTYPE_INT32;
        return NS_OK;
               
    // This group results in a double...

    case nsIDataType::VTYPE_INT64:        
    case nsIDataType::VTYPE_UINT64:
        // XXX Need boundary checking here. 
        // We may need to return NS_SUCCESS_LOSS_OF_INSIGNIFICANT_DATA
        LL_L2D(outData->u.mDoubleValue, inData.u.mInt64Value);
        outData->mType = nsIDataType::VTYPE_DOUBLE;
        return NS_OK;
    case nsIDataType::VTYPE_FLOAT:        
        outData->u.mDoubleValue = inData.u.mFloatValue;
        outData->mType = nsIDataType::VTYPE_DOUBLE;
        return NS_OK;
    case nsIDataType::VTYPE_DOUBLE:        
        outData->u.mDoubleValue = inData.u.mDoubleValue;
        outData->mType = nsIDataType::VTYPE_DOUBLE;
        return NS_OK;
    case nsIDataType::VTYPE_CHAR_STR:
    case nsIDataType::VTYPE_STRING_SIZE_IS:
        rv = String2Double(inData.u.str.mStringValue, &outData->u.mDoubleValue);
        if(NS_FAILED(rv))
            return rv;
        outData->mType = nsIDataType::VTYPE_DOUBLE;
        return NS_OK;
    case nsIDataType::VTYPE_ASTRING:
        rv = AString2Double(*inData.u.mAStringValue, &outData->u.mDoubleValue);
        if(NS_FAILED(rv))
            return rv;
        outData->mType = nsIDataType::VTYPE_DOUBLE;
        return NS_OK;
    case nsIDataType::VTYPE_WCHAR_STR:
    case nsIDataType::VTYPE_WSTRING_SIZE_IS:
        tempString.Assign(inData.u.wstr.mWStringValue);
        rv = AString2Double(tempString, &outData->u.mDoubleValue);
        if(NS_FAILED(rv))
            return rv;
        outData->mType = nsIDataType::VTYPE_DOUBLE;
        return NS_OK;
    
    // This group fails...

    case nsIDataType::VTYPE_VOID:        
    case nsIDataType::VTYPE_ID:        
    case nsIDataType::VTYPE_INTERFACE:        
    case nsIDataType::VTYPE_INTERFACE_IS:        
    case nsIDataType::VTYPE_ARRAY:        
    case nsIDataType::VTYPE_EMPTY:
    default:
        return NS_ERROR_CANNOT_CONVERT_DATA;
    }
}

/***************************************************************************/
// Array helpers...

static void FreeArray(nsDiscriminatedUnion* data)
{
    NS_ASSERTION(data->mType == nsIDataType::VTYPE_ARRAY, "bad FreeArray call");
    NS_ASSERTION(data->u.array.mArrayValue, "bad array");
    NS_ASSERTION(data->u.array.mArrayCount, "bad array count");

#define CASE__FREE_ARRAY_PTR(type_, ctype_)                                   \
        case nsIDataType:: type_ :                                            \
        {                                                                     \
            ctype_ ** p = (ctype_ **) data->u.array.mArrayValue;              \
            for(PRUint32 i = data->u.array.mArrayCount; i > 0; p++, i--)      \
                if(*p)                                                        \
                    nsMemory::Free((char*)*p);                                \
            break;                                                            \
        }

#define CASE__FREE_ARRAY_IFACE(type_, ctype_)                                 \
        case nsIDataType:: type_ :                                            \
        {                                                                     \
            ctype_ ** p = (ctype_ **) data->u.array.mArrayValue;              \
            for(PRUint32 i = data->u.array.mArrayCount; i > 0; p++, i--)      \
                if(*p)                                                        \
                    (*p)->Release();                                          \
            break;                                                            \
        }
    
    switch(data->u.array.mArrayType)
    {
        case nsIDataType::VTYPE_INT8:        
        case nsIDataType::VTYPE_INT16:        
        case nsIDataType::VTYPE_INT32:        
        case nsIDataType::VTYPE_INT64:        
        case nsIDataType::VTYPE_UINT8:        
        case nsIDataType::VTYPE_UINT16:        
        case nsIDataType::VTYPE_UINT32:        
        case nsIDataType::VTYPE_UINT64:        
        case nsIDataType::VTYPE_FLOAT:        
        case nsIDataType::VTYPE_DOUBLE:        
        case nsIDataType::VTYPE_BOOL:        
        case nsIDataType::VTYPE_CHAR:        
        case nsIDataType::VTYPE_WCHAR:        
            break;

        // XXX We ASSUME that "array of nsID" means "array of pointers to nsID".
        CASE__FREE_ARRAY_PTR(VTYPE_ID, nsID)
        CASE__FREE_ARRAY_PTR(VTYPE_CHAR_STR, char)
        CASE__FREE_ARRAY_PTR(VTYPE_WCHAR_STR, PRUnichar)
        CASE__FREE_ARRAY_IFACE(VTYPE_INTERFACE, nsISupports)

        // The rest are illegal.
        case nsIDataType::VTYPE_VOID:        
        case nsIDataType::VTYPE_INTERFACE_IS:        
        case nsIDataType::VTYPE_ASTRING:        
        case nsIDataType::VTYPE_WSTRING_SIZE_IS:        
        case nsIDataType::VTYPE_STRING_SIZE_IS:        
        case nsIDataType::VTYPE_ARRAY:
        case nsIDataType::VTYPE_EMPTY:
        default:
            NS_ERROR("bad type in array!");
            break;
    }
    
    // Free the array memory.
    nsMemory::Free((char*)data->u.array.mArrayValue);

#undef CASE__FREE_ARRAY_PTR
#undef CASE__FREE_ARRAY_IFACE
}    

static nsresult CloneArray(PRUint16 inType, const nsIID* inIID, 
                           PRUint32 inCount, void* inValue,
                           PRUint16* outType, nsIID* outIID, 
                           PRUint32* outCount, void** outValue)
{
    NS_ASSERTION(inCount, "bad param");
    NS_ASSERTION(inValue, "bad param");
    NS_ASSERTION(outType, "bad param");
    NS_ASSERTION(outCount, "bad param");
    NS_ASSERTION(outValue, "bad param");

    PRUint32 allocatedValueCount = 0;
    nsresult rv = NS_OK;
    PRUint32 i;

    // First we figure out the size of the elements for the new u.array.
    
    size_t elementSize;
    size_t allocSize;

    switch(inType)
    {
        case nsIDataType::VTYPE_INT8:        
            elementSize = sizeof(PRInt8); 
            break;
        case nsIDataType::VTYPE_INT16:        
            elementSize = sizeof(PRInt16); 
            break;
        case nsIDataType::VTYPE_INT32:        
            elementSize = sizeof(PRInt32); 
            break;
        case nsIDataType::VTYPE_INT64:        
            elementSize = sizeof(PRInt64); 
            break;
        case nsIDataType::VTYPE_UINT8:        
            elementSize = sizeof(PRUint8); 
            break;
        case nsIDataType::VTYPE_UINT16:        
            elementSize = sizeof(PRUint16); 
            break;
        case nsIDataType::VTYPE_UINT32:        
            elementSize = sizeof(PRUint32); 
            break;
        case nsIDataType::VTYPE_UINT64:        
            elementSize = sizeof(PRUint64); 
            break;
        case nsIDataType::VTYPE_FLOAT:        
            elementSize = sizeof(float); 
            break;
        case nsIDataType::VTYPE_DOUBLE:        
            elementSize = sizeof(double); 
            break;
        case nsIDataType::VTYPE_BOOL:        
            elementSize = sizeof(PRBool); 
            break;
        case nsIDataType::VTYPE_CHAR:        
            elementSize = sizeof(char); 
            break;
        case nsIDataType::VTYPE_WCHAR:        
            elementSize = sizeof(PRUnichar); 
            break;

        // XXX We ASSUME that "array of nsID" means "array of pointers to nsID".
        case nsIDataType::VTYPE_ID:        
        case nsIDataType::VTYPE_CHAR_STR:        
        case nsIDataType::VTYPE_WCHAR_STR:        
        case nsIDataType::VTYPE_INTERFACE:        
        case nsIDataType::VTYPE_INTERFACE_IS:        
            elementSize = sizeof(void*); 
            break;

        // The rest are illegal.
        case nsIDataType::VTYPE_ASTRING:        
        case nsIDataType::VTYPE_STRING_SIZE_IS:        
        case nsIDataType::VTYPE_WSTRING_SIZE_IS:        
        case nsIDataType::VTYPE_VOID:        
        case nsIDataType::VTYPE_ARRAY:
        case nsIDataType::VTYPE_EMPTY:
        default:
            NS_ERROR("bad type in array!");
            return NS_ERROR_CANNOT_CONVERT_DATA;
    }

    
    // Alloc the u.array.

    allocSize = inCount * elementSize;
    *outValue = nsMemory::Alloc(allocSize);
    if(!*outValue)
        return NS_ERROR_OUT_OF_MEMORY;

    // Clone the elements.

    switch(inType)
    {
        case nsIDataType::VTYPE_INT8:        
        case nsIDataType::VTYPE_INT16:        
        case nsIDataType::VTYPE_INT32:        
        case nsIDataType::VTYPE_INT64:        
        case nsIDataType::VTYPE_UINT8:        
        case nsIDataType::VTYPE_UINT16:        
        case nsIDataType::VTYPE_UINT32:        
        case nsIDataType::VTYPE_UINT64:        
        case nsIDataType::VTYPE_FLOAT:        
        case nsIDataType::VTYPE_DOUBLE:        
        case nsIDataType::VTYPE_BOOL:        
        case nsIDataType::VTYPE_CHAR:        
        case nsIDataType::VTYPE_WCHAR:        
            memcpy(*outValue, inValue, allocSize);
            break;

        case nsIDataType::VTYPE_INTERFACE_IS:        
            if(outIID)
                *outIID = *inIID;
            // fall through...
        case nsIDataType::VTYPE_INTERFACE:        
        {
            memcpy(*outValue, inValue, allocSize);
            
            nsISupports** p = (nsISupports**) *outValue;
            for(i = inCount; i > 0; p++, i--)
                if(*p)
                    (*p)->AddRef();
            break;
        }

        // XXX We ASSUME that "array of nsID" means "array of pointers to nsID".
        case nsIDataType::VTYPE_ID:
        {
            nsID** inp  = (nsID**) inValue;
            nsID** outp = (nsID**) *outValue;
            for(i = inCount; i > 0; i--)
            {
                nsID* idp = *(inp++);
                if(idp)
                {
                    if(nsnull == (*(outp++) = (nsID*)
                       nsMemory::Clone((char*)idp, sizeof(nsID))))
                        goto bad;
                }
                else
                    *(outp++) = nsnull;
                allocatedValueCount++;
            }
            break;
        }
    
        case nsIDataType::VTYPE_CHAR_STR:        
        {
            char** inp  = (char**) inValue;
            char** outp = (char**) *outValue;
            for(i = inCount; i > 0; i--)
            {
                char* str = *(inp++);
                if(str)
                {
                    if(nsnull == (*(outp++) = (char*)
                       nsMemory::Clone(str, (strlen(str)+1)*sizeof(char))))
                        goto bad;
                }
                else
                    *(outp++) = nsnull;
                allocatedValueCount++;
            }
            break;
        }

        case nsIDataType::VTYPE_WCHAR_STR:        
        {
            PRUnichar** inp  = (PRUnichar**) inValue;
            PRUnichar** outp = (PRUnichar**) *outValue;
            for(i = inCount; i > 0; i--)
            {
                PRUnichar* str = *(inp++);
                if(str)
                {
                    if(nsnull == (*(outp++) = (PRUnichar*)
                       nsMemory::Clone(str, 
                        (nsCRT::strlen(str)+1)*sizeof(PRUnichar))))
                        goto bad;
                }
                else
                    *(outp++) = nsnull;
                allocatedValueCount++;
            }
            break;
        }

        // The rest are illegal.
        case nsIDataType::VTYPE_VOID:        
        case nsIDataType::VTYPE_ARRAY:
        case nsIDataType::VTYPE_EMPTY:
        case nsIDataType::VTYPE_ASTRING:        
        case nsIDataType::VTYPE_STRING_SIZE_IS:        
        case nsIDataType::VTYPE_WSTRING_SIZE_IS:        
        default:
            NS_ERROR("bad type in array!");
            return NS_ERROR_CANNOT_CONVERT_DATA;
    }

    *outType = inType;
    *outCount = inCount;
    return NS_OK;

bad:
    if(*outValue)
    {
        char** p = (char**) *outValue;
        for(i = allocatedValueCount; i > 0; p++, i--)
            if(*p)
                nsMemory::Free(*p);
        nsMemory::Free((char*)*outValue);
        *outValue = nsnull;
    }
    return rv;
}

/***************************************************************************/

#define TRIVIAL_DATA_CONVERTER(type_, data_, member_, retval_)                \
    if(data_.mType == nsIDataType :: type_) {                                 \
        *retval_ = data_.u.member_;                                           \
        return NS_OK;                                                         \
    }

#define NUMERIC_CONVERSION_METHOD_BEGIN(type_, Ctype_, name_)                 \
/* static */ nsresult                                                         \
nsVariant::ConvertTo##name_ (const nsDiscriminatedUnion& data,                \
                             Ctype_ *_retval)                                 \
{                                                                             \
    TRIVIAL_DATA_CONVERTER(type_, data, m##name_##Value, _retval)             \
    nsDiscriminatedUnion tempData;                                            \
    nsVariant::Initialize(&tempData);                                         \
    nsresult rv = ToManageableNumber(data, &tempData);                        \
    /*                                                                     */ \
    /* NOTE: rv may indicate a success code that we want to preserve       */ \
    /* For the final return. So all the return cases below should return   */ \
    /* this rv when indicating success.                                    */ \
    /*                                                                     */ \
    if(NS_FAILED(rv))                                                         \
        return rv;                                                            \
    switch(tempData.mType)                                                    \
    {

#define CASE__NUMERIC_CONVERSION_INT32_JUST_CAST(Ctype_)                      \
    case nsIDataType::VTYPE_INT32:                                            \
        *_retval = ( Ctype_ ) tempData.u.mInt32Value;                         \
        return rv;

#define CASE__NUMERIC_CONVERSION_INT32_MIN_MAX(Ctype_, min_, max_)            \
    case nsIDataType::VTYPE_INT32:                                            \
    {                                                                         \
        PRInt32 value = tempData.u.mInt32Value;                               \
        if(value < min_ || value > max_)                                      \
            return NS_ERROR_LOSS_OF_SIGNIFICANT_DATA;                         \
        *_retval = ( Ctype_ ) value;                                          \
        return rv;                                                            \
    }

#define CASE__NUMERIC_CONVERSION_UINT32_JUST_CAST(Ctype_)                     \
    case nsIDataType::VTYPE_UINT32:                                           \
        *_retval = ( Ctype_ ) tempData.u.mUint32Value;                        \
        return rv;

#define CASE__NUMERIC_CONVERSION_UINT32_MAX(Ctype_, max_)                     \
    case nsIDataType::VTYPE_UINT32:                                           \
    {                                                                         \
        PRUint32 value = tempData.u.mUint32Value;                             \
        if(value > max_)                                                      \
            return NS_ERROR_LOSS_OF_SIGNIFICANT_DATA;                         \
        *_retval = ( Ctype_ ) value;                                          \
        return rv;                                                            \
    }

#define CASE__NUMERIC_CONVERSION_DOUBLE_JUST_CAST(Ctype_)                     \
    case nsIDataType::VTYPE_DOUBLE:                                           \
        *_retval = ( Ctype_ ) tempData.u.mDoubleValue;                        \
        return rv;

#define CASE__NUMERIC_CONVERSION_DOUBLE_MIN_MAX(Ctype_, min_, max_)           \
    case nsIDataType::VTYPE_DOUBLE:                                           \
    {                                                                         \
        double value = tempData.u.mDoubleValue;                               \
        if(value < min_ || value > max_)                                      \
            return NS_ERROR_LOSS_OF_SIGNIFICANT_DATA;                         \
        *_retval = ( Ctype_ ) value;                                          \
        return rv;                                                            \
    }

#define CASE__NUMERIC_CONVERSION_DOUBLE_MIN_MAX_INT(Ctype_, min_, max_)       \
    case nsIDataType::VTYPE_DOUBLE:                                           \
    {                                                                         \
        double value = tempData.u.mDoubleValue;                               \
        if(value < min_ || value > max_)                                      \
            return NS_ERROR_LOSS_OF_SIGNIFICANT_DATA;                         \
        *_retval = ( Ctype_ ) value;                                          \
        return (0.0 == fmod(value,1.0)) ?                                     \
            rv : NS_SUCCESS_LOSS_OF_INSIGNIFICANT_DATA;                       \
    }

#define CASES__NUMERIC_CONVERSION_NORMAL(Ctype_, min_, max_)                  \
    CASE__NUMERIC_CONVERSION_INT32_MIN_MAX(Ctype_, min_, max_)                \
    CASE__NUMERIC_CONVERSION_UINT32_MAX(Ctype_, max_)                         \
    CASE__NUMERIC_CONVERSION_DOUBLE_MIN_MAX_INT(Ctype_, min_, max_)

#define NUMERIC_CONVERSION_METHOD_END                                         \
    default:                                                                  \
        NS_ERROR("bad type returned from ToManageableNumber");                \
        return NS_ERROR_CANNOT_CONVERT_DATA;                                  \
    }                                                                         \
}

#define NUMERIC_CONVERSION_METHOD_NORMAL(type_, Ctype_, name_, min_, max_)    \
    NUMERIC_CONVERSION_METHOD_BEGIN(type_, Ctype_, name_)                     \
        CASES__NUMERIC_CONVERSION_NORMAL(Ctype_, min_, max_)                  \
    NUMERIC_CONVERSION_METHOD_END

/***************************************************************************/
// These expand into full public methods...

NUMERIC_CONVERSION_METHOD_NORMAL(VTYPE_INT8, PRUint8, Int8, (-127-1), 127)
NUMERIC_CONVERSION_METHOD_NORMAL(VTYPE_INT16, PRInt16, Int16, (-32767-1), 32767)

NUMERIC_CONVERSION_METHOD_BEGIN(VTYPE_INT32, PRInt32, Int32)
    CASE__NUMERIC_CONVERSION_INT32_JUST_CAST(PRInt32)
    CASE__NUMERIC_CONVERSION_UINT32_MAX(PRInt32, 2147483647)
    CASE__NUMERIC_CONVERSION_DOUBLE_MIN_MAX_INT(PRInt32, (-2147483647-1), 2147483647)
NUMERIC_CONVERSION_METHOD_END

NUMERIC_CONVERSION_METHOD_NORMAL(VTYPE_UINT8, PRUint8, Uint8, 0, 255)
NUMERIC_CONVERSION_METHOD_NORMAL(VTYPE_UINT16, PRUint16, Uint16, 0, 65535)

NUMERIC_CONVERSION_METHOD_BEGIN(VTYPE_UINT32, PRUint32, Uint32)
    CASE__NUMERIC_CONVERSION_INT32_MIN_MAX(PRUint32, 0, 2147483647)
    CASE__NUMERIC_CONVERSION_UINT32_JUST_CAST(PRUint32)
    CASE__NUMERIC_CONVERSION_DOUBLE_MIN_MAX_INT(PRUint32, 0, 4294967295U)
NUMERIC_CONVERSION_METHOD_END

// XXX toFloat convertions need to be fixed!
NUMERIC_CONVERSION_METHOD_BEGIN(VTYPE_FLOAT, float, Float)
    CASE__NUMERIC_CONVERSION_INT32_JUST_CAST(float)
    CASE__NUMERIC_CONVERSION_UINT32_JUST_CAST(float)
    CASE__NUMERIC_CONVERSION_DOUBLE_JUST_CAST(float)
NUMERIC_CONVERSION_METHOD_END

NUMERIC_CONVERSION_METHOD_BEGIN(VTYPE_DOUBLE, double, Double)
    CASE__NUMERIC_CONVERSION_INT32_JUST_CAST(double)
    CASE__NUMERIC_CONVERSION_UINT32_JUST_CAST(double)
    CASE__NUMERIC_CONVERSION_DOUBLE_JUST_CAST(double)
NUMERIC_CONVERSION_METHOD_END

// XXX toChar convertions need to be fixed!
NUMERIC_CONVERSION_METHOD_BEGIN(VTYPE_CHAR, char, Char)
    CASE__NUMERIC_CONVERSION_INT32_JUST_CAST(char)
    CASE__NUMERIC_CONVERSION_UINT32_JUST_CAST(char)
    CASE__NUMERIC_CONVERSION_DOUBLE_JUST_CAST(char)
NUMERIC_CONVERSION_METHOD_END

// XXX toWChar convertions need to be fixed!
NUMERIC_CONVERSION_METHOD_BEGIN(VTYPE_WCHAR, PRUnichar, WChar)
    CASE__NUMERIC_CONVERSION_INT32_JUST_CAST(PRUnichar)
    CASE__NUMERIC_CONVERSION_UINT32_JUST_CAST(PRUnichar)
    CASE__NUMERIC_CONVERSION_DOUBLE_JUST_CAST(PRUnichar)
NUMERIC_CONVERSION_METHOD_END

#undef NUMERIC_CONVERSION_METHOD_BEGIN
#undef CASE__NUMERIC_CONVERSION_INT32_JUST_CAST
#undef CASE__NUMERIC_CONVERSION_INT32_MIN_MAX
#undef CASE__NUMERIC_CONVERSION_UINT32_JUST_CAST
#undef CASE__NUMERIC_CONVERSION_UINT32_MIN_MAX
#undef CASE__NUMERIC_CONVERSION_DOUBLE_JUST_CAST
#undef CASE__NUMERIC_CONVERSION_DOUBLE_MIN_MAX
#undef CASE__NUMERIC_CONVERSION_DOUBLE_MIN_MAX_INT
#undef CASES__NUMERIC_CONVERSION_NORMAL
#undef NUMERIC_CONVERSION_METHOD_END
#undef NUMERIC_CONVERSION_METHOD_NORMAL

/***************************************************************************/

// Just leverage a numeric converter for bool (but restrict the values).
// XXX Is this really what we want to do?

/* static */ nsresult 
nsVariant::ConvertToBool(const nsDiscriminatedUnion& data, PRBool *_retval)
{
    TRIVIAL_DATA_CONVERTER(VTYPE_BOOL, data, mBoolValue, _retval)

    double val;
    nsresult rv = nsVariant::ConvertToDouble(data, &val);
    if(NS_FAILED(rv))
        return rv;
    *_retval = 0.0 != val;
    return rv;
}

/***************************************************************************/

/* static */ nsresult 
nsVariant::ConvertToInt64(const nsDiscriminatedUnion& data, PRInt64 *_retval)
{
    TRIVIAL_DATA_CONVERTER(VTYPE_INT64, data, mInt64Value, _retval)
    TRIVIAL_DATA_CONVERTER(VTYPE_UINT64, data, mUint64Value, _retval)

    nsDiscriminatedUnion tempData;
    nsVariant::Initialize(&tempData);
    nsresult rv = ToManageableNumber(data, &tempData);
    if(NS_FAILED(rv))
        return rv;
    switch(tempData.mType)
    {
    case nsIDataType::VTYPE_INT32:
        LL_I2L(*_retval, tempData.u.mInt32Value);
        return rv;
    case nsIDataType::VTYPE_UINT32:
        LL_UI2L(*_retval, tempData.u.mUint32Value);
        return rv;
    case nsIDataType::VTYPE_DOUBLE:
        // XXX should check for data loss here!
        LL_D2L(*_retval, tempData.u.mDoubleValue);
        return rv;
    default:
        NS_ERROR("bad type returned from ToManageableNumber");
        return NS_ERROR_CANNOT_CONVERT_DATA;
    }
}

/* static */ nsresult 
nsVariant::ConvertToUint64(const nsDiscriminatedUnion& data, PRUint64 *_retval)
{
    return nsVariant::ConvertToInt64(data, (PRInt64 *)_retval);
}

/***************************************************************************/

static PRBool String2ID(const nsDiscriminatedUnion& data, nsID* pid)
{
    nsAutoString tempString;
    nsAString* pString;

    switch(data.mType)
    {
        case nsIDataType::VTYPE_CHAR_STR:
        case nsIDataType::VTYPE_STRING_SIZE_IS:
            return pid->Parse(data.u.str.mStringValue);
        case nsIDataType::VTYPE_ASTRING:
            pString = data.u.mAStringValue;
            break;
        case nsIDataType::VTYPE_WCHAR_STR:
        case nsIDataType::VTYPE_WSTRING_SIZE_IS:
            tempString.Assign(data.u.wstr.mWStringValue);
            pString = &tempString;
            break;
        default:
            NS_ERROR("bad type in call to String2ID");
            return PR_FALSE;
    }
    
    char* pChars = ToNewCString(*pString);
    if(!pChars)
        return PR_FALSE;
    PRBool result = pid->Parse(pChars);
    nsMemory::Free(pChars);
    return result;
}

/* static */ nsresult 
nsVariant::ConvertToID(const nsDiscriminatedUnion& data, nsID * _retval)
{
    nsID id;

    switch(data.mType)
    {
    case nsIDataType::VTYPE_ID:
        *_retval = data.u.mIDValue;
        return NS_OK;
    case nsIDataType::VTYPE_INTERFACE:
        *_retval = NS_GET_IID(nsISupports);
        return NS_OK;
    case nsIDataType::VTYPE_INTERFACE_IS:
        *_retval = data.u.iface.mInterfaceID;
        return NS_OK;
    case nsIDataType::VTYPE_ASTRING:
    case nsIDataType::VTYPE_CHAR_STR:
    case nsIDataType::VTYPE_WCHAR_STR:
    case nsIDataType::VTYPE_STRING_SIZE_IS:
    case nsIDataType::VTYPE_WSTRING_SIZE_IS:
        if(!String2ID(data, &id))
            return NS_ERROR_CANNOT_CONVERT_DATA;
        *_retval = id;
        return NS_OK;
    default:
        return NS_ERROR_CANNOT_CONVERT_DATA;
    }
}

/***************************************************************************/

static nsresult ToString(const nsDiscriminatedUnion& data, 
                         nsACString & outString)
{
    char* ptr;

    switch(data.mType)
    {
    // all the stuff we don't handle...
    case nsIDataType::VTYPE_ASTRING:
    case nsIDataType::VTYPE_CHAR_STR:
    case nsIDataType::VTYPE_WCHAR_STR:
    case nsIDataType::VTYPE_STRING_SIZE_IS:
    case nsIDataType::VTYPE_WSTRING_SIZE_IS:
    case nsIDataType::VTYPE_WCHAR:        
        NS_ERROR("ToString being called for a string type - screwy logic!");
        // fall through...
    
    // XXX We might want stringified versions of these... ???
    
    case nsIDataType::VTYPE_VOID:        
    case nsIDataType::VTYPE_EMPTY:
    case nsIDataType::VTYPE_ARRAY:        
    case nsIDataType::VTYPE_INTERFACE:        
    case nsIDataType::VTYPE_INTERFACE_IS:        
    default:
        return NS_ERROR_CANNOT_CONVERT_DATA;

    // nsID has its own text formater.

    case nsIDataType::VTYPE_ID:        
        ptr = data.u.mIDValue.ToString();
        if(!ptr)
            return NS_ERROR_OUT_OF_MEMORY;
        outString.Assign(ptr);
        nsMemory::Free(ptr);
        return NS_OK;

    // the rest can be PR_smprintf'd and use common code.

#define CASE__SMPRINTF_NUMBER(type_, format_, cast_, member_)                 \
    case nsIDataType :: type_ :                                               \
        ptr = PR_smprintf( format_ , (cast_) data.u. member_ );               \
        break;

    CASE__SMPRINTF_NUMBER(VTYPE_INT8,   "%d",   int,      mInt8Value)
    CASE__SMPRINTF_NUMBER(VTYPE_INT16,  "%d",   int,      mInt16Value)
    CASE__SMPRINTF_NUMBER(VTYPE_INT32,  "%d",   int,      mInt32Value)
    CASE__SMPRINTF_NUMBER(VTYPE_INT64,  "%lld", PRInt64,  mInt64Value)
 
    CASE__SMPRINTF_NUMBER(VTYPE_UINT8,  "%u",   unsigned, mUint8Value)
    CASE__SMPRINTF_NUMBER(VTYPE_UINT16, "%u",   unsigned, mUint16Value)
    CASE__SMPRINTF_NUMBER(VTYPE_UINT32, "%u",   unsigned, mUint32Value)
    CASE__SMPRINTF_NUMBER(VTYPE_UINT64, "%llu", PRInt64,  mUint64Value)

    CASE__SMPRINTF_NUMBER(VTYPE_FLOAT,  "%f",   float,    mFloatValue)
    CASE__SMPRINTF_NUMBER(VTYPE_DOUBLE, "%f",   double,   mDoubleValue)

    // XXX Would we rather print "true" / "false" ?
    CASE__SMPRINTF_NUMBER(VTYPE_BOOL,   "%d",   int,      mBoolValue)

    CASE__SMPRINTF_NUMBER(VTYPE_CHAR,   "%c",   char,     mCharValue)

#undef CASE__SMPRINTF_NUMBER
    }

    if(!ptr)
        return NS_ERROR_OUT_OF_MEMORY;
    outString.Assign(ptr);
    PR_smprintf_free(ptr);
    return NS_OK;
}

/* static */ nsresult 
nsVariant::ConvertToAString(const nsDiscriminatedUnion& data, nsAWritableString & _retval)
{
    nsCAutoString tempCString;
    nsresult rv;
    
    switch(data.mType)
    {
    case nsIDataType::VTYPE_ASTRING:
        _retval.Assign(*data.u.mAStringValue);
        return NS_OK;
    case nsIDataType::VTYPE_CHAR_STR:
        tempCString.Assign(data.u.str.mStringValue);
        CopyASCIItoUCS2(tempCString, _retval);
        return NS_OK;
    case nsIDataType::VTYPE_WCHAR_STR:
        _retval.Assign(data.u.wstr.mWStringValue);
        return NS_OK;
    case nsIDataType::VTYPE_STRING_SIZE_IS:
        tempCString.Assign(data.u.str.mStringValue, data.u.str.mStringLength);
        CopyASCIItoUCS2(tempCString, _retval);
        return NS_OK;
    case nsIDataType::VTYPE_WSTRING_SIZE_IS:
        _retval.Assign(data.u.wstr.mWStringValue, data.u.wstr.mWStringLength);
        return NS_OK;
    case nsIDataType::VTYPE_WCHAR:        
        _retval.Assign(data.u.mWCharValue);
        return NS_OK;
    default:
        rv = ToString(data, tempCString);
        if(NS_FAILED(rv))
            return rv;
        CopyASCIItoUCS2(tempCString, _retval);
        return NS_OK;
    }
}

/* static */ nsresult 
nsVariant::ConvertToString(const nsDiscriminatedUnion& data, char **_retval)
{
    PRUint32 ignored;
    return nsVariant::ConvertToStringWithSize(data, &ignored, _retval);
}
/* static */ nsresult 
nsVariant::ConvertToWString(const nsDiscriminatedUnion& data, PRUnichar **_retval)
{
    PRUint32 ignored;
    return nsVariant::ConvertToWStringWithSize(data, &ignored, _retval);
}

/* static */ nsresult 
nsVariant::ConvertToStringWithSize(const nsDiscriminatedUnion& data, PRUint32 *size, char **str)
{
    nsAutoString  tempString;
    nsCAutoString tempCString;
    nsresult rv;

    switch(data.mType)
    {
    case nsIDataType::VTYPE_ASTRING:
        *size = data.u.mAStringValue->Length();
        *str = ToNewCString(*data.u.mAStringValue);
        return *str ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
    case nsIDataType::VTYPE_CHAR_STR:
        tempCString.Assign(data.u.str.mStringValue);
        *size = tempCString.Length();
        *str = ToNewCString(tempCString);
        return *str ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
    case nsIDataType::VTYPE_WCHAR_STR:
        tempString.Assign(data.u.wstr.mWStringValue);
        *size = tempString.Length();
        *str = ToNewCString(tempString);
        return *str ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
    case nsIDataType::VTYPE_STRING_SIZE_IS:
        tempCString.Assign(data.u.str.mStringValue, data.u.str.mStringLength);
        *size = tempCString.Length();
        *str = ToNewCString(tempCString);
        return *str ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
    case nsIDataType::VTYPE_WSTRING_SIZE_IS:
        tempString.Assign(data.u.wstr.mWStringValue, data.u.wstr.mWStringLength);
        *size = tempString.Length();
        *str = ToNewCString(tempString);
        return *str ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
    case nsIDataType::VTYPE_WCHAR:        
        tempString.Assign(data.u.mWCharValue);
        *size = tempString.Length();
        *str = ToNewCString(tempString);
        return *str ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
    default:
        rv = ToString(data, tempCString);
        if(NS_FAILED(rv))
            return rv;
        *size = tempCString.Length();
        *str = ToNewCString(tempCString);
        return *str ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
    }
}
/* static */ nsresult 
nsVariant::ConvertToWStringWithSize(const nsDiscriminatedUnion& data, PRUint32 *size, PRUnichar **str)
{
    nsAutoString  tempString;
    nsCAutoString tempCString;
    nsresult rv;

    switch(data.mType)
    {
    case nsIDataType::VTYPE_ASTRING:
        *size = data.u.mAStringValue->Length();
        *str = ToNewUnicode(*data.u.mAStringValue);
        return *str ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
    case nsIDataType::VTYPE_CHAR_STR:
        tempCString.Assign(data.u.str.mStringValue);
        *size = tempCString.Length();
        *str = ToNewUnicode(tempCString);
        return *str ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
    case nsIDataType::VTYPE_WCHAR_STR:
        tempString.Assign(data.u.wstr.mWStringValue);
        *size = tempString.Length();
        *str = ToNewUnicode(tempString);
        return *str ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
    case nsIDataType::VTYPE_STRING_SIZE_IS:
        tempCString.Assign(data.u.str.mStringValue, data.u.str.mStringLength);
        *size = tempCString.Length();
        *str = ToNewUnicode(tempCString);
        return *str ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
    case nsIDataType::VTYPE_WSTRING_SIZE_IS:
        tempString.Assign(data.u.wstr.mWStringValue, data.u.wstr.mWStringLength);
        *size = tempString.Length();
        *str = ToNewUnicode(tempString);
        return *str ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
    case nsIDataType::VTYPE_WCHAR:        
        tempString.Assign(data.u.mWCharValue);
        *size = tempString.Length();
        *str = ToNewUnicode(tempString);
        return *str ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
    default:
        rv = ToString(data, tempCString);
        if(NS_FAILED(rv))
            return rv;
        *size = tempCString.Length();
        *str = ToNewUnicode(tempCString);
        return *str ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
    }
}

/* static */ nsresult 
nsVariant::ConvertToISupports(const nsDiscriminatedUnion& data, nsISupports **_retval)
{
    switch(data.mType)
    {
    case nsIDataType::VTYPE_INTERFACE:
    case nsIDataType::VTYPE_INTERFACE_IS:
        return data.u.iface.mInterfaceValue->
                    QueryInterface(NS_GET_IID(nsISupports), (void**)_retval);
    default:
        return NS_ERROR_CANNOT_CONVERT_DATA;
    }
}

/* static */ nsresult 
nsVariant::ConvertToInterface(const nsDiscriminatedUnion& data, nsIID * *iid, void * *iface)
{
    const nsIID* piid;

    switch(data.mType)
    {
    case nsIDataType::VTYPE_INTERFACE:
        piid = &NS_GET_IID(nsISupports);
        break;
    case nsIDataType::VTYPE_INTERFACE_IS:
        piid = &data.u.iface.mInterfaceID;
        break;
    default:
        return NS_ERROR_CANNOT_CONVERT_DATA;
    }
    
    *iid = (nsIID*) nsMemory::Clone(piid, sizeof(nsIID));
    if(!*iid)
        return NS_ERROR_OUT_OF_MEMORY;
    return data.u.iface.mInterfaceValue->QueryInterface(*piid, iface);
}

/* static */ nsresult 
nsVariant::ConvertToArray(const nsDiscriminatedUnion& data, PRUint16 *type, nsIID* iid, PRUint32 *count, void * *ptr)
{
    // XXX perhaps we'd like to add support for converting each of the various
    // types into an array containing one element of that type. We can leverage
    // CloneArray to do this if we want to support this.

    if(data.mType == nsIDataType::VTYPE_ARRAY)
        return CloneArray(data.u.array.mArrayType, &data.u.array.mArrayInterfaceID, 
                          data.u.array.mArrayCount, data.u.array.mArrayValue,
                          type, iid, count, ptr);
    return NS_ERROR_CANNOT_CONVERT_DATA;
}

/***************************************************************************/
// static setter functions...

#define DATA_SETTER_PROLOGUE(data_)                                           \
    nsVariant::Cleanup(data_);

#define DATA_SETTER_EPILOGUE(data_, type_)                                    \
    data_->mType = nsIDataType :: type_;                                      \
    return NS_OK;

#define DATA_SETTER(data_, type_, member_, value_)                            \
    DATA_SETTER_PROLOGUE(data_)                                               \
    data_->u.member_ = value_;                                                \
    DATA_SETTER_EPILOGUE(data_, type_)

#define DATA_SETTER_WITH_CAST(data_, type_, member_, cast_, value_)           \
    DATA_SETTER_PROLOGUE(data_)                                               \
    data_->u.member_ = cast_ value_;                                          \
    DATA_SETTER_EPILOGUE(data_, type_)


/********************************************/

#define CASE__SET_FROM_VARIANT_VTYPE_PROLOGUE(type_)                          \
    {                                                                         \

#define CASE__SET_FROM_VARIANT_VTYPE__GETTER(member_, name_)                  \
        rv = aValue->GetAs##name_ (&(data->u. member_ ));

#define CASE__SET_FROM_VARIANT_VTYPE__GETTER_CAST(cast_, member_, name_)      \
        rv = aValue->GetAs##name_ ( cast_ &(data->u. member_ ));

#define CASE__SET_FROM_VARIANT_VTYPE_EPILOGUE(type_)                          \
        if(NS_SUCCEEDED(rv))                                                  \
        {                                                                     \
            data->mType  = nsIDataType :: type_ ;                             \
        }                                                                     \
        break;                                                                \
    }

#define CASE__SET_FROM_VARIANT_TYPE(type_, member_, name_)                    \
    case nsIDataType :: type_ :                                               \
        CASE__SET_FROM_VARIANT_VTYPE_PROLOGUE(type_)                          \
        CASE__SET_FROM_VARIANT_VTYPE__GETTER(member_, name_)                  \
        CASE__SET_FROM_VARIANT_VTYPE_EPILOGUE(type_)

#define CASE__SET_FROM_VARIANT_VTYPE_CAST(type_, cast_, member_, name_)       \
    case nsIDataType :: type_ :                                               \
        CASE__SET_FROM_VARIANT_VTYPE_PROLOGUE(type_)                          \
        CASE__SET_FROM_VARIANT_VTYPE__GETTER_CAST(cast_, member_, name_)      \
        CASE__SET_FROM_VARIANT_VTYPE_EPILOGUE(type_)


/* static */ nsresult 
nsVariant::SetFromVariant(nsDiscriminatedUnion* data, nsIVariant* aValue)
{
    PRUint16 type;
    nsresult rv;

    nsVariant::Cleanup(data);

    rv = aValue->GetDataType(&type);
    if(NS_FAILED(rv))
        return rv;

    switch(type)
    {
        CASE__SET_FROM_VARIANT_VTYPE_CAST(VTYPE_INT8, (PRUint8*), mInt8Value, Int8)
        CASE__SET_FROM_VARIANT_TYPE(VTYPE_INT16,  mInt16Value,  Int16)
        CASE__SET_FROM_VARIANT_TYPE(VTYPE_INT32,  mInt32Value,  Int32)
        CASE__SET_FROM_VARIANT_TYPE(VTYPE_UINT8,  mUint8Value,  Uint8)
        CASE__SET_FROM_VARIANT_TYPE(VTYPE_UINT16, mUint16Value, Uint16)
        CASE__SET_FROM_VARIANT_TYPE(VTYPE_UINT32, mUint32Value, Uint32)
        CASE__SET_FROM_VARIANT_TYPE(VTYPE_FLOAT,  mFloatValue,  Float)
        CASE__SET_FROM_VARIANT_TYPE(VTYPE_DOUBLE, mDoubleValue, Double)
        CASE__SET_FROM_VARIANT_TYPE(VTYPE_BOOL ,  mBoolValue,   Bool)
        CASE__SET_FROM_VARIANT_TYPE(VTYPE_CHAR,   mCharValue,   Char)
        CASE__SET_FROM_VARIANT_TYPE(VTYPE_WCHAR,  mWCharValue,  WChar)
        CASE__SET_FROM_VARIANT_TYPE(VTYPE_ID,     mIDValue,     ID)

        case nsIDataType::VTYPE_ASTRING:        
        case nsIDataType::VTYPE_WCHAR_STR:        
        case nsIDataType::VTYPE_WSTRING_SIZE_IS:        
            CASE__SET_FROM_VARIANT_VTYPE_PROLOGUE(VTYPE_ASTRING);
            data->u.mAStringValue = new nsString();
            if(!data->u.mAStringValue)
                return NS_ERROR_OUT_OF_MEMORY; 
            rv = aValue->GetAsAString(*data->u.mAStringValue);
            if(NS_FAILED(rv))
                delete data->u.mAStringValue;
            CASE__SET_FROM_VARIANT_VTYPE_EPILOGUE(VTYPE_ASTRING)

        case nsIDataType::VTYPE_CHAR_STR:        
        case nsIDataType::VTYPE_STRING_SIZE_IS:
            CASE__SET_FROM_VARIANT_VTYPE_PROLOGUE(VTYPE_STRING_SIZE_IS);
            rv = aValue->GetAsStringWithSize(&data->u.str.mStringLength,
                                             &data->u.str.mStringValue);
            CASE__SET_FROM_VARIANT_VTYPE_EPILOGUE(VTYPE_STRING_SIZE_IS)
        
        case nsIDataType::VTYPE_INTERFACE:
        case nsIDataType::VTYPE_INTERFACE_IS:
            CASE__SET_FROM_VARIANT_VTYPE_PROLOGUE(VTYPE_INTERFACE_IS);
            // XXX This iid handling is ugly!
            nsIID* iid;
            rv = aValue->GetAsInterface(&iid, (void**)&data->u.iface.mInterfaceValue);
            if(NS_SUCCEEDED(rv))
            {
                data->u.iface.mInterfaceID = *iid;
                nsMemory::Free((char*)iid);
            }
            CASE__SET_FROM_VARIANT_VTYPE_EPILOGUE(VTYPE_INTERFACE_IS)

        case nsIDataType::VTYPE_ARRAY:
            CASE__SET_FROM_VARIANT_VTYPE_PROLOGUE(VTYPE_ARRAY);
            rv = aValue->GetAsArray(&data->u.array.mArrayType,
                                    &data->u.array.mArrayInterfaceID,
                                    &data->u.array.mArrayCount,
                                    &data->u.array.mArrayValue);
            CASE__SET_FROM_VARIANT_VTYPE_EPILOGUE(VTYPE_ARRAY)

        case nsIDataType::VTYPE_VOID:
            rv = nsVariant::SetToVoid(data);
            break;
        case nsIDataType::VTYPE_EMPTY:
            rv = nsVariant::SetToEmpty(data);
            break;
        default:
            NS_ERROR("bad type in variant!");
            rv = NS_ERROR_FAILURE;
            break;
    }
    return rv;
}    

/* static */ nsresult 
nsVariant::SetFromInt8(nsDiscriminatedUnion* data, PRUint8 aValue)
{
    DATA_SETTER_WITH_CAST(data, VTYPE_INT8, mInt8Value, (PRUint8), aValue)
}
/* static */ nsresult 
nsVariant::SetFromInt16(nsDiscriminatedUnion* data, PRInt16 aValue)
{
    DATA_SETTER(data, VTYPE_INT16, mInt16Value, aValue)
}
/* static */ nsresult 
nsVariant::SetFromInt32(nsDiscriminatedUnion* data, PRInt32 aValue)
{
    DATA_SETTER(data, VTYPE_INT32, mInt32Value, aValue)
}
/* static */ nsresult 
nsVariant::SetFromInt64(nsDiscriminatedUnion* data, PRInt64 aValue)
{
    DATA_SETTER(data, VTYPE_INT64, mInt64Value, aValue)
}
/* static */ nsresult 
nsVariant::SetFromUint8(nsDiscriminatedUnion* data, PRUint8 aValue)
{
    DATA_SETTER(data, VTYPE_UINT8, mUint8Value, aValue)
}
/* static */ nsresult 
nsVariant::SetFromUint16(nsDiscriminatedUnion* data, PRUint16 aValue)
{
    DATA_SETTER(data, VTYPE_UINT16, mUint16Value, aValue)
}
/* static */ nsresult 
nsVariant::SetFromUint32(nsDiscriminatedUnion* data, PRUint32 aValue)
{
    DATA_SETTER(data, VTYPE_UINT32, mUint32Value, aValue)
}
/* static */ nsresult 
nsVariant::SetFromUint64(nsDiscriminatedUnion* data, PRUint64 aValue)
{
    DATA_SETTER(data, VTYPE_UINT64, mUint64Value, aValue)
}
/* static */ nsresult 
nsVariant::SetFromFloat(nsDiscriminatedUnion* data, float aValue)
{
    DATA_SETTER(data, VTYPE_FLOAT, mFloatValue, aValue)
}
/* static */ nsresult 
nsVariant::SetFromDouble(nsDiscriminatedUnion* data, double aValue)
{
    DATA_SETTER(data, VTYPE_DOUBLE, mDoubleValue, aValue)
}
/* static */ nsresult 
nsVariant::SetFromBool(nsDiscriminatedUnion* data, PRBool aValue)
{
    DATA_SETTER(data, VTYPE_BOOL, mBoolValue, aValue)
}
/* static */ nsresult 
nsVariant::SetFromChar(nsDiscriminatedUnion* data, char aValue)
{
    DATA_SETTER(data, VTYPE_CHAR, mCharValue, aValue)
}
/* static */ nsresult 
nsVariant::SetFromWChar(nsDiscriminatedUnion* data, PRUnichar aValue)
{
    DATA_SETTER(data, VTYPE_WCHAR, mWCharValue, aValue)
}
/* static */ nsresult 
nsVariant::SetFromID(nsDiscriminatedUnion* data, const nsID & aValue)
{
    DATA_SETTER(data, VTYPE_ID, mIDValue, aValue)
}
/* static */ nsresult 
nsVariant::SetFromAString(nsDiscriminatedUnion* data, const nsAReadableString & aValue)
{
    DATA_SETTER_PROLOGUE(data);
    if(!(data->u.mAStringValue = new nsString(aValue)))
        return NS_ERROR_OUT_OF_MEMORY;
    DATA_SETTER_EPILOGUE(data, VTYPE_ASTRING);
}
/* static */ nsresult 
nsVariant::SetFromString(nsDiscriminatedUnion* data, const char *aValue)
{
    DATA_SETTER_PROLOGUE(data);
    if(!aValue) 
        return NS_ERROR_NULL_POINTER;
    return SetFromStringWithSize(data, nsCRT::strlen(aValue), aValue);
}
/* static */ nsresult 
nsVariant::SetFromWString(nsDiscriminatedUnion* data, const PRUnichar *aValue)
{
    DATA_SETTER_PROLOGUE(data);
    if(!aValue) 
        return NS_ERROR_NULL_POINTER;
    return SetFromWStringWithSize(data, nsCRT::strlen(aValue), aValue);
}
/* static */ nsresult 
nsVariant::SetFromISupports(nsDiscriminatedUnion* data, nsISupports *aValue)
{
    return SetFromInterface(data, NS_GET_IID(nsISupports), aValue);
}
/* static */ nsresult 
nsVariant::SetFromInterface(nsDiscriminatedUnion* data, const nsIID& iid, nsISupports *aValue)
{
    DATA_SETTER_PROLOGUE(data);
    if(!aValue) 
        return NS_ERROR_NULL_POINTER;
    NS_ADDREF(aValue);
    data->u.iface.mInterfaceValue = aValue;
    data->u.iface.mInterfaceID = iid;
    DATA_SETTER_EPILOGUE(data, VTYPE_INTERFACE_IS);
}
/* static */ nsresult 
nsVariant::SetFromArray(nsDiscriminatedUnion* data, PRUint16 type, const nsIID* iid, PRUint32 count, void * aValue)
{
    DATA_SETTER_PROLOGUE(data);
    if(!aValue || !count) 
        return NS_ERROR_NULL_POINTER;

    nsresult rv = CloneArray(type, iid, count, aValue,
                             &data->u.array.mArrayType,
                             &data->u.array.mArrayInterfaceID,
                             &data->u.array.mArrayCount, 
                             &data->u.array.mArrayValue);
    if(NS_FAILED(rv))
        return rv;
    DATA_SETTER_EPILOGUE(data, VTYPE_ARRAY);
}
/* static */ nsresult 
nsVariant::SetFromStringWithSize(nsDiscriminatedUnion* data, PRUint32 size, const char *aValue)
{
    DATA_SETTER_PROLOGUE(data);
    if(!aValue) 
        return NS_ERROR_NULL_POINTER;
    if(!(data->u.str.mStringValue = 
         (char*) nsMemory::Clone(aValue, (size+1)*sizeof(char))))
        return NS_ERROR_OUT_OF_MEMORY;
    data->u.str.mStringLength = size;
    DATA_SETTER_EPILOGUE(data, VTYPE_STRING_SIZE_IS);
}
/* static */ nsresult 
nsVariant::SetFromWStringWithSize(nsDiscriminatedUnion* data, PRUint32 size, const PRUnichar *aValue)
{
    DATA_SETTER_PROLOGUE(data);
    if(!aValue) 
        return NS_ERROR_NULL_POINTER;
    if(!(data->u.wstr.mWStringValue = 
         (PRUnichar*) nsMemory::Clone(aValue, (size+1)*sizeof(PRUnichar))))
        return NS_ERROR_OUT_OF_MEMORY;
    data->u.wstr.mWStringLength = size;
    DATA_SETTER_EPILOGUE(data, VTYPE_WSTRING_SIZE_IS);
}
/* static */ nsresult 
nsVariant::SetToVoid(nsDiscriminatedUnion* data)
{
    DATA_SETTER_PROLOGUE(data);
    DATA_SETTER_EPILOGUE(data, VTYPE_VOID);
}
/* static */ nsresult 
nsVariant::SetToEmpty(nsDiscriminatedUnion* data)
{
    DATA_SETTER_PROLOGUE(data);
    DATA_SETTER_EPILOGUE(data, VTYPE_EMPTY);
}

/***************************************************************************/

/* static */ nsresult 
nsVariant::Initialize(nsDiscriminatedUnion* data)
{
    data->mType = nsIDataType::VTYPE_EMPTY;
    return NS_OK;
}   

/* static */ nsresult 
nsVariant::Cleanup(nsDiscriminatedUnion* data)
{
    switch(data->mType)
    {
        case nsIDataType::VTYPE_INT8:        
        case nsIDataType::VTYPE_INT16:        
        case nsIDataType::VTYPE_INT32:        
        case nsIDataType::VTYPE_INT64:        
        case nsIDataType::VTYPE_UINT8:        
        case nsIDataType::VTYPE_UINT16:        
        case nsIDataType::VTYPE_UINT32:        
        case nsIDataType::VTYPE_UINT64:        
        case nsIDataType::VTYPE_FLOAT:        
        case nsIDataType::VTYPE_DOUBLE:        
        case nsIDataType::VTYPE_BOOL:        
        case nsIDataType::VTYPE_CHAR:        
        case nsIDataType::VTYPE_WCHAR:        
        case nsIDataType::VTYPE_VOID:        
        case nsIDataType::VTYPE_ID:        
            break;
        case nsIDataType::VTYPE_ASTRING:        
            delete data->u.mAStringValue;
            break;
        case nsIDataType::VTYPE_CHAR_STR:        
        case nsIDataType::VTYPE_STRING_SIZE_IS:        
            nsMemory::Free((char*)data->u.str.mStringValue);
            break;
        case nsIDataType::VTYPE_WCHAR_STR:        
        case nsIDataType::VTYPE_WSTRING_SIZE_IS:        
            nsMemory::Free((char*)data->u.wstr.mWStringValue);
            break;
        case nsIDataType::VTYPE_INTERFACE:        
        case nsIDataType::VTYPE_INTERFACE_IS:        
            NS_IF_RELEASE(data->u.iface.mInterfaceValue);
            break;
        case nsIDataType::VTYPE_ARRAY:
            FreeArray(data);
            break;
        case nsIDataType::VTYPE_EMPTY:
            break;
        default:
            NS_ERROR("bad type in variant!");
            break;
    }
    
    data->mType = nsIDataType::VTYPE_EMPTY;
    return NS_OK;
}

/***************************************************************************/
/***************************************************************************/
// members...

NS_IMPL_ISUPPORTS2(nsVariant, nsIVariant, nsIWritableVariant)

nsVariant::nsVariant() 
    : mWritable(PR_TRUE)
{
    NS_INIT_ISUPPORTS();

    nsVariant::Initialize(&mData);

#ifdef DEBUG
    {
        // Assert that the nsIDataType consts match the values #defined in 
        // xpt_struct.h. Bad things happen somewhere if they don't.
        struct THE_TYPES {PRUint16 a; PRUint16 b;};
        static const THE_TYPES array[] = {
            {nsIDataType::VTYPE_INT8              , TD_INT8             },
            {nsIDataType::VTYPE_INT16             , TD_INT16            },
            {nsIDataType::VTYPE_INT32             , TD_INT32            },
            {nsIDataType::VTYPE_INT64             , TD_INT64            },
            {nsIDataType::VTYPE_UINT8             , TD_UINT8            },
            {nsIDataType::VTYPE_UINT16            , TD_UINT16           },
            {nsIDataType::VTYPE_UINT32            , TD_UINT32           },
            {nsIDataType::VTYPE_UINT64            , TD_UINT64           },
            {nsIDataType::VTYPE_FLOAT             , TD_FLOAT            },
            {nsIDataType::VTYPE_DOUBLE            , TD_DOUBLE           },
            {nsIDataType::VTYPE_BOOL              , TD_BOOL             },
            {nsIDataType::VTYPE_CHAR              , TD_CHAR             },
            {nsIDataType::VTYPE_WCHAR             , TD_WCHAR            },
            {nsIDataType::VTYPE_VOID              , TD_VOID             },
            {nsIDataType::VTYPE_ID                , TD_PNSIID           },
            {nsIDataType::VTYPE_ASTRING           , TD_DOMSTRING        },
            {nsIDataType::VTYPE_CHAR_STR          , TD_PSTRING          },
            {nsIDataType::VTYPE_WCHAR_STR         , TD_PWSTRING         },
            {nsIDataType::VTYPE_INTERFACE         , TD_INTERFACE_TYPE   },
            {nsIDataType::VTYPE_INTERFACE_IS      , TD_INTERFACE_IS_TYPE},
            {nsIDataType::VTYPE_ARRAY             , TD_ARRAY            },
            {nsIDataType::VTYPE_STRING_SIZE_IS    , TD_PSTRING_SIZE_IS  },
            {nsIDataType::VTYPE_WSTRING_SIZE_IS   , TD_PWSTRING_SIZE_IS }
        };
        static const int length = sizeof(array)/sizeof(array[0]);
        static PRBool inited = PR_FALSE;
        if(!inited)
        {
            for(int i = 0; i < length; i++)
                NS_ASSERTION(array[i].a == array[i].b, "bad const declaration");
            inited = PR_TRUE;
        }
    }    
#endif
}

nsVariant::~nsVariant()
{
    nsVariant::Cleanup(&mData);
}

// For all the data getters we just forward to the static (and sharable) 
// 'ConvertTo' functions.

/* readonly attribute PRUint16 dataType; */
NS_IMETHODIMP nsVariant::GetDataType(PRUint16 *aDataType)
{
    *aDataType = mData.mType;
    return NS_OK;
}

/* PRUint8 getAsInt8 (); */
NS_IMETHODIMP nsVariant::GetAsInt8(PRUint8 *_retval)
{
    return nsVariant::ConvertToInt8(mData, _retval);
}

/* PRInt16 getAsInt16 (); */
NS_IMETHODIMP nsVariant::GetAsInt16(PRInt16 *_retval)
{
    return nsVariant::ConvertToInt16(mData, _retval);
}

/* PRInt32 getAsInt32 (); */
NS_IMETHODIMP nsVariant::GetAsInt32(PRInt32 *_retval)
{
    return nsVariant::ConvertToInt32(mData, _retval);
}

/* PRInt64 getAsInt64 (); */
NS_IMETHODIMP nsVariant::GetAsInt64(PRInt64 *_retval)
{
    return nsVariant::ConvertToInt64(mData, _retval);
}

/* PRUint8 getAsUint8 (); */
NS_IMETHODIMP nsVariant::GetAsUint8(PRUint8 *_retval)
{
    return nsVariant::ConvertToUint8(mData, _retval);
}

/* PRUint16 getAsUint16 (); */
NS_IMETHODIMP nsVariant::GetAsUint16(PRUint16 *_retval)
{
    return nsVariant::ConvertToUint16(mData, _retval);
}

/* PRUint32 getAsUint32 (); */
NS_IMETHODIMP nsVariant::GetAsUint32(PRUint32 *_retval)
{
    return nsVariant::ConvertToUint32(mData, _retval);
}

/* PRUint64 getAsUint64 (); */
NS_IMETHODIMP nsVariant::GetAsUint64(PRUint64 *_retval)
{
    return nsVariant::ConvertToUint64(mData, _retval);
}

/* float getAsFloat (); */
NS_IMETHODIMP nsVariant::GetAsFloat(float *_retval)
{
    return nsVariant::ConvertToFloat(mData, _retval);
}

/* double getAsDouble (); */
NS_IMETHODIMP nsVariant::GetAsDouble(double *_retval)
{
    return nsVariant::ConvertToDouble(mData, _retval);
}

/* PRBool getAsBool (); */
NS_IMETHODIMP nsVariant::GetAsBool(PRBool *_retval)
{
    return nsVariant::ConvertToBool(mData, _retval);
}

/* char getAsChar (); */
NS_IMETHODIMP nsVariant::GetAsChar(char *_retval)
{
    return nsVariant::ConvertToChar(mData, _retval);
}

/* wchar getAsWChar (); */
NS_IMETHODIMP nsVariant::GetAsWChar(PRUnichar *_retval)
{
    return nsVariant::ConvertToWChar(mData, _retval);
}

/* [notxpcom] nsresult getAsID (out nsID retval); */
NS_IMETHODIMP_(nsresult) nsVariant::GetAsID(nsID *retval)
{
    return nsVariant::ConvertToID(mData, retval);
}

/* AString getAsAString (); */
NS_IMETHODIMP nsVariant::GetAsAString(nsAWritableString & _retval)
{
    return nsVariant::ConvertToAString(mData, _retval);
}

/* string getAsString (); */
NS_IMETHODIMP nsVariant::GetAsString(char **_retval)
{
    return nsVariant::ConvertToString(mData, _retval);
}

/* wstring getAsWString (); */
NS_IMETHODIMP nsVariant::GetAsWString(PRUnichar **_retval)
{
    return nsVariant::ConvertToWString(mData, _retval);
}

/* nsISupports getAsISupports (); */
NS_IMETHODIMP nsVariant::GetAsISupports(nsISupports **_retval)
{
    return nsVariant::ConvertToISupports(mData, _retval);
}

/* void getAsInterface (out nsIIDPtr iid, [iid_is (iid), retval] out nsQIResult iface); */
NS_IMETHODIMP nsVariant::GetAsInterface(nsIID * *iid, void * *iface)
{
    return nsVariant::ConvertToInterface(mData, iid, iface);
}

/* [notxpcom] nsresult getAsArray (out PRUint16 type, out nsIID iid, out PRUint32 count, out voidPtr ptr); */
NS_IMETHODIMP_(nsresult) nsVariant::GetAsArray(PRUint16 *type, nsIID *iid, PRUint32 *count, void * *ptr)
{
    return nsVariant::ConvertToArray(mData, type, iid, count, ptr);
}

/* void getAsStringWithSize (out PRUint32 size, [size_is (size), retval] out string str); */
NS_IMETHODIMP nsVariant::GetAsStringWithSize(PRUint32 *size, char **str)
{
    return nsVariant::ConvertToStringWithSize(mData, size, str);
}

/* void getAsWStringWithSize (out PRUint32 size, [size_is (size), retval] out wstring str); */
NS_IMETHODIMP nsVariant::GetAsWStringWithSize(PRUint32 *size, PRUnichar **str)
{
    return nsVariant::ConvertToWStringWithSize(mData, size, str);
}

/***************************************************************************/

/* attribute PRBool writable; */
NS_IMETHODIMP nsVariant::GetWritable(PRBool *aWritable)
{
    *aWritable = mWritable;
    return NS_OK;
}
NS_IMETHODIMP nsVariant::SetWritable(PRBool aWritable)
{
    if(!mWritable && aWritable)
        return NS_ERROR_FAILURE;
    mWritable = aWritable;
    return NS_OK;
}

/***************************************************************************/

// For all the data setters we just forward to the static (and sharable) 
// 'SetFrom' functions.

/* void setAsInt8 (in PRUint8 aValue); */
NS_IMETHODIMP nsVariant::SetAsInt8(PRUint8 aValue)
{
    if(!mWritable) return NS_ERROR_OBJECT_IS_IMMUTABLE;
    return nsVariant::SetFromInt8(&mData, aValue);
}

/* void setAsInt16 (in PRInt16 aValue); */
NS_IMETHODIMP nsVariant::SetAsInt16(PRInt16 aValue)
{
    if(!mWritable) return NS_ERROR_OBJECT_IS_IMMUTABLE;
    return nsVariant::SetFromInt16(&mData, aValue);
}

/* void setAsInt32 (in PRInt32 aValue); */
NS_IMETHODIMP nsVariant::SetAsInt32(PRInt32 aValue)
{
    if(!mWritable) return NS_ERROR_OBJECT_IS_IMMUTABLE;
    return nsVariant::SetFromInt32(&mData, aValue);
}

/* void setAsInt64 (in PRInt64 aValue); */
NS_IMETHODIMP nsVariant::SetAsInt64(PRInt64 aValue)
{
    if(!mWritable) return NS_ERROR_OBJECT_IS_IMMUTABLE;
    return nsVariant::SetFromInt64(&mData, aValue);
}

/* void setAsUint8 (in PRUint8 aValue); */
NS_IMETHODIMP nsVariant::SetAsUint8(PRUint8 aValue)
{
    if(!mWritable) return NS_ERROR_OBJECT_IS_IMMUTABLE;
    return nsVariant::SetFromUint8(&mData, aValue);
}

/* void setAsUint16 (in PRUint16 aValue); */
NS_IMETHODIMP nsVariant::SetAsUint16(PRUint16 aValue)
{
    if(!mWritable) return NS_ERROR_OBJECT_IS_IMMUTABLE;
    return nsVariant::SetFromUint16(&mData, aValue);
}

/* void setAsUint32 (in PRUint32 aValue); */
NS_IMETHODIMP nsVariant::SetAsUint32(PRUint32 aValue)
{
    if(!mWritable) return NS_ERROR_OBJECT_IS_IMMUTABLE;
    return nsVariant::SetFromUint32(&mData, aValue);
}

/* void setAsUint64 (in PRUint64 aValue); */
NS_IMETHODIMP nsVariant::SetAsUint64(PRUint64 aValue)
{
    if(!mWritable) return NS_ERROR_OBJECT_IS_IMMUTABLE;
    return nsVariant::SetFromUint64(&mData, aValue);
}

/* void setAsFloat (in float aValue); */
NS_IMETHODIMP nsVariant::SetAsFloat(float aValue)
{
    if(!mWritable) return NS_ERROR_OBJECT_IS_IMMUTABLE;
    return nsVariant::SetFromFloat(&mData, aValue);
}

/* void setAsDouble (in double aValue); */
NS_IMETHODIMP nsVariant::SetAsDouble(double aValue)
{
    if(!mWritable) return NS_ERROR_OBJECT_IS_IMMUTABLE;
    return nsVariant::SetFromDouble(&mData, aValue);
}

/* void setAsBool (in PRBool aValue); */
NS_IMETHODIMP nsVariant::SetAsBool(PRBool aValue)
{
    if(!mWritable) return NS_ERROR_OBJECT_IS_IMMUTABLE;
    return nsVariant::SetFromBool(&mData, aValue);
}

/* void setAsChar (in char aValue); */
NS_IMETHODIMP nsVariant::SetAsChar(char aValue)
{
    if(!mWritable) return NS_ERROR_OBJECT_IS_IMMUTABLE;
    return nsVariant::SetFromChar(&mData, aValue);
}

/* void setAsWChar (in wchar aValue); */
NS_IMETHODIMP nsVariant::SetAsWChar(PRUnichar aValue)
{
    if(!mWritable) return NS_ERROR_OBJECT_IS_IMMUTABLE;
    return nsVariant::SetFromWChar(&mData, aValue);
}

/* void setAsID (in nsIDRef aValue); */
NS_IMETHODIMP nsVariant::SetAsID(const nsID & aValue)
{
    if(!mWritable) return NS_ERROR_OBJECT_IS_IMMUTABLE;
    return nsVariant::SetFromID(&mData, aValue);
}

/* void setAsAString (in AString aValue); */
NS_IMETHODIMP nsVariant::SetAsAString(const nsAReadableString & aValue)
{
    if(!mWritable) return NS_ERROR_OBJECT_IS_IMMUTABLE;
    return nsVariant::SetFromAString(&mData, aValue);
}

/* void setAsString (in string aValue); */
NS_IMETHODIMP nsVariant::SetAsString(const char *aValue)
{
    if(!mWritable) return NS_ERROR_OBJECT_IS_IMMUTABLE;
    return nsVariant::SetFromString(&mData, aValue);
}

/* void setAsWString (in wstring aValue); */
NS_IMETHODIMP nsVariant::SetAsWString(const PRUnichar *aValue)
{
    if(!mWritable) return NS_ERROR_OBJECT_IS_IMMUTABLE;
    return nsVariant::SetFromWString(&mData, aValue);
}

/* void setAsISupports (in nsISupports aValue); */
NS_IMETHODIMP nsVariant::SetAsISupports(nsISupports *aValue)
{
    if(!mWritable) return NS_ERROR_OBJECT_IS_IMMUTABLE;
    return nsVariant::SetFromISupports(&mData, aValue);
}

/* void setAsInterface (in nsIIDRef iid, [iid_is (iid)] in nsQIResult iface); */
NS_IMETHODIMP nsVariant::SetAsInterface(const nsIID & iid, void * iface)
{
    if(!mWritable) return NS_ERROR_OBJECT_IS_IMMUTABLE;
    return nsVariant::SetFromInterface(&mData, iid, (nsISupports*)iface);
}

/* [noscript] void setAsArray (in PRUint16 type, in nsIIDPtr iid, in PRUint32 count, in voidPtr ptr); */
NS_IMETHODIMP nsVariant::SetAsArray(PRUint16 type, const nsIID * iid, PRUint32 count, void * ptr)
{
    if(!mWritable) return NS_ERROR_OBJECT_IS_IMMUTABLE;
    return nsVariant::SetFromArray(&mData, type, iid, count, ptr);
}

/* void setAsStringWithSize (in PRUint32 size, [size_is (size)] in string str); */
NS_IMETHODIMP nsVariant::SetAsStringWithSize(PRUint32 size, const char *str)
{
    if(!mWritable) return NS_ERROR_OBJECT_IS_IMMUTABLE;
    return nsVariant::SetFromStringWithSize(&mData, size, str);
}

/* void setAsWStringWithSize (in PRUint32 size, [size_is (size)] in wstring str); */
NS_IMETHODIMP nsVariant::SetAsWStringWithSize(PRUint32 size, const PRUnichar *str)
{
    if(!mWritable) return NS_ERROR_OBJECT_IS_IMMUTABLE;
    return nsVariant::SetFromWStringWithSize(&mData, size, str);
}

/* void setAsVoid (); */
NS_IMETHODIMP nsVariant::SetAsVoid()
{
    if(!mWritable) return NS_ERROR_OBJECT_IS_IMMUTABLE;
    return nsVariant::SetToVoid(&mData);
}

/* void setAsEmpty (); */
NS_IMETHODIMP nsVariant::SetAsEmpty()
{
    if(!mWritable) return NS_ERROR_OBJECT_IS_IMMUTABLE;
    return nsVariant::SetToEmpty(&mData);
}

/* void setFromVariant (in nsIVariant aValue); */
NS_IMETHODIMP nsVariant::SetFromVariant(nsIVariant *aValue)
{
    if(!mWritable) return NS_ERROR_OBJECT_IS_IMMUTABLE;
    return nsVariant::SetFromVariant(&mData, aValue);
}
