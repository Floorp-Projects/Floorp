/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

#include "nsString.h"
#include "nsVoidArray.h"
#include "rdf.h"
#include "nsIRDFService.h"
#include "nsIRDFDataSource.h"
#include "nsIRDFNode.h"
#include "nsIRDFObserver.h"
#include "nsRDFCID.h"
#include "nsIRDFContainer.h"
#include "nsIRDFContainerUtils.h"
#include "nsXPIDLString.h"
#include "nsIServiceManager.h"
#include "nsIWindowMediator.h"
#include "nsCOMPtr.h"
#include "nsIWebShell.h"
#include "nsIDOMWindow.h"
#include "nsIDOMElement.h"
#include "nsISimpleEnumerator.h"
#include "nsIDocumentViewer.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsWindowMediator.h"

// Interfaces Needed
#include "nsIDocShell.h"
#include "nsIInterfaceRequestor.h"
#include "nsIXULWindow.h"

static NS_DEFINE_CID(kRDFInMemoryDataSourceCID, NS_RDFINMEMORYDATASOURCE_CID);
static NS_DEFINE_CID(kRDFServiceCID,            NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kRDFContainerUtilsCID,     NS_RDFCONTAINERUTILS_CID);

static NS_DEFINE_CID(kRDFContainerCID,                    NS_RDFCONTAINER_CID);
static const char kURINC_WindowMediatorRoot[] = "NC:WindowMediatorRoot";


DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, Name);
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, URL);
static	nsIRDFService* gRDFService = nsnull;

nsresult NS_NewRDFContainer(nsIRDFDataSource* aDataSource,
                   nsIRDFResource* aResource,
                   nsIRDFContainer** aResult)
{
    nsresult rv;
  
    rv = nsComponentManager::CreateInstance( 
                                        kRDFContainerCID, NULL, NS_GET_IID(nsIRDFContainer),  (void**)aResult );
    if (NS_FAILED(rv))
    	return rv;

    rv = (*aResult)->Init(aDataSource, aResource);
    if (NS_FAILED(rv))
    {
    	NS_RELEASE(*aResult);
    }
    return rv;
}

nsresult GetDOMWindow( nsIXULWindow* inWindow, nsCOMPtr< nsIDOMWindow>& outDOMWindow )
{
	nsCOMPtr<nsIDocShell> docShell;

	inWindow->GetDocShell(getter_AddRefs(docShell));
   outDOMWindow = do_GetInterface(docShell);
   return outDOMWindow ? NS_OK : NS_ERROR_FAILURE;
}


nsCOMPtr<nsIDOMNode> GetDOMNodeFromDocShell(nsIDocShell *aShell)
{
  nsCOMPtr<nsIDOMNode> node;

  nsCOMPtr<nsIContentViewer> cv;
  aShell->GetContentViewer(getter_AddRefs(cv));
  if (cv) {
    nsCOMPtr<nsIDocumentViewer> docv(do_QueryInterface(cv));
    if (docv) {
      nsCOMPtr<nsIDocument> doc;
      docv->GetDocument(*getter_AddRefs(doc));
      if (doc) {
        nsCOMPtr<nsIDOMDocument> domdoc(do_QueryInterface(doc));
        if (domdoc) {
          nsCOMPtr<nsIDOMElement> element;
          domdoc->GetDocumentElement(getter_AddRefs(element));
          if (element)
            node = do_QueryInterface(element);
        }
      }
    }
  }

  return node;
}

void GetAttribute( nsIXULWindow* inWindow, const nsAutoString& inAttribute, nsAutoString& outValue )
{
  nsCOMPtr<nsIDocShell> shell;
  if ( inWindow &&
  		NS_SUCCEEDED(inWindow->GetDocShell(getter_AddRefs(shell))))
  {
  		nsCOMPtr<nsIDOMNode> node( GetDOMNodeFromDocShell ( shell ) );
 		
	  if (node )
	  {
	  	nsCOMPtr<nsIDOMElement> webshellElement( do_QueryInterface(node));
  		if ( webshellElement.get() )
  			webshellElement->GetAttribute(inAttribute.GetUnicode(), outValue );
  	  }
  }
}

