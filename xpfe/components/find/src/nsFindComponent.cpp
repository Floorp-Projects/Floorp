/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
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
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

#include "nsIFindComponent.h"

#include "nsCOMPtr.h"
#include "nsString.h"
#include "pratom.h"

#include "nsIAppShellService.h"
#include "nsIDOMDocument.h"
#include "nsIDocument.h"
#include "nsITextServicesDocument.h"
#include "nsTextServicesCID.h"
#include "nsIDocShell.h"
#include "nsIPresShell.h"
#include "nsIContent.h"
#include "nsIURL.h"
#include "nsIURL.h"
#include "nsIFactory.h"
#include "nsIServiceManager.h"
#include "nsIDOMWindowInternal.h"
#include "nsIScriptGlobalObject.h"

#include "nsISound.h"
#include "nsWidgetsCID.h"   // ugh! progID, please

#include "nsFindComponent.h"


#ifdef DEBUG
#define DEBUG_FIND
#endif


nsFindComponent::Context::Context()
{
  NS_INIT_REFCNT();
	// all our other members are self-initiating
}


nsFindComponent::Context::~Context()
{
    #ifdef DEBUG_law
    printf( "\nnsFindComponent::Context destructor called\n\n" );
    #endif
    // Close the dialog (if there is one).
    if ( mFindDialog ) {
        mFindDialog->Close();
        mFindDialog = 0;
    }
}

NS_IMETHODIMP
nsFindComponent::Context::Init( nsIDOMWindowInternal *aWindow,
                 nsIEditorShell* aEditorShell,
                 const nsString& lastSearchString,
                 const nsString& lastReplaceString,
                 PRBool lastCaseSensitive,
                 PRBool lastSearchBackward,
                 PRBool lastWrapSearch)
{
	if (!aWindow)
		return NS_ERROR_INVALID_ARG;
	
	mEditorShell     = aEditorShell;				// don't AddRef
	mTargetWindow    = aWindow;			// don't AddRef
	mSearchString    = lastSearchString;
	mReplaceString   = lastReplaceString;
	mCaseSensitive   = lastCaseSensitive;
	mSearchBackwards = lastSearchBackward;
	mWrapSearch      = lastWrapSearch;
	mFindDialog      = 0;
	
	return NS_OK;
}


static NS_DEFINE_CID(kCTextServicesDocumentCID, NS_TEXTSERVICESDOCUMENT_CID);


NS_IMETHODIMP
nsFindComponent::Context::MakeTSDocument(nsIDOMWindowInternal* aWindow, nsITextServicesDocument** aDoc)
{
  if (!aWindow)
    return NS_ERROR_INVALID_ARG;
    
  if (!aDoc)
    return NS_ERROR_NULL_POINTER;

  *aDoc = NULL;
  
  // Create the text services document.
  nsCOMPtr<nsITextServicesDocument>  tempDoc;
  nsresult rv = nsComponentManager::CreateInstance(kCTextServicesDocumentCID,
                                                      nsnull,
                                                      NS_GET_IID(nsITextServicesDocument),
                                                      getter_AddRefs(tempDoc));
  if (NS_FAILED(rv) || !tempDoc)
    return rv;

  nsCOMPtr<nsIScriptGlobalObject> globalObj = do_QueryInterface(aWindow, &rv);
  if (NS_FAILED(rv) || !globalObj)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDocShell> docShell;
  globalObj->GetDocShell(getter_AddRefs(docShell));
  if (!docShell)
    return NS_ERROR_FAILURE;
	
	nsCOMPtr<nsIPresShell> presShell;
	docShell->GetPresShell(getter_AddRefs(presShell));
  if (!presShell)
    return NS_ERROR_FAILURE;

	nsCOMPtr<nsIDocument> document;
	presShell->GetDocument(getter_AddRefs(document));
	nsCOMPtr<nsIDOMDocument> domDoc = do_QueryInterface(document);
  if (!domDoc)
    return NS_ERROR_FAILURE;

  // Initialize the text services document.
  rv = tempDoc->InitWithDocument(domDoc, presShell);
  if (NS_FAILED(rv))
    return rv;

  // Return the resulting text services document.
  *aDoc = tempDoc;
  NS_IF_ADDREF(*aDoc);
  
  return rv;
}

