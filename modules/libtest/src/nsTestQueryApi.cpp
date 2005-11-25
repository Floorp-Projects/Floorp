#include "nsITestQueryApi.h"
#include "nsTestQueryApi.h"
#include "layprobe.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIFactoryIID, NS_IFACTORY_IID);
static NS_DEFINE_IID(kITestQueryApiIID, NS_ITESTQUERYAPI_IID);
static NS_DEFINE_CID(kITestQueryApiCID, NS_TESTQUERYAPI_CID);

//
// nsTestQueryApi Class Declaration
/////////////////////////////////////

class nsTestQueryApi: public nsITestQueryApi
{
private:
	nsrefcnt mRefCnt;

public:
// Constructor and Destructor
	nsTestQueryApi();
	~nsTestQueryApi();

// nsISupports methods
	NS_IMETHOD QueryInterface(const nsIID &aIID,
							 void **aResult);
	NS_IMETHOD_(nsrefcnt) AddRef(void);
	NS_IMETHOD_(nsrefcnt) Release(void);

// nsITestQueryApi methods


	NS_IMETHOD GetFrames(
		XP_List** lppList
	); 	

	NS_IMETHOD FrameGetStringProperty( 
		MWContext*		FrameID,
		char*			PropertyName,
		char**			lpszPropVal
	);

	NS_IMETHOD FrameGetNumProperty( 
		MWContext*		FrameID,
		char*			PropertyName,
		int32*			lpPropVal			
	);
	
	NS_IMETHOD FrameGetElements(
		XP_List*	lppList,
		MWContext*	FrameID,
		int16		ElementLAPIType,
		char*		ElementName
	);


	NS_IMETHOD FrameGetElementFromPoint(
		LO_Element**	OUT_lppElement,
		MWContext*		FrameID,
		int xPos,
		int yPos
	);


	NS_IMETHOD GetFirstElement (
		LO_Element**	OUT_lppElement,
		MWContext*		FrameID
	);


	NS_IMETHOD GetNextElement (
		LO_Element**	OUT_lppElement,
		MWContext*		FrameID,
		LO_Element*		ElementID
	);

  
	NS_IMETHOD GetPrevElement (
		LO_Element**	OUT_lppElement,
		MWContext*		FrameID,
		LO_Element*		ElementID
	);

  
	NS_IMETHOD GetChildElement (
		LO_Element**	OUT_lppElement,
		MWContext*		FrameID,
		LO_Element*		ElementID
	);

	NS_IMETHOD ElementGetStringProperty( 
		MWContext*		FrameID,
		LO_Element*		ElementID,
		char*			PropertyName,
		char**			lpszPropVal
	);


	NS_IMETHOD ElementGetNumProperty( 
		MWContext*		FrameID,
		LO_Element*		ElementID,
		char*			PropertyName,
		int32*			lpPropVal			
	);


	NS_IMETHOD GetParnetElement ( 
		LO_Element**	OUT_lppElement,
		MWContext*		FrameID,
		LO_Element*		ElementID
	);
};

//
// nsITestQueryApi Factory Declaration
/////////////////////////////////////////

