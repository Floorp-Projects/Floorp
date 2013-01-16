#include "Converters.h"
#include "nsIStringStream.h"
#include "nsCOMPtr.h"
#include "nsComponentManagerUtils.h"

#include <stdio.h>
#include <algorithm>

//////////////////////////////////////////////////
// TestConverter
//////////////////////////////////////////////////

NS_IMPL_ISUPPORTS3(TestConverter,
                   nsIStreamConverter,
                   nsIStreamListener,
                   nsIRequestObserver)

TestConverter::TestConverter() {
}

// Convert aFromStream (of type aFromType), to _retval (nsIInputStream of type aToType).
// This Convert method simply converts the stream byte-by-byte, to the first character
// in the aToType "string".
NS_IMETHODIMP
TestConverter::Convert(nsIInputStream *aFromStream, 
                       const char *aFromType, 
                       const char *aToType, 
                       nsISupports *ctxt, 
                       nsIInputStream **_retval) {
    char buf[1024+1];
    uint32_t read;
    nsresult rv = aFromStream->Read(buf, 1024, &read);
    if (NS_FAILED(rv) || read == 0) return rv;

    // verify that the data we're converting matches the from type
    // if it doesn't then we're being handed the wrong data.
    char fromChar = *aFromType;

    if (fromChar != buf[0]) {
        printf("We're receiving %c, but are supposed to have %c.\n", buf[0], fromChar);
        return NS_ERROR_FAILURE;
    }


    // Get the first character 
    char toChar = *aToType;

    for (uint32_t i = 0; i < read; i++) 
        buf[i] = toChar;

    buf[read] = '\0';

    nsCOMPtr<nsIStringInputStream> str
      (do_CreateInstance("@mozilla.org/io/string-input-stream;1", &rv));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = str->SetData(buf, read);
    NS_ENSURE_SUCCESS(rv, rv);

    NS_ADDREF(*_retval = str);
    return NS_OK;
}

/* This method initializes any internal state before the stream converter
 * begins asynchronous conversion */
NS_IMETHODIMP
TestConverter::AsyncConvertData(const char *aFromType,
                                const char *aToType, 
                                nsIStreamListener *aListener, 
                                nsISupports *ctxt) {
    NS_ASSERTION(aListener, "null listener");

    mListener = aListener;

    // based on these types, setup internal state to handle the appropriate conversion.
    fromType = aFromType;
    toType = aToType;

    return NS_OK; 
}

static inline uint32_t
saturated(uint64_t aValue)
{
    return (uint32_t) std::min(aValue, (uint64_t) UINT32_MAX);
}

// nsIStreamListener method
/* This method handles asyncronous conversion of data. */
NS_IMETHODIMP
TestConverter::OnDataAvailable(nsIRequest* request,
                               nsISupports *ctxt, 
                               nsIInputStream *inStr, 
                               uint64_t sourceOffset, 
                               uint32_t count) {
    nsresult rv;
    nsCOMPtr<nsIInputStream> convertedStream;
    // just make a syncronous call to the Convert() method.
    // Anything can happen here, I just happen to be using the sync call to 
    // do the actual conversion.
    rv = Convert(inStr, fromType.get(), toType.get(), ctxt, getter_AddRefs(convertedStream));
    if (NS_FAILED(rv)) return rv;

    uint64_t len = 0;
    convertedStream->Available(&len);

    uint64_t offset = sourceOffset;
    while (len > 0) {
        uint32_t count = saturated(len);
        rv = mListener->OnDataAvailable(request, ctxt, convertedStream, offset, count);
        if (NS_FAILED(rv)) return rv;

        offset += count;
        len -= count;
    }
    return NS_OK;
}

// nsIRequestObserver methods
/* These methods just pass through directly to the mListener */
NS_IMETHODIMP
TestConverter::OnStartRequest(nsIRequest* request, nsISupports *ctxt) {
    return mListener->OnStartRequest(request, ctxt);
}

NS_IMETHODIMP
TestConverter::OnStopRequest(nsIRequest* request, nsISupports *ctxt, 
                             nsresult aStatus) {
    return mListener->OnStopRequest(request, ctxt, aStatus);
}

nsresult
CreateTestConverter(nsISupports* aOuter, REFNSIID aIID, void** aResult)
{
  nsCOMPtr<nsISupports> conv = new TestConverter();
  return conv->QueryInterface(aIID, aResult);
}