// ----------------------------------------------------------------
//	CharsMatch
//
//	Compare chars. Match if both are whitespace, or both are
//	non whitespace and same char.
// ----------------------------------------------------------------

inline static PRBool CharsMatch(PRUnichar c1, PRUnichar c2)
{
	return (nsCRT::IsAsciiSpace(c1) && nsCRT::IsAsciiSpace(c2)) ||
						(c1 == c2);
	
}


// ----------------------------------------------------------------
//	FindInString
//
//	Routine to search in an nsString which is smart about extra
//  whitespace, can search backwards, and do case insensitive search.
//
//	This uses a brute-force algorithm, which should be sufficient
//	for our purposes (text searching)
// 
//	searchStr contains the text from a content node, which can contain
//	extra white space between words, which we have to deal with.
//	The offsets passed in and back are offsets into searchStr,
//	and thus include extra white space.
//
//	If we are ignoring case, the strings have already been lowercased
// 	at this point.
//
//  startOffset is the offset in the search string to start seraching
//  at. If -1, it means search from the start (forwards) or end (backwards).
//
//	Returns -1 if the string is not found, or if the pattern is an
//	empty string, or if startOffset is off the end of the string.
// ----------------------------------------------------------------

static PRInt32 FindInString(const nsString &searchStr, const nsString &patternStr,
						PRInt32 startOffset, PRBool searchBackwards)
{
	PRInt32		foundOffset = -1;
	PRInt32		patternLen = patternStr.Length();
	PRInt32		searchStrLen = searchStr.Length();
		
	if (patternLen == 0)									// pattern is empty
		return -1;
	
	if (startOffset < 0)
		startOffset = (searchBackwards) ? searchStrLen : 0;
	
	if (startOffset > searchStrLen)			// bad start offset
		return -1;
	
	if (patternLen > searchStrLen)				// pattern is longer than string to search
		return -1;
	
	const PRUnichar	*searchBuf = searchStr.GetUnicode();
	const PRUnichar	*patternBuf = patternStr.GetUnicode();

	const PRUnichar	*searchEnd = searchBuf + searchStrLen;
	const PRUnichar	*patEnd = patternBuf + patternLen;
	
	if (searchBackwards)
	{
		// searching backwards
		const PRUnichar	*s = searchBuf + startOffset - patternLen - 1;
	
		while (s >= searchBuf)
		{
			if (CharsMatch(*patternBuf, *s))			// start potential match
			{
				const PRUnichar	*t = s;
				const PRUnichar	*p = patternBuf;
				PRInt32		curMatchOffset = t - searchBuf;
				PRBool		inWhitespace = nsCRT::IsAsciiSpace(*p);
				
				while (p < patEnd && CharsMatch(*p, *t))
				{
					if (inWhitespace && !nsCRT::IsAsciiSpace(*p))
					{
						// leaving p whitespace. Eat up addition whitespace in s
						while (t < searchEnd - 1 && nsCRT::IsAsciiSpace(*(t + 1)))
							t ++;
							
						inWhitespace = PR_FALSE;
					}
					else
						inWhitespace = nsCRT::IsAsciiSpace(*p);

					t ++;
					p ++;
				}
				
				if (p == patEnd)
				{
					foundOffset = curMatchOffset;
					goto done;
				}
				
				// could be smart about decrementing s here
			}
		
			s --;
		}
	
	}
	else
	{
		// searching forwards
		
		const PRUnichar	*s = &searchBuf[startOffset];
	
		while (s < searchEnd)
		{
			if (CharsMatch(*patternBuf, *s))			// start potential match
			{
				const PRUnichar	*t = s;
				const PRUnichar	*p = patternBuf;
				PRInt32		curMatchOffset = t - searchBuf;
				PRBool		inWhitespace = nsCRT::IsAsciiSpace(*p);
				
				while (p < patEnd && CharsMatch(*p, *t))
				{
					if (inWhitespace && !nsCRT::IsAsciiSpace(*p))
					{
						// leaving p whitespace. Eat up addition whitespace in s
						while (t < searchEnd - 1 && nsCRT::IsAsciiSpace(*(t + 1)))
							t ++;
							
						inWhitespace = PR_FALSE;
					}
					else
						inWhitespace = nsCRT::IsAsciiSpace(*p);

					t ++;
					p ++;
				}
				
				if (p == patEnd)
				{
					foundOffset = curMatchOffset;
					goto done;
				}
				
				// could be smart about incrementing s here
			}
			
			s ++;
		}
	
	}

done:
	return foundOffset;
}

