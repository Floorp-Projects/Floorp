/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http:/www.mozilla.org/NPL/
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

#if 0 // OBSOLETE

/* -*- Mode: C; tab-width: 4 -*-
 *   nsGIFDecoder.cpp --- interface to gif decoder
 */

#include "nsGIFCallback.h"


/* callbks: */
/*-------------class----------*/

NS_IMPL_ADDREF(GIFCallbk)
NS_IMPL_RELEASE(GIFCallbk)

NS_IMETHODIMP GIFCallbk::QueryInterface(const nsIID& aIID, void** aResult)
{   	
  if (NULL == aResult) {
    return NS_ERROR_NULL_POINTER;
  }
  
  static NS_DEFINE_IID(kIImgDCallbkIID, NS_IIMGDCALLBK_IID);
  static NS_DEFINE_IID(kGIFCallbkIID, NS_GIFCALLBK_IID);
  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

  if ( aIID.Equals(kGIFCallbkIID) ||
       aIID.Equals(kIImgDCallbkIID)||
       aIID.Equals(kISupportsIID)) {
	  *aResult = (void*) this;
    NS_INIT_REFCNT();
    return NS_OK;
  }
  
  return NS_NOINTERFACE;
}



/*-----------------------------*/

#endif /* 0 */
