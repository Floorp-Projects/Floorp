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
#include "nsWidgetsCID.h"
#include "nsVoidArray.h"
#include "nsRepository.h"

#include "nsITransferable.h" // for mime defs


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

static NS_DEFINE_IID(kIXIFFormatConverterIID,  NS_IFORMATCONVERTER_IID);
static NS_DEFINE_IID(kISupportsIID,  NS_ISUPPORTS_IID);
//static NS_DEFINE_IID(kCXIFConverterCID,  NS_XIFCONVERTER_CID);

static NS_DEFINE_IID(kCParserIID, NS_IPARSER_IID);
static NS_DEFINE_IID(kCParserCID, NS_PARSER_IID);

NS_IMPL_ADDREF(nsXIFFormatConverter)
NS_IMPL_RELEASE(nsXIFFormatConverter)

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

/**
 * @param aIID The name of the class implementing the method
 * @param _classiiddef The name of the #define symbol that defines the IID
 * for the class (e.g. NS_ISUPPORTS_IID)
 * 
*/ 
nsresult nsXIFFormatConverter::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{

  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }

  nsresult rv = NS_NOINTERFACE;

  if (aIID.Equals(kIXIFFormatConverterIID)) {
    *aInstancePtr = (void*) ((nsIFormatConverter*)this);
    NS_ADDREF_THIS();
    return NS_OK;
  }

  if (aIID.Equals(kISupportsIID)) {                                      
    *aInstancePtr = (void*)(nsISupports*)this;                        
    NS_ADDREF_THIS();                                                    
    return NS_OK;                                                        
  }


  return rv;
}


/**
  * 
  *
  */
NS_IMETHODIMP nsXIFFormatConverter::GetInputDataFlavors(nsVoidArray ** aDataFlavorList)
{
  nsVoidArray * array = new nsVoidArray();
  if (nsnull != array) {
    array->AppendElement(new nsString(kXIFMime));
    *aDataFlavorList = array;
  }
  return NS_OK;
}

/**
  * 
  *
  */
NS_IMETHODIMP nsXIFFormatConverter::GetOutputDataFlavors(nsVoidArray ** aDataFlavorList)
{
  nsVoidArray * array = new nsVoidArray();
  if (nsnull != array) {
    array->AppendElement(new nsString(kXIFMime));
    array->AppendElement(new nsString(kHTMLMime));
    array->AppendElement(new nsString(kUnicodeMime));
    array->AppendElement(new nsString(kTextMime));
    array->AppendElement(new nsString(kAOLMailMime));
    *aDataFlavorList = array;
  }
  return NS_OK;
}



/**
  * 
  *
  */
NS_IMETHODIMP nsXIFFormatConverter::CanConvert(nsString * aFromDataFlavor, nsString * aToDataFlavor)
{

  // This method currently only converts from XIF to the others
  if (!aFromDataFlavor->Equals(kXIFMime)) {
    return NS_ERROR_FAILURE;
  }

  if (aToDataFlavor->Equals(kTextMime)) {
    return NS_OK;
  } else if (aToDataFlavor->Equals(kHTMLMime)) {
    return NS_OK;
  } else if (aToDataFlavor->Equals(kUnicodeMime)) {
    return NS_OK;
  } else if (aToDataFlavor->Equals(kAOLMailMime)) {
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

/**
  * 
  *
  */
NS_IMETHODIMP nsXIFFormatConverter::Convert(nsString * aFromDataFlavor, void * aFromData, PRUint32 aDataLen,
                                            nsString * aToDataFlavor, void ** aToData, PRUint32 * aDataToLen)
{

  // This method currently only converts from XIF to the others

  if (!aFromDataFlavor->Equals(kXIFMime)) {
    return NS_ERROR_FAILURE;
  }

  nsAutoString text;
  nsAutoString srcText;

  // XIF on clipboard is going to always be double byte
  // since the data is in two byte chunks the length represents
  // the length in single 8 bit chars, so we need to divide by two
  srcText.SetString((PRUnichar *)aFromData, aDataLen/2);

  if (aToDataFlavor->Equals(kTextMime)) {
    if (NS_OK == ConvertFromXIFToText(srcText, text)) {
      *aToData = (void *)text.ToNewCString();
      *aDataToLen = text.Length();
    }
  } else if (aToDataFlavor->Equals(kHTMLMime) || aToDataFlavor->Equals(kUnicodeMime)) {
    if (NS_OK == ConvertFromXIFToHTML(srcText, text)) {
      *aToData = (void *)text.ToNewUnicode();
      *aDataToLen = text.Length()*2;
    }
  } else if (aToDataFlavor->Equals(kAOLMailMime)) {
    if (NS_OK == ConvertFromXIFToAOLMail(srcText, text)) {
      *aToData = (void *)text.ToNewCString();
      *aDataToLen = text.Length();
    }
  }

  return NS_OK;
}



/**
  * 
  *
  */
NS_IMETHODIMP nsXIFFormatConverter::ConvertFromXIFToText(const nsString & aFromStr, nsString & aToStr)
{
  aToStr = "";
  nsIParser* parser;
  nsresult rv = nsComponentManager::CreateInstance(kCParserCID, 
                                             nsnull, 
                                             kCParserIID, 
                                             (void **)&parser);
  if (NS_OK != rv)
    return rv;

  nsIHTMLContentSink* sink = nsnull;

  rv = NS_New_HTMLToTXT_SinkStream(&sink,&aToStr);

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
NS_IMETHODIMP nsXIFFormatConverter::ConvertFromXIFToHTML(const nsString & aFromStr, nsString & aToStr)
{
  aToStr = "";
  nsIParser* parser;

  nsresult rv = nsComponentManager::CreateInstance(kCParserCID, 
                                             nsnull, 
                                             kCParserIID, 
                                             (void **)&parser);
  if (NS_OK != rv)
    return rv;

  nsIHTMLContentSink* sink = nsnull;

  rv = NS_New_HTML_ContentSinkStream(&sink,&aToStr);

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
NS_IMETHODIMP nsXIFFormatConverter::ConvertFromXIFToAOLMail(const nsString & aFromStr, nsString & aToStr)
{
  nsAutoString html;
  if (NS_OK == ConvertFromXIFToHTML(aFromStr, html)) {
    aToStr = "<HTML>";
    aToStr.Append(html);
    aToStr.Append("</HTML>");
  }
  return NS_OK;
}