// utility method to discover which block we're in. The TSDoc interface doesn't give
// us this, because it can't assume a read-only document.
NS_IMETHODIMP
nsFindComponent::Context::GetCurrentBlockIndex(nsITextServicesDocument *aDoc, PRInt32 *outBlockIndex)
{
  PRInt32  blockIndex = 0;
  PRBool   isDone = PR_FALSE;
  
  while (NS_SUCCEEDED(aDoc->IsDone(&isDone)) && !isDone)
  {
    aDoc->PrevBlock();
    blockIndex ++;
  }
  
  *outBlockIndex = blockIndex;
  return NS_OK;
}
    
NS_IMETHODIMP
nsFindComponent::Context::SetupDocForSearch(nsITextServicesDocument *aDoc, PRInt32 *outBlockOffset)
{
  nsresult  rv;
  
  nsITextServicesDocument::TSDBlockSelectionStatus blockStatus;
  PRInt32 selOffset;
  PRInt32 selLength;
  
  if (!mSearchBackwards)	// searching forwards
  {
    rv = aDoc->LastSelectedBlock(&blockStatus, &selOffset, &selLength);
    if (NS_SUCCEEDED(rv) && (blockStatus != nsITextServicesDocument::eBlockNotFound))
    {
      switch (blockStatus)
      {
        case nsITextServicesDocument::eBlockOutside:		// No TB in S, but found one before/after S.
        case nsITextServicesDocument::eBlockPartial:		// S begins or ends in TB but extends outside of TB.
          // the TS doc points to the block we want.
          *outBlockOffset = selOffset + selLength;
          break;
                    
        case nsITextServicesDocument::eBlockInside:			// S extends beyond the start and end of TB.
          // we want the block after this one.
          rv = aDoc->NextBlock();
          *outBlockOffset = 0;
          break;
                
        case nsITextServicesDocument::eBlockContains:		// TB contains entire S.
          *outBlockOffset = selOffset + selLength;
          break;
        
        case nsITextServicesDocument::eBlockNotFound:		// There is no text block (TB) in or before the selection (S).
        default:
            NS_NOTREACHED("Shouldn't ever get this status");
      }
    
    }
    else		//failed to get last sel block. Just start at beginning
    {
      rv = aDoc->FirstBlock();
    }
  
  }
  else			// searching backwards
  {
    rv = aDoc->FirstSelectedBlock(&blockStatus, &selOffset, &selLength);
		if (NS_SUCCEEDED(rv) && (blockStatus != nsITextServicesDocument::eBlockNotFound))
		{
		  switch (blockStatus)
		  {
		    case nsITextServicesDocument::eBlockOutside:		// No TB in S, but found one before/after S.
		    case nsITextServicesDocument::eBlockPartial:		// S begins or ends in TB but extends outside of TB.
		      // the TS doc points to the block we want.
		      *outBlockOffset = selOffset;
		      break;
		                
		    case nsITextServicesDocument::eBlockInside:			// S extends beyond the start and end of TB.
		      // we want the block before this one.
		      rv = aDoc->PrevBlock();
		      *outBlockOffset = -1;
		      break;
		            
		    case nsITextServicesDocument::eBlockContains:		// TB contains entire S.
		      *outBlockOffset = selOffset;
		      break;
		    
		    case nsITextServicesDocument::eBlockNotFound:		// There is no text block (TB) in or before the selection (S).
		    default:
		        NS_NOTREACHED("Shouldn't ever get this status");
		  }
		}
		else
		{
		  rv = aDoc->LastBlock();
		}
  }

  return rv;
}


