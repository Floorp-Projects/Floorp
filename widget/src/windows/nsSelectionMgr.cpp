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

/**
 * Selection Manager for Windows.
 * Owns the copied text, listens for selection request events.
 */

class nsSelectionMgr : nsISelectionMgr
{
public:
  virtual void CopyToClipboard();

  virtual ostream& GetCopyOStream();

  virtual void HandlePasteRequest();

  virtual nsEventStatus ProcessEvent(const nsGUIEvent & anEvent) = 0;

private:
  ostream* mCopyStream;
}

NS_IMPL_ADDREF(nsDialog)
NS_IMPL_RELEASE(nsDialog)

nsSelectionMgr::nsSelectionMgr()
{
  NS_INIT_REFCNT();

  mCopyStream = 0;
}

nsSelectionMgr::~nsSelectionMgr()
{
}

nsresult nsSelectionMgr::GetCopyOStream(ostream** aStream)
{
  if (mCopyStream)
    delete mCopyStream;
  mCopyStream = new ostringstream;
  *aStream = mCopyStream;
}

nsresult nsSelectionMgr::CopyToClipboard(ostream& str)
{
  // we'd better already have a stream ...
  if (!mCopyStream)
      return NS_ERROR_NOT_INITIALIZED;

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

nsresult NS_NewSelectionMgr(nsISelectionMgr** aInstancePtrResult)
{
  nsSelectionMgr* sm = new nsSelectionMgr();
  return sm->QueryInterface(nsISelectionMgr::IID(),
                            (void**) aInstancePtrResult);
}


