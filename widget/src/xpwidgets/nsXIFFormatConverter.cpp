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
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

#include "nsString.h"
#include "nsISupportsArray.h"
#include "nsRepository.h"
#include "nsCOMPtr.h"
#include "nsISupportsPrimitives.h"
#include "nsXPIDLString.h"

#include "nsITransferable.h" // for mime defs, this is BAD

// XIF convertor stuff
#include "nsIParser.h"
#include "nsParserCIID.h"
#include "nsHTMLContentSinkStream.h"
#include "nsHTMLToTXTSinkStream.h"
#include "nsXIFDTD.h"
#include "nsIStringStream.h"

#include "nsString.h"
#include "nsWidgetsCID.h"
#include "nsXIFFormatConverter.h"
#include "nsPrimitiveHelpers.h"
#include "nsIDocumentEncoder.h"

static NS_DEFINE_CID(kCParserCID, NS_PARSER_IID);  // don't panic. NS_PARSER_IID just has the wrong name.

NS_IMPL_ADDREF(nsXIFFormatConverter)
NS_IMPL_RELEASE(nsXIFFormatConverter)
NS_IMPL_QUERY_INTERFACE1(nsXIFFormatConverter, nsIFormatConverter)


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
    rv = AddFlavorToList ( *_retval, kHTMLMime );
    if ( NS_FAILED(rv) )
      return rv;
#if NOT_NOW
// pinkerton
// no one uses this flavor right now, so it's just slowing things down. If anyone cares I
// can put it back in.
    rv = AddFlavorToList ( *_retval, kAOLMailMime );
    if ( NS_FAILED(rv) )
      return rv;
