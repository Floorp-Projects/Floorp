/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef XPCOM_STANDALONE
#define NS_IMPL_IDS

#include "nsIEventQueueService.h"
#include "nsIPersistentProperties2.h"
#include "nsIServiceManager.h"
#include "nsIURL.h"
#include "nsIIOService.h"
#include "nsNetCID.h"
#include "nsIChannel.h"
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

static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);
static NS_DEFINE_CID(kEventQueueCID, NS_EVENTQUEUE_CID);
static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);

/***************************************************************************/
extern "C" void
NS_SetupRegistry()
{
  nsComponentManager::AutoRegister(nsIComponentManager::NS_Startup,
                                   NULL /* default */);

	// startup netlib:	
	nsComponentManager::RegisterComponent(kEventQueueServiceCID, NULL, NULL, XPCOM_DLL, PR_FALSE, PR_FALSE);
    nsComponentManager::RegisterComponent(kIOServiceCID, NULL, NULL, NETLIB_DLL, PR_FALSE, PR_FALSE);

    // Create the Event Queue for this thread...
    nsIEventQueueService* pEventQService;
    
	pEventQService = nsnull;
    nsresult result = nsServiceManager::GetService(kEventQueueServiceCID,
                                                   NS_GET_IID(nsIEventQueueService),
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


#endif
int
main(int argc, char* argv[])
{
#ifndef XPCOM_STANDALONE
  nsresult ret;

  NS_SetupRegistry(); 

  nsIInputStream* in = nsnull;

  nsCOMPtr<nsIIOService> service(do_GetService(kIOServiceCID, &ret));
  if (NS_FAILED(ret)) return ret;

  nsCOMPtr<nsIEventQueueService> eventQService = 
           do_GetService(kEventQueueServiceCID, &ret);
  if (NS_FAILED(ret)) return ret;

  nsIChannel *channel = nsnull;
  ret = service->NewChannel(TEST_URL, nsnull, &channel);
  if (NS_FAILED(ret)) return ret;

  nsIEventQueue *eventQ = nsnull;
  ret = eventQService->GetThreadEventQueue(NS_CURRENT_THREAD, &eventQ);
  if (NS_FAILED(ret)) return ret;

  ret = channel->Open(&in);
  if (NS_FAILED(ret)) return ret;

  nsIPersistentProperties* props = nsnull;
  ret = nsComponentManager::CreateInstance(kPersistentPropertiesCID, NULL,
    NS_GET_IID(nsIPersistentProperties), (void**) &props);
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
    nsAutoString v;
    ret = props->GetStringProperty(NS_ConvertASCIItoUCS2(name), v);
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

    PRUnichar *pKey = nsnull;
    PRUnichar *pVal = nsnull;

	  ret = propElem->GetKey(&pKey);
	  if (NS_FAILED(ret)) {
		  printf("failed to get current element's key\n");
		  return 1;
	  }
	  ret = propElem->GetValue(&pVal);
	  if (NS_FAILED(ret)) {
		  printf("failed to get current element's value\n");
		  return 1;
	  }

    nsAutoString keyAdjustedLengthBuff(pKey);
    nsAutoString valAdjustedLengthBuff(pVal);

	  char* keyCStr = keyAdjustedLengthBuff.ToNewCString();
	  char* valCStr = valAdjustedLengthBuff.ToNewCString();
	  if (keyCStr && valCStr) 
		cout << keyCStr << "\t" << valCStr << endl;
	  delete[] keyCStr;
	  delete[] valCStr;
    delete[] pKey;
    delete[] pVal;
	  ret = propEnum->Next();
  }
#endif
  return 0;
}