NS_IMETHODIMP
nsFindComponent::Context::DoFind(PRBool *aDidFind)
{
  if (!aDidFind)
    return NS_ERROR_NULL_POINTER;
  
	if (!mTargetWindow)
		return NS_ERROR_NOT_INITIALIZED;

	*aDidFind = PR_FALSE;
	
	nsAutoString		matchString(mSearchString);
	if (!mCaseSensitive)
		matchString.ToLowerCase();
	
  nsresult	rv = NS_OK;

  // Construct a text services document to use. This is freed when we
  // return from this function.
  nsCOMPtr<nsITextServicesDocument> txtDoc;
  rv = MakeTSDocument(mTargetWindow, getter_AddRefs(txtDoc));
  if (NS_FAILED(rv) || !txtDoc)
    return rv;
  
  // Set up the TSDoc. We are going to start searching thus:
  // 
  // Searching forwards:
  //        Look forward from the end of the selection
  // Searching backwards:
  //        Look backwards from the start of the selection
  //
  PRInt32  selOffset = 0;
  rv = SetupDocForSearch(txtDoc, &selOffset);
  if (NS_FAILED(rv))
    return rv;  
  
  // find out where we started
  PRInt32  blockIndex;
  rv = GetCurrentBlockIndex(txtDoc, &blockIndex);
  if (NS_FAILED(rv))
    return rv;

  // remember where we started
  PRInt32  startingBlockIndex = blockIndex;
  
  // and set the starting position again (hopefully, in future we won't have to do this)
  rv = SetupDocForSearch(txtDoc, &selOffset);
  if (NS_FAILED(rv))
    return rv;  
    
  PRBool wrappedOnce = PR_FALSE;	// Remember whether we've already wrapped
	PRBool done = PR_FALSE;
	
  // Loop till we find a match or fail.
  while ( !done )
  {
      PRBool atExtremum = PR_FALSE;		// are we at the end (or start)

      while ( NS_SUCCEEDED(txtDoc->IsDone(&atExtremum)) && !atExtremum )
      {
          nsString str;
          rv = txtDoc->GetCurrentTextBlock(&str);
  
          if (NS_FAILED(rv))
              return rv;
  
          if (!mCaseSensitive)
              str.ToLowerCase();
          
          PRInt32 foundOffset = FindInString(str, matchString, selOffset, mSearchBackwards);
          selOffset = -1;			// reset for next block
  
          if (foundOffset != -1)
          {
              // Match found.  Select it, remember where it was, and quit.
              txtDoc->SetSelection(foundOffset, mSearchString.Length());
              txtDoc->ScrollSelectionIntoView();
              done = PR_TRUE;
             	*aDidFind = PR_TRUE;
              break;
          }
          else
          {
              // have we already been around once?
              if (wrappedOnce && (blockIndex == startingBlockIndex))
              {
                done = PR_TRUE;
                break;
              }

              // No match found in this block, try the next (or previous) one.
              if (mSearchBackwards) {
                  txtDoc->PrevBlock();
                  blockIndex--;
              } else {
                  txtDoc->NextBlock();
                  blockIndex++;
              }
          }
          
      } // while !atExtremum
      
      // At end (or matched).  Decide which it was...
      if (!done)
      {
          // Hit end without a match.  If we haven't passed this way already,
          // then reset to the first/last block (depending on search direction).
          if (!wrappedOnce)
          {
              // Reset now.
              wrappedOnce = PR_TRUE;
              // If not wrapping, give up.
              if ( !mWrapSearch ) {
                  done = PR_TRUE;
              }
              else
              {
                  if ( mSearchBackwards ) {
                      // Reset to last block.
                      rv = txtDoc->LastBlock();
                      // ugh
                      rv = GetCurrentBlockIndex(txtDoc, &blockIndex);
                      rv = txtDoc->LastBlock();
                  } else {
                      // Reset to first block.
                      rv = txtDoc->FirstBlock();
                      blockIndex = 0;
                  }
              }
          }
          else
          {
              // already wrapped.  This means no matches were found.
              done = PR_TRUE;
          }
      }
  }

	return NS_OK;
}


