/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#include "nsIDOMWindow.h"
#include "nsString.h"
#include "nsVoidArray.h"
#include "rdf.h"
#include "nsIRDFService.h"
#include "nsIRDFDataSource.h"
#include "nsIRDFNode.h"
#include "nsIRDFObserver.h"
#include "nsRDFCID.h"
#include "rdfutil.h"
#include "xp_core.h"
#include "plhash.h"
#include "plstr.h"
#include "prmem.h"
#include "prprf.h"
#include "nsXPIDLString.h"
#include "nsIServiceManager.h"
#include "nsIWindowMediator.h"
#include "nsCOMptr.h"
#include "nsIWebShell.h"

static NS_DEFINE_IID(kIRDFDataSourceIID,        NS_IRDFDATASOURCE_IID);
static NS_DEFINE_IID(kIRDFServiceIID,           NS_IRDFSERVICE_IID);
static NS_DEFINE_CID(kRDFInMemoryDataSourceCID, NS_RDFINMEMORYDATASOURCE_CID);
static NS_DEFINE_CID(kRDFServiceCID,            NS_RDFSERVICE_CID);

static NS_DEFINE_CID(kWindowMediatorCID, NS_WindowMediator_CID);
static NS_DEFINE_IID(kIWindowMediatorIID,NS_IWindowMediator_IID);

static const char kURINC_WindowMediatorRoot[] = "NC:WindowMediatorRoot";


DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, Name);
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, URL);
static	nsIRDFService* gRDFService = nsnull;



class nsWindowMediator;

struct nsWindowInfo
{
	nsWindowInfo( nsIDOMWindow* inWindow, const nsString* inType, PRInt32 inTimeStamp ):
				mWindowType( *inType ), mTimeStamp( inTimeStamp )
	{
		mWindow = inWindow;
		NS_ADDREF( mWindow );
	}
	
	~nsWindowInfo()
	{
		NS_RELEASE( mWindow );
	}
	
	nsCOMPtr<nsIRDFResource>	   mRDFID;
	nsIDOMWindow* mWindow;
	nsString		  mWindowType;
	PRInt32			  mTimeStamp;
	
};



class nsWindowMediator : public nsIWindowMediator
{
public:
	nsWindowMediator();
	virtual ~nsWindowMediator();

	NS_IMETHOD GetNextWindow( nsIDOMWindow* inStartWindow, nsString* inWindowType,
							  nsIDOMWindow** outWindow );
	
	NS_IMETHOD GetMostRecentWindow( nsString* inType, nsIDOMWindow** outWindow );
	
	NS_IMETHOD RegisterWindow( nsIDOMWindow* inWindow, const nsString* inType);
	
	NS_IMETHOD UnregisterWindow( nsIDOMWindow* inWindow );
	
	NS_IMETHOD UpdateWindowTimeStamp( nsIDOMWindow* inWindow );
	NS_IMETHOD UpdateWindowTitle( nsIDOMWindow* inWindow , const nsString& inTitle );
	NS_IMETHOD GetWindowForResource( nsIRDFResource* inResource, nsIDOMWindow** outWindow );
	// COM and RDF 
	NS_DECL_ISUPPORTS	

	// RDF
	  // nsIRDFDataSource
    NS_IMETHOD Init(const char* uri);

    NS_IMETHOD GetURI(char* *uri)
    {
        return mInner->GetURI(uri);
    }

    NS_IMETHOD GetSource(nsIRDFResource* property,
                         nsIRDFNode* target,
                         PRBool tv,
                         nsIRDFResource** source)
    {
        return mInner->GetSource(property, target, tv, source);
    }

    NS_IMETHOD GetSources(nsIRDFResource* property,
                          nsIRDFNode* target,
                          PRBool tv,
                          nsISimpleEnumerator** sources)
    {
        return mInner->GetSources(property, target, tv, sources);
    }

    NS_IMETHOD GetTarget(nsIRDFResource* source,
                         nsIRDFResource* property,
                         PRBool tv,
                         nsIRDFNode** target)
     {
       if ( property == kNC_Name )
       {
       	 NS_WARNING( "Asking for window Name" );
       }
       return mInner->GetTarget(source, property, tv, target);
     }

    NS_IMETHOD GetTargets(nsIRDFResource* source,
                          nsIRDFResource* property,
                          PRBool tv,
                          nsISimpleEnumerator** targets)
    {
        return mInner->GetTargets(source, property, tv, targets);
    }

