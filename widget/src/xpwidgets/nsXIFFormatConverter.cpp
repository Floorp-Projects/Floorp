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

#include "nsString.h"
#include "nsISupportsArray.h"
#include "nsRepository.h"
#include "nsCOMPtr.h"
#include "nsISupportsPrimitives.h"

#include "nsITransferable.h" // for mime defs, this is BAD


// These are temporary
#if defined(XP_UNIX) || defined(XP_MAC) || defined(XP_BEOS)
#include <strstream.h>
#endif

#ifdef XP_PC
#include <strstrea.h>
#endif

// XIF convertor stuff
#include "nsIParser.h"
#include "nsParserCIID.h"
#include "nsHTMLContentSinkStream.h"
#include "nsHTMLToTXTSinkStream.h"
#include "nsXIFDTD.h"

#include "nsString.h"
#include "nsWidgetsCID.h"
#include "nsXIFFormatConverter.h"


static NS_DEFINE_IID(kCParserCID, NS_PARSER_IID);

NS_IMPL_ADDREF(nsXIFFormatConverter)
NS_IMPL_RELEASE(nsXIFFormatConverter)
NS_IMPL_QUERY_INTERFACE(nsXIFFormatConverter, NS_GET_IID(nsIFormatConverter))

//-------------------------------------------------------------------------
//
// XIFFormatConverter constructor
//
//-------------------------------------------------------------------------
nsXIFFormatConverter::nsXIFFormatConverter()
{
  NS_INIT_REFCNT();
}

//-------------------------------------------------------------------------
//
// XIFFormatConverter destructor
//
//-------------------------------------------------------------------------
nsXIFFormatConverter::~nsXIFFormatConverter()
{
}


//
// GetInputDataFlavors
//
// Creates a new list and returns the list of all the flavors this converter
// knows how to import. In this case, it's just XIF.
//
// Flavors (strings) are wrapped in a primitive object so that JavaScript can
// access them easily via XPConnect.
//
NS_IMETHODIMP
nsXIFFormatConverter::GetInputDataFlavors(nsISupportsArray **_retval)
{
  if ( !_retval )
    return NS_ERROR_INVALID_ARG;
  
  nsresult rv = NS_NewISupportsArray ( _retval );  // addrefs for us
  if ( NS_SUCCEEDED(rv) )
    rv = AddFlavorToList ( *_retval, kXIFMime );
  
  return rv;
  
} // GetInputDataFlavors


//
// GetOutputDataFlavors
//
// Creates a new list and returns the list of all the flavors this converter
// knows how to export (convert). In this case, it's all sorts of things that XIF can be
// converted to.
//
// Flavors (strings) are wrapped in a primitive object so that JavaScript can
// access them easily via XPConnect.
//
NS_IMETHODIMP
nsXIFFormatConverter::GetOutputDataFlavors(nsISupportsArray **_retval)
{
  if ( !_retval )
    return NS_ERROR_INVALID_ARG;
  
  nsresult rv = NS_NewISupportsArray ( _retval );  // addrefs for us
  if ( NS_SUCCEEDED(rv) ) {
    rv = AddFlavorToList ( *_retval, kXIFMime );
    if ( NS_FAILED(rv) )
      return rv;
    rv = AddFlavorToList ( *_retval, kHTMLMime );
    if ( NS_FAILED(rv) )
      return rv;
    rv = AddFlavorToList ( *_retval, kUnicodeMime );
    if ( NS_FAILED(rv) )
      return rv;
    rv = AddFlavorToList ( *_retval, kTextMime );
    if ( NS_FAILED(rv) )
      return rv;
    rv = AddFlavorToList ( *_retval, kAOLMailMime );
    if ( NS_FAILED(rv) )
      return rv;
  }
  return rv;

} // GetOutputDataFlavors


//
// AddFlavorToList
//
// Convenience routine for adding a flavor wrapped in an nsISupportsString object
// to a list
//
nsresult
nsXIFFormatConverter :: AddFlavorToList ( nsISupportsArray* inList, const char* inFlavor )
{
  nsCOMPtr<nsISupportsString> dataFlavor;
  nsresult rv = nsComponentManager::CreateInstance(NS_SUPPORTS_STRING_PROGID, nsnull, 
                                                    NS_GET_IID(nsISupportsString), getter_AddRefs(dataFlavor));
  if ( dataFlavor ) {
    dataFlavor->SetData ( NS_CONST_CAST(char*, inFlavor) );
    // add to list as an nsISupports so the correct interface gets the addref
    // in AppendElement()
    nsCOMPtr<nsISupports> genericFlavor ( do_QueryInterface(dataFlavor) );
    inList->AppendElement ( genericFlavor);
  }
  return rv;

} // AddFlavorToList


