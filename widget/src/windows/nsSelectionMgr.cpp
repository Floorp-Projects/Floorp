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
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nsSelectionMgr.h"

#include <strstrea.h>
#include <windows.h>
#include "nscore.h"
#include "nsString.h"


// XXX BWEEP BWEEP This is ONLY TEMPORARY until the service manager
// has a way of registering instances
// (see http://bugzilla.mozilla.org/show_bug.cgi?id=3509 ).
nsISelectionMgr* theSelectionMgr = 0;

extern "C" NS_EXPORT nsISelectionMgr*
GetSelectionMgr()
{
  if (!theSelectionMgr)
  {
    nsresult result = NS_NewSelectionMgr(&theSelectionMgr);
    NS_ASSERTION(NS_SUCCEEDED(result),"Could not allocate the selection manager");
  }
  return theSelectionMgr;
}
// BWEEP BWEEP

/**
 * Selection Manager for Windows.
 * Owns the copied text, listens for selection request events.
 */

NS_IMPL_ADDREF(nsSelectionMgr)
NS_IMPL_RELEASE(nsSelectionMgr)

nsSelectionMgr::nsSelectionMgr()
{
  NS_INIT_REFCNT();

  mCopyStream = 0;

  // BWEEP see above
  theSelectionMgr = this;
}

nsSelectionMgr::~nsSelectionMgr()
{
}

nsresult nsSelectionMgr::QueryInterface(const nsIID& aIID,
                                        void** aInstancePtrResult)
{
  NS_PRECONDITION(aInstancePtrResult, "null pointer");
  if (!aInstancePtrResult) 
  {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(nsISupports::GetIID())) 
  {
    *aInstancePtrResult = (void*)(nsISupports*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(nsISelectionMgr::GetIID())) 
  {
    *aInstancePtrResult = (void*)(nsISelectionMgr*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return !NS_OK;
}

nsresult nsSelectionMgr::GetCopyOStream(ostream** aStream)
{
  if (mCopyStream)
    delete mCopyStream;
  mCopyStream = new ostrstream;
  *aStream = mCopyStream;
  return NS_OK;
}


static const char* gsAOLFormat = "AOLMAIL";
static const char* gsHTMLFormat = "text/html";

static void PlaceHTMLOnClipboard(PRUint32 aFormat, char* aData, int aLength)
{
  HGLOBAL     hGlobalMemory;
  PSTR        pGlobalMemory;

  PRUint32    cf_aol = RegisterClipboardFormat(gsAOLFormat);
  PRUint32    cf_html = RegisterClipboardFormat(gsHTMLFormat);

  char*       preamble = "";
  char*       postamble = "";

  if (aFormat == cf_aol)
  {
    preamble = "<HTML>";
    postamble = "</HTML>";
  }

  PRInt32 size = aLength + 1 + strlen(preamble) + strlen(postamble);


  if (aLength)
  {
    // Copy text to Global Memory Area
    hGlobalMemory = (HGLOBAL)GlobalAlloc(GHND, size);
    if (hGlobalMemory != NULL) 
    {
      pGlobalMemory = (PSTR) GlobalLock(hGlobalMemory);

      int i;

      // AOL requires HTML prefix/postamble
      char*     s  = preamble;
      PRInt32   len = strlen(s); 
      for (i=0; i < len; i++)
      {
	*pGlobalMemory++ = *s++;
      }

      s  = aData;
      len = aLength;
      for (i=0;i< len;i++) {
	*pGlobalMemory++ = *s++;
      }


      s = postamble;
      len = strlen(s); 
      for (i=0; i < len; i++)
      {
	*pGlobalMemory++ = *s++;
      }
      
      // Put data on Clipboard
      GlobalUnlock(hGlobalMemory);
      SetClipboardData(aFormat, hGlobalMemory);
    }
  }  
}

nsresult nsSelectionMgr::CopyToClipboard()
{
  // we'd better already have a stream ...
  if (!mCopyStream)
      return NS_ERROR_NOT_INITIALIZED;

  PRInt32 len = mCopyStream->pcount();
  char* str = (char*)mCopyStream->str();

  PRUint32 cf_aol = RegisterClipboardFormat(gsAOLFormat);
  PRUint32 cf_html = RegisterClipboardFormat(gsHTMLFormat);
	 
  if (len)
  {   
    OpenClipboard(NULL);
    EmptyClipboard();
	
    PlaceHTMLOnClipboard(cf_aol,str,len);
    PlaceHTMLOnClipboard(cf_html,str,len);
    PlaceHTMLOnClipboard(CF_TEXT,str,len);            
			
    CloseClipboard();
  }
  // in ostrstreams if you cal the str() function
  // then you are responsible for deleting the string
  if (str) delete str;

  delete mCopyStream;
  mCopyStream = 0;
  return NS_OK;
}

nsresult nsSelectionMgr::PasteTextBlocking(nsString* aPastedText)
{
  HGLOBAL   hglb; 
  LPSTR     lpstr; 
  nsresult  result = NS_ERROR_FAILURE;

  if (aPastedText && OpenClipboard(NULL))
  {
    // Just Grab TEXT for now, later we will grab HTML, XIF, etc.
    hglb = GetClipboardData(CF_TEXT); 
    if (hglb != NULL)
    {
      lpstr = (LPSTR)GlobalLock(hglb);
      aPastedText->SetString((char*)lpstr);
      GlobalUnlock(hglb);
      result = NS_OK;
    }
    CloseClipboard();
  }
  return result;
}

nsresult NS_NewSelectionMgr(nsISelectionMgr** aInstancePtrResult)
{
  nsSelectionMgr* sm = new nsSelectionMgr();
  return sm->QueryInterface(nsISelectionMgr::GetIID(),
                            (void**) aInstancePtrResult);
}


