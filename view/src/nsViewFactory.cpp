/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#include "nscore.h"
#include "nsIGenericFactory.h"
#include "nsISupports.h"
#include "nsCOMPtr.h"
#include "nsIServiceManager.h"

#include "nsViewsCID.h"
#include "nsView.h"
#include "nsScrollingView.h"
#include "nsScrollPortView.h"

#include "nsIModule.h"

static NS_DEFINE_CID(kCViewManager, NS_VIEW_MANAGER_CID);
static NS_DEFINE_CID(kCView, NS_VIEW_CID);
static NS_DEFINE_CID(kCScrollingView, NS_SCROLLING_VIEW_CID);
static NS_DEFINE_IID(kCScrollPortView, NS_SCROLL_PORT_VIEW_CID);
static NS_DEFINE_CID(kCComponentManager, NS_COMPONENTMANAGER_CID);

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIFactoryIID, NS_IFACTORY_IID);



class nsViewFactory : public nsIFactory
{   
public:   
    nsViewFactory(const nsCID &aClass);   
    virtual ~nsViewFactory();   

	NS_DECL_ISUPPORTS

	NS_DECL_NSIFACTORY

  private:   
    nsCID     mClassID;
};   

nsViewFactory::nsViewFactory(const nsCID &aClass)   
{   
	NS_INIT_ISUPPORTS();
	mClassID = aClass;
}   

nsViewFactory::~nsViewFactory()   
{   
}   

NS_IMPL_ISUPPORTS1(nsViewFactory, nsIFactory)

nsresult nsViewFactory::CreateInstance(nsISupports *aOuter,  
                                       const nsIID &aIID,  
                                       void **aResult)
{
	nsresult rv = NS_OK;

	if (aResult == NULL) {  
		return NS_ERROR_NULL_POINTER;  
	}  

	*aResult = NULL;  

	// views aren't reference counted, so have to be treated specially.
	// their lifetimes are managed by the view manager they are associated with.
	nsIView* view = nsnull;
	if (mClassID.Equals(kCView)) {
		view = new nsView();
	} else if (mClassID.Equals(kCScrollingView)) {
		view = new nsScrollingView();
	} else if (mClassID.Equals(kCScrollPortView)) {
		view = new nsScrollPortView(); 
	}

	if (nsnull == view)
		return NS_ERROR_OUT_OF_MEMORY;  
	rv = view->QueryInterface(aIID, aResult);
	if (NS_FAILED(rv))
		view->Destroy();
	
	return rv;  
}  

nsresult nsViewFactory::LockFactory(PRBool aLock)  
{  
  // Not implemented in simplest case.  
  return NS_OK;
}

#include "nsViewManager2.h"

NS_GENERIC_FACTORY_CONSTRUCTOR(nsViewManager2)

static NS_IMETHODIMP ViewManagerConstructor(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
  return nsViewManager2Constructor(aOuter, aIID, aResult);
}

#define nsViewManagerConstructor ViewManagerConstructor


//////////////////////////////////////////////////////////////////////
// Module object definition

class nsViewModule : public nsIModule
{
  public:
    NS_DECL_ISUPPORTS

    // Construction, Init and destruction
    nsViewModule();
    virtual ~nsViewModule();

    // nsIModule Interfaces
    NS_DECL_NSIMODULE

    // Facility for counting object instances
    int IncrementObjCount() { if (mObjCount < 0) mObjCount = 0; return ++mObjCount; }
    int DecrementObjCount()
    {
        NS_ASSERTION(mObjCount <= -1, "Object count negative.");
        return --mObjCount;
    }
    int GetObjCount() { return mObjCount; }

  private:
    int mObjCount;
};

//////////////////////////////////////////////////////////////////////
// Module Global
//
// This is used by the module to count object. Constructors and
// destructors of objects created by this module need to
// inc/dec the object count for the module. Unload decision will
// be taken based on this.
//
// Constructor:
//	gModule->IncrementObjCount();
//
// Descructor:
//	gModule->DecrementObjCount();
//
// WARNING: This is weak reference. XPCOM guarantees that this module
// 			object will be deleted just before the dll is unloaded. Hence,
//			holding a weak reference is ok.

static nsViewModule *gModule = NULL;

//////////////////////////////////////////////////////////////////////
// Module entry point

extern "C" NS_EXPORT nsresult NSGetModule(nsIComponentManager *servMgr,
                                          nsIFile* aPath,
                                          nsIModule** return_cobj)
{
    nsViewModule *viewModule;
    nsresult rv = NS_OK;

    NS_ASSERTION(return_cobj, "Null argument");
    NS_ASSERTION(gModule == NULL, "nsViewModule: Module already created.");

    viewModule = new nsViewModule;
    if (viewModule == NULL) return NS_ERROR_OUT_OF_MEMORY;

    rv = viewModule->QueryInterface(NS_GET_IID(nsIModule), (void **)return_cobj);

    if (NS_FAILED(rv))
    {
        delete viewModule;
        viewModule = NULL;
    }

    // WARNING: Weak Reference
    gModule = viewModule;

    return rv;
}

