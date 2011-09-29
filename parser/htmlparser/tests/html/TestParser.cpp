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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsXPCOM.h"
#include "nsIComponentManager.h"
#include "nsComponentManagerUtils.h"
#include "nsParserCIID.h"
#include "nsIParser.h"
#include "nsILoggingSink.h"
#include "nsIInputStream.h"
#include "nsCOMPtr.h"

// Class IID's
static NS_DEFINE_CID(kParserCID, NS_PARSER_CID);
static NS_DEFINE_CID(kLoggingSinkCID, NS_LOGGING_SINK_CID);

//----------------------------------------------------------------------

nsresult ParseData(char* anInputStream,char* anOutputStream) {
  NS_ENSURE_ARG_POINTER(anInputStream);
  NS_ENSURE_ARG_POINTER(anOutputStream);
	
  nsresult result = NS_OK;

  // Create a parser
  nsCOMPtr<nsIParser> parser(do_CreateInstance(kParserCID, &result));
  if (NS_FAILED(result)) {
    printf("\nUnable to create a parser\n");
    return result;
  }
  // Create a sink
  nsCOMPtr<nsILoggingSink> sink(do_CreateInstance(kLoggingSinkCID, &result));
  if (NS_FAILED(result)) {
    printf("\nUnable to create a sink\n");
    return result;
  }

  PRFileDesc* in = PR_Open(anInputStream, PR_RDONLY, 0777);
  if (!in) {
    printf("\nUnable to open input file - %s\n", anInputStream);
    return result;
  }
  
  PRFileDesc* out = PR_Open(anOutputStream,
                            PR_CREATE_FILE|PR_TRUNCATE|PR_RDWR, 0777);
  if (!out) {
    printf("\nUnable to open output file - %s\n", anOutputStream);
    return result;
  }

  nsString stream;
  char buffer[1024] = {0}; // XXX Yikes!
  bool done = false;
  PRInt32 length = 0;
  while(!done) {
    length = PR_Read(in, buffer, sizeof(buffer));
    if (length != 0) {
      stream.Append(NS_ConvertUTF8toUTF16(buffer, length));
    }
    else {
      done=PR_TRUE;
    }
  }

  sink->SetOutputStream(out);
  parser->SetContentSink(sink);
  result = parser->Parse(stream, 0, NS_LITERAL_CSTRING("text/html"), PR_TRUE);
  
  PR_Close(in);
  PR_Close(out);

  return result;
}


//---------------------------------------------------------------------

int main(int argc, char** argv)
{
  if (argc < 3) {
    printf("\nUsage: <inputfile> <outputfile>\n"); 
    return -1;
  }

  nsresult rv = NS_InitXPCOM2(nsnull, nsnull, nsnull);
  if (NS_FAILED(rv)) {
    printf("NS_InitXPCOM2 failed\n");
    return -1;
  }

  ParseData(argv[1],argv[2]);

  return 0;
}
