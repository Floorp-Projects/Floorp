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
 *   Pierre Phaneuf <pp@ludusdesign.com>
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


#include "nsProxyEvent.h"
#include "nsIProxyObjectManager.h"
#include "nsProxyEventPrivate.h"

#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsCOMPtr.h"

#include "nsMemory.h"
#include "nsHashtable.h"

#include "nsAutoLock.h"
#include "nsIInterfaceInfoManager.h"
#include "xptcall.h"

// LIFETIME_CACHE will cache class for the entire cyle of the application.
#define LIFETIME_CACHE

static uint32 zero_methods_descriptor;


/* ssc@netscape.com wishes he could get rid of this instance of
 * |NS_DEFINE_IID|, but |ProxyEventClassIdentity| is not visible from
 * here.
 */
static NS_DEFINE_IID(kProxyObject_Identity_Class_IID, NS_PROXYEVENT_IDENTITY_CLASS_IID);

//////////////////////////////////////////////////////////////////////////////////////////////////
//  nsProxyEventClass
//////////////////////////////////////////////////////////////////////////////////////////////////
NS_IMPL_THREADSAFE_ISUPPORTS1(nsProxyEventClass, nsProxyEventClass)

// static
nsProxyEventClass*
nsProxyEventClass::GetNewOrUsedClass(REFNSIID aIID)
{
    /* find in our hash table */
    
    nsProxyObjectManager *manager = nsProxyObjectManager::GetInstance();
    if (manager == nsnull) return nsnull;
	
	// dont need to lock the map, because this is only called
	// by nsProxyEventClass::GetNewOrUsed which locks it for us.

	nsHashtable *iidToClassMap =  manager->GetIIDToProxyClassMap();
    
    if (iidToClassMap == nsnull)
    {
        return nsnull;
    }

    nsProxyEventClass* clazz = nsnull;
    nsIDKey key(aIID);

#ifdef PROXYEVENTCLASS_DEBUG 
	char* iidStr = aIID.ToString();
	printf("GetNewOrUsedClass  %s\n", iidStr);
	nsCRT::free(iidStr);
#endif

    clazz = (nsProxyEventClass*) iidToClassMap->Get(&key);
	if(clazz)
	{
        NS_ADDREF(clazz);
#ifdef PROXYEVENTCLASS_DEBUG
		char* iidStr = aIID.ToString();
		printf("GetNewOrUsedClass  %s hit\n", iidStr);
		nsCRT::free(iidStr);
#endif
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

                while(NS_SUCCEEDED(oldest->GetParent(getter_AddRefs(parent)))&&
                      parent)
                {
                    oldest = parent;
                }

                PRBool isISupportsDescendent = PR_FALSE;
                nsID* iid;
                if(NS_SUCCEEDED(oldest->GetInterfaceIID(&iid))) 
                {
                    isISupportsDescendent = iid->Equals(NS_GET_IID(nsISupports));
                    nsMemory::Free(iid);
                }
                
                NS_ASSERTION(isISupportsDescendent, "!isISupportsDescendent");

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
    NS_ADDREF_THIS();
    
    mInfo = aInfo;

    /* add use to the used classes */
    nsIDKey key(aIID);

    nsProxyObjectManager *manager = nsProxyObjectManager::GetInstance();
    if (manager == nsnull) return;
	// dont need to lock the map, because this is only called
	// by GetNewOrUsed which locks it for us.

	nsHashtable *iidToClassMap =  manager->GetIIDToProxyClassMap();
    
    if (iidToClassMap != nsnull)
    {
        iidToClassMap->Put(&key, this);
#ifdef LIFETIME_CACHE
		// extra addref to hold it in the cache
        NS_ADDREF_THIS();
#endif
#ifdef PROXYEVENTCLASS_DEBUG
		char* iidStr = aIID.ToString();
		printf("GetNewOrUsedClass  %s put\n", iidStr);
		nsCRT::free(iidStr);
#endif
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

    if (nsProxyObjectManager::IsManagerShutdown())
        return;

#ifndef LIFETIME_CACHE
    nsIDKey key(mIID);
                
    nsCOMPtr<nsProxyObjectManager> manager = getter_AddRefs(nsProxyObjectManager::GetInstance());
    if (manager == nsnull) return;
	nsHashtable *iidToClassMap =  manager->GetIIDToProxyClassMap();
    
    if (iidToClassMap != nsnull)
    {
        iidToClassMap->Remove(&key);
#ifdef PROXYEVENTCLASS_DEBUG
		char* iidStr = mIID.ToString();
		printf("GetNewOrUsedClass  %s remove\n", iidStr);
		nsCRT::free(iidStr);
#endif
    }
#endif
}

nsresult
nsProxyEventClass::CallQueryInterfaceOnProxy(nsProxyEventObject* self, REFNSIID aIID, nsProxyEventObject** aInstancePtr)
{
    NS_PRECONDITION(aInstancePtr, "Requires non-null result");

    nsresult rv;

    *aInstancePtr = nsnull;  // in case of error.


    // The functions we will call: QueryInterface(REFNSIID aIID, void** aInstancePtr)

    nsXPTCMiniVariant var[2];

    var[0].val.p     = (void*)&aIID;
    var[1].val.p     = (void*)aInstancePtr;

    nsCOMPtr<nsIInterfaceInfo> interfaceInfo;
    const nsXPTMethodInfo *mi;

    nsCOMPtr<nsIInterfaceInfoManager> iim = getter_AddRefs(XPTI_GetInterfaceInfoManager());

    if (!iim) return NS_NOINTERFACE;
    iim->GetInfoForName("nsISupports", getter_AddRefs(interfaceInfo));
    interfaceInfo->GetMethodInfo(0, &mi); // 0 is QueryInterface

    rv = self->CallMethod(0, mi, var);

    if (NS_SUCCEEDED(rv))
    {
        nsISupports *aIdentificationObject;

        rv = (*aInstancePtr)->QueryInterface(kProxyObject_Identity_Class_IID, (void**)&aIdentificationObject);

        if (NS_FAILED(rv))
        {
            // okay, aInstancePtr was not a proxy.  Lets create one.
            nsProxyObjectManager *manager = nsProxyObjectManager::GetInstance();
            if (manager == nsnull) 
            {
                NS_IF_RELEASE((*aInstancePtr));
                return NS_ERROR_FAILURE;
            }

            rv = manager->GetProxyForObject(self->GetQueue(), 
                                            aIID, 
                                            self->GetRealObject(), 
                                            self->GetProxyType(),
                                            (void**) &aIdentificationObject);
        }

        NS_IF_RELEASE((*aInstancePtr));
        (*aInstancePtr) = NS_STATIC_CAST(nsProxyEventObject*, aIdentificationObject);
    }
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
        *aInstancePtr = NS_STATIC_CAST(void*, self);  
        NS_ADDREF(self);
        return NS_OK;
    }

    nsProxyEventObject* sibling;
    {
    nsProxyObjectManager* manager = nsProxyObjectManager::GetInstance();
    nsAutoMonitor mon(manager->GetMonitor());

    // This includes checking for nsISupports and the iid of self.
    // And it also checks for other wrappers that have been constructed
    // for this object.
    if(nsnull != (sibling = self->LockedFind(aIID)))
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
        if(NS_SUCCEEDED(current->GetInterfaceIID(&iid)) && iid)
        {
            PRBool found = aIID.Equals(*iid);
            nsMemory::Free(iid);
            if(found)
            {
                *aInstancePtr = (void*) self;
                NS_ADDREF(self);
                return NS_OK;
            }
        }
    }
    }

    return CallQueryInterfaceOnProxy(self, aIID, (nsProxyEventObject**)aInstancePtr);
}
