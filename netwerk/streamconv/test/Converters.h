#include "nsIStreamConverter.h"
#include "nsString.h"
#include "nsIFactory.h"
#include "nsCOMPtr.h"

/* This file defines stream converter components, and their accompanying factory class.
 * These converters implement the nsIStreamConverter interface and support both
 * asynchronous and synchronous stream conversion.
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
    NS_DECL_NSIREQUESTOBSERVER
    NS_DECL_NSISTREAMLISTENER

    TestConverter();
    virtual ~TestConverter() {}

    // nsIStreamConverter methods
    NS_IMETHOD Convert(nsIInputStream *aFromStream, const char *aFromType, 
                       const char *aToType, nsISupports *ctxt, nsIInputStream **_retval);


    NS_IMETHOD AsyncConvertData(const char *aFromType, const char *aToType, 
                                nsIStreamListener *aListener, nsISupports *ctxt);

    // member data
    nsCOMPtr<nsIStreamListener> mListener;
    nsCString fromType;
    nsCString toType;
};

//////////////////////////////////////////////////
// FACTORY
class TestConverterFactory : public nsIFactory
{
public:
    TestConverterFactory(const nsCID &aClass, const char* className, const char* contractID);

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
    const char* mContractID;
};
