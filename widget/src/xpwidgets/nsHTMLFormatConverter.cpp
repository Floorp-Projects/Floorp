/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsString.h"
#include "nsCRT.h"
#include "nsISupportsArray.h"
#include "nsIComponentManager.h"
#include "nsCOMPtr.h"
#include "nsXPCOM.h"
#include "nsISupportsPrimitives.h"
#include "nsXPIDLString.h"

#include "nsITransferable.h" // for mime defs, this is BAD

// HTML convertor stuff
#include "nsIParser.h"
#include "nsIDTD.h"
#include "nsParserCIID.h"
#include "nsIContentSink.h"

#include "nsString.h"
#include "nsWidgetsCID.h"
#include "nsHTMLFormatConverter.h"
#include "nsPrimitiveHelpers.h"
#include "nsIDocumentEncoder.h"
#include "nsIHTMLToTextSink.h"

static NS_DEFINE_CID(kCParserCID, NS_PARSER_CID);

NS_IMPL_ADDREF(nsHTMLFormatConverter)
NS_IMPL_RELEASE(nsHTMLFormatConverter)
NS_IMPL_QUERY_INTERFACE1(nsHTMLFormatConverter, nsIFormatConverter)


//-------------------------------------------------------------------------
//
// HTMLFormatConverter constructor
//
//-------------------------------------------------------------------------
nsHTMLFormatConverter::nsHTMLFormatConverter()
{
}

//-------------------------------------------------------------------------
//
// HTMLFormatConverter destructor
//
//-------------------------------------------------------------------------
nsHTMLFormatConverter::~nsHTMLFormatConverter()
{
}


//
// GetInputDataFlavors
//
// Creates a new list and returns the list of all the flavors this converter
// knows how to import. In this case, it's just HTML.
//
// Flavors (strings) are wrapped in a primitive object so that JavaScript can
// access them easily via XPConnect.
//
NS_IMETHODIMP
nsHTMLFormatConverter::GetInputDataFlavors(nsISupportsArray **_retval)
{
  if ( !_retval )
    return NS_ERROR_INVALID_ARG;
  
  nsresult rv = NS_NewISupportsArray ( _retval );  // addrefs for us
  if ( NS_SUCCEEDED(rv) )
    rv = AddFlavorToList ( *_retval, kHTMLMime );
  
  return rv;
  
} // GetInputDataFlavors