#endif
    rv = AddFlavorToList ( *_retval, kUnicodeMime );
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
  nsresult rv = nsComponentManager::CreateInstance(NS_SUPPORTS_STRING_CONTRACTID, nsnull, 
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

    // STRING USE WARNING: reduce conversions here?
  
  *_retval = PR_FALSE;
  nsAutoString fromFlavor; fromFlavor.AssignWithConversion( aFromDataFlavor );
  if ( fromFlavor.EqualsWithConversion(kXIFMime) ) {
    nsAutoString toFlavor; toFlavor.AssignWithConversion( aToDataFlavor );
    if ( toFlavor.EqualsWithConversion(kHTMLMime) )
      *_retval = PR_TRUE;
    else if ( toFlavor.EqualsWithConversion(kUnicodeMime) )
      *_retval = PR_TRUE;
#if NOT_NOW
// pinkerton
// no one uses this flavor right now, so it's just slowing things down. If anyone cares I
// can put it back in.
    else if ( toFlavor.Equals(kAOLMailMime) )
      *_retval = PR_TRUE;
#endif
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
  
  nsCAutoString fromFlavor ( aFromDataFlavor );
  if ( fromFlavor.Equals(kXIFMime) ) {
    nsCAutoString toFlavor ( aToDataFlavor );

    // XIF on clipboard is going to always be double byte so it will be in a primitive
    // class of nsISupportsWString. Also, since the data is in two byte chunks the 
    // length represents the length in 1-byte chars, so we need to divide by two.
    nsCOMPtr<nsISupportsWString> dataWrapper0 ( do_QueryInterface(aFromData) );
    if ( dataWrapper0 ) {
      nsXPIDLString data;
      dataWrapper0->ToString ( getter_Copies(data) );  //еее COPY #1
      if ( data ) {
        PRUnichar* castedData = NS_CONST_CAST(PRUnichar*, NS_STATIC_CAST(const PRUnichar*, data));
        nsAutoString dataStr ( CBufDescriptor(castedData, PR_TRUE, aDataLen) );  //еее try not to copy the data

        // note: conversion to text/plain is done inside the clipboard. we do not need to worry 
        // about it here.
        if ( toFlavor.Equals(kHTMLMime) || toFlavor.Equals(kUnicodeMime) ) {
          nsAutoString outStr;
          nsresult res;
          if (toFlavor.Equals(kHTMLMime))
            res = ConvertFromXIFToHTML(dataStr, outStr);
          else
            res = ConvertFromXIFToUnicode(dataStr, outStr);
          if ( NS_SUCCEEDED(res) ) {
            PRInt32 dataLen = outStr.Length() * 2;
            nsPrimitiveHelpers::CreatePrimitiveForData ( toFlavor, (void*)outStr.GetUnicode(), dataLen, aToData );
            if ( *aToData ) 
              *aDataToLen = dataLen;
          }
        } // else if HTML or Unicode
        else if ( toFlavor.Equals(kAOLMailMime) ) {
          nsAutoString outStr;
          if ( NS_SUCCEEDED(ConvertFromXIFToAOLMail(dataStr, outStr)) ) {
            PRInt32 dataLen = outStr.Length() * 2;
            nsPrimitiveHelpers::CreatePrimitiveForData ( toFlavor, (void*)outStr.GetUnicode(), dataLen, aToData );
            if ( *aToData ) 
              *aDataToLen = dataLen;
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


#if USE_PLAIN_TEXT
//
// ConvertFromXIFToText
//
// Takes XIF and converts it to plain text using the correct charset for the platform/OS/language.
//
// *** This code is now obsolete, but I'm leaving it around for reference about how to do 
// *** charset conversion with streams
//
NS_IMETHODIMP
nsXIFFormatConverter::ConvertFromXIFToText(const nsAutoString & aFromStr, nsCAutoString & aToStr)
{
  // Figure out the correct charset we need to use. We are guaranteed that this does not change
  // so we cache it.
  static nsAutoString platformCharset;
  static PRBool hasDeterminedCharset = PR_FALSE;
  if ( !hasDeterminedCharset ) {
    nsresult res;
    nsCOMPtr <nsIPlatformCharset> platformCharsetService = do_GetService(NS_PLATFORMCHARSET_CONTRACTID, &res);
    if (NS_SUCCEEDED(res))
      res = platformCharsetService->GetCharset(kPlatformCharsetSel_PlainTextInClipboard, platformCharset);
    if (NS_FAILED(res))
      platformCharset.SetString("ISO-8859-1");
      
    hasDeterminedCharset = PR_TRUE;
  }
  
  // create the parser to do the conversion.
  aToStr = "";
  nsCOMPtr<nsIParser> parser;
  nsresult rv = nsComponentManager::CreateInstance(kCParserCID, nsnull, NS_GET_IID(nsIParser),
                                                     getter_AddRefs(parser));
  if ( !parser )
    return rv;

  // create a string stream to hold the converted text in the appropriate charset. The stream
  // owns the char buffer it creates, so we don't have to worry about it.
  nsCOMPtr<nsISupports> stream;
  char* buffer = nsnull;
  rv = NS_NewCharOutputStream ( getter_AddRefs(stream), &buffer );   // owns |buffer|
  if ( !stream )    
    return rv;
  nsCOMPtr<nsIOutputStream> outStream ( do_QueryInterface(stream) );
  
  // convert it!
  nsCOMPtr<nsIHTMLContentSink> sink;
  rv = NS_New_HTMLToTXT_SinkStream(getter_AddRefs(sink),outStream,&platformCharset,
                                   nsIDocumentEncoder::OutputSelectionOnly
                                    | nsIDocumentEncoder::OutputAbsoluteLinks);
  if ( sink ) {
    parser->SetContentSink(sink);
	
    nsCOMPtr<nsIDTD> dtd;
    rv = NS_NewXIFDTD(getter_AddRefs(dtd));
    if ( dtd ) {
      parser->RegisterDTD(dtd);
      parser->Parse(aFromStr, 0, "text/xif",PR_FALSE,PR_TRUE);           
    }
  }
  
  // assign the data back into our out param.
  aToStr = buffer;

  return NS_OK;
} // ConvertFromXIFToText
#endif


//
// ConvertFromXIFToUnicode
//
// Takes XIF and converts it to plain text but in unicode. We can't just use the xif->text
// routine as it also does a charset conversion which isn't what we want.
//
NS_IMETHODIMP
nsXIFFormatConverter::ConvertFromXIFToUnicode(const nsAutoString & aFromStr, nsAutoString & aToStr)
{
  // create the parser to do the conversion.
  aToStr.SetLength(0);
  nsCOMPtr<nsIParser> parser;
  nsresult rv = nsComponentManager::CreateInstance(kCParserCID, nsnull, NS_GET_IID(nsIParser),
                                                     getter_AddRefs(parser));
  if ( !parser )
    return rv;

  // convert it!
  nsCOMPtr<nsIHTMLContentSink> sink;
  rv = NS_New_HTMLToTXT_SinkStream(getter_AddRefs(sink), &aToStr, 0,
                                   nsIDocumentEncoder::OutputSelectionOnly
                                    | nsIDocumentEncoder::OutputAbsoluteLinks);
  if ( sink ) {
    parser->SetContentSink(sink);
	
    nsCOMPtr<nsIDTD> dtd;
    rv = NS_NewXIFDTD(getter_AddRefs(dtd));
    if ( dtd ) {
      parser->RegisterDTD(dtd);
      parser->Parse(aFromStr, 0, NS_ConvertASCIItoUCS2("text/xif"),PR_FALSE,PR_TRUE);           
    }
  }
  
  return NS_OK;
} // ConvertFromXIFToUnicode


/**
  * 
  *
  */
NS_IMETHODIMP
nsXIFFormatConverter::ConvertFromXIFToHTML(const nsAutoString & aFromStr, nsAutoString & aToStr)
{
  aToStr.SetLength(0);
  nsCOMPtr<nsIParser> parser;
  nsresult rv = nsComponentManager::CreateInstance(kCParserCID, 
                                             nsnull, 
                                             NS_GET_IID(nsIParser), 
                                             getter_AddRefs(parser));
  if ( !parser )
    return rv;

  nsCOMPtr<nsIHTMLContentSink> sink;
  rv = NS_New_HTML_ContentSinkStream(getter_AddRefs(sink), &aToStr,
                                     nsIDocumentEncoder::OutputSelectionOnly
                                      | nsIDocumentEncoder::OutputAbsoluteLinks);
  if ( sink ) {
    parser->SetContentSink(sink);
	
    nsCOMPtr<nsIDTD> dtd;
    rv = NS_NewXIFDTD(getter_AddRefs(dtd));
    if ( dtd ) {
      parser->RegisterDTD(dtd);
      parser->Parse(aFromStr, 0, NS_ConvertASCIItoUCS2("text/xif"),PR_FALSE,PR_TRUE);           
    }
  }
  return NS_OK;
}

/**
  * 
  *
  */
NS_IMETHODIMP
nsXIFFormatConverter::ConvertFromXIFToAOLMail(const nsAutoString & aFromStr, nsAutoString & aToStr)
{
  nsAutoString html;
  if (NS_OK == ConvertFromXIFToHTML(aFromStr, html)) {
    aToStr.AssignWithConversion("<HTML>");
    aToStr.Append(html);
    aToStr.AppendWithConversion("</HTML>");
  }
  return NS_OK;
}