//
// CanConvert
//
// Determines if we support the given conversion. Currently, this method only
// converts from XIF to others.
//
NS_IMETHODIMP
nsXIFFormatConverter::CanConvert(const char *aFromDataFlavor, const char *aToDataFlavor, PRBool *_retval)
{
  if ( !_retval )
    return NS_ERROR_INVALID_ARG;
  
  *_retval = PR_FALSE;
  nsAutoString fromFlavor ( aFromDataFlavor );
  if ( fromFlavor.Equals(kXIFMime) ) {
    nsAutoString toFlavor ( aToDataFlavor );
    if ( toFlavor.Equals(kTextMime) )
      *_retval = PR_TRUE;
    else if ( toFlavor.Equals(kHTMLMime) )
      *_retval = PR_TRUE;
    else if ( toFlavor.Equals(kUnicodeMime) )
      *_retval = PR_TRUE;
    else if ( toFlavor.Equals(kAOLMailMime) )
      *_retval = PR_TRUE;
  }
  return NS_OK;

} // CanConvert



//
// Convert
//
// Convert data from one flavor to another. The data is wrapped in primitive objects so that it is
// accessable from JS. Currently, this only accepts XIF input, so anything else is invalid.
//
//XXX This method copies the data WAAAAY too many time for my liking. Grrrrrr. Mostly it's because
//XXX we _must_ put things into nsStrings so that the parser will accept it. Lame lame lame lame. We
//XXX also can't just get raw unicode out of the nsString, so we have to allocate heap to get
//XXX unicode out of the string. Lame lame lame.
//
NS_IMETHODIMP
nsXIFFormatConverter::Convert(const char *aFromDataFlavor, nsISupports *aFromData, PRUint32 aDataLen, 
                               const char *aToDataFlavor, nsISupports **aToData, PRUint32 *aDataToLen)
{
  if ( !aToData || !aDataToLen )
    return NS_ERROR_INVALID_ARG;

  nsresult rv = NS_OK;
  
  nsAutoString fromFlavor ( aFromDataFlavor );
  if ( fromFlavor.Equals(kXIFMime) ) {
    nsAutoString toFlavor ( aToDataFlavor );

    // XIF on clipboard is going to always be double byte so it will be in a primitive
    // class of nsISupportsWString. Also, since the data is in two byte chunks the 
    // length represents the length in 1-byte chars, so we need to divide by two.
    nsCOMPtr<nsISupportsWString> dataWrapper ( do_QueryInterface(aFromData) );
    if ( dataWrapper ) {
      PRUnichar* data = nsnull;
      dataWrapper->ToString ( &data );
      if ( data ) {
        nsAutoString dataStr ( data );
        nsAutoString outStr;

        if ( toFlavor.Equals(kTextMime) ) {
          if ( NS_SUCCEEDED(ConvertFromXIFToText(dataStr, outStr)) ) {
            nsCOMPtr<nsISupportsString> dataWrapper;
            nsComponentManager::CreateInstance(NS_SUPPORTS_STRING_PROGID, nsnull, 
                                                NS_GET_IID(nsISupportsString), getter_AddRefs(dataWrapper) );
            if ( dataWrapper ) {
              char* holderBecauseNSStringIsLame = outStr.ToNewCString();
              dataWrapper->SetData ( holderBecauseNSStringIsLame );
              nsCOMPtr<nsISupports> genericDataWrapper ( do_QueryInterface(dataWrapper) );
              *aToData = genericDataWrapper;
              NS_ADDREF(*aToData);
              *aDataToLen = outStr.Length();
              delete [] holderBecauseNSStringIsLame;
            }
          }
        } // if plain text
        else if ( toFlavor.Equals(kHTMLMime) ) {
          if ( NS_SUCCEEDED(ConvertFromXIFToHTML(dataStr, outStr)) ) {
            nsCOMPtr<nsISupportsWString> dataWrapper;
            nsComponentManager::CreateInstance(NS_SUPPORTS_WSTRING_PROGID, nsnull, 
                                                NS_GET_IID(nsISupportsWString), getter_AddRefs(dataWrapper) );
            if ( dataWrapper ) {
              PRUnichar* holderBecauseNSStringIsLame = outStr.ToNewUnicode();
              dataWrapper->SetData ( holderBecauseNSStringIsLame );
              nsCOMPtr<nsISupports> genericDataWrapper ( do_QueryInterface(dataWrapper) );
              *aToData = genericDataWrapper;
              NS_ADDREF(*aToData);
              *aDataToLen = outStr.Length() * 2;
              delete [] holderBecauseNSStringIsLame;
            }
          }
        } // else if HTML
        else if ( toFlavor.Equals(kAOLMailMime) ) {
          if ( NS_SUCCEEDED(ConvertFromXIFToAOLMail(dataStr, outStr)) ) {
            nsCOMPtr<nsISupportsWString> dataWrapper;
            nsComponentManager::CreateInstance(NS_SUPPORTS_WSTRING_PROGID, nsnull, 
                                                NS_GET_IID(nsISupportsWString), getter_AddRefs(dataWrapper) );
            if ( dataWrapper ) {
              PRUnichar* holderBecauseNSStringIsLame = outStr.ToNewUnicode();
              dataWrapper->SetData ( holderBecauseNSStringIsLame );
              nsCOMPtr<nsISupports> genericDataWrapper ( do_QueryInterface(dataWrapper) );
              *aToData = genericDataWrapper;
              NS_ADDREF(*aToData);
              *aDataToLen = outStr.Length() * 2;
              delete [] holderBecauseNSStringIsLame;
            }
          }
        } // else if AOL mail
        else {
          *aToData = nsnull;
          *aDataToLen = 0;
          rv = NS_ERROR_FAILURE;      
        }
      }
    }
    
  } // if we got xif mime
  else
    rv = NS_ERROR_FAILURE;      
    
  return rv;
  
} // Convert



