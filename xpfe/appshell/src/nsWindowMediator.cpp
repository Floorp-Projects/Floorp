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

#include "nsCOMPtr.h"
#include "nsAutoLock.h"
#include "nsString.h"
#include "nsVoidArray.h"
#include "rdf.h"
#include "nsIBaseWindow.h"
#include "nsIWidget.h"
#include "nsIDOMWindow.h"
#include "nsIDOMWindowInternal.h"
#include "nsIDOMElement.h"
#include "nsIDocumentViewer.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIRDFContainer.h"
#include "nsIRDFContainerUtils.h"
#include "nsIRDFDataSource.h"
#include "nsIRDFNode.h"
#include "nsIRDFObserver.h"
#include "nsIRDFService.h"
#include "nsIServiceManager.h"
#include "nsISimpleEnumerator.h"
#include "nsIWebShell.h"
#include "nsAppShellWindowEnumerator.h"
#include "nsRDFCID.h"
#include "nsWindowMediator.h"
#include "nsXPIDLString.h"

// Interfaces Needed
#include "nsIDocShell.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIXULWindow.h"

static NS_DEFINE_CID(kRDFInMemoryDataSourceCID, NS_RDFINMEMORYDATASOURCE_CID);
static NS_DEFINE_CID(kRDFServiceCID,            NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kRDFContainerUtilsCID,     NS_RDFCONTAINERUTILS_CID);

static NS_DEFINE_CID(kRDFContainerCID,                    NS_RDFCONTAINER_CID);
static const char kURINC_WindowMediatorRoot[] = "NC:WindowMediatorRoot";


DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, Name);
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, URL);
static	nsIRDFService* gRDFService = nsnull;

static nsresult GetDOMWindow( nsIXULWindow* inWindow,
                  nsCOMPtr< nsIDOMWindowInternal>& outDOMWindow);
static nsCOMPtr<nsIDOMNode> GetDOMNodeFromDocShell(nsIDocShell *aShell);
static void GetAttribute( nsIXULWindow* inWindow,
                          const nsAString& inAttribute,
                          nsAString& outValue);
static void GetWindowType( nsIXULWindow* inWindow, nsAutoString& outType);
static inline PRUint32 GetWindowZ( nsIXULWindow *inWindow);

static nsresult NS_NewMediatorListRDFContainer(nsIRDFDataSource* aDataSource,
                   nsIRDFResource* aResource,
                   nsIRDFContainer** aResult)
{
    nsresult rv;
  
    rv = nsComponentManager::CreateInstance( 
                                        kRDFContainerCID, NULL, NS_GET_IID(nsIRDFContainer),  (void**)aResult);
    if (NS_FAILED(rv))
    	return rv;

    rv = (*aResult)->Init(aDataSource, aResource);
    if (NS_FAILED(rv))
    {
    	NS_RELEASE(*aResult);
    }
    return rv;
}

nsresult GetDOMWindow( nsIXULWindow* inWindow, nsCOMPtr< nsIDOMWindowInternal>& outDOMWindow)
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


void GetAttribute( nsIXULWindow* inWindow,
                   const nsAString& inAttribute, nsAString& outValue)
{
  nsCOMPtr<nsIDocShell> shell;
  if ( inWindow &&
  		NS_SUCCEEDED(inWindow->GetDocShell(getter_AddRefs(shell))))
  {
  		nsCOMPtr<nsIDOMNode> node( GetDOMNodeFromDocShell(shell));
 		
	  if (node)
	  {
	  	nsCOMPtr<nsIDOMElement> webshellElement( do_QueryInterface(node));
  		if ( webshellElement.get())
  			webshellElement->GetAttribute(nsAutoString(inAttribute), outValue);
  	  }
  }
}

void GetWindowType(nsIXULWindow* inWindow, nsAString& outType)
{
 	GetAttribute(inWindow, NS_LITERAL_STRING("windowtype"), outType);
}

/* return an integer corresponding to the relative z order of the window.
   should probably be an explicit read-only method on nsIXULWindow */
PRUint32 GetWindowZ(nsIXULWindow *inWindow)
{
	PRUint32 order;
	inWindow->GetZlevel(&order);
	return order;
}

nsIRDFResource	*nsWindowMediator::kNC_WindowMediatorRoot = NULL;
nsIRDFResource	*nsWindowMediator::kNC_Name = NULL;
nsIRDFResource	*nsWindowMediator::kNC_URL = NULL;