void GetWindowType( nsIXULWindow* inWindow, nsAutoString& outType )
{
 	nsAutoString typeAttrib("windowtype");
 	GetAttribute( inWindow, typeAttrib, outType );
}

class nsWindowMediator;

struct nsWindowInfo
{
	nsWindowInfo( nsIXULWindow* inWindow, PRInt32 inTimeStamp ):
				 mTimeStamp( inTimeStamp ), mWindow( inWindow )
	{
	}
	
	~nsWindowInfo()
	{
	}
	
	nsCOMPtr<nsIRDFResource>	   mRDFID;
	PRInt32			  mTimeStamp;
	nsCOMPtr<nsIXULWindow> mWindow;
	
	nsAutoString    GetType()
		{ 
			nsAutoString rtnString;
		 	GetWindowType( mWindow, rtnString );
		 	return rtnString;
		}
};



class nsWindowEnumerator : public nsISimpleEnumerator
{

public:
	nsWindowEnumerator ( const PRUnichar* inTypeString,  nsWindowMediator& inMediator,
      PRBool enumXULWindow  );
	virtual ~nsWindowEnumerator();
	NS_IMETHOD HasMoreElements(PRBool *retval);
  NS_IMETHOD GetNext(nsISupports **retval);

  NS_DECL_ISUPPORTS
private:
	friend class nsWindowMediator;
	
	PRInt32 FindNext();
	void WindowRemoved( PRInt32 inIndex);
	
	nsWindowMediator* mWindowMediator;
	nsString mType;
	PRInt32 mCurrentPosition;
   PRBool   mEnumXULWindow;
};



nsIRDFResource	*nsWindowMediator::kNC_WindowMediatorRoot = NULL;
nsIRDFResource	*nsWindowMediator::kNC_Name = NULL;
nsIRDFResource	*nsWindowMediator::kNC_URL = NULL;

PRInt32		nsWindowMediator::gRefCnt;
nsIRDFDataSource *nsWindowMediator::mInner = NULL;


nsWindowMediator::nsWindowMediator() :
	mTimeStamp( 0 ), mEnumeratorList(), mWindowList()
{
	 NS_INIT_REFCNT();

   // This should really be done in the static constructor fn.
   nsresult rv;
   rv = Init();
   NS_ASSERTION(NS_SUCCEEDED(rv), "uh oh, couldn't Init() for some reason");
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
			if ( gRDFService )
			{
				gRDFService->UnregisterDataSource(this);
				nsServiceManager::ReleaseService(kRDFServiceCID, gRDFService);
				gRDFService = nsnull;
		 	}
      NS_IF_RELEASE(kNC_WindowMediatorRoot);
      NS_IF_RELEASE( kNC_Name );
      NS_IF_RELEASE(kNC_URL);
      NS_IF_RELEASE(mInner);
    }
}

       
        
NS_IMETHODIMP nsWindowMediator::RegisterWindow( nsIXULWindow* inWindow )
{	
	if ( inWindow == NULL  )
		return NS_ERROR_INVALID_ARG;
	
	mTimeStamp++;
	
	// Create window info struct and add to list of windows
	nsWindowInfo* windowInfo = new nsWindowInfo ( inWindow, mTimeStamp );
	if ( windowInfo == NULL )
		return NS_ERROR_OUT_OF_MEMORY;
		
	if ( !mWindowList.AppendElement( windowInfo ) )
	{
	 	delete windowInfo;
	 	return NS_ERROR_OUT_OF_MEMORY;
	}
	 
	nsresult returnValue;
	if ( NS_FAILED( returnValue = AddWindowToRDF( windowInfo )  ))
	{
	 	mWindowList.RemoveElementAt( mWindowList.Count() );
	 	delete windowInfo;
	 	return returnValue;
	 }
	 
	 return NS_OK;
}