NS_IMETHODIMP
nsFindComponent::Context::DoReplace()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsFindComponent::Context::Reset( nsIDOMWindowInternal *aNewWindow )
{
	if (!aNewWindow)
		return NS_ERROR_INVALID_ARG;

  mTargetWindow = aNewWindow;		// don't AddRef
  return NS_OK;
}

NS_IMETHODIMP
nsFindComponent::Context::GetSearchString(PRUnichar * *aSearchString) {
    nsresult rv = NS_OK;
    if ( aSearchString ) {
        *aSearchString = mSearchString.ToNewUnicode();
        if ( !*aSearchString ) {
            rv = NS_ERROR_OUT_OF_MEMORY;
        }
    } else {
        rv = NS_ERROR_NULL_POINTER;
    }
    return rv;
}

NS_IMETHODIMP
nsFindComponent::Context::SetSearchString(const PRUnichar *aSearchString) {
    nsresult rv = NS_OK;
    mSearchString = aSearchString ? nsString( aSearchString ) : nsString();
    return rv;
}

NS_IMETHODIMP
nsFindComponent::Context::GetReplaceString(PRUnichar * *aReplaceString) {
    nsresult rv = NS_OK;
    if ( aReplaceString ) {
        *aReplaceString = mReplaceString.ToNewUnicode();
        if ( !*aReplaceString ) {
            rv = NS_ERROR_OUT_OF_MEMORY;
        }
    } else {
        rv = NS_ERROR_NULL_POINTER;
    }
    return rv;
}

NS_IMETHODIMP
nsFindComponent::Context::SetReplaceString(const PRUnichar *aReplaceString) {
    nsresult rv = NS_OK;
    mReplaceString = aReplaceString ? nsString( aReplaceString ) : nsString();
    return rv;
}

NS_IMETHODIMP
nsFindComponent::Context::GetSearchBackwards(PRBool *aBool) {
    nsresult rv = NS_OK;
    if ( aBool ) {
        *aBool = mSearchBackwards;
    } else {
        rv = NS_ERROR_NULL_POINTER;
    }
    return rv;
}

NS_IMETHODIMP
nsFindComponent::Context::SetSearchBackwards(PRBool aBool) {
    nsresult rv = NS_OK;
    mSearchBackwards = aBool;
    return rv;
}

NS_IMETHODIMP
nsFindComponent::Context::GetCaseSensitive(PRBool *aBool) {
    nsresult rv = NS_OK;
    if ( aBool ) {
        *aBool = mCaseSensitive;
    } else {
        rv = NS_ERROR_NULL_POINTER;
    }
    return rv;
}

NS_IMETHODIMP
nsFindComponent::Context::SetCaseSensitive(PRBool aBool) {
    nsresult rv = NS_OK;
    mCaseSensitive = aBool;
    return rv;
}

NS_IMETHODIMP 
nsFindComponent::Context::GetWrapSearch(PRBool *aBool) {
    nsresult rv = NS_OK;
    if ( aBool ) {
        *aBool = mWrapSearch;
    } else {
        rv = NS_ERROR_NULL_POINTER;
    }
    return rv;
}

NS_IMETHODIMP
nsFindComponent::Context::SetWrapSearch(PRBool aBool)
{
    nsresult rv = NS_OK;
    mWrapSearch = aBool;
    return rv;
}

NS_IMETHODIMP
nsFindComponent::Context::GetTargetWindow( nsIDOMWindowInternal * *aWindow)
{
  NS_ENSURE_ARG_POINTER(aWindow);
  NS_IF_ADDREF(*aWindow = mTargetWindow);
  return NS_OK;
}

NS_IMETHODIMP
nsFindComponent::Context::GetFindDialog( nsIDOMWindowInternal  * *aDialog)
{
  NS_ENSURE_ARG_POINTER(aDialog);
  NS_IF_ADDREF(*aDialog = mFindDialog);
  return NS_OK;
}

NS_IMETHODIMP
nsFindComponent::Context::SetFindDialog( nsIDOMWindowInternal *aDialog )
{
  mFindDialog = aDialog;
  return NS_OK;
}

