/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#include "pratom.h"
#include "prefapi.h"
#include "nsIFactory.h"
#include "nsIComponentManager.h"
#include "nsIPref.h"
#ifdef XP_MAC
#include "nsINetSupport.h"
#include "nsIStreamListener.h"
#endif
#include "nsIServiceManager.h"
#include "nsCOMPtr.h"

static NS_DEFINE_IID(kIPrefIID, NS_IPREF_IID);
static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);

static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);

class nsPref: public nsIPref {
  NS_DECL_ISUPPORTS

private:
  nsPref();
  ~nsPref();

  static void useDefaultPrefFile(nsPref *aPrefInst);
  static nsPref *mInstance;

public:
  static nsPref *GetInstance();

  // Initialize/shutdown
  NS_IMETHOD Startup(char *filename);
  NS_IMETHOD Shutdown();

  // Config file input
  NS_IMETHOD ReadUserJSFile(char *filename);
  NS_IMETHOD ReadLIJSFile(char *filename);

  // JS stuff
  NS_IMETHOD GetConfigContext(JSContext **js_context);
  NS_IMETHOD GetGlobalConfigObject(JSObject **js_object);
  NS_IMETHOD GetPrefConfigObject(JSObject **js_object);

  NS_IMETHOD EvaluateConfigScript(const char * js_buffer, size_t length,
				  const char* filename, 
				  PRBool bGlobalContext, 
				  PRBool bCallbacks);

  // Getters
  NS_IMETHOD GetCharPref(const char *pref, 
			 char * return_buf, int * buf_length);
  NS_IMETHOD GetIntPref(const char *pref, int32 * return_int);	
  NS_IMETHOD GetBoolPref(const char *pref, XP_Bool * return_val);	
  NS_IMETHOD GetBinaryPref(const char *pref, 
			   void * return_val, int * buf_length);	
  NS_IMETHOD GetColorPref(const char *pref,
			  uint8 *red, uint8 *green, uint8 *blue);
  NS_IMETHOD GetColorPrefDWord(const char *pref, uint32 *colorref);
  NS_IMETHOD GetRectPref(const char *pref, 
			 int16 *left, int16 *top, 
			 int16 *right, int16 *bottom);

  // Setters
  NS_IMETHOD SetCharPref(const char *pref,const char* value);
  NS_IMETHOD SetIntPref(const char *pref,int32 value);
  NS_IMETHOD SetBoolPref(const char *pref,PRBool value);
  NS_IMETHOD SetBinaryPref(const char *pref,void * value, long size);
  NS_IMETHOD SetColorPref(const char *pref, 
			  uint8 red, uint8 green, uint8 blue);
  NS_IMETHOD SetColorPrefDWord(const char *pref, uint32 colorref);
  NS_IMETHOD SetRectPref(const char *pref, 
			 int16 left, int16 top, int16 right, int16 bottom);

  // Get Defaults
  NS_IMETHOD GetDefaultCharPref(const char *pref, 
				char * return_buf, int * buf_length);
  NS_IMETHOD GetDefaultIntPref(const char *pref, int32 * return_int);
  NS_IMETHOD GetDefaultBoolPref(const char *pref, XP_Bool * return_val);
  NS_IMETHOD GetDefaultBinaryPref(const char *pref, 
				  void * return_val, int * buf_length);
  NS_IMETHOD GetDefaultColorPref(const char *pref, 
				 uint8 *red, uint8 *green, uint8 *blue);
  NS_IMETHOD GetDefaultColorPrefDWord(const char *pref, 
				      uint32 *colorref);
  NS_IMETHOD GetDefaultRectPref(const char *pref, 
				int16 *left, int16 *top, 
				int16 *right, int16 *bottom);

  // Set defaults
  NS_IMETHOD SetDefaultCharPref(const char *pref,const char* value);
  NS_IMETHOD SetDefaultIntPref(const char *pref,int32 value);
  NS_IMETHOD SetDefaultBoolPref(const char *pref,PRBool value);
  NS_IMETHOD SetDefaultBinaryPref(const char *pref,
				  void * value, long size);
  NS_IMETHOD SetDefaultColorPref(const char *pref, 
				 uint8 red, uint8 green, uint8 blue);
  NS_IMETHOD SetDefaultRectPref(const char *pref, 
				int16 left, int16 top, 
				int16 right, int16 bottom);
  