PRInt32		nsWindowMediator::gRefCnt;
nsIRDFDataSource *nsWindowMediator::mInner = NULL;


nsWindowMediator::nsWindowMediator() :
	mEnumeratorList(), mOldestWindow(0), mTopmostWindow(0),
	mTimeStamp(0), mListLock(0)
{
   NS_INIT_REFCNT();

   // This should really be done in the static constructor fn.
   nsresult rv;
   rv = Init();
   NS_ASSERTION(NS_SUCCEEDED(rv), "uh oh, couldn't Init() for some reason");
}

nsWindowMediator::~nsWindowMediator()
{
  if (--gRefCnt == 0) {

    // Delete data
    while (mOldestWindow)
      UnregisterWindow(mOldestWindow);

    // unregister this from the RDF service
    if (gRDFService) {
      gRDFService->UnregisterDataSource(this);
      nsServiceManager::ReleaseService(kRDFServiceCID, gRDFService);
      gRDFService = nsnull;
    }
    NS_IF_RELEASE(kNC_WindowMediatorRoot);
    NS_IF_RELEASE(kNC_Name);
    NS_IF_RELEASE(kNC_URL);
    NS_IF_RELEASE(mInner);
    if (mListLock)
      PR_DestroyLock(mListLock);
  }
}



NS_IMETHODIMP nsWindowMediator::RegisterWindow(nsIXULWindow* inWindow)
{
	if (inWindow == NULL)
		return NS_ERROR_INVALID_ARG;
	
	mTimeStamp++;

	// Create window info struct and add to list of windows
	nsWindowInfo* windowInfo = new nsWindowInfo (inWindow, mTimeStamp);
	if (windowInfo == NULL)
		return NS_ERROR_OUT_OF_MEMORY;

	nsresult returnValue = AddWindowToRDF(windowInfo);
	if (NS_FAILED(returnValue)) {
 		delete windowInfo;
		return returnValue;
	}

	nsAutoLock lock(mListLock);
	if (mOldestWindow)
		windowInfo->InsertAfter(mOldestWindow->mOlder, 0);
	else
		mOldestWindow = windowInfo;

	return NS_OK;
}

NS_IMETHODIMP nsWindowMediator::UnregisterWindow(nsIXULWindow* inWindow)
{
  // Find Window info
  nsWindowInfo *info,
               *listEnd;

  nsAutoLock lock(mListLock);
  info = mOldestWindow;
  listEnd = 0;
  while (info != listEnd) {
    if (info->mWindow.get() == inWindow)
      return UnregisterWindow(info);
    info = info->mYounger;
    listEnd = mOldestWindow;
  }

  return NS_ERROR_INVALID_ARG;
}


NS_IMETHODIMP nsWindowMediator::UnregisterWindow(nsWindowInfo *inInfo)
{
  // Inform the iterators
  PRInt32 index = -1;
  while (++index < mEnumeratorList.Count()) 
    ((nsAppShellWindowEnumerator*)mEnumeratorList[index])->WindowRemoved(inInfo);

  // Remove From RDF
  nsCOMPtr<nsIRDFContainer> container;
  nsresult rv = NS_NewMediatorListRDFContainer(mInner, kNC_WindowMediatorRoot, getter_AddRefs(container));
  if (NS_SUCCEEDED(rv))
    container->RemoveElement(inInfo->mRDFID, PR_TRUE);
 	
  // Remove from the lists and free up 
  if (inInfo == mOldestWindow)
    mOldestWindow = inInfo->mYounger;
  if (inInfo == mTopmostWindow)
    mTopmostWindow = inInfo->mLower;
  inInfo->Unlink(PR_TRUE, PR_TRUE);
  if (inInfo == mOldestWindow)
    mOldestWindow = 0;
  if (inInfo == mTopmostWindow)
    mTopmostWindow = 0;
  delete inInfo;	

  // inform WindowWatcher
  nsWindowInfo *info = MostRecentWindowInfo(0);
  nsCOMPtr<nsIDOMWindow> domWindow;
  if (info && info->mWindow) {
    nsCOMPtr<nsIDOMWindowInternal> idomWindow;
    GetDOMWindow(info->mWindow, idomWindow);
    domWindow = do_QueryInterface(idomWindow);
  }

  // failure or no more windows sets it to 0, which is acceptable
  mWatcher->SetActiveWindow(domWindow);

  return NS_OK;
}