//////////////////////////////////////////////////////////////////////
// VIEW Decoder Module Implementation

NS_IMPL_ISUPPORTS(nsViewModule, NS_GET_IID(nsIModule))

nsViewModule::nsViewModule(void)
  : mObjCount(-1)
{
    NS_INIT_ISUPPORTS();
}

nsViewModule::~nsViewModule(void)
{
    NS_ASSERTION(mObjCount <= 0, "Module released while having outstanding objects.");
}

//
// The class object for us is just the factory and nothing more.
//
NS_IMETHODIMP
nsViewModule::GetClassObject(nsIComponentManager *aCompMgr, const nsCID & aClass,
                            const nsIID &aIID, void **aFactory)
{
    nsresult rv = NS_OK;
    
    if (nsnull == aFactory)
        return NS_ERROR_NULL_POINTER;

    // XXX should we do this ?
    *aFactory = nsnull;
        
    if (aClass.Equals(kCViewManager))
    {
        nsCOMPtr<nsIGenericFactory> factory;
        rv = NS_NewGenericFactory(getter_AddRefs(factory), &nsViewManagerConstructor);
        if (NS_SUCCEEDED(rv))
        {
            rv = factory->QueryInterface(aIID, aFactory);
        }
    }
    else if (aClass.Equals(kCView) || aClass.Equals(kCScrollingView) || aClass.Equals(kCScrollPortView))
    {
        rv = NS_ERROR_OUT_OF_MEMORY;
        nsViewFactory* factory = new nsViewFactory(aClass);
        if (factory)
        {
            NS_ADDREF(factory);
            rv = factory->QueryInterface(aIID, aFactory);
            NS_RELEASE(factory);
        }
    }
	
    return rv;
}

NS_IMETHODIMP
nsViewModule::RegisterSelf(nsIComponentManager *aCompMgr,
                           nsIFile* aPath,
                           const char *registryLocation,
                           const char *componentType)
{
    nsresult rv;
  
#ifdef DEBUG_dp
    printf("***Registering view library...");
#endif

    rv = aCompMgr->RegisterComponentSpec(kCViewManager, "View Manager",
                                         "@mozilla.org/view-manager;1",
                                         aPath, PR_TRUE, PR_TRUE);
    if (NS_FAILED(rv)) return rv;
    rv = aCompMgr->RegisterComponentSpec(kCView, "View",
                                         "@mozilla.org/view;1",
                                         aPath, PR_TRUE, PR_TRUE);
    if (NS_FAILED(rv)) return rv;
    rv = aCompMgr->RegisterComponentSpec(kCScrollingView, "Scrolling View",
                                         "@mozilla.org/scrolling-view;1",
                                         aPath, PR_TRUE, PR_TRUE);
    if (NS_FAILED(rv)) return rv;
    rv = aCompMgr->RegisterComponentSpec(kCScrollPortView, "Scroll Port View",
                                         "@mozilla.org/scroll-port-view;1",
                                         aPath, PR_TRUE, PR_TRUE);
#ifdef DEBUG_dp
    printf("done.\n");
#endif
    return rv;
}

NS_IMETHODIMP
nsViewModule::UnregisterSelf(nsIComponentManager *aCompMgr,
                             nsIFile* aPath,
                             const char *registryLocation)
{
    nsresult rv;
    rv = aCompMgr->UnregisterComponentSpec(kCViewManager, aPath);
    if (NS_FAILED(rv)) return rv;
    rv = aCompMgr->UnregisterComponentSpec(kCView, aPath);
    if (NS_FAILED(rv)) return rv;
    rv = aCompMgr->UnregisterComponentSpec(kCScrollingView, aPath);
    if (NS_FAILED(rv)) return rv;
    rv = aCompMgr->UnregisterComponentSpec(kCScrollPortView, aPath);
    return rv;
}

NS_IMETHODIMP
nsViewModule::CanUnload(nsIComponentManager *aCompMgr, PRBool *okToUnload)
{
    if (mObjCount < 0)
    {
        // Dll doesn't count objects.
        return NS_ERROR_FAILURE;
    }

    PRBool unloadable = PR_FALSE;
    if (mObjCount == 0)
    {
        // This dll can be unloaded now
        unloadable = PR_TRUE;
    }

    if (okToUnload) *okToUnload = unloadable;
    return NS_OK;
}

//////////////////////////////////////////////////////////////////////