NS_IMETHODIMP nsWindowMediator::UnregisterWindow( nsIXULWindow* inWindow )
{
	// Find Window info
	PRInt32 count = mWindowList.Count();
	nsWindowInfo* windowInfo = NULL;
	PRInt32 windowIndex = 0;
	for ( windowIndex = 0; windowIndex< count; windowIndex++ )
	{
		windowInfo = (nsWindowInfo*) mWindowList[windowIndex];
		if ( ( windowInfo )->mWindow.get() == inWindow )
			break;
		windowInfo = NULL;
	}
	
	if ( windowInfo == NULL )	
		return NS_ERROR_INVALID_ARG;
		
  // Inform the iterators
  PRInt32 index = -1;
  while (++index < mEnumeratorList.Count() ) 
  {
  	((nsWindowEnumerator*)mEnumeratorList[ index ] )->WindowRemoved ( windowIndex );
  }
  
 	// Remove From RDF
  nsCOMPtr<nsIRDFContainer> container;
  nsresult rv = NS_NewRDFContainer(mInner, kNC_WindowMediatorRoot, getter_AddRefs(container));
  if (NS_SUCCEEDED(rv))
  {
  	container->RemoveElement( windowInfo->mRDFID, PR_TRUE );
  }
 	
	// Remove from the list and free up 
	mWindowList.RemoveElement( windowInfo );
	
	delete windowInfo;	
	return NS_OK;
}


NS_METHOD nsWindowMediator::GetEnumerator( const PRUnichar* inType, nsISimpleEnumerator** outEnumerator )
{
		if ( outEnumerator == NULL )
			return NS_ERROR_INVALID_ARG;
	
		nsWindowEnumerator* enumerator = new nsWindowEnumerator( inType, *this, PR_FALSE );
		if (enumerator )
		{
			return enumerator->QueryInterface( NS_GET_IID(nsISimpleEnumerator) , (void**)outEnumerator );
		}
		return NS_ERROR_OUT_OF_MEMORY;
}
	
NS_METHOD nsWindowMediator::GetXULWindowEnumerator( const PRUnichar* inType, nsISimpleEnumerator** outEnumerator )
{
		if ( outEnumerator == NULL )
			return NS_ERROR_INVALID_ARG;
	
		nsWindowEnumerator* enumerator = new nsWindowEnumerator( inType, *this, PR_TRUE );
		if (enumerator )
		{
			return enumerator->QueryInterface( NS_GET_IID(nsISimpleEnumerator) , (void**)outEnumerator );
		}
		return NS_ERROR_OUT_OF_MEMORY;
}	
 

PRInt32 nsWindowMediator::AddEnumerator( nsWindowEnumerator* inEnumerator )
{
	return mEnumeratorList.AppendElement( inEnumerator );
}

PRInt32 nsWindowMediator::RemoveEnumerator( nsWindowEnumerator* inEnumerator)
{
	return mEnumeratorList.RemoveElement( inEnumerator );		
}

	
/*
	Returns the window of type inType ( if null return any window type ) which has the most recent
	time stamp
*/
NS_IMETHODIMP nsWindowMediator::GetMostRecentWindow( const PRUnichar* inType, nsIDOMWindow** outWindow )
{
	*outWindow = NULL;
	PRInt32 lastTimeStamp = -1;
	PRInt32 count = mWindowList.Count();
	nsIXULWindow* mostRecentWindow = NULL;
	nsAutoString typeString( inType );
	// Find the most window with the highest time stamp that matches the requested type
	for ( int32 i = 0; i< count; i++ )
	{	
		nsWindowInfo* windowInfo = (nsWindowInfo*) mWindowList[i];
		if ( ( inType == NULL || windowInfo->GetType() == typeString ) && windowInfo->mTimeStamp >= lastTimeStamp )
		{
			mostRecentWindow = windowInfo->mWindow;
			lastTimeStamp = windowInfo->mTimeStamp;
		}
	}
	
	if( mostRecentWindow )
	{
			nsCOMPtr <nsIDOMWindow> DOMWindow;
			if( NS_SUCCEEDED ( GetDOMWindow( mostRecentWindow, DOMWindow  ) ) )
			{	
				*outWindow = DOMWindow;
				(*outWindow)->AddRef();
				return NS_OK;
			}
			else
			{
				return NS_ERROR_FAILURE;
			}
	}
	
	return NS_OK;
	
}


