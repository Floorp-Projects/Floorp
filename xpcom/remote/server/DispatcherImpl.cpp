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
#include "DispatcherImpl.h"
#include "RPCServerService.h"
#include "IRPCChannel.h"
#include "xptcall.h"

class ObjectDescription {
public:
  nsISupports* p;
  OID          oid;
public:
  ObjectDescription(nsISupports *_p, OID _oid) {
    p = _p; oid = _oid;
  }
  ~ObjectDescription() {
  }
};



NS_IMPL_ISUPPORTS(DispatcherImpl, NS_GET_IID(IDispatcher));


DispatcherImpl::DispatcherImpl()  
        : m_objects(NULL), m_currentOID(0) 
{
    NS_INIT_REFCNT();
}

DispatcherImpl::~DispatcherImpl() {
  PRUint32 size = m_objects->GetSize();
  for (PRUint32 i = 0; i < size; i++) {
    ObjectDescription* obj = (ObjectDescription*)(*m_objects)[i];
    delete obj;
  }
  delete m_objects;
}

NS_IMETHODIMP DispatcherImpl::Dispatch(IRPCall *call) {
    RPCServerService * rpcService = RPCServerService::GetInstance();
    IRPCChannel *channel = NULL;
    OID oid; 
    PRUint32 methodIndex;
    PRUint32 paramCount;
    nsXPTCVariant* params;
    nsresult *result;
    call->GetXPTCData(&oid,&methodIndex, &paramCount, &params, &result);
    nsISupports * p = NULL;
    OIDToPointer(&p,oid);
    *result = XPTC_InvokeByIndex(p, methodIndex,
				 paramCount, params);
    call->Marshal();
    rpcService->GetRPCChannel(&channel);
    channel->SendReceive(call);
    return NS_OK;
}

NS_IMETHODIMP DispatcherImpl::OIDToPointer(nsISupports **_p,OID _oid) {
  
  PRUint32 size = m_objects->GetSize();
  PRUint32 i;
  ObjectDescription* desc;

  for (i = 0; i < size; i++) {
    desc = (ObjectDescription*)(*m_objects)[i];
    if (desc->oid == _oid) {
      *_p = desc->p;
      return NS_OK;
    }
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP DispatcherImpl:: PointerToOID(OID *_oid, nsISupports *_p) {
  PRUint32 size = m_objects->GetSize();
  PRUint32 i;
  ObjectDescription* desc;

  for (i = 0; i < size; i++) {
    desc = (ObjectDescription*)(*m_objects)[i];
    if (desc->p == _p) {
      *_oid = desc->oid;
      return NS_OK;
    }
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP DispatcherImpl::RegisterWithOID(nsISupports *_p, OID _oid) {

  if (_p == NULL) 
     return NS_OK;
  if (m_objects == NULL) {
    m_objects = new nsVector();
    if (m_objects == NULL)
            return NS_ERROR_OUT_OF_MEMORY;
  }

  ObjectDescription* desc = new ObjectDescription(_p, _oid);
  PRUint32 size = m_objects->GetSize();
  PRUint32 i, rv;
  rv = -1;
  for (i = 0; i < size; i++) {
    if ((*m_objects)[i] == NULL) {
      fprintf(stderr, "Adding %p to index %d\n", desc, i);
      (*m_objects)[i] = desc, rv = i;
      break;
    }
 }
  
  if (i >= size) {
    fprintf(stderr, "Adding %p to the end\n", desc);
    rv = m_objects->Add(desc);
  }
  return rv == -1 ? NS_ERROR_FAILURE : NS_OK;
}


NS_IMETHODIMP DispatcherImpl::UnregisterWithOID(nsISupports *_p, OID _oid) {

  if (_p == NULL) 
     return NS_OK;
  
  PRUint32 size = m_objects->GetSize();
  PRUint32 i, rv;
  rv = -1;
  for (i = 0; i < size; i++) {
    ObjectDescription* desc = (ObjectDescription*)(*m_objects)[i];
    if (desc == NULL) continue;
    if (desc->oid == _oid) {
      // this is kind of security  measure :)
      if (desc->p == _p) {
	delete desc;
	(*m_objects)[i] = NULL;
	  return NS_OK;
      }
      return NS_ERROR_FAILURE;
    }
    
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP DispatcherImpl::GetOID(OID *_oid) {
  *_oid = --m_currentOID;
  return NS_OK;
}







