#include "nsIStreamConverter.h"
#include "nsString2.h"
#include "nsIFactory.h"

/* This file defines stream converter components, and their accompanying factory class.
 * These converters implement the nsIStreamConverter interface and support both
 * asyncronous and syncronous stream conversion.
 */

///////////////////////////////////////////////
// TestConverter
#define NS_TESTCONVERTER_CID                         \
{ /* B8A067B0-4450-11d3-A16E-0050041CAF44 */         \
    0xb8a067b0,                                      \
    0x4450,                                          \
    0x11d3,                                          \
    {0xa1, 0x6e, 0x00, 0x50, 0x04, 0x1c, 0xaf, 0x44} \
}
static NS_DEFINE_CID(kTestConverterCID,          NS_TESTCONVERTER_CID);

class TestConverter : public nsIStreamConverter {
public:
    NS_DECL_ISUPPORTS

    TestConverter();
    virtual ~TestConverter() {;};

    // nsIStreamConverter methods
    NS_IMETHOD Convert(nsIInputStream *aFromStream, const PRUnichar *aFromType, 
                       const PRUnichar *aToType, nsISupports *ctxt, nsIInputStream **_retval);


    NS_IMETHOD AsyncConvertData(const PRUnichar *aFromType, const PRUnichar *aToType, 
                                nsIStreamListener *aListener, nsISupports *ctxt);

    // nsIStreamListener method
    NS_IMETHOD OnDataAvailable(nsIChannel *channel, nsISupports *ctxt, nsIInputStream *inStr, 
                               PRUint32 sourceOffset, PRUint32 count);

    // nsIStreamObserver methods
    NS_IMETHOD OnStartRequest(nsIChannel *channel, nsISupports *ctxt);

    NS_IMETHOD OnStopRequest(nsIChannel *channel, nsISupports *ctxt, nsresult status, const PRUnichar *errorMsg);

    // member data
    nsIStreamListener *mListener;
    nsString2 fromType;
    nsString2 toType;
};

//////////////////////////////////////////////////
// TestConverter1
#define NS_TESTCONVERTER1_CID                             \
{ /* 2BA20BB0-5598-11d3-9E63-0010A4053FD0 */         \
    0x2ba20bb0,                                      \
    0x5598,                                          \
    0x11d3,                                          \
    {0x9e, 0x63, 0x00, 0x10, 0xa4, 0x5, 0x3f, 0xd0} \
}
static NS_DEFINE_CID(kTestConverter1CID, NS_TESTCONVERTER1_CID);

// Derrives solely from TestConverter.
class TestConverter1 : public TestConverter {
};

//////////////////////////////////////////////////
// FACTORY
class TestConverterFactory : public nsIFactory
{
public:
    TestConverterFactory(const nsCID &aClass, const char* className, const char* progID);

    // nsISupports methods
    NS_DECL_ISUPPORTS

    // nsIFactory methods
    NS_IMETHOD CreateInstance(nsISupports *aOuter,
                              const nsIID &aIID,
                              void **aResult);

    NS_IMETHOD LockFactory(PRBool aLock);

protected:
    virtual ~TestConverterFactory();

protected:
    nsCID       mClassID;
    const char* mClassName;
    const char* mProgID;
};