    NS_IMETHOD Assert(nsIRDFResource* aSource, 
                      nsIRDFResource* aProperty, 
                      nsIRDFNode* aTarget,
                      PRBool aTruthValue);

    NS_IMETHOD Unassert(nsIRDFResource* aSource,
                        nsIRDFResource* aProperty,
                        nsIRDFNode* aTarget);

    NS_IMETHOD HasAssertion(nsIRDFResource* source,
                            nsIRDFResource* property,
                            nsIRDFNode* target,
                            PRBool tv,
                            PRBool* hasAssertion)
    {
        return mInner->HasAssertion(source, property, target, tv, hasAssertion);
    }

    NS_IMETHOD AddObserver(nsIRDFObserver* n)
    {
        return mInner->AddObserver(n);
    }

    NS_IMETHOD RemoveObserver(nsIRDFObserver* n)
    {
        return mInner->RemoveObserver(n);
    }

    NS_IMETHOD ArcLabelsIn( nsIRDFNode* node, nsISimpleEnumerator** labels)
    {
        return mInner->ArcLabelsIn(node, labels);
    }

    NS_IMETHOD ArcLabelsOut(nsIRDFResource* source, nsISimpleEnumerator** labels)
    {
        return mInner->ArcLabelsOut(source, labels);
    }

    NS_IMETHOD GetAllResources(nsISimpleEnumerator** aCursor)
    {
        return mInner->GetAllResources(aCursor);
    }

    NS_IMETHOD Flush(void);

    NS_IMETHOD GetAllCommands(nsIRDFResource* source,
                              nsIEnumerator** commands);

    NS_IMETHOD IsCommandEnabled(nsISupportsArray* aSources,
                                nsIRDFResource*   aCommand,
                                nsISupportsArray* aArguments,
                                PRBool* aResult);

    NS_IMETHOD DoCommand(nsISupportsArray* aSources,
                         nsIRDFResource*   aCommand,
                         nsISupportsArray* aArguments);
private:
	// Helper functions
	nsresult AddWindowToRDF( nsWindowInfo* ioWindowInfo );
	
	nsVoidArray	mWindowList;
	PRInt32		mTimeStamp;

    // pseudo-constants for RDF
    static nsIRDFResource* kNC_WindowMediatorRoot;
    static nsIRDFResource* kNC_Name;
     static nsIRDFResource* kNC_URL;
    static PRInt32 gRefCnt;
    static nsIRDFDataSource* mInner;
    
    
    nsresult AddWindowToRDF( nsString& intitle );
};

nsIRDFResource	*nsWindowMediator::kNC_WindowMediatorRoot = NULL;
nsIRDFResource	*nsWindowMediator::kNC_Name = NULL;
nsIRDFResource	*nsWindowMediator::kNC_URL = NULL;

PRInt32		nsWindowMediator::gRefCnt;
nsIRDFDataSource *nsWindowMediator::mInner = NULL;



nsWindowMediator::nsWindowMediator() :
	mTimeStamp( 0 )
{
	 NS_INIT_REFCNT();
	 if (gRefCnt++ == 0)
	 {
        nsresult rv = nsServiceManager::GetService(kRDFServiceCID,
						kIRDFServiceIID,
						(nsISupports**) &gRDFService);

		gRDFService->GetResource(kURINC_WindowMediatorRoot,   &kNC_WindowMediatorRoot);
		gRDFService->GetResource(kURINC_Name, &kNC_Name);
		gRDFService->GetResource(kURINC_URL, &kNC_URL);
    }
}

nsWindowMediator::~nsWindowMediator()
{
	

    if (--gRefCnt == 0)
    {
    	// Delete data
   		PRInt32 count = mWindowList.Count();
		for ( int32 i = 0; i< count; i++ )
		{
			nsWindowInfo* windowInfo = ( nsWindowInfo*) mWindowList[i];
			UnregisterWindow ( (windowInfo )->mWindow );
		}
		
	// unregister this from the RDF service
		gRDFService->UnregisterDataSource(this);
		nsServiceManager::ReleaseService(kRDFServiceCID, gRDFService);
		gRDFService = nsnull;
		
        NS_RELEASE(kNC_WindowMediatorRoot);
        NS_RELEASE( kNC_Name );
        NS_RELEASE(kNC_URL);
        NS_RELEASE(mInner);
    }
}

       
        
