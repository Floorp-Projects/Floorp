/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TestCommon.h"
#include "nsXPCOM.h"
#include "nsStringAPI.h"
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
#include "nsComponentManagerUtils.h"
#include "nsServiceManagerUtils.h"

#define TEST_URL "resource:/res/test.properties"
static NS_DEFINE_CID(kPersistentPropertiesCID, NS_IPERSISTENTPROPERTIES_CID);

static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);

/***************************************************************************/

int
main(int argc, char* argv[])
{
  if (test_common_init(&argc, &argv) != 0)
    return -1;

  nsresult ret;

  nsCOMPtr<nsIServiceManager> servMan;
  NS_InitXPCOM2(getter_AddRefs(servMan), nullptr, nullptr);

  nsIInputStream* in = nullptr;

  nsCOMPtr<nsIIOService> service(do_GetService(kIOServiceCID, &ret));
  if (NS_FAILED(ret)) return ret;

  nsIChannel *channel = nullptr;
  ret = service->NewChannel(NS_LITERAL_CSTRING(TEST_URL), nullptr, nullptr, &channel);
  if (NS_FAILED(ret)) return ret;

  ret = channel->Open(&in);
  if (NS_FAILED(ret)) return ret;

  nsIPersistentProperties* props;
  ret = CallCreateInstance(kPersistentPropertiesCID, &props);
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
    printf("\"%d\"=\"%s\"\n", i, NS_ConvertUTF16toUTF8(v).get());
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
  
  bool hasMore;
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

    printf("%s\t%s\n", key.get(), NS_ConvertUTF16toUTF8(value).get());
  }
  return 0;
}
