/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */


#include "nsProxyEvent.h"
#include "nsProxyObjectManager.h"
#include "nsProxyEventPrivate.h"

#include "nsRepository.h"
#include "nsIServiceManager.h"
#include "nsCOMPtr.h"

#include "nsIAllocator.h"
#include "nsHashtable.h"


#include "nsIInterfaceInfoManager.h"
#include "xptcall.h"



static uint32 zero_methods_descriptor;

static NS_DEFINE_IID(kProxyEventClassIID, NS_PROXYEVENT_CLASS_IID);

//////////////////////////////////////////////////////////////////////////////////////////////////
//  nsProxyEventClass
//////////////////////////////////////////////////////////////////////////////////////////////////
NS_IMPL_ISUPPORTS(nsProxyEventClass, kProxyEventClassIID)

// static
nsProxyEventClass*
nsProxyEventClass::GetNewOrUsedClass(REFNSIID aIID)
{
    /* find in our hash table */
    
    nsProxyObjectManager *manager = nsProxyObjectManager::GetInstance();
    nsHashtable *iidToClassMap =  manager->GetIIDToProxyClassMap();
    
    if (iidToClassMap == nsnull)
    {
        return nsnull;
    }

    nsProxyEventClass* clazz = nsnull;
    nsIDKey key(aIID);

    if(iidToClassMap->Exists(&key))
    {
        clazz = (nsProxyEventClass*) iidToClassMap->Get(&key);
        NS_ADDREF(clazz);
    }
    else
    {
        nsCOMPtr<nsIInterfaceInfoManager> iimgr = getter_AddRefs(XPTI_GetInterfaceInfoManager());
        if(iimgr)
        {
            nsCOMPtr<nsIInterfaceInfo> info;
            if(NS_SUCCEEDED(iimgr->GetInfoForIID(&aIID, getter_AddRefs(info))))
            {
                /* 
                   Check to see if isISupportsDescendent 
                */
                nsCOMPtr<nsIInterfaceInfo> oldest = info;
                nsCOMPtr<nsIInterfaceInfo> parent;

                while(NS_SUCCEEDED(oldest->GetParent(getter_AddRefs(parent))))
                {
                    oldest = parent;
                }

                PRBool isISupportsDescendent = PR_FALSE;
                nsID* iid;
                if(NS_SUCCEEDED(oldest->GetIID(&iid))) 
                {
                    isISupportsDescendent = iid->Equals(NS_GET_IID(nsISupports));
                    nsAllocator::Free(iid);
                }
                
                NS_VERIFY(isISupportsDescendent,"!isISupportsDescendent");

                if (isISupportsDescendent)  
                {
                    clazz = new nsProxyEventClass(aIID, info);
                    if(!clazz->mDescriptors)
                        NS_RELEASE(clazz);  // sets clazz to NULL
                }
            }
        }
    }
    return clazz;
}

nsProxyEventClass::nsProxyEventClass()
{
    NS_WARNING("This constructor should never be called");
}

nsProxyEventClass::nsProxyEventClass(REFNSIID aIID, nsIInterfaceInfo* aInfo)
: mIID(aIID),
  mDescriptors(NULL)
{
    NS_INIT_REFCNT();
    NS_ADDREF_THIS();
    
    mInfo = aInfo;

    /* add use to the used classes */
    nsIDKey key(aIID);

    nsProxyObjectManager *manager = nsProxyObjectManager::GetInstance();
    nsHashtable *iidToClassMap =  manager->GetIIDToProxyClassMap();
    
    if (iidToClassMap == nsnull)
    {
        iidToClassMap->Put(&key, this);
    }

    uint16 methodCount;
    if(NS_SUCCEEDED(mInfo->GetMethodCount(&methodCount)))
    {
        if(methodCount)
        {
            int wordCount = (methodCount/32)+1;
            if(NULL != (mDescriptors = new uint32[wordCount]))
            {
                memset(mDescriptors, 0, wordCount * sizeof(uint32));
            }
        }
        else
        {
            mDescriptors = &zero_methods_descriptor;
        }
    }
}