NS_IMETHODIMP nsWindowMediator::UpdateWindowTimeStamp( nsIXULWindow* inWindow )
{
	PRInt32 count = mWindowList.Count();
	for ( int32 i = 0; i< count; i++ )
	{	
		nsWindowInfo* windowInfo = (nsWindowInfo*) mWindowList[i];
		if (  windowInfo->mWindow.get() == inWindow ) 
		{
			mTimeStamp++;
			windowInfo->mTimeStamp = mTimeStamp;
			return NS_OK;
		}
	}
	
	return NS_ERROR_FAILURE;   
}


NS_IMETHODIMP  nsWindowMediator::UpdateWindowTitle( nsIXULWindow* inWindow, const PRUnichar* inTitle )
{
	PRInt32 count = mWindowList.Count();
	nsresult rv;
	for ( int32 i = 0; i< count; i++ )
	{	
		nsWindowInfo* windowInfo = (nsWindowInfo*) mWindowList[i];
		if (  windowInfo->mWindow.get() == inWindow ) 
		{
			nsIRDFResource* window = windowInfo->mRDFID;
			// Get rid of the old value
			nsIRDFNode* target = NULL;
			if ( NS_SUCCEEDED(mInner->GetTarget( window, kNC_Name, PR_TRUE, &target) ) && (target != nsnull) )
			{
				mInner->Unassert( window, kNC_Name, target );
				NS_IF_RELEASE( target );
			}
            
            // Add new title             
			nsCOMPtr<nsIRDFLiteral> newTitle;
			if ( gRDFService && NS_FAILED(rv = gRDFService->GetLiteral( inTitle, getter_AddRefs(newTitle) ) ) )
			{
				NS_ERROR("unable to create literal for window name");
				return rv;
			}

			// Should this title be displayed
			PRBool display = PR_TRUE;
			nsAutoString typeAttrib("intaskslist");
			nsAutoString displayString;
 			GetAttribute( inWindow, typeAttrib, displayString );
 			displayString.ToLowerCase();
 			
			if ( displayString.Equals("false") )
				display=PR_FALSE;
				
			rv = Assert( window , kNC_Name, newTitle, display );
  			if (rv != NS_RDF_ASSERTION_ACCEPTED)
			{
				NS_ERROR("unable to set window name");
			}
			return NS_OK;
		}
	} 
	return NS_ERROR_FAILURE;
}

NS_IMETHODIMP  nsWindowMediator::GetWindowForResource( const PRUnichar* inResource, nsIDOMWindow** outWindow )
{
	if ( outWindow == NULL )
		return NS_ERROR_INVALID_ARG;
	*outWindow = NULL;
	PRInt32 count = mWindowList.Count();
	// Find the window
	nsresult result = NS_ERROR_FAILURE;
	nsAutoString temp( inResource );
	char* resourceString = temp.ToNewCString();
	for ( int32 i = 0; i< count; i++ )
	{	
		nsWindowInfo* windowInfo = (nsWindowInfo*) mWindowList[i];
		PRBool isMatch = PR_FALSE;

		if ( NS_SUCCEEDED( windowInfo->mRDFID->EqualsString( resourceString, &isMatch) ) && isMatch ) 
		{
			nsCOMPtr <nsIDOMWindow> DOMWindow;
			if( NS_SUCCEEDED ( GetDOMWindow( windowInfo->mWindow, DOMWindow  ) ) )
			{	
				*outWindow = DOMWindow;
				(*outWindow)->AddRef();
				result = NS_OK;
			}
			break;
		}
	} 

	delete resourceString;
	// Rather than returning an error when you pass in crap, return NS_OK
	// Doing this to avoid throwing a JS exception becasue event bubbling is not working as of 7/26/99
	//return result;
	return NS_OK;
}

