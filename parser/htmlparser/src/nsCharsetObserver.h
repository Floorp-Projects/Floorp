/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL")=0; you may not use this file except in
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
#ifndef nsCharsetObserver_h__
#define nsCharsetObserver_h__


#include "nsIElementObserver.h"
#include "nsIObserver.h"
#include "nsISupports.h"
#include "nsString.h"
#include "nsIParser.h"


class nsCharsetObserver: public nsIElementObserver, public nsIObserver {
  NS_DECL_ISUPPORTS

public:

  nsCharsetObserver();
  virtual ~nsCharsetObserver();

  /*
   *   This method return the tag which the observer care about
   */
  NS_IMETHOD GetTagName(nsString& oTag);

  /*
   *   Subject call observer when the parser hit the tag
   *   @param aDocumentID- ID of the document
   *   @param aTag- the tag
   *   @param numOfAttributes - number of attributes
   *   @param nameArray - array of name. 
   *   @param valueArray - array of value
   */
  NS_IMETHOD Notify(PRUint32 aDocumentID, const nsString& aTag, PRUint32 numOfAttributes, 
                    const nsString* nameArray, const nsString* valueArray,
                    PRBool* oContinue);

  NS_IMETHOD Notify(nsISupports** result) = 0;
};

 
#endif /* nsCharsetObserver_h__ */
