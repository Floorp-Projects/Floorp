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
#include <strstream.h>
#include "RPCallImpl.h"
#include "prmem.h"
#include "nsIInterfaceInfoManager.h"
#include "nsIInterfaceInfo.h"

NS_IMPL_ISUPPORTS(RPCallImpl, NS_GET_IID(IRPCall));

RPCallImpl::RPCallImpl(IMarshalToolkit * _marshalToolkit) {
    NS_INIT_REFCNT();
    params = NULL;
    info = NULL;
    oid = 0;
    bundle = NULL;
    size = 0;
    callSide = unDefined;
    interfaceInfo = NULL;
    result = NS_ERROR_FAILURE;
    marshalToolkit = _marshalToolkit;
}

RPCallImpl::~RPCallImpl() {
    if (params) {
        free(params);
    }
    
    if (bundle) {
        free(bundle);
    }
    NS_IF_RELEASE(interfaceInfo);
}



NS_IMETHODIMP RPCallImpl::Marshal(const nsIID &_iid, OID _oid, PRUint16 _methodIndex,
				   const nsXPTMethodInfo* _info,
				   nsXPTCMiniVariant* _params) {
    if (callSide == unDefined) {
        callSide = onClient;
    }
    iid = _iid;
    oid = _oid;
    nsIInterfaceInfoManager *iimgr = NULL;
    iimgr = XPTI_GetInterfaceInfoManager();
    if (!iimgr) {
        return NS_ERROR_FAILURE; //nb how are we going to handle critical errors?
    }
    if (NS_FAILED(iimgr->GetInfoForIID(&iid, &interfaceInfo))) {
        NS_IF_RELEASE(iimgr);
        printf("--RemoteObjectProxy::GetInterfaceInfo NS_ERROR_FAILURE 1 \n");
        return NS_ERROR_FAILURE;
    }
    NS_IF_RELEASE(iimgr);

    methodIndex = _methodIndex;
    info = (nsXPTMethodInfo*) _info;
    paramCount = info->GetParamCount();
    if (paramCount > 0) {
        params = (nsXPTCVariant*)malloc(sizeof(nsXPTCVariant) * paramCount);
        if (params == nsnull) {
            return NS_ERROR_OUT_OF_MEMORY;
        }
        for (int i = 0; i < paramCount; i++) {
            (params)[i].Init(_params[i], info->GetParam(i).GetType());
	    if (info->GetParam(i).IsOut()) {
		params[i].flags |= nsXPTCVariant::PTR_IS_DATA;
		params[i].ptr = params[i].val.p = _params[i].val.p;
	    }
        }
    }
    return Marshal();
}



NS_IMETHODIMP RPCallImpl::Marshal(void) {
    ostrstream out;
    if (callSide == onClient) {
        out.write((const char *)&iid, sizeof(nsIID));
        out.write((const char *)&oid, sizeof(OID));
        out.write((const char *)&methodIndex, sizeof(PRUint16));
    }
    for (int i = 0; i < paramCount; i++) {
        nsXPTParamInfo param = info->GetParam(i);
        nsXPTType type = param.GetType();
        PRBool isOut = param.IsOut();
        if ((callSide == onClient  && !param.IsIn())
            || (callSide == onServer && !param.IsOut())) {
            continue;
        }
        nsXPTCVariant *value = & params[i];
        switch(type.TagPart()) {
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
            marshalToolkit->WriteSimple(&out,value,&type,isOut);
            break;
        case nsXPTType::T_IID    :
            marshalToolkit->WriteIID(&out,value,&type,isOut);
            break;
        case nsXPTType::T_CHAR_STR  :
        case nsXPTType::T_WCHAR_STR :      
            marshalToolkit->WriteString(&out,value,&type,isOut);
            break;
        case nsXPTType::T_INTERFACE :      	    
        case nsXPTType::T_INTERFACE_IS :      	    
            marshalToolkit->WriteInterface(&out,value,&type,isOut,NULL);
            break;
        case nsXPTType::T_PSTRING_SIZE_IS: 
        case nsXPTType::T_PWSTRING_SIZE_IS:
        case nsXPTType::T_ARRAY:
            {
                PRUint32 arraySize;
                if(!GetArraySizeFromParam(interfaceInfo,info,param,methodIndex,i,params,GET_LENGTH, &arraySize)) { 
                    return NS_ERROR_FAILURE;
                }
                if (type.TagPart() == nsXPTType::T_ARRAY) {
                    nsXPTType datumType;
                    if(NS_FAILED(interfaceInfo->GetTypeForParam(methodIndex, &param, 1,&datumType))) {
                        return NS_ERROR_FAILURE;
                    }
                    marshalToolkit->WriteArray(&out,value,&type,isOut,&datumType,arraySize);
                } else {
                    marshalToolkit->WriteString(&out,value,&type,isOut, arraySize);
                }
                break;
            }
        default:
            return NS_ERROR_FAILURE;
        }
    }
    if (callSide == onServer) {
        out.write((const char*)&result,sizeof(nsresult));
    }
    size = out.pcount();
    bundle = malloc(size);
    memcpy(bundle,out.str(),size);
    return NS_OK;
}