#ifdef XP_MAC
#pragma mark -
#endif

// ctor
nsFindComponent::nsFindComponent()
    : mLastSearchString(),
      mLastCaseSensitive( PR_TRUE ),
      mLastSearchBackwards( PR_FALSE ),
      mLastWrapSearch( PR_FALSE )
{
    NS_INIT_REFCNT();

    // Initialize "last" stuff from prefs, if we wanted to be really clever...
}

// dtor
nsFindComponent::~nsFindComponent()
{
}

NS_IMETHODIMP
nsFindComponent::CreateContext( nsIDOMWindowInternal *aWindow, nsIEditorShell* aEditorShell,
                                nsISupports **aResult )
{

    if (!aResult)
			return NS_ERROR_NULL_POINTER;
      
    // Construct a new Context with this document.
    Context		*newContext = new Context();
   	if (!newContext)
   		return NS_ERROR_OUT_OF_MEMORY;
   
     // Do the expected AddRef on behalf of caller.
    newContext->AddRef();

    nsresult	rv = newContext->Init( aWindow,
                        aEditorShell,
                        mLastSearchString,
                        mLastReplaceString,
                        mLastCaseSensitive,
                        mLastSearchBackwards,
                        mLastWrapSearch);
 		if (NS_FAILED(rv))
 		{
 			NS_RELEASE(newContext);
 			return rv;
 		}
 
		*aResult = newContext;
    return NS_OK;
}

static nsresult OpenDialogWithArg( nsIDOMWindowInternal     *parent,
                                   nsISearchContext *arg, 
                                   const char       *url ) {
    nsresult rv = NS_OK;

    if ( parent && arg && url ) {
        // Get JS context from parent window.
        nsCOMPtr<nsIScriptGlobalObject> sgo = do_QueryInterface( parent, &rv );
        if ( NS_SUCCEEDED( rv ) && sgo ) {
            nsCOMPtr<nsIScriptContext> context;
            sgo->GetContext( getter_AddRefs( context ) );
            if ( context ) {
                JSContext *jsContext = (JSContext*)context->GetNativeContext();
                if ( jsContext ) {
                    void *stackPtr;
                    jsval *argv = JS_PushArguments( jsContext,
                                                    &stackPtr,
                                                    "sss%ip",
                                                    url,
                                                    "_blank",
                                                    "chrome,resizable=no,dependent=yes",
                                                    (const nsIID*)(&NS_GET_IID(nsISearchContext)),
                                                    (nsISupports*)arg );
                    if ( argv ) {
                        nsIDOMWindowInternal *newWindow;
                        rv = parent->OpenDialog( jsContext, argv, 4, &newWindow );
                        if ( NS_SUCCEEDED( rv ) ) {
                            newWindow->Release();
                        } else {
                        }
                        JS_PopArguments( jsContext, stackPtr );
                    } else {
                        DEBUG_PRINTF( PR_STDOUT, "%s %d: JS_PushArguments failed\n",
                                      (char*)__FILE__, (int)__LINE__ );
                        rv = NS_ERROR_FAILURE;
                    }
                } else {
                    DEBUG_PRINTF( PR_STDOUT, "%s %d: GetNativeContext failed\n",
                                  (char*)__FILE__, (int)__LINE__ );
                    rv = NS_ERROR_FAILURE;
                }
            } else {
                DEBUG_PRINTF( PR_STDOUT, "%s %d: GetContext failed\n",
                              (char*)__FILE__, (int)__LINE__ );
                rv = NS_ERROR_FAILURE;
            }
        } else {
            DEBUG_PRINTF( PR_STDOUT, "%s %d: QueryInterface (for nsIScriptGlobalObject) failed, rv=0x%08X\n",
                          (char*)__FILE__, (int)__LINE__, (int)rv );
        }
    } else {
        DEBUG_PRINTF( PR_STDOUT, "%s %d: OpenDialogWithArg was passed a null pointer!\n",
                      __FILE__, (int)__LINE__ );
        rv = NS_ERROR_NULL_POINTER;
    }
    return rv;
}

