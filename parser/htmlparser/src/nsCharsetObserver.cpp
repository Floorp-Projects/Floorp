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
#include "nsIServiceManager.h"

#define NS_IMPL_IDS
#include "nsICharsetAlias.h"

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

NS_IMETHODIMP_(const char*) nsCharsetObserver::GetTagName()
{
  return "META";
}

NS_IMETHODIMP nsCharsetObserver::Notify(
                     PRUint32 aDocumentID, 
                     eHTMLTags aTag, 
                     PRUint32 numOfAttributes, 
                     const nsString* nameArray, 
                     const nsString* valueArray)
{
    if(eHTMLTag_meta != aTag) 
        return NS_ERROR_ILLEGAL_VALUE;

    nsresult res = NS_OK;

    // Only process if we get the HTTP-EQUIV=Content-Type in meta
    // We totaly need 4 attributes
    //   HTTP-EQUIV
    //   CONTENT
    //   currentCharset            - pseudo attribute fake by parser
    //   currentCharsetSource      - pseudo attribute fake by parser

    if((numOfAttributes >= 4) && 
       (nameArray[0].EqualsIgnoreCase("HTTP-EQUIV")) &&
       (valueArray[0].EqualsIgnoreCase("Content-Type")))
    {
      nsAutoString currentCharset("unknown");
      nsAutoString charsetSourceStr("unknown");

      for(PRUint32 i=0; i < numOfAttributes; i++) 
      {
         if(nameArray[i].EqualsIgnoreCase("currentCharset")) 
         {
           currentCharset = valueArray[i];
         } else if(nameArray[i].EqualsIgnoreCase("currentCharsetSource")) {
           charsetSourceStr = valueArray[i];
         }
      }

      // if we cannot find currentCharset or currentCharsetSource
      // return error.
      if( currentCharset.Equals("unknown") ||
          charsetSourceStr.Equals("unknown") )
      {
         return NS_ERROR_ILLEGAL_VALUE;
      }

      PRInt32 err;
      PRInt32 charsetSourceInt = charsetSourceStr.ToInteger(&err);

      // if we cannot convert the string into nsCharsetSource, return error
      if(NS_FAILED(err))
         return NS_ERROR_ILLEGAL_VALUE;

      nsCharsetSource currentCharsetSource = (nsCharsetSource)charsetSourceInt;

      if (nameArray[1].EqualsIgnoreCase("CONTENT")) 
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

                  if(kCharsetFromMetaTag > currentCharsetSource)
                  {
                     nsICharsetAlias* calias = nsnull;
                     res = nsServiceManager::GetService(
                                kCharsetAliasCID,
                                kICharsetAliasIID,
                                (nsISupports**) &calias);
                     if(NS_SUCCEEDED(res) && (nsnull != calias) ) {
                         PRBool same = PR_FALSE;
                         res = calias->Equals( theCharset, currentCharset, &same);
                         if(NS_SUCCEEDED(res) && (! same))
                         {
                            nsAutoString preferred;
                            res = calias->GetPreferred(theCharset, preferred);
                            if(NS_SUCCEEDED(res))
                            {
                               // XXX
                               // ask nsWebShellProxy to load docuement
                               // where the docuemtn ID is euqal to aDocuemntID
                               // Charset equal to preferred
                               // and charsetSource is kCharetFromMetaTag        
                            }
                         }
                         nsServiceManager::ReleaseService(kCharsetAliasCID, calias);
                     }
                  }
                  
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
