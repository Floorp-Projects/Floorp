/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL. You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All Rights
 * Reserved.
 */

#define NS_IMPL_IDS

#include "nsIEventQueueService.h"
#include "nsIProperties.h"
#include "nsIServiceManager.h"
#include "nsIURL.h"
#ifndef NECKO
#include "nsINetService.h"
static NS_DEFINE_IID(kNetServiceCID, NS_NETSERVICE_CID);
static NS_DEFINE_IID(kINetServiceIID, NS_INETSERVICE_IID);
#else
#include "nsIIOService.h"
#include "nsIChannel.h"
static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);
#endif // NECKO
#include "nsIComponentManager.h"
#include "nsIEnumerator.h"
#include <iostream.h>  //BAD DOG -- no biscuit!

#include "nsSpecialSystemDirectory.h"

#ifdef XP_PC
#include "plevent.h"
#endif

#define TEST_URL "resource:/res/test.properties"

#ifdef XP_PC
#define NETLIB_DLL "netlib.dll"
#define RAPTORBASE_DLL "raptorbase.dll"
#define XPCOM_DLL "xpcom32.dll"
#else
#ifdef XP_MAC
#define NETLIB_DLL "NETLIB_DLL"
#define RAPTORBASE_DLL "base.shlb"
#define XPCOM_DLL "XPCOM_DLL"
#else
#define NETLIB_DLL "libnetlib"MOZ_DLL_SUFFIX
#define RAPTORBASE_DLL "libraptorbase"MOZ_DLL_SUFFIX
#define XPCOM_DLL "libxpcom"MOZ_DLL_SUFFIX
#endif
#endif
static NS_DEFINE_IID(kEventQueueCID, NS_EVENTQUEUE_CID);
static NS_DEFINE_IID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_IID(kIEventQueueServiceIID, NS_IEVENTQUEUESERVICE_IID);
static NS_DEFINE_IID(kIPersistentPropertiesIID, NS_IPERSISTENTPROPERTIES_IID);



/***************************************************************************/
extern "C" void
NS_SetupRegistry()
{
  nsComponentManager::AutoRegister(nsIComponentManager::NS_Startup,
                                   NULL /* default */);

	// startup netlib:	
	nsComponentManager::RegisterComponent(kEventQueueServiceCID, NULL, NULL, XPCOM_DLL, PR_FALSE, PR_FALSE);
#ifndef NECKO
    nsComponentManager::RegisterComponent(kNetServiceCID, NULL, NULL, NETLIB_DLL, PR_FALSE, PR_FALSE);
#else
    nsComponentManager::RegisterComponent(kIOServiceCID, NULL, NULL, NETLIB_DLL, PR_FALSE, PR_FALSE);
#endif // NECKO

    // Create the Event Queue for this thread...
    nsIEventQueueService* pEventQService;
    
	pEventQService = nsnull;
    nsresult result = nsServiceManager::GetService(kEventQueueServiceCID,
                                                   kIEventQueueServiceIID,
                                                   (nsISupports **)&pEventQService);
    if (NS_SUCCEEDED(result)) {
      // XXX: What if this fails?
      result = pEventQService->CreateThreadEventQueue();
    }

	nsComponentManager::RegisterComponent(kPersistentPropertiesCID, 
 										 NULL,
 										 NULL, 
										 RAPTORBASE_DLL, 
										 PR_FALSE, 
										 PR_FALSE);
}