  // Copy prefs
  NS_IMETHOD CopyCharPref(const char *pref, char ** return_buf);
  NS_IMETHOD CopyBinaryPref(const char *pref,
			    void ** return_value, int *size);

  NS_IMETHOD CopyDefaultCharPref( const char 
				  *pref,  char ** return_buffer );
  NS_IMETHOD CopyDefaultBinaryPref(const char *pref, 
				   void ** return_val, int * size);	

  // Path prefs
  NS_IMETHOD CopyPathPref(const char *pref, char ** return_buf);
  NS_IMETHOD SetPathPref(const char *pref, 
			 const char *path, PRBool set_default);

  // Pref info
  NS_IMETHOD PrefIsLocked(const char *pref, XP_Bool *res);

  // Save pref files
  NS_IMETHOD SavePrefFile(void);
  NS_IMETHOD SavePrefFileAs(const char *filename);
  NS_IMETHOD SaveLIPrefFile(const char *filename);

  // Callbacks
  NS_IMETHOD RegisterCallback( const char* domain,
			       PrefChangedFunc callback, 
			       void* instance_data );
  NS_IMETHOD UnregisterCallback( const char* domain,
				 PrefChangedFunc callback, 
				 void* instance_data );
};

nsPref* nsPref::mInstance = NULL;

static PRInt32 g_InstanceCount = 0;
static PRInt32 g_LockCount = 0;

static nsresult _convertRes(int res)
{
  nsresult nsres = NS_OK;
  switch (res) {
  case PREF_OUT_OF_MEMORY:
    nsres = NS_ERROR_OUT_OF_MEMORY;
    break;
  case PREF_NOT_INITIALIZED:
    nsres = NS_ERROR_NOT_INITIALIZED;
    break;
  case PREF_TYPE_CHANGE_ERR:
  case PREF_ERROR:
  case PREF_BAD_LOCKFILE:
    nsres = NS_ERROR_UNEXPECTED;
    break;
  case PREF_VALUECHANGED:
    nsres = NS_PREF_VALUE_CHANGED;
    break;
  };

  return nsres;
}

/*
 * Constructor/Destructor
 */

nsPref::nsPref() {
  PR_AtomicIncrement(&g_InstanceCount);
  NS_INIT_REFCNT();
}

nsPref::~nsPref() {
  PR_AtomicDecrement(&g_InstanceCount);
  mInstance = NULL;
}

void
nsPref::useDefaultPrefFile(nsPref *aPrefInst)
{
  /* temporary hack to load up pref files */

  if (!aPrefInst)
    return;

  aPrefInst->Startup("prefs.js");

  return;
}

nsPref *nsPref::GetInstance()
{
  if (mInstance == NULL) {
    mInstance = new nsPref();
    useDefaultPrefFile(mInstance);
  }
  return mInstance;
}

/*
 * nsISupports Implementation
 */

NS_IMPL_ISUPPORTS(nsPref, kIPrefIID);

/*
 * nsIPref Implementation
 */

NS_IMETHODIMP nsPref::Startup(char *filename)
{
  return _convertRes(PREF_Init(filename));
}
NS_IMETHODIMP nsPref::Shutdown()
{
  PREF_Cleanup();

  return NS_OK;
}

/*
 * Config file input
 */

NS_IMETHODIMP nsPref::ReadUserJSFile(char *filename)
{
  return _convertRes(PREF_ReadUserJSFile(filename));
}

NS_IMETHODIMP nsPref::ReadLIJSFile(char *filename)
{
  return _convertRes(PREF_ReadLIJSFile(filename));
}

/*
 * JS stuff
 */

NS_IMETHODIMP nsPref::GetConfigContext(JSContext **js_context)
{
  return _convertRes(PREF_GetConfigContext(js_context));
}

NS_IMETHODIMP nsPref::GetGlobalConfigObject(JSObject **js_object)
{
  return _convertRes(PREF_GetGlobalConfigObject(js_object));
}

NS_IMETHODIMP nsPref::GetPrefConfigObject(JSObject **js_object)
{
  return _convertRes(PREF_GetPrefConfigObject(js_object));
}