NS_IMETHODIMP nsWindowMediator::ConvertISupportsToDOMWindow( nsISupports* inInterface, nsIDOMWindow** outWindow )
{
   return inInterface->QueryInterface(NS_GET_IID(nsIDOMWindow)  , (void**) outWindow );
} 


// COM
NS_IMPL_ADDREF( nsWindowMediator );
NS_IMPL_RELEASE( nsWindowMediator );
NS_IMPL_QUERY_INTERFACE2(nsWindowMediator, nsIWindowMediator, nsIRDFDataSource)

// RDF
nsresult
nsWindowMediator::Init()
{
	nsresult rv;

	 if (gRefCnt++ == 0)
	 {
      rv = nsServiceManager::GetService( kRDFServiceCID, NS_GET_IID(nsIRDFService), (nsISupports**) &gRDFService );
      if (NS_FAILED(rv)) return rv;

			gRDFService->GetResource( kURINC_WindowMediatorRoot,   &kNC_WindowMediatorRoot );
			gRDFService->GetResource (kURINC_Name, &kNC_Name );
			gRDFService->GetResource( kURINC_URL, &kNC_URL );
    }

	if (NS_FAILED(rv = nsComponentManager::CreateInstance(kRDFInMemoryDataSourceCID,
	                                                nsnull,
	                                                NS_GET_IID(nsIRDFDataSource),
	                                                (void**) &mInner)))
	{
		return rv;
	}

	NS_WITH_SERVICE(nsIRDFContainerUtils, rdfc, kRDFContainerUtilsCID, &rv);
	if (NS_FAILED(rv))
		return rv;

	rv = rdfc->MakeSeq(mInner, kNC_WindowMediatorRoot, NULL );
	NS_ASSERTION(NS_SUCCEEDED(rv), "Unable to make NC:WindowMediatorRoot a sequence");
	if (NS_FAILED(rv))
		return rv;
	if ( gRDFService == NULL )
		return NS_ERROR_NULL_POINTER;
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


NS_IMETHODIMP nsWindowMediator::Change(nsIRDFResource* aSource,
                                       nsIRDFResource* aProperty,
                                       nsIRDFNode* aOldTarget,
                                       nsIRDFNode* aNewTarget)
{
  // XXX TODO: filter out changes we don't care about
  return mInner->Change(aSource, aProperty, aOldTarget, aNewTarget);
}

NS_IMETHODIMP nsWindowMediator::Move(nsIRDFResource* aOldSource,
                                     nsIRDFResource* aNewSource,
                                     nsIRDFResource* aProperty,
                                     nsIRDFNode* aTarget)
{
  // XXX TODO: filter out changes we don't care about
  return mInner->Move(aOldSource, aNewSource, aProperty, aTarget);
}


NS_IMETHODIMP nsWindowMediator::GetAllCommands(nsIRDFResource* source,
                                       nsIEnumerator/*<nsIRDFResource>*/** commands)
{
    NS_NOTYETIMPLEMENTED("write me!");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsWindowMediator::GetAllCmds(nsIRDFResource* source,
                                       nsISimpleEnumerator/*<nsIRDFResource>*/** commands)
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
	if ( !ioWindowInfo || !(ioWindowInfo->mWindow) || !gRDFService )
		return NS_ERROR_NULL_POINTER;
		
	nsCOMPtr<nsIRDFResource> window;
	nsresult rv;	
	
	nsAutoString	windowTitle;
	
	// Make up a unique ID and create the RDF NODE
	nsAutoString uniqueID = "window-";
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
	#if 0	
	// Get the RDF literal and add it to our node 
	nsCOMPtr<nsIRDFLiteral> windowTitleLiteral;
	if (NS_FAILED(rv = gRDFService->GetLiteral( windowTitle.GetUnicode(), getter_AddRefs(windowTitleLiteral))))
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
	#endif
	// Add the element to the container
  nsCOMPtr<nsIRDFContainer> container;
  rv = NS_NewRDFContainer(mInner, kNC_WindowMediatorRoot, getter_AddRefs(container));
  if (NS_FAILED(rv))
  	return rv;

	if (NS_FAILED(rv = container->AppendElement( window ) ) /* rdf_ContainerAppendElement(mInner, kNC_WindowMediatorRoot, window) */ )
	{
		NS_ERROR("unable to add window to container");
		return rv;
	}
	return NS_OK;
}



// window Enumerator
nsWindowEnumerator::nsWindowEnumerator ( const PRUnichar* inTypeString, nsWindowMediator& inMediator,
   PRBool enumXULWindow )
	: mWindowMediator( &inMediator ), mType( inTypeString ), mCurrentPosition( -1 ),
   mEnumXULWindow(enumXULWindow)
{
	NS_INIT_REFCNT();
	mWindowMediator->AddEnumerator( this );
	mWindowMediator->AddRef();
}

nsWindowEnumerator::~nsWindowEnumerator()
{
	mWindowMediator->RemoveEnumerator( this );
	mWindowMediator->Release();
}

NS_IMETHODIMP nsWindowEnumerator::HasMoreElements(PRBool *retval)
{
	if ( !retval )
		return NS_ERROR_INVALID_ARG;
		
	*retval = PR_FALSE;
	if ( FindNext() != -1 )
		*retval = PR_TRUE;
	return NS_OK;
}
	
NS_IMETHODIMP nsWindowEnumerator::GetNext(nsISupports **retval)
{
	if ( !retval )
		return NS_ERROR_INVALID_ARG;
	*retval = NULL;
	PRInt32 index = FindNext();
	if ( index >= 0 )
	{
		nsWindowInfo* windowInfo = (nsWindowInfo*) mWindowMediator->mWindowList[index];
      if(mEnumXULWindow)
         CallQueryInterface(windowInfo->mWindow, retval);
      else
         {
	   	nsCOMPtr<nsIDOMWindow> domWindow;
		   GetDOMWindow( windowInfo->mWindow, domWindow );
         CallQueryInterface(domWindow, retval);
         }
		
		mCurrentPosition = index;
	}
	return NS_OK;
}

PRInt32 nsWindowEnumerator::FindNext()
{
	PRInt32 numWindows =  mWindowMediator->mWindowList.Count();
	PRBool allWindows = (mType.Length() == 0);
	for ( PRInt32 i = mCurrentPosition+1; i < numWindows; i++ )
	{
		nsWindowInfo* windowInfo = (nsWindowInfo*) mWindowMediator->mWindowList[i];
		if (  allWindows  || ( windowInfo->GetType() == mType ) )
		{
			return i;
		}
	}
	return -1;
}

// if a window is being removed adjust the iterator's current position
void nsWindowEnumerator::WindowRemoved( PRInt32 inIndex)
{
	if ( inIndex <= mCurrentPosition )
	{
			mCurrentPosition--;
	}
}

/* 
 * Implementations of nsISupports interface methods...
 */
NS_IMPL_ADDREF(nsWindowEnumerator);
NS_IMPL_RELEASE(nsWindowEnumerator);
NS_IMPL_QUERY_INTERFACE1(nsWindowEnumerator, nsISimpleEnumerator);

