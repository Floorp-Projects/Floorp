/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Sun Microsystems,
 * Inc. Portions created by Sun are
 * Copyright (C) 1999 Sun Microsystems, Inc. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#include <stdlib.h>
#include <iostream.h>
#include "MarshalToolkitImpl.h"


NS_IMPL_ISUPPORTS(MarshalToolkitImpl, NS_GET_IID(nsISupports));


MarshalToolkitImpl::MarshalToolkitImpl(void) {
    NS_INIT_REFCNT();
}

MarshalToolkitImpl::~MarshalToolkitImpl() {
}

PRInt16 MarshalToolkitImpl::GetSimpleSize(nsXPTType *type) {
    PRInt16 size = -1;
    switch(type->TagPart()) {
    case nsXPTType::T_I8:
    case nsXPTType::T_U8:
        size = sizeof(PRInt8);
        break;
    case nsXPTType::T_I16:
    case nsXPTType::T_U16:
        size = sizeof(PRInt16);
        break;
    case nsXPTType::T_I32:
    case nsXPTType::T_U32:
        size = sizeof(PRInt32);
        break;
    case nsXPTType::T_I64:
    case nsXPTType::T_U64:
        size = sizeof(PRInt64);
            break;
    case nsXPTType::T_FLOAT:
        size = sizeof(float);
        break;
    case nsXPTType::T_DOUBLE:
        size = sizeof(double);
        break;
    case nsXPTType::T_BOOL:
        size = sizeof(PRBool);
        break;
    case nsXPTType::T_CHAR:
        size = sizeof(char);
        break;
    case nsXPTType::T_WCHAR:
        size = sizeof(PRUnichar);
        break;
    default:
        break;
    }
    return size;
}

NS_IMETHODIMP MarshalToolkitImpl::ReadSimple(istream *in, nsXPTCMiniVariant *value, nsXPTType * type, PRBool  isOut, nsIAllocator * allocator) {
    PRInt16 size = GetSimpleSize(type);
    if (size <= 0 ) {
	return NS_ERROR_FAILURE;
    }
    void * data = NULL;
    if (isOut) {
	if (allocator) {
	    value->val.p = allocator->Alloc(size);
	}
	 data = value->val.p;
    } else {
	data = value;
    }
     in->read((char*)data,size);
    return NS_OK;
}

NS_IMETHODIMP MarshalToolkitImpl::WriteSimple(ostream *out, nsXPTCMiniVariant *value, nsXPTType * type, PRBool  isOut) {
    PRInt16 size = GetSimpleSize(type);
    if (size <= 0 ) {
	return NS_ERROR_FAILURE;
    }
    void *data = (isOut)? value->val.p : value;
    out->write((const char*)data,size);
    return NS_OK;
}

NS_IMETHODIMP MarshalToolkitImpl::ReadIID(istream *in, nsXPTCMiniVariant *value, nsXPTType * type, PRBool  isOut, nsIAllocator * allocator) {
    void * data = malloc(sizeof(nsIID)); // nb I guess it is more convinient
    if (isOut) {
	if (allocator) {
	    value->val.p = allocator->Alloc(sizeof(void*));
	}
	memcpy(value->val.p,(void *)&data,sizeof(void*));
    } else {
	value->val.p = data;
    }
    in->read((char*)data,sizeof(nsIID));
    return NS_OK;
}

NS_IMETHODIMP MarshalToolkitImpl::WriteIID(ostream *out, nsXPTCMiniVariant *value, nsXPTType * type, PRBool  isOut) {
    void * data = NULL;
    if (isOut) {
	memcpy((void *)&data, value->val.p, sizeof(void*));
    } else {
	data = value->val.p;
    }
    out->write((const char*)data, sizeof(nsIID));
    return NS_OK;
}

NS_IMETHODIMP MarshalToolkitImpl::ReadString(istream *in, nsXPTCMiniVariant *value, nsXPTType * type, PRBool  isOut,
					    nsIAllocator * allocator) {
    PRUint32 length;
    void *data = NULL;
    PRBool isStringWithSize = PR_FALSE;
    in->read((char*)&length, sizeof(PRUint32)); //length 0 means  NULL string
    PRUint32 actualLength = 0;
    if (length) {
	switch (type->TagPart()) { 
	case nsXPTType::T_CHAR_STR:  
	case nsXPTType::T_WCHAR_STR: 
	    actualLength = length;
	    break;
	case nsXPTType::T_PSTRING_SIZE_IS: 
	case nsXPTType::T_PWSTRING_SIZE_IS:
	    isStringWithSize = PR_TRUE;
	    actualLength = length - 1 ;  //length could not be less than 2. It is impossible to have empty ("") string for this case
	    break;
	default:
	    return NS_ERROR_FAILURE;
	};
	data = malloc(actualLength); 
    }

    if (isOut) {
	if (allocator) {
	    value->val.p = allocator->Alloc(sizeof(void*));
	}
	memcpy(value->val.p,(void *)&data,sizeof(void*));
    } else {
	value->val.p = data;
    }
    if (length-1 > 0) {
	in->read((char*)data,length-1);
    }
    if (!isStringWithSize
	&& length) {
	((char*)data)[length-1]=0;
    }
    return NS_OK;
}

