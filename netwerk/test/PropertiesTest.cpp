/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "TestCommon.h"
#include "nsXPCOM.h"
#include "nsString.h"
#include "nsIEventQueueService.h"
#include "nsIPersistentProperties2.h"
#include "nsIServiceManager.h"
#include "nsIComponentRegistrar.h"
#include "nsIURL.h"
#include "nsIIOService.h"
#include "nsNetCID.h"
#include "nsIChannel.h"
#include "nsIComponentManager.h"
#include "nsIEnumerator.h"
#include <stdio.h>
#include "nsReadableUtils.h"


#define TEST_URL "resource:/res/test.properties"
static NS_DEFINE_CID(kPersistentPropertiesCID, NS_IPERSISTENTPROPERTIES_CID);

static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);
static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);

/***************************************************************************/

int
main(int argc, char* argv[])
{
  if (test_common_init(&argc, &argv) != 0)
    return -1;

  nsresult ret;


  nsCOMPtr<nsIServiceManager> servMan;
  NS_InitXPCOM2(getter_AddRefs(servMan), nsnull, nsnull);
  nsCOMPtr<nsIComponentRegistrar> registrar = do_QueryInterface(servMan);
  NS_ASSERTION(registrar, "Null nsIComponentRegistrar");
  registrar->AutoRegister(nsnull);

  nsIInputStream* in = nsnull;

  nsCOMPtr<nsIIOService> service(do_GetService(kIOServiceCID, &ret));
  if (NS_FAILED(ret)) return ret;

  nsCOMPtr<nsIEventQueueService> eventQService = 
           do_GetService(kEventQueueServiceCID, &ret);
  if (NS_FAILED(ret)) return ret;

  nsIChannel *channel = nsnull;
  ret = service->NewChannel(NS_LITERAL_CSTRING(TEST_URL), nsnull, nsnull, &channel);
  if (NS_FAILED(ret)) return ret;

  nsIEventQueue *eventQ = nsnull;
  ret = eventQService->GetThreadEventQueue(NS_CURRENT_THREAD, &eventQ);
  if (NS_FAILED(ret)) return ret;

  ret = channel->Open(&in);
  if (NS_FAILED(ret)) return ret;

  nsIPersistentProperties* props = nsnull;
  ret = nsComponentManager::CreateInstance(kPersistentPropertiesCID, nsnull,
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
    ret = props->GetStringProperty(nsDependentCString(name), v);
    if (NS_FAILED(ret) || (!v.Length())) {
      break;
    }
    char* value = ToNewCString(v);
    if (value) {
      printf("\"%d\"=\"%s\"\n", i, value);
      delete[] value;
    }
    else {
      printf("%d: ToNewCString failed\n", i);
    }
    i++;
  }

  nsCOMPtr<nsISimpleEnumerator> propEnum;
  ret = props->Enumerate(getter_AddRefs(propEnum));

  if (NS_FAILED(ret)) {
    printf("cannot enumerate properties\n");
    return 1;
  }
  

  printf("\nKey\tValue\n");
  printf(  "---\t-----\n");
  
  PRBool hasMore;
  while (NS_SUCCEEDED(propEnum->HasMoreElements(&hasMore)) &&
         hasMore) {
    nsCOMPtr<nsISupports> sup;
    ret = propEnum->GetNext(getter_AddRefs(sup));
    
    nsCOMPtr<nsIPropertyElement> propElem = do_QueryInterface(sup, &ret);
	  if (NS_FAILED(ret)) {
      printf("failed to get current item\n");
      return 1;
	  }

    nsCAutoString key;
    nsAutoString value;

    ret = propElem->GetKey(key);
	  if (NS_FAILED(ret)) {
		  printf("failed to get current element's key\n");
		  return 1;
	  }
    ret = propElem->GetValue(value);
	  if (NS_FAILED(ret)) {
		  printf("failed to get current element's value\n");
		  return 1;
	  }

    printf("%s\t%s\n", key.get(), value.get());
  }
  return 0;
}
