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



#include "nsCharsetObserver.h"

static NS_DEFINE_IID(kIElementObserverIID, NS_IELEMENTOBSERVER_IID);
static NS_DEFINE_IID(kIObserverIID, NS_IOBSERVER_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);


nsCharsetObserver::nsCharsetObserver()
{
  NS_INIT_REFCNT();
}
nsCharsetObserver::~nsCharsetObserver()
{
}

NS_IMPL_ADDREF ( nsCharsetObserver );
NS_IMPL_RELEASE ( nsCharsetObserver );

NS_IMETHODIMP nsCharsetObserver::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{

  if( NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  *aInstancePtr = NULL;

  if( aIID.Equals ( kIElementObserverIID )) {
    *aInstancePtr = (void*) ((nsIElementObserver*) this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if( aIID.Equals ( kIObserverIID )) {
    *aInstancePtr = (void*) ((nsIObserver*) this);
    NS_ADDREF_THIS();
    return NS_OK;
  }

  if( aIID.Equals ( kISupportsIID )) {
    *aInstancePtr = (void*) (this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

NS_IMETHODIMP nsCharsetObserver::GetTagName(nsString& oTag)
{
  oTag = "META";
  return NS_OK;
}

NS_IMETHODIMP nsCharsetObserver::Notify(const nsString& aTag, 
                     PRUint32 numOfAttributes, 
                     const nsString* nameArray, 
                     const nsString* valueArray,
                     PRBool *oContinue)
{
    nsresult res = NS_OK;
    *oContinue = PR_TRUE; // be default, continue
    if(aTag.EqualsIgnoreCase("META")) 
    {
      if((numOfAttributes >= 2) && 
         (nameArray[0].EqualsIgnoreCase("HTTP-EQUIV")) &&
         (valueArray[0].EqualsIgnoreCase("Content-Type")) && 
         (nameArray[1].EqualsIgnoreCase("CONTENT")) ) 
      {
         nsAutoString type;
         valueArray[2].Left(type, 9); // length of "text/html" == 9
         if(type.EqualsIgnoreCase("text/html")) 
         {
            PRInt32 charsetValueStart = valueArray[2].RFind("charset=", PR_TRUE ) ;
            if(kNotFound != charsetValueStart) {	
                  charsetValueStart += 8; // 8 = "charset=".length 
                  PRInt32 charsetValueEnd = valueArray[2].FindCharInSet("\'\";", charsetValueStart  );
                  if(kNotFound == charsetValueEnd ) 
                     charsetValueEnd = valueArray[2].Length();
                  nsAutoString theCharset;
                  valueArray[2].Mid(theCharset, charsetValueStart, charsetValueEnd - charsetValueStart);
                  // XXX 
                  // Get the charest in theCharset
                  // Now, how can we verify the object, if
                  // we pass the object  (nsScanner) when we create this object
                  // then it mean we need to create one nsIElementObserver
                  // for every nsParser object
                  // However, currently we register ourself to a global
                  // nsIObserverService, not to a nsParser. 
                  
             } // if
         } // if
      } // if
    } // if
    return res;
}

NS_IMETHODIMP Notify(nsISupports** result) 
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
