#include "Converters.h"
#include "nsIStringStream.h"

//////////////////////////////////////////////////
// TestConverter
//////////////////////////////////////////////////

NS_IMPL_ISUPPORTS2(TestConverter, nsIStreamConverter, nsIStreamListener);

TestConverter::TestConverter() {
    NS_INIT_ISUPPORTS();
}

// Convert aFromStream (of type aFromType), to _retval (nsIInputStream of type aToType).
// This Convert method simply converts the stream byte-by-byte, to the first character
// in the aToType "string".
NS_IMETHODIMP
TestConverter::Convert(nsIInputStream *aFromStream, 
                       const PRUnichar *aFromType, 
                       const PRUnichar *aToType, 
                       nsISupports *ctxt, 
                       nsIInputStream **_retval) {
    char buf[1024];
    PRUint32 read;
    nsresult rv = aFromStream->Read(buf, 1024, &read);
    if (NS_FAILED(rv) || read == 0) return rv;

    // Get the first character 
    nsString to(aToType);
    char *toMIME = to.ToNewCString();
    char toChar = *toMIME;
    nsMemory::Free(toMIME);

    for (PRUint32 i = 0; i < read; i++) 
        buf[i] = toChar;

    nsString convDataStr;
    convDataStr.AssignWithConversion(buf);
    nsIInputStream *inputData = nsnull;
    nsISupports *inputDataSup = nsnull;

    rv = NS_NewStringInputStream(&inputDataSup, convDataStr);
    if (NS_FAILED(rv)) return rv;

    rv = inputDataSup->QueryInterface(NS_GET_IID(nsIInputStream), (void**)&inputData);
    NS_RELEASE(inputDataSup);
    *_retval = inputData;
    if (NS_FAILED(rv)) return rv;

    return NS_OK; 
}

/* This method initializes any internal state before the stream converter
 * begins asyncronous conversion */
NS_IMETHODIMP
TestConverter::AsyncConvertData(const PRUnichar *aFromType,
                                const PRUnichar *aToType, 
                                nsIStreamListener *aListener, 
                                nsISupports *ctxt) {
    NS_ASSERTION(aListener, "null listener");

    mListener = aListener;
    NS_ADDREF(mListener);

    // based on these types, setup internal state to handle the appropriate conversion.
    fromType = aFromType;
    toType = aToType;

    return NS_OK; 
};

// nsIStreamListener method
/* This method handles asyncronous conversion of data. */
NS_IMETHODIMP
TestConverter::OnDataAvailable(nsIChannel *channel,
                               nsISupports *ctxt, 
                               nsIInputStream *inStr, 
                               PRUint32 sourceOffset, 
                               PRUint32 count) {
    nsresult rv;
    nsIInputStream *convertedStream = nsnull;
    // just make a syncronous call to the Convert() method.
    // Anything can happen here, I just happen to be using the sync call to 
    // do the actual conversion.
    rv = Convert(inStr, fromType.GetUnicode(), toType.GetUnicode(), ctxt, &convertedStream);
    if (NS_FAILED(rv)) return rv;

    PRUint32 len;
    convertedStream->Available(&len);
    return mListener->OnDataAvailable(channel, ctxt, convertedStream, sourceOffset, len);
};

// nsIStreamObserver methods
/* These methods just pass through directly to the mListener */
NS_IMETHODIMP
TestConverter::OnStartRequest(nsIChannel *channel, nsISupports *ctxt) {
    return mListener->OnStartRequest(channel, ctxt);
};

NS_IMETHODIMP
TestConverter::OnStopRequest(nsIChannel *channel, nsISupports *ctxt, 
                             nsresult aStatus, const PRUnichar* aStatusArg) {
    return mListener->OnStopRequest(channel, ctxt, aStatus, aStatusArg);
};


////////////////////////////////////////////////////////////////////////
// TestConverterFactory
////////////////////////////////////////////////////////////////////////
TestConverterFactory::TestConverterFactory(const nsCID &aClass, 
                                   const char* className,
                                   const char* contractID)
    : mClassID(aClass), mClassName(className), mContractID(contractID)
{
    NS_INIT_ISUPPORTS();
}

TestConverterFactory::~TestConverterFactory()
{
}

NS_IMPL_ISUPPORTS(TestConverterFactory, NS_GET_IID(nsIFactory));

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
    } else if (mClassID.Equals(kTestConverter1CID)) {
        TestConverter1 *conv = new TestConverter1();
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

