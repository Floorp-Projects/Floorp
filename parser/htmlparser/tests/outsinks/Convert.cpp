/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 * 
 * Contributor(s): Akkana Peck.
 */

#include "nsParserCIID.h"
#include "nsIParser.h"
#include "nsHTMLContentSinkStream.h"
#include "nsHTMLToTXTSinkStream.h"
#include "nsIComponentManager.h"
#include "CNavDTD.h"

extern "C" void NS_SetupRegistry();

#ifdef XP_PC
#define PARSER_DLL "raptorhtmlpars.dll"
#endif
#ifdef XP_MAC
#endif
#if defined(XP_UNIX) || defined(XP_BEOS)
#define PARSER_DLL "libraptorhtmlpars"MOZ_DLL_SUFFIX
#endif

// Class IID's
static NS_DEFINE_IID(kParserCID, NS_PARSER_IID);

// Interface IID's
static NS_DEFINE_IID(kIParserIID, NS_IPARSER_IID);

//----------------------------------------------------------------------
// Convert html on stdin to either plaintext or (if toHTML) html
//----------------------------------------------------------------------
nsresult HTML2text(int toHTML)
{
  nsresult rv = NS_OK;

  nsString inString;
  nsString outString;

  // Read in the string from the file: very inefficient, but who cares?
  char c;
  while ((c = getchar()) != EOF)
    inString += c;

  //printf("Input string is: %s\n-------------------- \n",
  //       inString.ToNewCString());

    // Create a parser
  nsIParser* parser;
   rv = nsComponentManager::CreateInstance(kParserCID, nsnull,
                                           kIParserIID,(void**)&parser);
  if (NS_FAILED(rv))
  {
    printf("Unable to create a parser : 0x%x\n", rv);
    return NS_ERROR_FAILURE;
  }

  nsIHTMLContentSink* sink = nsnull;

  if (toHTML)
    rv = NS_New_HTML_ContentSinkStream(&sink, &outString, 0);

  else  // default to plaintext
    rv = NS_New_HTMLToTXT_SinkStream(&sink, &outString, 72, 0);

  if (NS_FAILED(rv))
  {
    printf("Couldn't create new content sink: 0x%x\n", rv);
    return rv;
  }

  parser->SetContentSink(sink);
  nsIDTD* dtd = nsnull;
  rv = NS_NewNavHTMLDTD(&dtd);
  if (NS_FAILED(rv))
  {
    printf("Couldn't create new content sink: 0x%x\n", rv);
    return rv;
  }

  parser->RegisterDTD(dtd);

  parser->Parse(inString, 0, "text/html", PR_FALSE, PR_TRUE);

  printf("Output string is: %s\n-------------------- \n",
         outString.ToNewCString());

  NS_IF_RELEASE(dtd);
  NS_IF_RELEASE(sink);
  NS_RELEASE(parser);

  return rv;
}

//----------------------------------------------------------------------

int main(int argc, char** argv)
{
  NS_SetupRegistry();

  if (argc > 1 && argv[1][0] == '-' && argv[1][1] == 'h')
    HTML2text(1);
  else
    HTML2text(0);
}