NS_METHOD nsWindowMediator::GetEnumerator( const PRUnichar* inType, nsISimpleEnumerator** outEnumerator)
{
  if (outEnumerator == NULL)
    return NS_ERROR_INVALID_ARG;

  nsAutoLock lock(mListLock);
  nsAppShellWindowEnumerator *enumerator = new nsASDOMWindowEarlyToLateEnumerator(inType, *this);
  if (enumerator)
    return enumerator->QueryInterface(NS_GET_IID(nsISimpleEnumerator) , (void**)outEnumerator);

  return NS_ERROR_OUT_OF_MEMORY;
}
	
NS_METHOD nsWindowMediator::GetXULWindowEnumerator(const PRUnichar* inType, nsISimpleEnumerator** outEnumerator)
{
  if (outEnumerator == NULL)
    return NS_ERROR_INVALID_ARG;

  nsAutoLock lock(mListLock);
  nsAppShellWindowEnumerator *enumerator = new nsASXULWindowEarlyToLateEnumerator(inType, *this);
  if (enumerator)
    return enumerator->QueryInterface( NS_GET_IID(nsISimpleEnumerator) , (void**)outEnumerator);

  return NS_ERROR_OUT_OF_MEMORY;
}	

NS_METHOD nsWindowMediator::GetZOrderDOMWindowEnumerator(
            const PRUnichar *aWindowType, PRBool aFrontToBack,
            nsISimpleEnumerator **_retval)
{
  if (!_retval)
    return NS_ERROR_INVALID_ARG;

  nsAutoLock lock(mListLock);
  nsAppShellWindowEnumerator *enumerator;
  if (aFrontToBack)
    enumerator = new nsASDOMWindowFrontToBackEnumerator(aWindowType, *this);
  else
    enumerator = new nsASDOMWindowBackToFrontEnumerator(aWindowType, *this);
  if (enumerator)
    return enumerator->QueryInterface(NS_GET_IID(nsISimpleEnumerator), (void**) _retval);

  return NS_ERROR_OUT_OF_MEMORY;
}

NS_METHOD nsWindowMediator::GetZOrderXULWindowEnumerator(
            const PRUnichar *aWindowType, PRBool aFrontToBack,
            nsISimpleEnumerator **_retval)
{
  if (!_retval)
    return NS_ERROR_INVALID_ARG;

  nsAutoLock lock(mListLock);
  nsAppShellWindowEnumerator *enumerator;
  if (aFrontToBack)
    enumerator = new nsASXULWindowFrontToBackEnumerator(aWindowType, *this);
  else
    enumerator = new nsASXULWindowBackToFrontEnumerator(aWindowType, *this);
  if (enumerator)
    return enumerator->QueryInterface(NS_GET_IID(nsISimpleEnumerator), (void**) _retval);

  return NS_ERROR_OUT_OF_MEMORY;
}

PRInt32 nsWindowMediator::AddEnumerator(nsAppShellWindowEnumerator * inEnumerator)
{
  return mEnumeratorList.AppendElement(inEnumerator);
}

PRInt32 nsWindowMediator::RemoveEnumerator( nsAppShellWindowEnumerator * inEnumerator)
{
  return mEnumeratorList.RemoveElement(inEnumerator);
}