nsProxyEventClass::~nsProxyEventClass()
{
    if(mDescriptors && mDescriptors != &zero_methods_descriptor)
        delete [] mDescriptors;

    nsIDKey key(mIID);
    
    nsProxyObjectManager *manager = nsProxyObjectManager::GetInstance();
    nsHashtable *iidToClassMap =  manager->GetIIDToProxyClassMap();
    
    if (iidToClassMap != nsnull)
    {
        iidToClassMap->Remove(&key);
    }
}

nsresult
nsProxyEventClass::CallQueryInterfaceOnProxy(nsProxyEventObject* self, REFNSIID aIID, nsProxyEventObject** aInstancePtr)
{
    nsresult rv;

	*aInstancePtr = (nsProxyEventObject*)0xDEADBEEF;  // in case of error.

	
    // The functions we will call: QueryInterface(REFNSIID aIID, void** aInstancePtr)

    nsXPTCMiniVariant var[2];

    var[0].val.p     = (void*)&aIID;
    var[1].val.p     = (void*)aInstancePtr;

    nsIInterfaceInfoManager *iim = XPTI_GetInterfaceInfoManager();
    
	if (iim == nsnull) 
		return NS_NOINTERFACE;

	nsIInterfaceInfo *nsISupportsInfo;
    const nsXPTMethodInfo *mi;

    iim->GetInfoForName("nsISupports", &nsISupportsInfo);
    nsISupportsInfo->GetMethodInfo(0, &mi); // 0 is QueryInterface

    rv = self->CallMethod(0, mi, var);

    return rv;
}


/***************************************************************************/
// This 'ProxyEventClassIdentity' class and singleton allow us to figure out if
// any given nsISupports* is implemented by a nsProxy object. This is done
// using a QueryInterface call on the interface pointer with our ID. If
// that call returns NS_OK and the pointer is to a nsProxyEventObject.  It must
// be released when done.

// NS_PROXYEVENT_IDENTITY_CLASS_IID defined in nsProxyEventPrivate.h
class ProxyEventClassIdentity
{
    // no instance methods...
public:
    NS_DEFINE_STATIC_IID_ACCESSOR(NS_PROXYEVENT_IDENTITY_CLASS_IID)

    static void* GetSingleton()
    {
        static ProxyEventClassIdentity* singleton = NULL;
        if(!singleton)
            singleton = new ProxyEventClassIdentity();
        return (void*) singleton;
    }
};

NS_IMETHODIMP
nsProxyEventClass::DelegatedQueryInterface(nsProxyEventObject* self,
                                          REFNSIID aIID,
                                          void** aInstancePtr)
{
    
    if(aIID.Equals(NS_GET_IID(ProxyEventClassIdentity)))
    {
        *aInstancePtr = (void**)self;  //todo this should be a static cast
        NS_ADDREF(self);
        return NS_OK;
    }

    nsProxyEventObject* sibling;
   
    // This includes checking for nsISupports and the iid of self.
    // And it also checks for other wrappers that have been constructed
    // for this object.
    if(nsnull != (sibling = self->Find(aIID)))
    {
        NS_ADDREF(sibling);
        *aInstancePtr = (void*) sibling;
        return NS_OK;
    }

    // check if asking for an interface that we inherit from
    nsCOMPtr<nsIInterfaceInfo> current = GetInterfaceInfo();
    nsCOMPtr<nsIInterfaceInfo> parent;

    while(NS_SUCCEEDED(current->GetParent(getter_AddRefs(parent))) && parent)
    {
        current = parent;

        nsIID* iid;
        if(NS_SUCCEEDED(current->GetIID(&iid)) && iid)
        {
            PRBool found = aIID.Equals(*iid);
            nsAllocator::Free(iid);
            if(found)
            {
                *aInstancePtr = (void*) self;
                NS_ADDREF(self);
                return NS_OK;
            }
        }
    }

    return CallQueryInterfaceOnProxy(self, aIID, (nsProxyEventObject**)aInstancePtr);
}