/**
  * 
  *
  */
NS_IMETHODIMP
nsXIFFormatConverter::ConvertFromXIFToText(const nsString & aFromStr, nsString & aToStr)
{
  aToStr = "";
  nsIParser* parser;
  nsresult rv = nsComponentManager::CreateInstance(kCParserCID, 
                                             nsnull, 
                                             NS_GET_IID(nsIParser), 
                                             (void **)&parser);
  if (NS_OK != rv)
    return rv;

  nsIHTMLContentSink* sink = nsnull;

  rv = NS_New_HTMLToTXT_SinkStream(&sink,&aToStr,0);

  if (NS_OK == rv) {
    parser->SetContentSink(sink);
	
    nsIDTD* dtd = nsnull;
    rv = NS_NewXIFDTD(&dtd);
    if (NS_OK == rv) {
      parser->RegisterDTD(dtd);
      nsAutoString str(aFromStr);
      parser->Parse(str, 0, "text/xif",PR_FALSE,PR_TRUE);           
    }
    NS_IF_RELEASE(dtd);
  }
  NS_IF_RELEASE(sink);
  NS_RELEASE(parser);

  return NS_OK;
}

/**
  * 
  *
  */
NS_IMETHODIMP
nsXIFFormatConverter::ConvertFromXIFToHTML(const nsString & aFromStr, nsString & aToStr)
{
  aToStr = "";
  nsIParser* parser;

  nsresult rv = nsComponentManager::CreateInstance(kCParserCID, 
                                             nsnull, 
                                             NS_GET_IID(nsIParser), 
                                             (void **)&parser);
  if (NS_OK != rv)
    return rv;

  nsIHTMLContentSink* sink = nsnull;

  rv = NS_New_HTML_ContentSinkStream(&sink,&aToStr,0);

  if (NS_OK == rv) {
    parser->SetContentSink(sink);
	
    nsIDTD* dtd = nsnull;
    rv = NS_NewXIFDTD(&dtd);
    if (NS_OK == rv) {
      parser->RegisterDTD(dtd);
      nsAutoString str(aFromStr);
      parser->Parse(str, 0, "text/xif",PR_FALSE,PR_TRUE);           
    }
    NS_IF_RELEASE(dtd);
  }
  NS_IF_RELEASE(sink);
  NS_RELEASE(parser);
  return NS_OK;
}

/**
  * 
  *
  */
NS_IMETHODIMP
nsXIFFormatConverter::ConvertFromXIFToAOLMail(const nsString & aFromStr, nsString & aToStr)
{
  nsAutoString html;
  if (NS_OK == ConvertFromXIFToHTML(aFromStr, html)) {
    aToStr = "<HTML>";
    aToStr.Append(html);
    aToStr.Append("</HTML>");
  }
  return NS_OK;
}