NS_IMETHODIMP
nsFindComponent::Find(nsISupports *aContext, PRBool *aDidFind)
{
    nsresult rv = NS_OK;

    // See if find dialog is already up.
    if ( aContext ) {
        nsCOMPtr<nsISearchContext> context = do_QueryInterface( aContext, &rv );
        if ( NS_SUCCEEDED( rv ) && context ) {
            nsCOMPtr<nsIDOMWindowInternal> dialog;
            rv = context->GetFindDialog( getter_AddRefs( dialog ) );
            if ( NS_SUCCEEDED( rv ) && dialog ) {
                // Just give focus back to the dialog.
                dialog->Focus();
                return NS_OK;
            }
        }
        // Test for error (GetFindDialog succeeds if there's no dialog).
        if ( NS_FAILED( rv ) ) {
            DEBUG_PRINTF( PR_STDOUT, "%s %d: Error getting find dialog, rv=0x%08X\n",
                          __FILE__, (int)__LINE__, (int)rv );
            return rv;
        }
    }

    if (aContext && GetAppShell())
    {
        nsCOMPtr<nsISearchContext> context = do_QueryInterface( aContext, &rv );
        if (NS_FAILED(rv))
            return rv;

        // Open Find dialog and prompt for search parameters.
        char * urlStr = "chrome://global/content/finddialog.xul";

        // We need the parent's nsIDOMWindowInternal...
        // 1. Get topLevelWindow nsIWebShellContainer (chrome included).
        nsCOMPtr<nsIDOMWindowInternal> window;
        rv = context->GetTargetWindow( getter_AddRefs( window ) );
        if ( NS_SUCCEEDED( rv ) && window )
        {
        	nsCOMPtr<nsIDOMWindow> topWindow;
        	window->GetTop(getter_AddRefs(topWindow));
        	if (topWindow) {
              nsCOMPtr<nsIDOMWindowInternal> topInternal = do_QueryInterface(topWindow);
              rv = OpenDialogWithArg(topInternal, context, urlStr);
            }
        }
     } else {
        rv = NS_ERROR_NULL_POINTER;
    }

    return rv;
}

NS_IMETHODIMP
nsFindComponent::Replace( nsISupports *aContext )
{
	if (!aContext)
  	return NS_ERROR_NULL_POINTER;

	return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsFindComponent::FindNext(nsISupports *aContext, PRBool *aDidFind)
{
    nsresult rv = NS_OK;

		if (!aContext)
			return NS_ERROR_NULL_POINTER;
			
		Context *context = (Context*)aContext;

        // If we haven't searched yet, put up dialog (via Find).
        if ( context->mSearchString.IsEmpty() ) {
            return this->Find( aContext, aDidFind );
        }

		context->DoFind(aDidFind);

		// Record this for out-of-the-blue FindNext calls.
		mLastSearchString    = context->mSearchString;
		mLastCaseSensitive   = context->mCaseSensitive;
		mLastSearchBackwards = context->mSearchBackwards;
		mLastWrapSearch      = context->mWrapSearch;

    if (!*aDidFind)
    {
      static NS_DEFINE_IID(kSoundCID,       NS_SOUND_CID);
      nsCOMPtr<nsISound> sound = do_CreateInstance(kSoundCID);
      if (sound)
        sound->Beep();    
    }

    return rv;
}

NS_IMETHODIMP
nsFindComponent::ResetContext( nsISupports *aContext,
                               nsIDOMWindowInternal *aNewWindow,
                               nsIEditorShell* aEditorShell )
{
	NS_ENSURE_ARG(aContext);
	NS_ENSURE_ARG(aNewWindow);
	
	// Pass on the new document to the context.
	Context *context = (Context*)aContext;
	context->Reset(aNewWindow);

	return NS_OK;
}

// nsFindComponent::Context implementation...
NS_IMPL_ISUPPORTS( nsFindComponent::Context, NS_GET_IID(nsISearchContext) )

NS_IMPL_IAPPSHELLCOMPONENT( nsFindComponent, nsIFindComponent, NS_IFINDCOMPONENT_PROGID, 0 )