//
// GetOutputDataFlavors
//
// Creates a new list and returns the list of all the flavors this converter
// knows how to export (convert). In this case, it's all sorts of things that HTML can be
// converted to.
//
// Flavors (strings) are wrapped in a primitive object so that JavaScript can
// access them easily via XPConnect.
//
NS_IMETHODIMP
nsHTMLFormatConverter::GetOutputDataFlavors(nsISupportsArray **_retval)
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
// Convenience routine for adding a flavor wrapped in an nsISupportsCString object
// to a list
//
nsresult
nsHTMLFormatConverter :: AddFlavorToList ( nsISupportsArray* inList, const char* inFlavor )
{
  nsresult rv;
  
  nsCOMPtr<nsISupportsCString> dataFlavor =
      do_CreateInstance(NS_SUPPORTS_CSTRING_CONTRACTID, &rv);
  if ( dataFlavor ) {
    dataFlavor->SetData ( nsDependentCString(inFlavor) );
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
// converts from HTML to others.
//
NS_IMETHODIMP
nsHTMLFormatConverter::CanConvert(const char *aFromDataFlavor, const char *aToDataFlavor, PRBool *_retval)
{
  if ( !_retval )
    return NS_ERROR_INVALID_ARG;

    // STRING USE WARNING: reduce conversions here?
  
  *_retval = PR_FALSE;
  nsAutoString fromFlavor; fromFlavor.AssignWithConversion( aFromDataFlavor );
  if ( !nsCRT::strcmp(aFromDataFlavor, kHTMLMime) ) {
    if ( !nsCRT::strcmp(aToDataFlavor, kHTMLMime) )
      *_retval = PR_TRUE;
    else if ( !nsCRT::strcmp(aToDataFlavor, kUnicodeMime) )
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
// accessable from JS. Currently, this only accepts HTML input, so anything else is invalid.
//
//XXX This method copies the data WAAAAY too many time for my liking. Grrrrrr. Mostly it's because
//XXX we _must_ put things into nsStrings so that the parser will accept it. Lame lame lame lame. We
//XXX also can't just get raw unicode out of the nsString, so we have to allocate heap to get
//XXX unicode out of the string. Lame lame lame.
//
NS_IMETHODIMP
nsHTMLFormatConverter::Convert(const char *aFromDataFlavor, nsISupports *aFromData, PRUint32 aDataLen, 
                               const char *aToDataFlavor, nsISupports **aToData, PRUint32 *aDataToLen)
{
  if ( !aToData || !aDataToLen )
    return NS_ERROR_INVALID_ARG;

  nsresult rv = NS_OK;

  if ( !nsCRT::strcmp(aFromDataFlavor, kHTMLMime) ) {
    nsCAutoString toFlavor ( aToDataFlavor );

    // HTML on clipboard is going to always be double byte so it will be in a primitive
    // class of nsISupportsString. Also, since the data is in two byte chunks the 
    // length represents the length in 1-byte chars, so we need to divide by two.
    nsCOMPtr<nsISupportsString> dataWrapper0 ( do_QueryInterface(aFromData) );
    if ( dataWrapper0 ) {
      nsAutoString dataStr;
      dataWrapper0->GetData ( dataStr );  //еее COPY #1
      if ( !dataStr.IsEmpty() ) {

        // note: conversion to text/plain is done inside the clipboard. we do not need to worry 
        // about it here.
        if ( toFlavor.Equals(kHTMLMime) || toFlavor.Equals(kUnicodeMime) ) {
          nsresult res;
          if (toFlavor.Equals(kHTMLMime)) {
            PRInt32 dataLen = dataStr.Length() * 2;
            nsPrimitiveHelpers::CreatePrimitiveForData ( toFlavor.get(), (void*)dataStr.get(), dataLen, aToData );
            if ( *aToData )
              *aDataToLen = dataLen;
          } else {
            nsAutoString outStr;
            res = ConvertFromHTMLToUnicode(dataStr, outStr);
            if (NS_SUCCEEDED(res)) {
              PRInt32 dataLen = outStr.Length() * 2;
              nsPrimitiveHelpers::CreatePrimitiveForData ( toFlavor.get(), (void*)outStr.get(), dataLen, aToData );
              if ( *aToData ) 
                *aDataToLen = dataLen;
            }
          }
        } // else if HTML or Unicode
        else if ( toFlavor.Equals(kAOLMailMime) ) {
          nsAutoString outStr;
          if ( NS_SUCCEEDED(ConvertFromHTMLToAOLMail(dataStr, outStr)) ) {
            PRInt32 dataLen = outStr.Length() * 2;
            nsPrimitiveHelpers::CreatePrimitiveForData ( toFlavor.get(), (void*)outStr.get(), dataLen, aToData );
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
    
  } // if we got html mime
  else
    rv = NS_ERROR_FAILURE;      
    
  return rv;
  
} // Convert


//
// ConvertFromHTMLToUnicode
//
// Takes HTML and converts it to plain text but in unicode.
//
NS_IMETHODIMP
nsHTMLFormatConverter::ConvertFromHTMLToUnicode(const nsAutoString & aFromStr, nsAutoString & aToStr)
{
  // create the parser to do the conversion.
  aToStr.SetLength(0);
  nsresult rv;
  nsCOMPtr<nsIParser> parser = do_CreateInstance(kCParserCID, &rv);
  if ( !parser )
    return rv;

  // convert it!
  nsCOMPtr<nsIContentSink> sink;

  sink = do_CreateInstance(NS_PLAINTEXTSINK_CONTRACTID);
  NS_ENSURE_TRUE(sink, NS_ERROR_FAILURE);

  nsCOMPtr<nsIHTMLToTextSink> textSink(do_QueryInterface(sink));
  NS_ENSURE_TRUE(textSink, NS_ERROR_FAILURE);

  textSink->Initialize(&aToStr, nsIDocumentEncoder::OutputSelectionOnly
                       | nsIDocumentEncoder::OutputAbsoluteLinks, 0);

  parser->SetContentSink(sink);

  parser->Parse(aFromStr, 0, NS_LITERAL_CSTRING("text/html"), PR_FALSE, PR_TRUE, eDTDMode_fragment);
  
  return NS_OK;
} // ConvertFromHTMLToUnicode


/**
  * 
  *
  */
NS_IMETHODIMP
nsHTMLFormatConverter::ConvertFromHTMLToAOLMail(const nsAutoString & aFromStr,
                                                nsAutoString & aToStr)
{
  aToStr.AssignLiteral("<HTML>");
  aToStr.Append(aFromStr);
  aToStr.AppendLiteral("</HTML>");

  return NS_OK;
}

