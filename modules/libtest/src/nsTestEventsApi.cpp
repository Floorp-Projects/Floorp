#include "nsITestEventsApi.h"
#include "nsTestEventsApi.h"
#include "layprobe.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIFactoryIID, NS_IFACTORY_IID);
static NS_DEFINE_IID(kITestEventsApiIID, NS_ITESTEVENTSAPI_IID);
static NS_DEFINE_CID(kITestEventsApiCID, NS_TESTEVENTSAPI_CID);

//
// nsTestEventsApi Class Declaration
///////////////////////////////////////////////

class nsTestEventsApi: public nsITestEventsApi
{
private:
	nsrefcnt mRefCnt;

public:
// Constructor and Destructor
	nsTestEventsApi();
	~nsTestEventsApi();

// nsISupports methods
	NS_IMETHOD QueryInterface(const nsIID &aIID,
							 void **aResult);
	NS_IMETHOD_(nsrefcnt) AddRef(void);
	NS_IMETHOD_(nsrefcnt) Release(void);

// nsITestEventsApi methods

	NS_IMETHOD RegisterNotifyCallback(
		int32*			OUT_lpCallbackID,
		ID_NOTIFY_PT*	lpFunc,
		int32			EventID
	);

	NS_IMETHOD RegisterElementCallback(
		int32*				OUT_lpCallbackID,
		ELEMENT_NOTIFY_PT*	lpFunc,
		int32				EventID
	);

	NS_IMETHOD UnregisterCallbackFunction(
		void* CalbackID
	);
};

//
// nsTestEventsApi Factory Declaration
/////////////////////////////////////////////

class nsTestEventsApiFactory: public nsIFactory
{
private:
	nsrefcnt mRefCnt;

public:
	nsTestEventsApiFactory();
	~nsTestEventsApiFactory();

// nsISupports methods
	NS_IMETHOD QueryInterface(const nsIID &aIID,
							 void **aResult);
	NS_IMETHOD_(nsrefcnt) AddRef(void);
	NS_IMETHOD_(nsrefcnt) Release(void);

// nsIFactory methods
	NS_IMETHOD CreateInstance(nsISupports *aOuter,
							 const nsIID &aIID,
							 void **aResult);

	NS_IMETHOD LockFactory(PRBool aLock);
};

///////////////////////////////////////////////
//      nsTestEventsApi Implementation       //
///////////////////////////////////////////////

nsTestEventsApi::nsTestEventsApi()
{
	mRefCnt = 0;
}

nsTestEventsApi::~nsTestEventsApi()
{
	assert(mRefCnt == 0);
}

NS_METHOD nsTestEventsApi::QueryInterface(const nsIID &aIID,
                                  void **aResult)
{
	if (aResult == NULL)
		return NS_ERROR_NULL_POINTER;

	// Always NULL result, in case of failure
	*aResult = NULL;

	if (aIID.Equals(kISupportsIID))
	{
		*aResult = (void *) this;
	}
	else if (aIID.Equals(kITestEventsApiIID))
	{
		*aResult = (void *) this;
	}

	if (aResult != NULL)
		return NS_ERROR_NO_INTERFACE;

	AddRef();
	return NS_OK;
}

nsrefcnt nsTestEventsApi::AddRef()
{
	return ++mRefCnt;
}

nsrefcnt nsTestEventsApi::Release()
{
	if (--mRefCnt == 0)
	{
		delete this;
		return 0; // Don't access mRefCnt after deleting!
	}   
	return mRefCnt;
}


// API Callback regisration
/////////////////////////////////////////

NS_METHOD nsTestEventsApi::RegisterNotifyCallback(
	int32*			OUT_lppCallbackID,
	ID_NOTIFY_PT*	lpFunc,
	int32			EventID
)
{
	*OUT_lppCallbackID = LAPIRegisterNotifyCallback(lpFunc, EventID);

	if (*OUT_lppCallbackID)
		return NS_OK;
	else
		return LAPIGetLastError();
}


NS_METHOD nsTestEventsApi::RegisterElementCallback(
	int32*				OUT_lppCallbackID,
	ELEMENT_NOTIFY_PT*	lpFunc,
	int32				EventID
)
{
	*OUT_lppCallbackID = LAPIRegisterElementCallback(lpFunc, EventID);

	if (*OUT_lppCallbackID)
		return NS_OK;
	else
		return LAPIGetLastError();
}


NS_METHOD nsTestEventsApi::UnregisterCallbackFunction(
	void* CallbackID
)
{
	if (UnregisterCallbackFunction(CallbackID))
		return NS_OK;
	else
		return LAPI_E_GENERAL_ERROR;
}


///////////////////////////////////////////////////
//    nsTestEventsApiFactory Implementation      //
///////////////////////////////////////////////////

nsTestEventsApiFactory::nsTestEventsApiFactory()
{
	mRefCnt = 0;
}

nsTestEventsApiFactory::~nsTestEventsApiFactory()
{
	assert(mRefCnt == 0);
}

NS_METHOD nsTestEventsApiFactory::QueryInterface(const nsIID &aIID,
                                         void **aResult)
{
	if (aResult == NULL)
		return NS_ERROR_NULL_POINTER;

	// Always NULL result, in case of failure
	*aResult = NULL;

	if (aIID.Equals(kISupportsIID))
	{
		*aResult = (void *) this;
	}
	else if (aIID.Equals(kIFactoryIID))
	{
		*aResult = (void *) this;
	}

	if (*aResult == NULL)
		return NS_ERROR_NO_INTERFACE;

	AddRef(); // Increase reference count for caller
	return NS_OK;
}

nsrefcnt nsTestEventsApiFactory::AddRef()
{
	return ++mRefCnt;
}

nsrefcnt nsTestEventsApiFactory::Release()
{
	if (--mRefCnt == 0)
	{
		delete this;
		return 0; // Don't access mRefCnt after deleting!
	}
	
	return mRefCnt;
}

NS_METHOD nsTestEventsApiFactory::CreateInstance(nsISupports *aOuter,
                                         const nsIID &aIID,
                                         void **aResult)
{
	if (aResult == NULL)
		return NS_ERROR_NULL_POINTER;

	*aResult = NULL;

	nsISupports *inst = new nsTestEventsApi();

	if (inst == NULL)
	    return NS_ERROR_OUT_OF_MEMORY;

	nsresult res = inst->QueryInterface(aIID, aResult);

	if (res != NS_OK)
		delete inst; // We didn't get the right interface, so clean up

	return res;
}

NS_METHOD nsTestEventsApiFactory::LockFactory(PRBool aLock)
{
	return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult GetTestEventsApiFactory(nsIFactory **aResult)
{
	if (aResult == NULL)
		return NS_ERROR_NULL_POINTER;

	*aResult = NULL;

	nsISupports *inst = new nsTestEventsApiFactory();

	if (inst == NULL)
		return NS_ERROR_OUT_OF_MEMORY;

	nsresult res = inst->QueryInterface(kIFactoryIID, (void**)aResult);

	if (res != NS_OK)
	    delete inst; // We didn't get the right interface, so clean up
	
	return res;
}
