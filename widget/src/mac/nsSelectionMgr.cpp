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

#include <sstream>
#include <string>
#include <Scrap.h>
#include <TextEdit.h>
#include "nscore.h"
#include "nsString.h"

NS_IMPL_ADDREF(nsSelectionMgr)
NS_IMPL_RELEASE(nsSelectionMgr)

// XXX BWEEP BWEEP This is ONLY TEMPORARY until the service manager
// has a way of registering instances
// (see http://bugzilla.mozilla.org/show_bug.cgi?id=3509 ).
nsISelectionMgr* theSelectionMgr = 0;

extern "C" NS_EXPORT nsISelectionMgr*
GetSelectionMgr()
{
  return theSelectionMgr;
}
// BWEEP BWEEP

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
  mCopyStream = new stringstream;
  *aStream = mCopyStream;
  return NS_OK;
}

nsresult nsSelectionMgr::CopyToClipboard()
{
  // we'd better already have a stream ...
  if (!mCopyStream)
      return NS_ERROR_NOT_INITIALIZED;

  string      theString = mCopyStream->str();
  PRInt32     len = theString.length();
  const char* str = theString.data();

  if (len)
  {
    char * ptr = NS_CONST_CAST(char*,str);
    for (PRInt32 plen = len; plen > 0; plen --, ptr ++)
      if (*ptr == '\n')
        *ptr = '\r';

    OSErr err = ::ZeroScrap();
    err = ::PutScrap(len, 'TEXT', str);
    ::TEFromScrap();
  }

  delete mCopyStream;
  mCopyStream = 0;
  return NS_OK;
}

nsresult nsSelectionMgr::PasteTextBlocking(nsString* aPastedText)
{
  if (!aPastedText)
    return NS_ERROR_NULL_POINTER;
  
  aPastedText->Truncate(0);
  
  long     scrapOffset = 0;
  Handle   destHandle = ::NewHandle(0);
  if (!destHandle) return NS_ERROR_OUT_OF_MEMORY;
  
  // Just Grab TEXT for now, later we will grab HTML, XIF, etc.
  long		scrapLen = ::GetScrap(destHandle, 'TEXT', &scrapOffset);
  if (scrapLen < 0)  // really an error, no text in scrap
  	goto done;
  
  HLock(destHandle);
  aPastedText->SetString(*destHandle, scrapLen);
  
done:
  ::DisposeHandle(destHandle);
  return NS_OK;
}

nsresult NS_NewSelectionMgr(nsISelectionMgr** aInstancePtrResult)
{
  nsSelectionMgr* sm = new nsSelectionMgr();
  return sm->QueryInterface(nsISelectionMgr::GetIID(),
                            (void**) aInstancePtrResult);
}