class nsTestQueryApiFactory: public nsIFactory
{
private:
	nsrefcnt mRefCnt;

public:
	nsTestQueryApiFactory();
	~nsTestQueryApiFactory();

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
//      nsTestQueryApi Implementation       //
///////////////////////////////////////////////

nsTestQueryApi::nsTestQueryApi()
{
	mRefCnt = 0;
}

nsTestQueryApi::~nsTestQueryApi()
{
	assert(mRefCnt == 0);
}

NS_METHOD nsTestQueryApi::QueryInterface(const nsIID &aIID,
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
	else if (aIID.Equals(kITestQueryApiIID))
	{
		*aResult = (void *) this;
	}

	if (aResult != NULL)
		return NS_ERROR_NO_INTERFACE;

	AddRef();
	return NS_OK;
}

nsrefcnt nsTestQueryApi::AddRef()
{
	return ++mRefCnt;
}

nsrefcnt nsTestQueryApi::Release()
{
	if (--mRefCnt == 0)
	{
		delete this;
		return 0; // Don't access mRefCnt after deleting!
	}   
	return mRefCnt;
}

NS_METHOD nsTestQueryApi::GetFrames(
	XP_List** lppList
)
{
	if (LAPIGetFrames(lppList))
		return NS_OK;
	else
		return LAPIGetLastError();
}

NS_METHOD nsTestQueryApi::FrameGetStringProperty( 
	MWContext*		FrameID,
	char*			PropertyName,
	char**			lpszPropVal
)
{
	if (LAPIFrameGetStringProperty(FrameID, PropertyName, lpszPropVal))
		return NS_OK;
	else
		return LAPIGetLastError();
}



NS_METHOD nsTestQueryApi::FrameGetNumProperty( 
	MWContext*		FrameID,
	char*			PropertyName,
	int32*			lpPropVal			
)
{
	if (LAPIFrameGetNumProperty(FrameID, PropertyName, lpPropVal))
		return NS_OK;
	else
		return LAPIGetLastError();
}


NS_METHOD nsTestQueryApi::FrameGetElements(
	XP_List*		OUT_lpFList,
	MWContext*		FrameID,
	int16			ElementLAPIType,
	char*			ElementName
)
{
	if (!OUT_lpFList)
		return NS_ERROR_NULL_POINTER;
	
	if (LAPIFrameGetElements(OUT_lpFList, FrameID, ElementLAPIType, ElementName))
		return NS_OK;	
	else
		return LAPIGetLastError();
}



NS_METHOD nsTestQueryApi::FrameGetElementFromPoint(
	LO_Element**	OUT_lppElement,
	MWContext*		FrameID,
	int				xPos,
	int				yPos
)
{
	(*OUT_lppElement) = LAPIFrameGetElementFromPoint(FrameID, xPos, yPos);
	
	if ((*OUT_lppElement))
		return NS_OK;
	else
		return LAPIGetLastError();
}


NS_METHOD nsTestQueryApi::GetFirstElement (
	LO_Element** OUT_lppElement,
	MWContext * FrameID
)
{
	(*OUT_lppElement) = LAPIGetFirstElement(FrameID);

	if (*OUT_lppElement != NULL)
		return NS_OK;
	else
		return LAPIGetLastError();
}
		
NS_METHOD nsTestQueryApi::GetNextElement (
	LO_Element**	OUT_lppElement,
	MWContext*		FrameID,
	LO_Element*		ElementID
)
{
	(*OUT_lppElement) = LAPIGetNextElement(FrameID, ElementID);

	if (*OUT_lppElement != NULL)
		return NS_OK;
	else
		return LAPIGetLastError();
}


NS_METHOD nsTestQueryApi::GetPrevElement (
	LO_Element**	OUT_lppElement,
	MWContext*		FrameID,
	LO_Element*		ElementID
)
{
	(*OUT_lppElement) = LAPIGetPrevElement(FrameID, ElementID);

	if (*OUT_lppElement != NULL)
		return NS_OK;
	else
		return LAPIGetLastError();
}


NS_METHOD nsTestQueryApi::GetChildElement (
	LO_Element**	OUT_lppElement,
	MWContext*		FrameID,
	LO_Element*		ElementID
)
{
	(*OUT_lppElement) = LAPIGetChildElement(FrameID, ElementID);

	if (*OUT_lppElement != NULL)
		return NS_OK;
	else
		return LAPIGetLastError();
}


NS_METHOD nsTestQueryApi::GetParnetElement ( 
	LO_Element**	OUT_lppElement,
	MWContext*		FrameID,
	LO_Element*		ElementID
)
{
	(*OUT_lppElement) = LAPIGetParentElement(FrameID, ElementID);

	if (*OUT_lppElement != NULL)
		return NS_OK;
	else
		return LAPIGetLastError();
}




NS_METHOD nsTestQueryApi::ElementGetStringProperty( 
	MWContext*		FrameID,
	LO_Element*		ElementID,
	char*			PropertyName,
	char**			lpszPropVal
)
{
	if (LAPIElementGetStringProperty(FrameID, ElementID, PropertyName, lpszPropVal))
		return NS_OK;
	else
		return LAPIGetLastError();
}


NS_METHOD nsTestQueryApi::ElementGetNumProperty( 
	MWContext*		FrameID,
	LO_Element*		ElementID,
	char*			PropertyName,
	int32*			lpPropVal			
)
{
	if (LAPIElementGetNumProperty(FrameID, ElementID, PropertyName, lpPropVal))
		return NS_OK;
	else
		return LAPIGetLastError();
}



////////////////////////////////////////////////
//   nsITestQueryApi Factory Implementation   //
////////////////////////////////////////////////

nsTestQueryApiFactory::nsTestQueryApiFactory()
{
	mRefCnt = 0;
}

nsTestQueryApiFactory::~nsTestQueryApiFactory()
{
	assert(mRefCnt == 0);
}

NS_METHOD nsTestQueryApiFactory::QueryInterface(const nsIID &aIID,
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

nsrefcnt nsTestQueryApiFactory::AddRef()
{
	return ++mRefCnt;
}

nsrefcnt nsTestQueryApiFactory::Release()
{
	if (--mRefCnt == 0)
	{
		delete this;
		return 0; // Don't access mRefCnt after deleting!
	}
	
	return mRefCnt;
}

NS_METHOD nsTestQueryApiFactory::CreateInstance(nsISupports *aOuter,
                                         const nsIID &aIID,
                                         void **aResult)
{
	if (aResult == NULL)
		return NS_ERROR_NULL_POINTER;

	*aResult = NULL;

	nsISupports *inst = new nsTestQueryApi();

	if (inst == NULL)
	    return NS_ERROR_OUT_OF_MEMORY;

	nsresult res = inst->QueryInterface(aIID, aResult);

	if (res != NS_OK)
		delete inst; // We didn't get the right interface, so clean up

	return res;
}

NS_METHOD nsTestQueryApiFactory::LockFactory(PRBool aLock)
{
	return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult GetTestQueryApiFactory(nsIFactory **aResult)
{
	if (aResult == NULL)
		return NS_ERROR_NULL_POINTER;

	*aResult = NULL;

	nsISupports *inst = new nsTestQueryApiFactory();

	if (inst == NULL)
		return NS_ERROR_OUT_OF_MEMORY;

	nsresult res = inst->QueryInterface(kIFactoryIID, (void**)aResult);

	if (res != NS_OK)
	    delete inst; // We didn't get the right interface, so clean up
	
	return res;
}
