/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Initial Developer of the Original Code is Robert Sayre.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#include "nsIParser.h"
#include "nsIDTD.h"
#include "nsNetCID.h"
#include "nsNetUtil.h"
#include "nsParserCIID.h"
#include "nsParserCIID.h"
#include "nsIContentSink.h"
#include "nsIHTMLToTextSink.h"
#include "nsIDocumentEncoder.h"

#include "nsIScriptableUnescapeHTML.h"
#include "nsScriptableUnescapeHTML.h"

NS_IMPL_ISUPPORTS1(nsScriptableUnescapeHTML, nsIScriptableUnescapeHTML)

static NS_DEFINE_CID(kCParserCID, NS_PARSER_CID);

// From /widget/HTMLConverter
//
// Takes HTML and converts it to plain text but in unicode.
//
NS_IMETHODIMP
nsScriptableUnescapeHTML::Unescape(const nsAString & aFromStr, 
                                   nsAString & aToStr)
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

  parser->Parse(aFromStr, 0, NS_LITERAL_CSTRING("text/html"),
                PR_TRUE, eDTDMode_fragment);
  
  return NS_OK;
}


     