NS_IMETHODIMP nsPref::EvaluateConfigScript(const char * js_buffer,
					   size_t length,
					   const char* filename, 
					   PRBool bGlobalContext, 
					   PRBool bCallbacks)
{
  return _convertRes(PREF_EvaluateConfigScript(js_buffer,
					       length,
					       filename,
					       bGlobalContext,
					       bCallbacks,
					       PR_TRUE));
}

/*
 * Getters
 */

NS_IMETHODIMP nsPref::GetCharPref(const char *pref, 
				  char * return_buf, int * buf_length)
{
  return _convertRes(PREF_GetCharPref(pref, return_buf, buf_length));
}

NS_IMETHODIMP nsPref::GetIntPref(const char *pref, int32 * return_int)
{
  return _convertRes(PREF_GetIntPref(pref, return_int));
}

NS_IMETHODIMP nsPref::GetBoolPref(const char *pref, XP_Bool * return_val)
{
  return _convertRes(PREF_GetBoolPref(pref, return_val));
}

NS_IMETHODIMP nsPref::GetBinaryPref(const char *pref, 
				    void * return_val, int * buf_length)
{
  return _convertRes(PREF_GetBinaryPref(pref, return_val, buf_length));
}

NS_IMETHODIMP nsPref::GetColorPref(const char *pref,
				   uint8 *red, uint8 *green, uint8 *blue)
{
  return _convertRes(PREF_GetColorPref(pref, red, green, blue));
}

NS_IMETHODIMP nsPref::GetColorPrefDWord(const char *pref, 
					uint32 *colorref)
{
  return _convertRes(PREF_GetColorPrefDWord(pref, colorref));
}

NS_IMETHODIMP nsPref::GetRectPref(const char *pref, 
				  int16 *left, int16 *top, 
				  int16 *right, int16 *bottom)
{
  return _convertRes(PREF_GetRectPref(pref, left, top, right, bottom));
}

/*
 * Setters
 */

NS_IMETHODIMP nsPref::SetCharPref(const char *pref,const char* value)
{
  return _convertRes(PREF_SetCharPref(pref, value));
}

NS_IMETHODIMP nsPref::SetIntPref(const char *pref,int32 value)
{
  return _convertRes(PREF_SetIntPref(pref, value));
}

NS_IMETHODIMP nsPref::SetBoolPref(const char *pref,PRBool value)
{
  return _convertRes(PREF_SetBoolPref(pref, value));
}

NS_IMETHODIMP nsPref::SetBinaryPref(const char *pref,void * value, long size)
{
  return _convertRes(PREF_SetBinaryPref(pref, value, size));
}

NS_IMETHODIMP nsPref::SetColorPref(const char *pref, 
				   uint8 red, uint8 green, uint8 blue)
{
  return _convertRes(PREF_SetColorPref(pref, red, green, blue));
}

NS_IMETHODIMP nsPref::SetColorPrefDWord(const char *pref, 
					uint32 value)
{
  return _convertRes(PREF_SetColorPrefDWord(pref, value));
}

NS_IMETHODIMP nsPref::SetRectPref(const char *pref, 
				  int16 left, int16 top, 
				  int16 right, int16 bottom)
{
  return _convertRes(PREF_SetRectPref(pref, left, top, right, bottom));
}

/*
 * Get Defaults
 */

NS_IMETHODIMP nsPref::GetDefaultCharPref(const char *pref, 
					 char * return_buf,
					 int * buf_length)
{
  return _convertRes(PREF_GetDefaultCharPref(pref, return_buf, buf_length));
}

NS_IMETHODIMP nsPref::GetDefaultIntPref(const char *pref,
					int32 * return_int)
{
  return _convertRes(PREF_GetDefaultIntPref(pref, return_int));
}

NS_IMETHODIMP nsPref::GetDefaultBoolPref(const char *pref,
					 XP_Bool * return_val)
{
  return _convertRes(PREF_GetDefaultBoolPref(pref, return_val));
}

NS_IMETHODIMP nsPref::GetDefaultBinaryPref(const char *pref, 
					   void * return_val,
					   int * buf_length)
{
  return _convertRes(PREF_GetDefaultBinaryPref(pref, return_val, buf_length));
}