NS_IMETHODIMP RPCallImpl::Demarshal(void *  data, PRUint32  _size) {
    if(callSide == unDefined) {
        callSide = onServer;
    }
    istrstream in((const char*)data,_size);
    nsIAllocator * allocator = NULL;
    if (callSide == onServer) {
        in.read((char*)&iid,sizeof(nsIID));
        in.read((char*)&oid,sizeof(OID));
        in.read((char*)&methodIndex,sizeof(PRUint16));
        nsIInterfaceInfoManager *iimgr = NULL;
        iimgr = XPTI_GetInterfaceInfoManager();
        if (!iimgr) {
            return NS_ERROR_FAILURE; //nb how are we going to handle critical errors?
        }
        if (NS_FAILED(iimgr->GetInfoForIID(&iid, &interfaceInfo))) {
            NS_IF_RELEASE(iimgr);
            printf("--RemoteObjectProxy::GetInterfaceInfo NS_ERROR_FAILURE 1 \n");
            return NS_ERROR_FAILURE;
        }
        NS_IF_RELEASE(iimgr);
        interfaceInfo->GetMethodInfo(methodIndex,(const nsXPTMethodInfo**)&info);
        paramCount = info->GetParamCount();
        if (paramCount > 0) {
            params = (nsXPTCVariant*)malloc(sizeof(nsXPTCVariant) * paramCount);
            if (params == nsnull) {
                return NS_ERROR_OUT_OF_MEMORY;
            }
        }
        allocator = nsAllocator::GetGlobalAllocator();
    }
    for (int i = 0; i < paramCount; i++) {
        nsXPTParamInfo param = info->GetParam(i);
        PRBool isOut = param.IsOut();
        nsXPTCMiniVariant tmpValue = params[i]; //we need to set value for client side
        nsXPTCMiniVariant * value;
        value = &tmpValue;
        nsXPTType type = param.GetType();
        if ( (callSide == onServer && !param.IsIn() 
              || (callSide == onClient && !param.IsOut()))){ 
            if (callSide == onServer) { //we need to allocate memory for out parametr
                value->val.p = allocator->Alloc(sizeof(nsXPTCMiniVariant)); // sizeof(nsXPTCMiniVariant) is good
                params[i].Init(*value,type);
                params[i].flags |= nsXPTCVariant::PTR_IS_DATA;
                params[i].ptr = params[i].val.p;
            }
            continue;
        }
        switch(type.TagPart()) {
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
            marshalToolkit->ReadSimple(&in,value,&type,isOut,allocator);
            break;
        case nsXPTType::T_IID    :
            marshalToolkit->ReadIID(&in,value,&type,isOut,allocator);
            break;

        case nsXPTType::T_PSTRING_SIZE_IS: 
        case nsXPTType::T_PWSTRING_SIZE_IS:
        case nsXPTType::T_CHAR_STR  :
        case nsXPTType::T_WCHAR_STR :      
            marshalToolkit->ReadString(&in,value,&type,isOut,allocator);
            break;
        case nsXPTType::T_INTERFACE :      	    
        case nsXPTType::T_INTERFACE_IS :      	    
            marshalToolkit->ReadInterface(&in,value,&type,isOut,NULL,allocator);
            break;
            
        case nsXPTType::T_ARRAY:
            {
                nsXPTType datumType;
                if(NS_FAILED(interfaceInfo->GetTypeForParam(methodIndex, &param, 1,&datumType))) {
                    return NS_ERROR_FAILURE;
                }
                marshalToolkit->ReadArray(&in,value,&type,isOut,&datumType,allocator);
                break;
            }
        default:
            return NS_ERROR_FAILURE;
        }
        params[i].Init(*value,type);
	if (isOut) {
            params[i].flags |= nsXPTCVariant::PTR_IS_DATA;
            params[i].ptr = params[i].val.p;
        }
    }
    return NS_OK;
}

NS_IMETHODIMP RPCallImpl::GetRawData(void ** data, PRUint32 * _size) {
    if (!bundle) {
        return NS_ERROR_NULL_POINTER;
    }
    *data = malloc(size);
    *_size = size;
    memcpy(*data,bundle,size);
    return NS_OK;
}

//you should not free(params) 
NS_IMETHODIMP RPCallImpl::GetXPTCData(OID * _oid,PRUint32 *_methodIndex, 
				      PRUint32 *_paramCount, nsXPTCVariant** _params, nsresult ** _result) {
    *_oid = oid;
    *_methodIndex = methodIndex;
    *_paramCount = paramCount;
    *_params = params;
    *_result = &result;
    return NS_OK;
}



PRBool RPCallImpl::GetArraySizeFromParam( nsIInterfaceInfo *interfaceInfo, 
                                          const nsXPTMethodInfo* method,
                                          const nsXPTParamInfo& param,
                                          uint16 methodIndex,
                                          uint8 paramIndex,
                                          nsXPTCMiniVariant* nativeParams,
                                          SizeMode mode,
                                          PRUint32* result) {
    //code borrowed from mozilla/js/src/xpconnect/src/xpcwrappedjsclass.cpp
    uint8 argnum;
    nsresult rv;
    if(mode == GET_SIZE) {
        rv = interfaceInfo->GetSizeIsArgNumberForParam(methodIndex, &param, 0, &argnum);
    } else {
        rv = interfaceInfo->GetLengthIsArgNumberForParam(methodIndex, &param, 0, &argnum);
    }
    if(NS_FAILED(rv)) {
        return PR_FALSE;
    }
    const nsXPTParamInfo& arg_param = method->GetParam(argnum);
    const nsXPTType& arg_type = arg_param.GetType();
    
    // XXX require PRUint32 here - need to require in compiler too!
    if(arg_type.IsPointer() || arg_type.TagPart() != nsXPTType::T_U32)
        return PR_FALSE;
    
    if(arg_param.IsOut())
        *result = *(PRUint32*)nativeParams[argnum].val.p;
    else
        *result = nativeParams[argnum].val.u32;
    
    return PR_TRUE;
}