NS_IMETHODIMP MarshalToolkitImpl::WriteString(ostream *out, nsXPTCMiniVariant *value, nsXPTType * type, PRBool  isOut, PRInt32 size) {
    void * data = NULL;
    if (isOut) {
	memcpy((void *)&data, value->val.p, sizeof(void*));
    } else {
	data = value->val.p;
    }
    PRUint32 length = 0;
    if (!data) {
	out->write((const char*)&length,sizeof(PRUint32));
	return NS_OK;
    }
    switch (type->TagPart()) { 
    case nsXPTType::T_CHAR_STR:  
    case nsXPTType::T_WCHAR_STR: 
	length = strlen((char*)data);
	break;
    case nsXPTType::T_PSTRING_SIZE_IS: 
    case nsXPTType::T_PWSTRING_SIZE_IS:
	length = size;
	break;
    default:
	return NS_ERROR_FAILURE;
    };
    length++; //0 for NULL, 1 for "", etc.
    out->write((const char*)&length,sizeof(PRUint32));
    out->write((const char*)data, length-1);
    return NS_OK;
}

NS_IMETHODIMP MarshalToolkitImpl::ReadInterface(istream *in, nsXPTCMiniVariant *value, nsXPTType * type, PRBool  isOut, nsIID * iid,
						nsIAllocator * allocator) {
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP MarshalToolkitImpl::WriteInterface(ostream *out, nsXPTCMiniVariant *value, nsXPTType * type, PRBool  isOut, nsIID * iid) {
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP MarshalToolkitImpl::ReadArray(istream *in, nsXPTCMiniVariant *value, nsXPTType * type, PRBool  isOut, nsXPTType * datumType, nsIAllocator * allocator) {
    PRUint32 length;
    in->read((char*)&length,sizeof(PRUint32));
    PRInt16 elemSize = GetSimpleSize(datumType);
    if(elemSize<0) { //not simple
	elemSize = sizeof(void*); //it has to be a pointer
    }
    void * data = malloc(elemSize*length); 
    if (isOut) {
	if (allocator) {
	    value->val.p = allocator->Alloc(sizeof(void*));
	}
	memcpy(value->val.p,(void *)&data,sizeof(void*));
    } else {
	value->val.p = data;
    }
    char *current = (char*)data;
    nsIID iid;
    for (int i = 0; i < length;i++) {
	switch(datumType->TagPart()) {
	case nsXPTType::T_I8  :
	case nsXPTType::T_I16 :
        case nsXPTType::T_I32 :     
        case nsXPTType::T_I64 :             
        case nsXPTType::T_U8  :             
        case nsXPTType::T_U16 :             
        case nsXPTType::T_U32 :             
        case nsXPTType::T_U64 :             
        case nsXPTType::T_FLOAT  :           
        case nsXPTType::T_DOUBLE :         
        case nsXPTType::T_BOOL   :         
        case nsXPTType::T_CHAR   :         
        case nsXPTType::T_WCHAR  : 
	    ReadSimple(in,(nsXPTCMiniVariant*)current,datumType,PR_FALSE,allocator);
	    break;
	case nsXPTType::T_IID    :
	    ReadIID(in,(nsXPTCMiniVariant*)current,datumType,PR_FALSE,allocator);
	    break;
	case nsXPTType::T_CHAR_STR  :
        case nsXPTType::T_WCHAR_STR :      
	    ReadString(in,(nsXPTCMiniVariant*)current,datumType,PR_FALSE,allocator);
	    break;
        case nsXPTType::T_INTERFACE :  //nb 
	    break;
	default:
	    return NS_ERROR_FAILURE;
	}
	current += elemSize;
    }
    return NS_OK;
}

NS_IMETHODIMP MarshalToolkitImpl::WriteArray(ostream *out, nsXPTCMiniVariant *value, nsXPTType * type, PRBool  isOut, nsXPTType * datumType, PRUint32 length) {
    PRInt16  elemSize = GetSimpleSize(datumType);
    if(elemSize < 0) { //not simple
	elemSize = sizeof(void*); //it has to be a pointer
    }
    char *current = NULL;
    if (isOut) {
	memcpy(&current,value->val.p,sizeof(void*));
    } else {
	current = (char*)value->val.p;
    }
    out->write((char*)&length,sizeof(PRUint32));
    for (int i = 0; i < length;i++) {
	switch(datumType->TagPart()) {
	case nsXPTType::T_I8  :
	case nsXPTType::T_I16 :
        case nsXPTType::T_I32 :     
        case nsXPTType::T_I64 :             
        case nsXPTType::T_U8  :             
        case nsXPTType::T_U16 :             
        case nsXPTType::T_U32 :             
        case nsXPTType::T_U64 :             
        case nsXPTType::T_FLOAT  :           
        case nsXPTType::T_DOUBLE :         
        case nsXPTType::T_BOOL   :         
        case nsXPTType::T_CHAR   :         
        case nsXPTType::T_WCHAR  : 
	    WriteSimple(out,(nsXPTCMiniVariant*)current,datumType,PR_FALSE);
	    break;
	case nsXPTType::T_IID    :
	    WriteIID(out,(nsXPTCMiniVariant*)current,datumType,PR_FALSE);
	    break;
	case nsXPTType::T_CHAR_STR  :
        case nsXPTType::T_WCHAR_STR :      
	    WriteString(out,(nsXPTCMiniVariant*)current,datumType,PR_FALSE);
	    break;
        case nsXPTType::T_INTERFACE :      	    //nb 
	    break;
	default:
	    return NS_ERROR_FAILURE;
	}
	current += elemSize;

    }
    return NS_OK;
}