NS_IMETHODIMP nsPref::GetDefaultColorPref(const char *pref, 
					  uint8 *red, uint8 *green, 
					  uint8 *blue)
{
  return _convertRes(PREF_GetDefaultColorPref(pref, red, green, blue));
}

NS_IMETHODIMP nsPref::GetDefaultColorPrefDWord(const char *pref, 
					       uint32 *colorref)
{
  return _convertRes(PREF_GetDefaultColorPrefDWord(pref, colorref));
}

NS_IMETHODIMP nsPref::GetDefaultRectPref(const char *pref, 
					 int16 *left, int16 *top, 
					 int16 *right, int16 *bottom)
{
  return _convertRes(PREF_GetDefaultRectPref(pref, 
					     left, top, right, bottom));
}

/*
 * Set defaults
 */

NS_IMETHODIMP nsPref::SetDefaultCharPref(const char *pref,const char* value)
{
  return _convertRes(PREF_SetDefaultCharPref(pref, value));
}

NS_IMETHODIMP nsPref::SetDefaultIntPref(const char *pref,int32 value)
{
  return _convertRes(PREF_SetDefaultIntPref(pref, value));
}

NS_IMETHODIMP nsPref::SetDefaultBoolPref(const char *pref, PRBool value)
{
  return _convertRes(PREF_SetDefaultBoolPref(pref, value));
}

NS_IMETHODIMP nsPref::SetDefaultBinaryPref(const char *pref,
					   void * value, long size)
{
  return _convertRes(PREF_SetDefaultBinaryPref(pref, value, size));
}

NS_IMETHODIMP nsPref::SetDefaultColorPref(const char *pref, 
					  uint8 red, uint8 green, uint8 blue)
{
  return _convertRes(PREF_SetDefaultColorPref(pref, red, green, blue));
}

NS_IMETHODIMP nsPref::SetDefaultRectPref(const char *pref, 
					 int16 left, int16 top,
					 int16 right, int16 bottom)
{
  return _convertRes(PREF_SetDefaultRectPref(pref, left, top, right, bottom));
}
  
/*
 * Copy prefs
 */

NS_IMETHODIMP nsPref::CopyCharPref(const char *pref, char ** return_buf)
{
  return _convertRes(PREF_CopyCharPref(pref, return_buf));
}

NS_IMETHODIMP nsPref::CopyBinaryPref(const char *pref,
				     void ** return_value, int *size)
{
  return _convertRes(PREF_CopyBinaryPref(pref, return_value, size));
}

NS_IMETHODIMP nsPref::CopyDefaultCharPref( const char *pref,
					   char ** return_buffer )
{
  return _convertRes(PREF_CopyDefaultCharPref(pref, return_buffer));
}

NS_IMETHODIMP nsPref::CopyDefaultBinaryPref(const char *pref, 
					    void ** return_val, int * size)
{
  return _convertRes(PREF_CopyDefaultBinaryPref(pref, return_val, size));
}

/*
 * Path prefs
 */

NS_IMETHODIMP nsPref::CopyPathPref(const char *pref, char ** return_buf)
{
  return _convertRes(PREF_CopyPathPref(pref, return_buf));
}

NS_IMETHODIMP nsPref::SetPathPref(const char *pref, 
				  const char *path, PRBool set_default)
{
  return _convertRes(PREF_SetPathPref(pref, path, set_default));
}

/*
 * Pref info
 */

NS_IMETHODIMP nsPref::PrefIsLocked(const char *pref, XP_Bool *res)
{
  if (res == NULL) {
    return NS_ERROR_INVALID_POINTER;
  }

  *res = PREF_PrefIsLocked(pref);
  return NS_OK;
}

/*
 * Save pref files
 */

NS_IMETHODIMP nsPref::SavePrefFile(void)
{
  return _convertRes(PREF_SavePrefFile());
}

NS_IMETHODIMP nsPref::SavePrefFileAs(const char *filename)
{
  return _convertRes(PREF_SavePrefFileAs(filename));
}

NS_IMETHODIMP nsPref::SaveLIPrefFile(const char *filename)
{
  return _convertRes(PREF_SaveLIPrefFile(filename));
}

