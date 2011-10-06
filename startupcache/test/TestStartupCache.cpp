/* -*-  Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
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
 * The Original Code is Startup Cache.
 *
 * The Initial Developer of the Original Code is
 * The Mozilla Foundation <http://www.mozilla.org/>.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Benedict Hsieh <bhsieh@mozilla.com>
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

#include "TestHarness.h"

#include "nsThreadUtils.h"
#include "nsIClassInfo.h"
#include "nsIOutputStream.h"
#include "nsIObserver.h"
#include "nsISerializable.h"
#include "nsISupports.h"
#include "nsIStartupCache.h"
#include "nsIStringStream.h"
#include "nsIStorageStream.h"
#include "nsIObjectInputStream.h"
#include "nsIObjectOutputStream.h"
#include "nsIURI.h"
#include "nsStringAPI.h"

namespace mozilla {
namespace scache {

NS_IMPORT nsresult
NewObjectInputStreamFromBuffer(char* buffer, PRUint32 len, 
                               nsIObjectInputStream** stream);

// We can't retrieve the wrapped stream from the objectOutputStream later,
// so we return it here.
NS_IMPORT nsresult
NewObjectOutputWrappedStorageStream(nsIObjectOutputStream **wrapperStream,
                                    nsIStorageStream** stream);

NS_IMPORT nsresult
NewBufferFromStorageStream(nsIStorageStream *storageStream, 
                           char** buffer, PRUint32* len);
}
}

using namespace mozilla::scache;

#define NS_ENSURE_STR_MATCH(str1, str2, testname)  \
PR_BEGIN_MACRO                                     \
if (0 != strcmp(str1, str2)) {                     \
  fail("failed " testname);                        \
  return NS_ERROR_FAILURE;                         \
}                                                  \
passed("passed " testname);                        \
PR_END_MACRO

nsresult
WaitForStartupTimer() {
  nsresult rv;
  nsCOMPtr<nsIStartupCache> sc 
    = do_GetService("@mozilla.org/startupcache/cache;1");
  PR_Sleep(10 * PR_TicksPerSecond());
  
  bool complete;
  while (true) {
    NS_ProcessPendingEvents(nsnull);
    rv = sc->StartupWriteComplete(&complete);
    if (NS_FAILED(rv) || complete)
      break;
    PR_Sleep(1 * PR_TicksPerSecond());
  }
  return rv;
}

nsresult
TestStartupWriteRead() {
  nsresult rv;
  nsCOMPtr<nsIStartupCache> sc 
    = do_GetService("@mozilla.org/startupcache/cache;1", &rv);
  if (!sc) {
    fail("didn't get a pointer...");
    return NS_ERROR_FAILURE;
  } else {
    passed("got a pointer?");
  }
  sc->InvalidateCache();
  
  char* buf = "Market opportunities for BeardBook";
  char* id = "id";
  char* outbufPtr = NULL;
  nsAutoArrayPtr<char> outbuf;  
  PRUint32 len;
  
  rv = sc->PutBuffer(id, buf, strlen(buf) + 1);
  NS_ENSURE_SUCCESS(rv, rv);
  
  rv = sc->GetBuffer(id, &outbufPtr, &len);
  NS_ENSURE_SUCCESS(rv, rv);
  outbuf = outbufPtr;
  NS_ENSURE_STR_MATCH(buf, outbuf, "pre-write read");

  rv = sc->ResetStartupWriteTimer();
  rv = WaitForStartupTimer();
  NS_ENSURE_SUCCESS(rv, rv);
  
  rv = sc->GetBuffer(id, &outbufPtr, &len);
  NS_ENSURE_SUCCESS(rv, rv);
  outbuf = outbufPtr;
  NS_ENSURE_STR_MATCH(buf, outbuf, "simple write/read");

  return NS_OK;
}

nsresult
TestWriteInvalidateRead() {
  nsresult rv;
  char* buf = "BeardBook competitive analysis";
  char* id = "id";
  char* outbuf = NULL;
  PRUint32 len;
  nsCOMPtr<nsIStartupCache> sc 
    = do_GetService("@mozilla.org/startupcache/cache;1", &rv);
  sc->InvalidateCache();

  rv = sc->PutBuffer(id, buf, strlen(buf) + 1);
  NS_ENSURE_SUCCESS(rv, rv);

  sc->InvalidateCache();

  rv = sc->GetBuffer(id, &outbuf, &len);
  delete[] outbuf;
  if (rv == NS_ERROR_NOT_AVAILABLE) {
    passed("buffer not available after invalidate");
  } else if (NS_SUCCEEDED(rv)) {
    fail("GetBuffer succeeded unexpectedly after invalidate");
    return NS_ERROR_UNEXPECTED;
  } else {
    fail("GetBuffer gave an unexpected failure, expected NOT_AVAILABLE");
    return rv;
  }

  sc->InvalidateCache();
  return NS_OK;
}

nsresult
TestWriteObject() {
  nsresult rv;

  nsCOMPtr<nsIURI> obj
    = do_CreateInstance("@mozilla.org/network/simple-uri;1");
  if (!obj) {
    fail("did not create object in test write object");
    return NS_ERROR_UNEXPECTED;
  }
  NS_NAMED_LITERAL_CSTRING(spec, "http://www.mozilla.org");
  obj->SetSpec(spec);
  nsCOMPtr<nsIStartupCache> sc = do_GetService("@mozilla.org/startupcache/cache;1", &rv);

  sc->InvalidateCache();
  
  // Create an object stream. Usually this is done with
  // NewObjectOutputWrappedStorageStream, but that uses
  // StartupCache::GetSingleton in debug builds, and we
  // don't have access to that here. Obviously.
  char* id = "id";
  nsCOMPtr<nsIStorageStream> storageStream
    = do_CreateInstance("@mozilla.org/storagestream;1");
  NS_ENSURE_ARG_POINTER(storageStream);
  
  rv = storageStream->Init(256, (PRUint32) -1, nsnull);
  NS_ENSURE_SUCCESS(rv, rv);
  
  nsCOMPtr<nsIObjectOutputStream> objectOutput
    = do_CreateInstance("@mozilla.org/binaryoutputstream;1");
  if (!objectOutput)
    return NS_ERROR_OUT_OF_MEMORY;
  
  nsCOMPtr<nsIOutputStream> outputStream
    = do_QueryInterface(storageStream);
  
  rv = objectOutput->SetOutputStream(outputStream);

  if (NS_FAILED(rv)) {
    fail("failed to create output stream");
    return rv;
  }
  nsCOMPtr<nsISupports> objQI(do_QueryInterface(obj));
  rv = objectOutput->WriteObject(objQI, PR_TRUE);
  if (NS_FAILED(rv)) {
    fail("failed to write object");
    return rv;
  }

  char* bufPtr = NULL;
  nsAutoArrayPtr<char> buf;
  PRUint32 len;
  NewBufferFromStorageStream(storageStream, &bufPtr, &len);
  buf = bufPtr;

  // Since this is a post-startup write, it should be written and
  // available.
  rv = sc->PutBuffer(id, buf, len);
  if (NS_FAILED(rv)) {
    fail("failed to insert input stream");
    return rv;
  }
    
  char* buf2Ptr = NULL;
  nsAutoArrayPtr<char> buf2;
  PRUint32 len2;
  nsCOMPtr<nsIObjectInputStream> objectInput;
  rv = sc->GetBuffer(id, &buf2Ptr, &len2);
  if (NS_FAILED(rv)) {
    fail("failed to retrieve buffer");
    return rv;
  }
  buf2 = buf2Ptr;

  rv = NewObjectInputStreamFromBuffer(buf2, len2, getter_AddRefs(objectInput));
  if (NS_FAILED(rv)) {
    fail("failed to created input stream");
    return rv;
  }  
  buf2.forget();

  nsCOMPtr<nsISupports> deserialized;
  rv = objectInput->ReadObject(PR_TRUE, getter_AddRefs(deserialized));
  if (NS_FAILED(rv)) {
    fail("failed to read object");
    return rv;
  }
  
  bool match = false;
  nsCOMPtr<nsIURI> uri(do_QueryInterface(deserialized));
  if (uri) {
    nsCString outSpec;
    uri->GetSpec(outSpec);
    match = outSpec.Equals(spec);
  }
  if (!match) {
    fail("deserialized object has incorrect information");
    return rv;
  }
  
  passed("write object");
  return NS_OK;
}

nsresult
TestEarlyShutdown() {
  nsresult rv;
  nsCOMPtr<nsIStartupCache> sc 
    = do_GetService("@mozilla.org/startupcache/cache;1", &rv);
  sc->InvalidateCache();

  char* buf = "Find your soul beardmate on BeardBook";
  char* id = "id";
  PRUint32 len;
  char* outbuf = NULL;
  
  sc->ResetStartupWriteTimer();
  rv = sc->PutBuffer(buf, id, strlen(buf) + 1);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIObserver> obs;
  sc->GetObserver(getter_AddRefs(obs));
  obs->Observe(nsnull, "xpcom-shutdown", nsnull);
  rv = WaitForStartupTimer();
  NS_ENSURE_SUCCESS(rv, rv);
  
  rv = sc->GetBuffer(id, &outbuf, &len);
  delete[] outbuf;

  if (rv == NS_ERROR_NOT_AVAILABLE) {
    passed("buffer not available after early shutdown");
  } else if (NS_SUCCEEDED(rv)) {
    fail("GetBuffer succeeded unexpectedly after early shutdown");
    return NS_ERROR_UNEXPECTED;
  } else {
    fail("GetBuffer gave an unexpected failure, expected NOT_AVAILABLE");
    return rv;
  }
 
  return NS_OK;
}


int main(int argc, char** argv)
{
  int rv = 0;
  nsresult rv2;
  ScopedXPCOM xpcom("Startup Cache");
  
  if (NS_FAILED(TestStartupWriteRead()))
    rv = 1;
  if (NS_FAILED(TestWriteInvalidateRead()))
    rv = 1;
  if (NS_FAILED(TestWriteObject()))
    rv = 1;
  if (NS_FAILED(TestEarlyShutdown()))
    rv = 1;
  
  return rv;
}