NS_IMETHODIMP nsWindowMediator::RegisterWindow( nsIDOMWindow* inWindow, const nsString* inType )
{
	NS_ASSERTION( inWindow !=NULL && inType != NULL, "illegal arguments" );
	if ( inWindow == NULL || inType == NULL )
		return NS_ERROR_INVALID_ARG;
	
	mTimeStamp++;
	
	// Create window info struct and add to list of windows
	nsWindowInfo* windowInfo = new nsWindowInfo ( inWindow, inType, mTimeStamp );
	if ( windowInfo == NULL )
		return NS_ERROR_OUT_OF_MEMORY;
		
	 if ( !mWindowList.AppendElement( windowInfo ) )
	 {
	 	delete windowInfo;
	 	return NS_ERROR_OUT_OF_MEMORY;
	 }
	 
	 nsresult returnValue;
	 if ( NS_FAILED (returnValue = AddWindowToRDF( windowInfo )  ))
	 {
	 	mWindowList.RemoveElementAt( mWindowList.Count() );
	 	delete windowInfo;
	 	return returnValue;
	 }
	 return NS_OK;
}

NS_IMETHODIMP nsWindowMediator::UnregisterWindow( nsIDOMWindow* inWindow )
{
	// Find Window info
	PRInt32 count = mWindowList.Count();
	nsWindowInfo* windowInfo = NULL;
	
	for ( PRInt32 i = 0; i< count; i++ )
	{
		
		windowInfo = (nsWindowInfo*) mWindowList[i];
		if ( ( windowInfo )->mWindow == inWindow )
			break;
		windowInfo = NULL;
	}
	
	NS_ASSERTION( windowInfo!=NULL, " Not a registered window ");
	if ( windowInfo == NULL )	
		return NS_ERROR_INVALID_ARG;
	
	// Remove from the list and free up 
	mWindowList.RemoveElement( windowInfo );
	
	delete windowInfo;	
	return NS_OK;
}


	
	
NS_METHOD nsWindowMediator::GetNextWindow( nsIDOMWindow* inStartWindow, nsString* inWindowType,
							  nsIDOMWindow** outWindow )
{
	NS_ASSERTION( outWindow != NULL, "Illegal argument" );
	if ( outWindow == NULL )
		return NS_ERROR_INVALID_ARG;
	*outWindow = NULL;
	
	// Find where to start
	PRInt32 start = 0;
	PRInt32 numWindows = mWindowList.Count();
	if ( inStartWindow != NULL )
	{
		for ( start = 0; start< numWindows; start++ )
		{
		
			nsWindowInfo* windowInfo = (nsWindowInfo*) mWindowList[start];
			if ( ( windowInfo )->mWindow == inStartWindow )
				break;
			windowInfo = NULL;
		}
	}
	
	// Now find the window
	for ( PRInt32 i = start; i < numWindows; i++ )
	{
		nsWindowInfo* windowInfo = (nsWindowInfo*) mWindowList[i];
		if ( ( inWindowType == NULL) || ( windowInfo )->mWindowType == *inWindowType )
		{
			*outWindow = windowInfo->mWindow;
			NS_ADDREF( *outWindow );
			return NS_OK;
		}
	}
	return NS_OK;
}
	
/*
	Returns the window of type inType ( if null return any window type ) which has the most recent
	time stamp
*/
NS_IMETHODIMP nsWindowMediator::GetMostRecentWindow( nsString* inType, nsIDOMWindow** outWindow )
{
	nsWindowInfo* windoInfo = NULL;
	*outWindow = NULL;
	PRInt32 lastTimeStamp = -1;
	PRInt32 count = mWindowList.Count();
	// Find the most window with the highest time stamp that matches the requested type
	for ( int32 i = 0; i< count; i++ )
	{	
		nsWindowInfo* windowInfo = (nsWindowInfo*) mWindowList[i];
		if ( ( inType == NULL || windowInfo->mWindowType == *inType ) && windowInfo->mTimeStamp >= lastTimeStamp )
		{
			*outWindow = windowInfo->mWindow;
			lastTimeStamp = windowInfo->mTimeStamp;
		}
	}
	
	// If we found a window increment the ref count and return
	if( *outWindow )
	{
		NS_ADDREF( *outWindow );
		return NS_OK;
	}
	else
	{
		return NS_ERROR_FAILURE;
	}
}


NS_IMETHODIMP nsWindowMediator::UpdateWindowTimeStamp( nsIDOMWindow* inWindow )
{
	PRInt32 count = mWindowList.Count();
	// Find the window
	for ( int32 i = 0; i< count; i++ )
	{	
		nsWindowInfo* windowInfo = (nsWindowInfo*) mWindowList[i];
		if (  windowInfo->mWindow == inWindow ) 
		{
			mTimeStamp++;
			windowInfo->mTimeStamp = mTimeStamp;
			return NS_OK;
		}
	}
	return NS_ERROR_FAILURE;   
}