/*
 * Callbacks
 */

NS_IMETHODIMP nsPref::RegisterCallback( const char* domain,
					PrefChangedFunc callback, 
					void* instance_data )
{
  PREF_RegisterCallback(domain, callback, instance_data);
  return NS_OK;
}

NS_IMETHODIMP nsPref::UnregisterCallback( const char* domain,
					  PrefChangedFunc callback, 
					  void* instance_data )
{
  return _convertRes(PREF_UnregisterCallback(domain, callback, instance_data));
}

class nsPrefFactory: public nsIFactory {
  NS_DECL_ISUPPORTS
  
  nsPrefFactory() {
    NS_INIT_REFCNT();
    PR_AtomicIncrement(&g_InstanceCount);
  }

  ~nsPrefFactory() {
    PR_AtomicDecrement(&g_InstanceCount);
  }

  NS_IMETHOD CreateInstance(nsISupports *aDelegate,
                            const nsIID &aIID,
                            void **aResult);

  NS_IMETHOD LockFactory(PRBool aLock) {
    if (aLock) {
      PR_AtomicIncrement(&g_LockCount);
    } else {
      PR_AtomicDecrement(&g_LockCount);
    }
    return NS_OK;
  };
};

static NS_DEFINE_IID(kFactoryIID, NS_IFACTORY_IID);

NS_IMPL_ISUPPORTS(nsPrefFactory, kFactoryIID);

nsresult nsPrefFactory::CreateInstance(nsISupports *aDelegate,
                                            const nsIID &aIID,
                                            void **aResult)
{
  if (aDelegate != NULL) {
    return NS_ERROR_NO_AGGREGATION;
  }

  nsPref *t = nsPref::GetInstance();
  
  if (t == NULL) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  
  nsresult res = t->QueryInterface(aIID, aResult);
  
  if (NS_FAILED(res)) {
    *aResult = NULL;
  }

  return res;
}

extern "C" NS_EXPORT nsresult 
NSGetFactory(nsISupports* serviceMgr,
             const nsCID &aClass,
             const char *aClassName,
             const char *aProgID,
             nsIFactory **aFactory)
{
  if (aFactory == NULL) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aClass.Equals(kPrefCID)) {
    nsPrefFactory *factory = new nsPrefFactory();
    nsresult res = factory->QueryInterface(kFactoryIID, (void **) aFactory);
    if (NS_FAILED(res)) {
      *aFactory = NULL;
      delete factory;
    }
    return res;
  }
  return NS_NOINTERFACE;
}

extern "C" NS_EXPORT PRBool NSCanUnload(nsISupports* serviceMgr)
{
  return PRBool(g_InstanceCount == 0 && g_LockCount == 0);
}

extern "C" NS_EXPORT nsresult NSRegisterSelf(nsISupports* aServMgr, const char *path)
{
  nsresult rv;

  nsCOMPtr<nsIServiceManager> servMgr(do_QueryInterface(aServMgr, &rv));
  if (NS_FAILED(rv)) return rv;

  nsIComponentManager* compMgr;
  rv = servMgr->GetService(kComponentManagerCID, 
                           nsIComponentManager::GetIID(), 
                           (nsISupports**)&compMgr);
  if (NS_FAILED(rv)) return rv;

  rv = compMgr->RegisterComponent(kPrefCID, NULL, NULL, path, 
                                  PR_TRUE, PR_TRUE);

  (void)servMgr->ReleaseService(kComponentManagerCID, compMgr);
  return rv;
}

extern "C" NS_EXPORT nsresult NSUnregisterSelf(nsISupports* aServMgr, const char *path)
{
  nsresult rv;

  nsCOMPtr<nsIServiceManager> servMgr(do_QueryInterface(aServMgr, &rv));
  if (NS_FAILED(rv)) return rv;

  nsIComponentManager* compMgr;
  rv = servMgr->GetService(kComponentManagerCID, 
                           nsIComponentManager::GetIID(), 
                           (nsISupports**)&compMgr);
  if (NS_FAILED(rv)) return rv;

  rv = compMgr->UnregisterFactory(kPrefCID, path);

  (void)servMgr->ReleaseService(kComponentManagerCID, compMgr);
  return rv;
}