/*
	Returns the window of type inType ( if null return any window type ) which has the most recent
	time stamp
*/
NS_IMETHODIMP nsWindowMediator::GetMostRecentWindow( const PRUnichar* inType, nsIDOMWindowInternal** outWindow) {

  NS_ENSURE_ARG_POINTER(outWindow);
  *outWindow = NULL;

  // Find the most window with the highest time stamp that matches
  // the requested type

  nsAutoLock lock(mListLock);
  nsWindowInfo *info = MostRecentWindowInfo(inType);

  if (info && info->mWindow) {
    nsCOMPtr <nsIDOMWindowInternal> DOMWindow;
    if(NS_SUCCEEDED(GetDOMWindow(info->mWindow, DOMWindow))) {	
      *outWindow = DOMWindow;
      NS_ADDREF(*outWindow);
      return NS_OK;
    }
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}


nsWindowInfo *
nsWindowMediator::MostRecentWindowInfo(const PRUnichar* inType)
{
  PRInt32       lastTimeStamp = -1;
  nsAutoString  typeString(inType);
  PRBool        allWindows = !inType || typeString.Length() == 0;

  // Find the most window with the highest time stamp that matches
  // the requested type
  nsWindowInfo *searchInfo,
               *listEnd,
               *foundInfo = 0;

  searchInfo = mOldestWindow;
  listEnd = 0;
  while (searchInfo != listEnd) {
    if ((allWindows || searchInfo->TypeEquals(typeString)) &&
        searchInfo->mTimeStamp >= lastTimeStamp) {

      foundInfo = searchInfo;
      lastTimeStamp = searchInfo->mTimeStamp;
    }
    searchInfo = searchInfo->mYounger;
    listEnd = mOldestWindow;
  }
  return foundInfo;
}


NS_IMETHODIMP nsWindowMediator::UpdateWindowTimeStamp( nsIXULWindow* inWindow)
{
  nsWindowInfo *info,
               *listEnd;

  nsAutoLock lock(mListLock);
  info = mOldestWindow;
  listEnd = 0;
  while (info != listEnd) {
    if (info->mWindow.get() == inWindow) {
      // increment the window's time stamp
      info->mTimeStamp = ++mTimeStamp;

      // inform WindowWatcher
      nsCOMPtr<nsIDOMWindowInternal> idomwindow;
      GetDOMWindow(info->mWindow, idomwindow);
      nsCOMPtr<nsIDOMWindow> domwindow(do_QueryInterface(idomwindow));
      // if for some reason anything failed, it'll be set to 0, which is
      // better than an invalid pointer.
      mWatcher->SetActiveWindow(domwindow);

      return NS_OK;
    }
    info = info->mYounger;
    listEnd = mOldestWindow;
  }
  return NS_ERROR_FAILURE; 
}


NS_IMETHODIMP  nsWindowMediator::UpdateWindowTitle( nsIXULWindow* inWindow, const PRUnichar* inTitle )
{
  nsWindowInfo *info,
               *listEnd;
  nsresult     rv;

  nsAutoLock lock(mListLock);
  for (info = mOldestWindow, listEnd = 0;
       info != listEnd;
       info = info->mYounger, listEnd = mOldestWindow) {

    if (info->mWindow.get() != inWindow)
      continue;

    nsIRDFResource* window = info->mRDFID;
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
    nsAutoString displayString;
    GetAttribute( inWindow, NS_LITERAL_STRING("intaskslist"), displayString );
    displayString.ToLowerCase();

    if ( displayString.Equals(NS_LITERAL_STRING("false")) )
      display=PR_FALSE;

    rv = Assert( window , kNC_Name, newTitle, display );
    if (rv != NS_RDF_ASSERTION_ACCEPTED)
    {
      NS_ERROR("unable to set window name");
    }
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

/* This method's plan is to intervene only when absolutely necessary.
   We will get requests to place our windows behind unknown windows.
   For the most part, we need to leave those alone (turning them into
   explicit requests to be on top breaks Windows.) So generally we
   calculate a change as seldom as possible.
*/
NS_IMETHODIMP nsWindowMediator::CalculateZPosition(
                nsIXULWindow   *inWindow,
                PRUint32        inPosition,
                nsIWidget      *inBelow,
                PRUint32       *outPosition,
                nsIWidget     **outBelow,
                PRBool         *outAltered) {

  if (!outBelow)
    return NS_ERROR_NULL_POINTER;

  *outBelow = 0;

  if (!inWindow || !outPosition || !outAltered)
    return NS_ERROR_NULL_POINTER;

  if (inPosition != nsIWindowMediator::zLevelTop &&
      inPosition != nsIWindowMediator::zLevelBottom &&
      inPosition != nsIWindowMediator::zLevelBelow)
// || inPosition == nsIWindowMediator::zLevelBelow && !inBelow

    return NS_ERROR_INVALID_ARG;

  nsWindowInfo *info;
  nsIXULWindow *belowWindow = 0;
  PRBool        found;
  nsresult      result = NS_OK;
  
  PRUint32      inZ;
  inWindow->GetZlevel(&inZ);

  *outPosition = inPosition;
  *outAltered = PR_FALSE;

  nsAutoLock lock(mListLock);

  if (inPosition == nsIWindowMediator::zLevelBelow) {

    // locate inBelow. it had better be in the z-list if it's a valid window.
    info = mTopmostWindow;
    found = PR_FALSE;
    if (inBelow && info)
      do {
        nsCOMPtr<nsIWidget> scanWidget;
        nsCOMPtr<nsIBaseWindow> base(do_QueryInterface(info->mWindow));
        if (base)
          base->GetMainWidget(getter_AddRefs(scanWidget));
      
        if (inBelow == scanWidget.get()) {
          found = PR_TRUE;
          break;
        }
        info = info->mLower;
      } while (info != mTopmostWindow);

    if (!found) {
      /* Treat unknown windows as a request to be on top.
         Not as it should be, but that's what Windows gives us.
         Note we change inPosition, but not *outPosition. This forces
         us to go through the "on top" calculation just below, without
         necessarily changing the output parameters. */
      inPosition = nsIWindowMediator::zLevelTop;
    }
  }

  if (inPosition == nsIWindowMediator::zLevelTop) {
    if (mTopmostWindow && GetWindowZ(mTopmostWindow->mWindow) > inZ) {

      // asked for topmost, can't have it. locate highest allowed position.
      info = mTopmostWindow;
      do {
        if (GetWindowZ(info->mWindow) <= inZ)
          break;
        info = info->mLower;
      } while (info != mTopmostWindow);

      *outPosition = nsIWindowMediator::zLevelBelow;
      belowWindow = info->mHigher->mWindow;
      *outAltered = PR_TRUE;

    }
  } else if (inPosition == nsIWindowMediator::zLevelBottom) {
    if (mTopmostWindow && GetWindowZ(mTopmostWindow->mHigher->mWindow) < inZ) {

      // asked for bottommost, can't have it. locate lowest allowed position.
      info = mTopmostWindow;
      do {
        info = info->mHigher;
        if (GetWindowZ(info->mWindow) >= inZ)
          break;
      } while (info != mTopmostWindow);

      *outPosition = nsIWindowMediator::zLevelBelow;
      belowWindow = info->mWindow;
      *outAltered = PR_TRUE;
    }
  } else {

    unsigned long relativeZ;

    // check that we're in the right z-plane
    if (found) {
      belowWindow = info->mWindow;
      relativeZ = GetWindowZ(belowWindow);
      if (relativeZ > inZ) {

        // might be OK. is lower window, if any, lower?
        if (info->mLower != info && GetWindowZ(info->mLower->mWindow) > inZ) {

          do {
            if (GetWindowZ(info->mWindow) <= inZ)
              break;
            info = info->mLower;
          } while (info != mTopmostWindow);

          belowWindow = info->mHigher->mWindow;
          *outAltered = PR_TRUE;
        }
      } else if (relativeZ < inZ) {

        // nope. look for a higher window to be behind.
        do {
          info = info->mHigher;
          if (GetWindowZ(info->mWindow) >= inZ)
            break;
        } while (info != mTopmostWindow);

        if (GetWindowZ(info->mWindow) >= inZ)
          belowWindow = info->mWindow;
        else
          *outPosition = nsIWindowMediator::zLevelTop;
        *outAltered = PR_TRUE;

      } // else they're equal, so it's OK
    }
  }

  if (NS_SUCCEEDED(result) && belowWindow) {
    nsCOMPtr<nsIBaseWindow> base(do_QueryInterface(belowWindow));
    if (base)
      base->GetMainWidget(outBelow);
    else
      result = NS_ERROR_NO_INTERFACE;
  }

  return result;
}

NS_IMETHODIMP nsWindowMediator::SetZPosition(
                nsIXULWindow *inWindow,
                PRUint32      inPosition,
                nsIXULWindow *inBelow) {

  nsWindowInfo *inInfo,
               *belowInfo,
               *listEnd;

  if (inPosition != nsIWindowMediator::zLevelTop &&
      inPosition != nsIWindowMediator::zLevelBottom &&
      inPosition != nsIWindowMediator::zLevelBelow ||

      !inWindow)

//    inPosition == nsIWindowMediator::zLevelBelow && !inBelow)

    return NS_ERROR_INVALID_ARG;

  nsAutoLock lock(mListLock);

  /* locate inWindow and unlink it from the z-order list
     notice we look for it in the age list, not the z-order list.
     this is because the former is guaranteed complete, while
     now may be this window's first exposure to the latter. */
  inInfo = mOldestWindow;
  listEnd = 0;
  while (inInfo != listEnd) {
    if (inWindow == inInfo->mWindow.get())
      break;
    inInfo = inInfo->mYounger;
    listEnd = mOldestWindow;
  }
  if (inInfo == listEnd)
    return NS_ERROR_INVALID_ARG;

  /* locate inBelow, place inWindow behind it. inBelow, if given,
     had better be in the z-order list. */
  if (inPosition == nsIWindowMediator::zLevelBelow) {
    belowInfo = mTopmostWindow;
    listEnd = 0;
    while (belowInfo != listEnd) {
      if (inBelow == belowInfo->mWindow.get())
        break;
      belowInfo = belowInfo->mLower;
      listEnd = mTopmostWindow;
    }
    if (belowInfo == listEnd)
      if (inBelow)
        return NS_ERROR_INVALID_ARG;
      else
        inPosition = nsIWindowMediator::zLevelTop;
  }
  if (inPosition == nsIWindowMediator::zLevelTop ||
      inPosition == nsIWindowMediator::zLevelBottom)
    belowInfo = mTopmostWindow ? mTopmostWindow->mHigher : 0;

  if (inInfo != belowInfo) {
    inInfo->Unlink( PR_FALSE, PR_TRUE );
    inInfo->InsertAfter( 0, belowInfo );
  }
  if (inPosition == nsIWindowMediator::zLevelTop)
    mTopmostWindow = inInfo;

  return NS_OK;
}

NS_IMETHODIMP  nsWindowMediator::GetWindowForResource( const PRUnichar* inResource, nsIDOMWindowInternal** outWindow )
{
	if ( outWindow == NULL )
		return NS_ERROR_INVALID_ARG;
	*outWindow = NULL;

	// Find the window
	//nsresult result = NS_ERROR_FAILURE;
	nsAutoString temp( inResource );
	char* resourceString = temp.ToNewCString();
	nsWindowInfo *info,
	             *listEnd;

	nsAutoLock lock(mListLock);
	info = mOldestWindow;
	listEnd = 0;
	while (info != listEnd)
	{
		PRBool isMatch = PR_FALSE;

		if ( NS_SUCCEEDED( info->mRDFID->EqualsString( resourceString, &isMatch) ) && isMatch ) 
		{
			nsCOMPtr <nsIDOMWindowInternal> DOMWindow;
			if( NS_SUCCEEDED ( GetDOMWindow( info->mWindow, DOMWindow  ) ) )
			{	
        *outWindow = DOMWindow;
        NS_ADDREF(*outWindow);
			}
			break;
		}

		info = info->mYounger;
		listEnd = mOldestWindow;
	}

	delete resourceString;
	// Rather than returning an error when you pass in crap, return NS_OK
	// Doing this to avoid throwing a JS exception becasue event bubbling is not working as of 7/26/99
	//return result;
	return NS_OK;
}

NS_IMETHODIMP nsWindowMediator::ConvertISupportsToDOMWindow( nsISupports* inInterface, nsIDOMWindowInternal** outWindow )
{
   return inInterface->QueryInterface(NS_GET_IID(nsIDOMWindowInternal)  , (void**) outWindow );
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

  if (gRefCnt++ == 0) {

    mListLock = PR_NewLock();
    if (!mListLock)
      return NS_ERROR_OUT_OF_MEMORY;

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
      return rv;

    nsCOMPtr<nsIRDFContainerUtils> rdfc = 
             do_GetService(kRDFContainerUtilsCID, &rv);
    if (NS_FAILED(rv))
      return rv;

    rv = rdfc->MakeSeq(mInner, kNC_WindowMediatorRoot, NULL );
    NS_ASSERTION(NS_SUCCEEDED(rv), "Unable to make NC:WindowMediatorRoot a sequence");
    if (NS_FAILED(rv))
      return rv;
    if ( gRDFService == NULL )
      return NS_ERROR_NULL_POINTER;

    mWatcher = do_GetService("@mozilla.org/embedcomp/window-watcher;1", &rv);
    if (NS_FAILED(rv))
      return rv;

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
	nsAutoString uniqueID(NS_LITERAL_STRING("window-"));
	uniqueID.Append(windowTitle );
	uniqueID.AppendInt( mTimeStamp, 10 );
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
	if (NS_FAILED(rv = gRDFService->GetLiteral( windowTitle.get(), getter_AddRefs(windowTitleLiteral))))
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
  rv = NS_NewMediatorListRDFContainer(mInner, kNC_WindowMediatorRoot, getter_AddRefs(container));
  if (NS_FAILED(rv))
  	return rv;

	if (NS_FAILED(rv = container->AppendElement( window ) ) /* rdf_ContainerAppendElement(mInner, kNC_WindowMediatorRoot, window) */ )
	{
		NS_ERROR("unable to add window to container");
		return rv;
	}
	return NS_OK;
}