NS_IMETHODIMP  nsWindowMediator::UpdateWindowTitle( nsIDOMWindow* inWindow, const nsString& inTitle )
{
	PRInt32 count = mWindowList.Count();
	nsresult rv;
	// Find the window
	for ( int32 i = 0; i< count; i++ )
	{	
		nsWindowInfo* windowInfo = (nsWindowInfo*) mWindowList[i];
		if (  windowInfo->mWindow == inWindow ) 
		{
			nsIRDFResource* window = windowInfo->mRDFID;
			// Get rid of the old value
			nsIRDFNode* target = NULL;
			if ( NS_SUCCEEDED(mInner->GetTarget( window, kNC_Name, true, &target) ) )
			{
				mInner->Unassert( window, kNC_Name, target );
				NS_IF_RELEASE( target );
			}
            
            // Add new title             
			nsCOMPtr<nsIRDFLiteral> newTitle;
		//	nsString windowTitle;
		//	inWindow->GetName( windowTitle );
			if (NS_FAILED(rv = gRDFService->GetLiteral( inTitle, getter_AddRefs(newTitle))))
			{
				NS_ERROR("unable to create literal for window name");
				return rv;
			}

			rv = Assert( window , kNC_Name, newTitle, PR_TRUE);
  			if (rv != NS_RDF_ASSERTION_ACCEPTED)
			{
				NS_ERROR("unable to set window name");
			}
			return NS_OK;
		}
	} 
	return NS_ERROR_FAILURE;
}

NS_IMETHODIMP  nsWindowMediator::GetWindowForResource( nsIRDFResource* inResource, nsIDOMWindow** outWindow )
{
	if ( outWindow == NULL )
		return NS_ERROR_INVALID_ARG;
		*outWindow = NULL;
	PRInt32 count = mWindowList.Count();
	// Find the window
	for ( int32 i = 0; i< count; i++ )
	{	
		nsWindowInfo* windowInfo = (nsWindowInfo*) mWindowList[i];
		if (  windowInfo->mRDFID.get() == inResource ) 
		{
			*outWindow = windowInfo->mWindow;
			return NS_OK;
		}
	} 
	return NS_ERROR_FAILURE;
}

// COM
NS_IMPL_ADDREF( nsWindowMediator );
NS_IMPL_RELEASE( nsWindowMediator );

NS_IMETHODIMP nsWindowMediator::QueryInterface(REFNSIID iid, void **result)
{
	if (! result)
		return NS_ERROR_NULL_POINTER;

	*result = nsnull;
	if (iid.Equals(kIRDFDataSourceIID) ||
		iid.Equals(kIWindowMediatorIID))
	{
		*result = NS_STATIC_CAST(nsIWindowMediator *, this);
		AddRef();
		return NS_OK;
	}
	return(NS_NOINTERFACE);
}

// RDF
NS_IMETHODIMP nsWindowMediator::Init(const char* uri)
{
	nsresult rv;

	if (NS_FAILED(rv = nsComponentManager::CreateInstance(kRDFInMemoryDataSourceCID,
	                                                nsnull,
	                                                kIRDFDataSourceIID,
	                                                (void**) &mInner)))
	{
		return rv;
	}

	if (NS_FAILED(rv = mInner->Init(uri)))
	{
	 	NS_ERROR( "unable to init mInner");
		return rv;
	}

	if (NS_FAILED(rv = rdf_MakeSeq(mInner, kNC_WindowMediatorRoot)))
	{
	    NS_ERROR("Unable to make NC:WindowMediatorRoot a sequence");
	    return rv;
	}
#if 1
	nsString s1( "foobar" );
	AddWindowToRDF( s1 );
	nsString s2("quakeworld");
	AddWindowToRDF( s2 );
#endif
    // register this as a named data source with the RDF service
    return gRDFService->RegisterDataSource(this, PR_FALSE);
}

NS_IMETHODIMP nsWindowMediator::Assert(nsIRDFResource* aSource,
                               nsIRDFResource* aProperty,
                               nsIRDFNode* aTarget,
                               PRBool aTruthValue)
{
    // XXX TODO: filter out asserts we don't care about
    return mInner->Assert(aSource, aProperty, aTarget, aTruthValue);
}

