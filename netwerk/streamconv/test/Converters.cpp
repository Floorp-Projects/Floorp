#include "Converters.h"
#include "nsIStringStream.h"
#include "nsCOMPtr.h"
#include "nsReadableUtils.h"

//////////////////////////////////////////////////
// TestConverter
//////////////////////////////////////////////////

NS_IMPL_ISUPPORTS2(TestConverter, nsIStreamConverter, nsIStreamListener)

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
    PRUint32 read;
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

    for (PRUint32 i = 0; i < read; i++) 
        buf[i] = toChar;

    buf[read] = '\0';

    return NS_NewCStringInputStream(_retval, nsDependentCString(buf));
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

// nsIStreamListener method
/* This method handles asyncronous conversion of data. */
NS_IMETHODIMP
TestConverter::OnDataAvailable(nsIRequest* request,
                               nsISupports *ctxt, 
                               nsIInputStream *inStr, 
                               PRUint32 sourceOffset, 
                               PRUint32 count) {
    nsresult rv;
    nsCOMPtr<nsIInputStream> convertedStream;
    // just make a syncronous call to the Convert() method.
    // Anything can happen here, I just happen to be using the sync call to 
    // do the actual conversion.
    rv = Convert(inStr, fromType.get(), toType.get(), ctxt, getter_AddRefs(convertedStream));
    if (NS_FAILED(rv)) return rv;

    PRUint32 len;
    convertedStream->Available(&len);
    return mListener->OnDataAvailable(request, ctxt, convertedStream, sourceOffset, len);
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


////////////////////////////////////////////////////////////////////////
// TestConverterFactory
////////////////////////////////////////////////////////////////////////
TestConverterFactory::TestConverterFactory(const nsCID &aClass, 
                                   const char* className,
                                   const char* contractID)
    : mClassID(aClass), mClassName(className), mContractID(contractID)
{
}

TestConverterFactory::~TestConverterFactory()
{
}

NS_IMPL_ISUPPORTS1(TestConverterFactory, nsIFactory)

NS_IMETHODIMP
TestConverterFactory::CreateInstance(nsISupports *aOuter,
                                 const nsIID &aIID,
                                 void **aResult)
{
    if (! aResult)
        return NS_ERROR_NULL_POINTER;

    if (aOuter)
        return NS_ERROR_NO_AGGREGATION;

    *aResult = nsnull;

    nsresult rv = NS_OK;

    nsISupports *inst = nsnull;
    if (mClassID.Equals(kTestConverterCID)) {
        TestConverter *conv = new TestConverter();
        if (!conv) return NS_ERROR_OUT_OF_MEMORY;
        conv->QueryInterface(NS_GET_IID(nsISupports), (void**)&inst);
    }
    else {
        return NS_ERROR_NO_INTERFACE;
    }

    if (!inst)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(inst);
    *aResult = inst;
    NS_RELEASE(inst);
    return rv;
}

nsresult TestConverterFactory::LockFactory(PRBool aLock)
{
    // Not implemented in simplest case.
    return NS_OK;
}
////////////////////////////////////////////////////////////////////////
// TestConverterFactory END
////////////////////////////////////////////////////////////////////////