int
main(int argc, char* argv[])
{
  nsresult ret;

  NS_SetupRegistry(); 

  nsIInputStream* in = nsnull;

#ifndef NECKO
  nsINetService* pNetService = nsnull;
  ret = nsServiceManager::GetService(kNetServiceCID, 
                                     kINetServiceIID,
                                     (nsISupports**) &pNetService);
  if (NS_FAILED(ret) || (!pNetService)) {
    printf("cannot get net service\n");
    return 1;
  }
  nsIURI *url = nsnull;
  ret = pNetService->CreateURL(&url, nsString(TEST_URL), nsnull, nsnull,
    nsnull);
  if (NS_FAILED(ret) || (!url)) {
    printf("cannot create URL\n");
    return 1;
  }

  ret = pNetService->OpenBlockingStream(url, nsnull, &in);
  if (NS_FAILED(ret) || (!in)) {
    printf("cannot open stream\n");
    return 1;
  }
#else
  NS_WITH_SERVICE(nsIIOService, service, kIOServiceCID, &ret);
  if (NS_FAILED(ret)) return ret;

  NS_WITH_SERVICE(nsIEventQueueService, eventQService, kEventQueueServiceCID, &ret);
  if (NS_FAILED(ret)) return ret;

  nsIChannel *channel = nsnull;
  // XXX NECKO verb? loadgroup? getter?
  ret = service->NewChannel("load", TEST_URL, nsnull, nsnull, nsnull, &channel);
  if (NS_FAILED(ret)) return ret;

  nsIEventQueue *eventQ = nsnull;
  ret = eventQService->GetThreadEventQueue(PR_CurrentThread(), &eventQ);
  if (NS_FAILED(ret)) return ret;

  ret = channel->OpenInputStream(0, -1, &in);
  if (NS_FAILED(ret)) return ret;
#endif // NECKO

  nsIPersistentProperties* props = nsnull;
  ret = nsComponentManager::CreateInstance(kPersistentPropertiesCID, NULL,
    kIPersistentPropertiesIID, (void**) &props);
  if (NS_FAILED(ret) || (!props)) {
    printf("create nsIPersistentProperties failed\n");
    return 1;
  }
  ret = props->Load(in);
  if (NS_FAILED(ret)) {
    printf("cannot load properties\n");
    return 1;
  }
  int i = 1;
  while (1) {
    char name[16];
    name[0] = 0;
    sprintf(name, "%d", i);
    nsAutoString v("");
    ret = props->GetProperty(name, v);
    if (NS_FAILED(ret) || (!v.Length())) {
      break;
    }
    char* value = v.ToNewCString();
    if (value) {
      cout << "\"" << i << "\"=\"" << value << "\"" << endl;
      delete[] value;
    }
    else {
      printf("%d: ToNewCString failed\n", i);
    }
    i++;
  }

  nsIBidirectionalEnumerator* propEnum = nsnull;
  ret = props->EnumerateProperties(&propEnum);
  if (NS_FAILED(ret)) {
	printf("cannot enumerate properties\n");
	return 1;
  }
  ret = propEnum->First();
  if (NS_FAILED(ret))
  {
	printf("enumerator is empty\n");
	return 1;
  }

  cout << endl << "Key" << "\t" << "Value" << endl;
  cout <<		  "---" << "\t" << "-----" << endl;
  while (NS_SUCCEEDED(ret))
  {
	  nsIPropertyElement* propElem = nsnull;
	  ret = propEnum->CurrentItem((nsISupports**)&propElem);
	  if (NS_FAILED(ret)) {
		printf("failed to get current item\n");
		return 1;
	  }
	  nsString* key = nsnull;
	  nsString* val = nsnull;
	  ret = propElem->GetKey(&key);
	  if (NS_FAILED(ret)) {
		  printf("failed to get current element's key\n");
		  return 1;
	  }
	  ret = propElem->GetValue(&val);
	  if (NS_FAILED(ret)) {
		  printf("failed to get current element's value\n");
		  return 1;
	  }
	  char* keyCStr = key->ToNewCString();
	  char* valCStr = val->ToNewCString();
	  if (keyCStr && valCStr) 
		cout << keyCStr << "\t" << valCStr << endl;
	  delete[] keyCStr;
	  delete[] valCStr;
      delete key;
      delete val;
	  ret = propEnum->Next();
  }

  return 0;
}
