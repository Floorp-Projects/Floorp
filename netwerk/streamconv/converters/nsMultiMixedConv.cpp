#include "nsMultiMixedConv.h"
#include "nsIAllocator.h"
#include "plstr.h"
#include "nsIStringStream.h"
#include "nsIHTTPChannel.h"
#include "nsIAtom.h"
#include "nsIServiceManager.h"
#include "nsIGenericFactory.h"
#include "nsCOMPtr.h"
static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);



// nsISupports implementation
NS_IMPL_ISUPPORTS2(nsMultiMixedConv, nsIStreamConverter, nsIStreamListener);


// nsIStreamConverter implementation
NS_IMETHODIMP
nsMultiMixedConv::Convert(nsIInputStream *aFromStream,
                          const PRUnichar *aFromType,
                          const PRUnichar *aToType,
                          nsISupports *aCtxt, nsIInputStream **_retval) {
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsMultiMixedConv::AsyncConvertData(const PRUnichar *aFromType, const PRUnichar *aToType,
                                   nsIStreamListener *aListener, nsISupports *aCtxt) {
    NS_ASSERTION(aListener && aFromType && aToType, "null pointer passed into multi mixed converter");

    mFinalListener = aListener;
    NS_ADDREF(mFinalListener);

    return NS_OK;
}


// nsIStreamListener implementation
NS_IMETHODIMP
nsMultiMixedConv::OnDataAvailable(nsIChannel *channel, nsISupports *ctxt,
                                  nsIInputStream *inStr, PRUint32 sourceOffset, PRUint32 count) {
    nsresult rv;
    char *buffer = nsnull, *rootMemPtr = nsnull, *delimiter = nsnull, *bndry = nsnull;
    PRUint32 bufLen, read;
    rv = inStr->GetLength(&bufLen);
    if (NS_FAILED(rv)) return rv;

    nsIHTTPChannel *HTTPChannel= nsnull;
    if (!mBoundaryCStr && channel) {
        rv = channel->QueryInterface(NS_GET_IID(nsIHTTPChannel), (void**)&HTTPChannel);
        if (NS_FAILED(rv)) return rv;
        // XXX shouldn't return failure in this case. what to do?

        nsIAtom *header = NS_NewAtom("content-type");
        if (!header) {
            rv = NS_ERROR_OUT_OF_MEMORY;
        } else {
            rv = HTTPChannel->GetResponseHeader(header, &delimiter);
            NS_RELEASE(header);
            NS_RELEASE(HTTPChannel);
        }
        if (NS_FAILED(rv)) return rv;

        bndry = PL_strstr(delimiter, bndry);
        if (!bndry) {
            // if we dont' have a boundary we're hosed.
            ;
        }

        bndry = PL_strchr(bndry, '=');
        if (!bndry) {
            // ""
            ;
        }

        bndry += 1;

        nsString2 boundaryString(bndry, eOneByte);
        boundaryString.StripWhitespace();
        mBoundaryCStr = boundaryString.ToNewCString();
        if (!mBoundaryCStr) return NS_ERROR_OUT_OF_MEMORY;
        mBoundaryStrLen = boundaryString.Length();

    }
#if 1
    nsString2 test("--aBoundary", eOneByte);
    mBoundaryCStr = test.ToNewCString();
    mBoundaryStrLen = test.Length();
#endif

    buffer = (char*)nsAllocator::Alloc(bufLen);
    if (!buffer) return NS_ERROR_OUT_OF_MEMORY;

    rv = inStr->Read(buffer, bufLen, &read);
    if (NS_FAILED(rv)) return rv;
    buffer[bufLen] = '\0';

    if (!mBuffer.IsEmpty()) {
        // don't forget about any data that was left in the buffer.
        mBuffer.Append(buffer);
        //nsAllocator::Free(buffer);
        rootMemPtr = buffer = mBuffer.ToNewCString();
    }

    // search the buffered data for the delimiting token.
    char *boundaryLoc = PL_strstr(buffer, mBoundaryCStr);
    do {
        if (boundaryLoc) {
            // check for another
            char *boundaryLoc2 = PL_strstr(boundaryLoc + mBoundaryStrLen, mBoundaryCStr);

            if (!mBoundaryStart) {
                mBoundaryStart = PR_TRUE;
                // XXX we need to set the http headers that can sit within each boundary.

                if (boundaryLoc2) {
                    // there was another boundary loc (signifying the end of this part)
                    // in the buffer. Build up this hunk and fire it off.
                    char tmpChar = *boundaryLoc2;
                    *boundaryLoc2 = '\0';

                    rv = SendPart(boundaryLoc + mBoundaryStrLen, channel, ctxt);
                    if (NS_FAILED(rv)) return rv;

                    *boundaryLoc2 = tmpChar;

                    // increment the buffer and start over.
                    buffer = boundaryLoc2;
                    mBoundaryStart = PR_FALSE;
                    boundaryLoc = PL_strstr(buffer, mBoundaryCStr);
                } else {
                    buffer = boundaryLoc + mBoundaryStrLen;
                    mBuffer.Append(buffer);
                    boundaryLoc = PL_strstr(buffer, mBoundaryCStr);
                }
            } else {
                // this boundaryLoc is an ending delimiter. package up the data and send it off
                mBoundaryStart = PR_FALSE;
                char tmpChar = *boundaryLoc;
                *boundaryLoc = '\0';

                rv = SendPart(buffer, channel, ctxt);

                *boundaryLoc = tmpChar;

                if (NS_FAILED(rv)) return rv;

                buffer = boundaryLoc;
                mBuffer = buffer;
                boundaryLoc = PL_strstr(buffer, mBoundaryCStr);
            }
        } else {
            mBuffer.Append(buffer);
        }
    } while (boundaryLoc);

    //nsAllocator::Free((char*)rootMemPtr);

    return NS_OK;
}


// nsIStreamObserver implementation
NS_IMETHODIMP
nsMultiMixedConv::OnStartRequest(nsIChannel *channel, nsISupports *ctxt) {
    return NS_OK;
}

NS_IMETHODIMP
nsMultiMixedConv::OnStopRequest(nsIChannel *channel, nsISupports *ctxt,
                                nsresult status, const PRUnichar *errorMsg) {
    return NS_OK;
}


// nsMultiMixedConv methods
nsMultiMixedConv::nsMultiMixedConv() {
    NS_INIT_ISUPPORTS();
    mFinalListener = nsnull;
    mBoundaryCStr = nsnull;
    mBoundaryStrLen = 0;
}

nsMultiMixedConv::~nsMultiMixedConv() {
    NS_IF_RELEASE(mFinalListener);
    if (mBoundaryCStr) nsAllocator::Free(mBoundaryCStr);
}

nsresult
nsMultiMixedConv::Init() {
    mBoundaryStart = PR_FALSE;
    return NS_OK;
}


nsresult
nsMultiMixedConv::SendPart(const char *aBuffer, nsIChannel *aChannel, nsISupports *aCtxt) {

    nsresult rv;
    // fire off the "part"
    rv = mFinalListener->OnStartRequest(aChannel, aCtxt);
    if (NS_FAILED(rv)) return rv;


    nsISupports *stringStreamSup = nsnull;
    rv = NS_NewStringInputStream(&stringStreamSup, aBuffer);
    if (NS_FAILED(rv)) return rv;

    nsIInputStream *inStr = nsnull;
    rv = stringStreamSup->QueryInterface(NS_GET_IID(nsIInputStream), (void**)&inStr);
    if (NS_FAILED(rv)) return rv;

    PRUint32 len;
    rv = inStr->GetLength(&len);
    if (NS_FAILED(rv)) return rv;

    rv = mFinalListener->OnDataAvailable(aChannel, aCtxt, inStr, 0, len);
    NS_RELEASE(stringStreamSup);
    if (NS_FAILED(rv)) return rv;

    rv = mFinalListener->OnStopRequest(aChannel, aCtxt, NS_OK, nsnull);
    return rv;
}



////////////////////////////////////////////////////////////////////////
// Factory
////////////////////////////////////////////////////////////////////////
MultiMixedFactory::MultiMixedFactory(const nsCID &aClass, 
                                   const char* className,
                                   const char* progID)
    : mClassID(aClass), mClassName(className), mProgID(progID)
{
    NS_INIT_ISUPPORTS();
}

MultiMixedFactory::~MultiMixedFactory()
{
    NS_ASSERTION(mRefCnt == 0, "non-zero refcnt at destruction");
}

NS_IMPL_ISUPPORTS(MultiMixedFactory, NS_GET_IID(nsIFactory));

NS_IMETHODIMP
MultiMixedFactory::CreateInstance(nsISupports *aOuter,
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
    if (mClassID.Equals(kMultiMixedConverterCID)) {
        nsMultiMixedConv *conv = new nsMultiMixedConv();
        if (!conv) return NS_ERROR_OUT_OF_MEMORY;
        rv = conv->Init();
        if (NS_FAILED(rv)) return rv;

        rv = conv->QueryInterface(NS_GET_IID(nsISupports), (void**)&inst);
        if (NS_FAILED(rv)) return rv;
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

nsresult
MultiMixedFactory::LockFactory(PRBool aLock)
{
    // Not implemented in simplest case.
    return NS_OK;
}

// return the proper factory to the caller
extern "C" PR_IMPLEMENT(nsresult)
NSGetFactory(nsISupports* aServMgr,
             const nsCID &aClass,
             const char *aClassName,
             const char *aProgID,
             nsIFactory **aFactory)
{
    nsresult rv;
    if (aFactory == nsnull)
        return NS_ERROR_NULL_POINTER;

    nsIGenericFactory* fact;
    if (aClass.Equals(kMultiMixedConverterCID)) {
        rv = NS_NewGenericFactory(&fact, nsMultiMixedConv::Create);
    }
    else {
        rv = NS_ERROR_FAILURE;
    }

    if (NS_SUCCEEDED(rv))
        *aFactory = fact;
    return rv;
}



extern "C" PR_IMPLEMENT(nsresult)
NSRegisterSelf(nsISupports* aServMgr , const char* aPath)
{
    nsresult rv;

    nsCOMPtr<nsIServiceManager> servMgr(do_QueryInterface(aServMgr, &rv));
    if (NS_FAILED(rv)) return rv;

    NS_WITH_SERVICE(nsIComponentManager, compMgr, kComponentManagerCID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = compMgr->RegisterComponent(kMultiMixedConverterCID,
                                    "MultiMixedConverter",
                                    "component:||netscape|streamConverters|multimixedconverter",
                                    aPath, PR_TRUE, PR_TRUE);

    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}


extern "C" PR_IMPLEMENT(nsresult)
NSUnregisterSelf(nsISupports* aServMgr, const char* aPath)
{
    nsresult rv;

    nsCOMPtr<nsIServiceManager> servMgr(do_QueryInterface(aServMgr, &rv));
    if (NS_FAILED(rv)) return rv;

    NS_WITH_SERVICE(nsIComponentManager, compMgr, kComponentManagerCID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = compMgr->UnregisterComponent(kMultiMixedConverterCID, aPath);
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}


////////////////////////////////////////////////////////////////////////
// Factory END
////////////////////////////////////////////////////////////////////////