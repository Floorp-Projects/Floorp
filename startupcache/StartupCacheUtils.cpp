
#include "nsCOMPtr.h"
#include "nsIInputStream.h"
#include "nsIStringStream.h"
#include "nsAutoPtr.h"
#include "StartupCacheUtils.h"
#include "mozilla/scache/StartupCache.h"

namespace mozilla {
namespace scache {

NS_EXPORT nsresult
NS_NewObjectInputStreamFromBuffer(char* buffer, PRUint32 len, 
                                  nsIObjectInputStream** stream)
{
  nsCOMPtr<nsIStringInputStream> stringStream
    = do_CreateInstance("@mozilla.org/io/string-input-stream;1");
  nsCOMPtr<nsIObjectInputStream> objectInput 
    = do_CreateInstance("@mozilla.org/binaryinputstream;1");
  
  stringStream->AdoptData(buffer, len);
  objectInput->SetInputStream(stringStream);
  
  objectInput.forget(stream);
  return NS_OK;
}

NS_EXPORT nsresult
NS_NewObjectOutputWrappedStorageStream(nsIObjectOutputStream **wrapperStream,
                                       nsIStorageStream** stream)
{
  nsCOMPtr<nsIStorageStream> storageStream;

  nsresult rv = NS_NewStorageStream(256, PR_UINT32_MAX, getter_AddRefs(storageStream));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIObjectOutputStream> objectOutput
    = do_CreateInstance("@mozilla.org/binaryoutputstream;1");
  nsCOMPtr<nsIOutputStream> outputStream
    = do_QueryInterface(storageStream);
  
  objectOutput->SetOutputStream(outputStream);
  
#ifdef DEBUG
  // Wrap in debug stream to detect unsupported writes of 
  // multiply-referenced non-singleton objects
  StartupCache* sc = StartupCache::GetSingleton();
  NS_ENSURE_TRUE(sc, NS_ERROR_UNEXPECTED);
  nsCOMPtr<nsIObjectOutputStream> debugStream;
  sc->GetDebugObjectOutputStream(objectOutput, getter_AddRefs(debugStream));
  debugStream.forget(wrapperStream);
#else
  objectOutput.forget(wrapperStream);
#endif
  
  storageStream.forget(stream);
  return NS_OK;
}

NS_EXPORT nsresult
NS_NewBufferFromStorageStream(nsIStorageStream *storageStream, 
                              char** buffer, PRUint32* len) 
{
  nsresult rv;
  nsCOMPtr<nsIInputStream> inputStream;
  rv = storageStream->NewInputStream(0, getter_AddRefs(inputStream));
  NS_ENSURE_SUCCESS(rv, rv);
  
  PRUint32 avail, read;
  rv = inputStream->Available(&avail);
  NS_ENSURE_SUCCESS(rv, rv);
  
  nsAutoArrayPtr<char> temp (new char[avail]);
  rv = inputStream->Read(temp, avail, &read);
  if (NS_SUCCEEDED(rv) && avail != read)
    rv = NS_ERROR_UNEXPECTED;
  
  if (NS_FAILED(rv)) {
    return rv;
  }
  
  *len = avail;
  *buffer = temp.forget();
  return NS_OK;
}

}
}
