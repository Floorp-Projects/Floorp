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

#include "nsIDataFlavor.h"
#include "nsWidgetsCID.h"
#include "nsISupportsArray.h"
#include "nsRepository.h"

// These are temporary
#if defined(XP_UNIX) || defined(XP_MAC)
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

#include "nsIDataFlavor.h"
#include "nsWidgetsCID.h"
#include "nsXIFFormatConverter.h"

static NS_DEFINE_IID(kIXIFFormatConverterIID,  NS_IFORMATCONVERTER_IID);
//static NS_DEFINE_IID(kCXIFConverterCID,  NS_XIFCONVERTER_CID);

static NS_DEFINE_IID(kCParserIID, NS_IPARSER_IID);
static NS_DEFINE_IID(kCParserCID, NS_PARSER_IID);

static NS_DEFINE_IID(kIDataFlavorIID,    NS_IDATAFLAVOR_IID);
static NS_DEFINE_IID(kCDataFlavorCID,    NS_DATAFLAVOR_CID);


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

  return rv;
}


static void AddFlavor(nsISupportsArray * aArray, const nsString & aMime, const nsString & aReadable)
{
  nsIDataFlavor * flavor;
  nsresult rv = nsComponentManager::CreateInstance(kCDataFlavorCID, nsnull, kIDataFlavorIID, (void**) &flavor);
  if (NS_OK == rv) {
    flavor->Init(aMime, aReadable);
    if (nsnull != flavor) {
      aArray->AppendElement(flavor);
    }
  }
}

/**
  * 
  *
  */
NS_IMETHODIMP nsXIFFormatConverter::GetInputDataFlavors(nsISupportsArray ** aDataFlavorList)
{
  nsISupportsArray * array;
  nsresult rv = NS_NewISupportsArray(&array);
  if (NS_OK == rv) {
    AddFlavor(array, kXIFMime, "XIF");
    *aDataFlavorList = array;
  }
  return NS_OK;
}

/**
  * 
  *
  */
NS_IMETHODIMP nsXIFFormatConverter::GetOutputDataFlavors(nsISupportsArray ** aDataFlavorList)
{
  nsISupportsArray * array;
  nsresult rv = NS_NewISupportsArray(&array);
  if (NS_OK == rv) {
    AddFlavor(array, kXIFMime,     "XIF");
    AddFlavor(array, kTextMime,    "Text");
    AddFlavor(array, kAOLMailMime, "AOLMail");
    AddFlavor(array, kHTMLMime,    "HTML");
    *aDataFlavorList = array;
  }
  return NS_OK;
}



/**
  * 
  *
  */
NS_IMETHODIMP nsXIFFormatConverter::CanConvert(nsIDataFlavor * aFromDataFlavor, nsIDataFlavor * aToDataFlavor)
{

  // This method currently only converts from XIF to the others
  nsAutoString  fromMimeInQuestion;
  aFromDataFlavor->GetMimeType(fromMimeInQuestion);

  if (!fromMimeInQuestion.Equals(kXIFMime)) {
    return NS_ERROR_FAILURE;
  }

  nsAutoString  toMimeInQuestion;
  aToDataFlavor->GetMimeType(toMimeInQuestion);

  if (toMimeInQuestion.Equals(kTextMime)) {
    return NS_OK;
  } else if (toMimeInQuestion.Equals(kHTMLMime)) {
    return NS_OK;
  } else if (toMimeInQuestion.Equals(kAOLMailMime)) {
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

/**
  * 
  *
  */
NS_IMETHODIMP nsXIFFormatConverter::Convert(nsIDataFlavor * aFromDataFlavor, void * aFromData, PRUint32 aDataLen,
                                      nsIDataFlavor * aToDataFlavor, void ** aToData, PRUint32 * aDataToLen)
{

  // This method currently only converts from XIF to the others
  nsAutoString  fromMimeInQuestion;
  aFromDataFlavor->GetMimeType(fromMimeInQuestion);

  if (!fromMimeInQuestion.Equals(kXIFMime)) {
    return NS_ERROR_FAILURE;
  }

  nsAutoString  toMimeInQuestion;
  aToDataFlavor->GetMimeType(toMimeInQuestion);

  nsAutoString text;
  nsAutoString srcText;
  srcText.SetString((char *)aFromData, aDataLen);

  if (toMimeInQuestion.Equals(kTextMime)) {
    if (NS_OK == ConvertFromXIFToText(srcText, text)) {
      *aToData = (void *)text.ToNewCString();
      *aDataToLen = text.Length()+1;
    }
  } else if (toMimeInQuestion.Equals(kHTMLMime)) {
    if (NS_OK == ConvertFromXIFToHTML(srcText, text)) {
      *aToData = (void *)text.ToNewCString();
      *aDataToLen = text.Length()+1;
    }
  } else if (toMimeInQuestion.Equals(kAOLMailMime)) {
    if (NS_OK == ConvertFromXIFToAOLMail(srcText, text)) {
      *aToData = (void *)text.ToNewCString();
      *aDataToLen = text.Length()+1;
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

//  rv = NS_New_HTML_ContentSinkStream(&sink,PR_FALSE,PR_FALSE);
//  Changed to do plain text only for Dogfood -- gpk 3/14/99
  rv = NS_New_HTMLToTXT_SinkStream(&sink);

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

    ((nsHTMLToTXTSinkStream*)sink)->GetStringBuffer(aToStr);
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

  rv = NS_New_HTML_ContentSinkStream(&sink,PR_FALSE,PR_FALSE);

  ostrstream* copyStream = new ostrstream;

  ((nsHTMLContentSinkStream*)sink)->SetOutputStream(*copyStream);
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

  PRInt32 len = copyStream->pcount();
  char* str = (char*)copyStream->str();

  if (str) {
    aToStr.SetString(str, len);
    delete[] str;
  }

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