NS_IMETHODIMP nsWindowMediator::Unassert(nsIRDFResource* aSource,
                                 nsIRDFResource* aProperty,
                                 nsIRDFNode* aTarget)
{
    // XXX TODO: filter out unasserts we don't care about
    return mInner->Unassert(aSource, aProperty, aTarget);
}


NS_IMETHODIMP nsWindowMediator::Flush(void)
{
    NS_NOTYETIMPLEMENTED("write me!");
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP nsWindowMediator::GetAllCommands(nsIRDFResource* source,
                                       nsIEnumerator/*<nsIRDFResource>*/** commands)
{
    NS_NOTYETIMPLEMENTED("write me!");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsWindowMediator::IsCommandEnabled(nsISupportsArray/*<nsIRDFResource>*/* aSources,
                                         nsIRDFResource*   aCommand,
                                         nsISupportsArray/*<nsIRDFResource>*/* aArguments,
                                         PRBool* aResult)
{
    NS_NOTYETIMPLEMENTED("write me!");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsWindowMediator::DoCommand(nsISupportsArray* aSources,
                                  nsIRDFResource*   aCommand,
                                  nsISupportsArray* aArguments)
{
    NS_NOTYETIMPLEMENTED("write me!");
    return NS_ERROR_NOT_IMPLEMENTED;
}


// Private helpers
nsresult nsWindowMediator::AddWindowToRDF( nsWindowInfo* ioWindowInfo )
{
	if ( ioWindowInfo == NULL || ioWindowInfo->mWindow == NULL )
		return false;
		
	nsCOMPtr<nsIRDFResource> window;
	nsresult rv;	
	
	nsString	windowTitle;
	ioWindowInfo->mWindow->GetName( windowTitle );
	
	// Make up a unique ID and create the RDF NODE
	nsString uniqueID = "window-";
	uniqueID.Append(windowTitle );
	uniqueID.Append( mTimeStamp, 10 );
	char cID[ 256];
	uniqueID.ToCString( cID, 256);
	if (NS_FAILED(rv = gRDFService->GetResource(cID, getter_AddRefs(window) )))
	{
		NS_ERROR("unable to get window resource");
		return rv;
	}
	ioWindowInfo->mRDFID = window ;
		
	// Get the RDF literal and add it to our node 
	nsCOMPtr<nsIRDFLiteral> windowTitleLiteral;
	if (NS_FAILED(rv = gRDFService->GetLiteral( windowTitle, getter_AddRefs(windowTitleLiteral))))
	{
		NS_ERROR("unable to create literal for window name");
		return rv;
	}

	rv = Assert(window, kNC_Name, windowTitleLiteral, PR_TRUE);
    if (rv != NS_RDF_ASSERTION_ACCEPTED)
	{
		NS_ERROR("unable to set window name");
		return rv;
	}
	
	// Add the element to the container
	if (NS_FAILED(rv = rdf_ContainerAppendElement(mInner, kNC_WindowMediatorRoot, window)))
	{
		NS_ERROR("unable to add window to container");
		return rv;
	}
	return NS_OK;
}

// Private helpers
#if 1
nsresult nsWindowMediator::AddWindowToRDF( nsString& intitle )
{
	nsCOMPtr<nsIRDFResource> window;
	nsresult rv;
	nsString title(intitle);
	title+="unique stuff";
	const char* uniqueID= title.ToNewCString(); // XXXX need to fix
	if (NS_FAILED(rv = gRDFService->GetResource(uniqueID, getter_AddRefs(window) )))
	{
		NS_ERROR("unable to get window resource");
		return rv;
	}

	nsCOMPtr<nsIRDFLiteral> windowName;
	
	if ( title.Length() == 0 )
		title = "Mozilla";

	if (NS_FAILED(rv = gRDFService->GetLiteral( title, getter_AddRefs(windowName))))
	{
		NS_ERROR("unable to create literal for window name");
		return rv;
	}
	
	rv = Assert(window, kNC_Name, windowName, PR_TRUE);
    if (rv != NS_RDF_ASSERTION_ACCEPTED)
	{
		NS_ERROR("unable to set window name");
		return rv;
	}

	if (NS_FAILED(rv = rdf_ContainerAppendElement(mInner, kNC_WindowMediatorRoot, window)))
	{
		NS_ERROR("unable to add window to container");
		return rv;
	}

	return NS_OK;
}
#endif

nsresult NS_NewWindowMediator(nsIWindowMediator** result)
{
	nsWindowMediator* ds = new nsWindowMediator();

	if (! ds)
		return NS_ERROR_NULL_POINTER;

	*result = ds;
	NS_ADDREF(*result);
	return(NS_OK);
}


