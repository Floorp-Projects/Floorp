/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Akkana Peck.
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

#include <ctype.h>      // for isdigit()

#include "nsXPCOM.h"
#include "nsParserCIID.h"
#include "nsIParser.h"
#include "nsIHTMLContentSink.h"
#include "nsIContentSerializer.h"
#include "nsLayoutCID.h"
#include "nsIHTMLToTextSink.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsIComponentRegistrar.h"
#include "nsReadableUtils.h"
#include "nsCRT.h"

static NS_DEFINE_IID(kIParserIID, NS_IPARSER_IID);
static NS_DEFINE_CID(kParserCID, NS_PARSER_CID);

int
Compare(nsString& str, nsString& aFileName)
{
  // Open the file in a Unix-centric way,
  // until I find out how to use nsFileSpec:
  char* filename = ToNewCString(aFileName);
  FILE* file = fopen(filename, "r");
  if (!file)
  {
    fprintf(stderr, "Can't open file %s", filename);
    perror(" ");
    delete[] filename;
    return 2;
  }
  delete[] filename;

  // Inefficiently read from the file:
  nsString inString;
  int c;
  int index = 0;
  int different = 0;
  while ((c = getc(file)) != EOF)
  {
    inString.Append(PRUnichar(c));
    // CVS isn't doing newline comparisons on these files for some reason.
    // So compensate for possible newline problems in the CVS file:
    if (c == '\n' && str[index] == '\r')
      ++index;
    if (c != str[index++])
    {
      //printf("Comparison failed at char %d: generated was %d, file had %d\n",
      //       index, (int)str[index-1], (int)c);
      different = index;
      break;
    }
  }
  if (file != stdin)
    fclose(file);

  if (!different)
    return 0;
  else
  {
    nsAutoString left;
    str.Left(left, different);
    char* cstr = ToNewUTF8String(left);
    printf("Comparison failed at char %d:\n-----\n%s\n-----\n",
           different, cstr);
    Recycle(cstr);
    return 1;
  }
}

//----------------------------------------------------------------------
// Convert html on stdin to either plaintext or (if toHTML) html
//----------------------------------------------------------------------
nsresult
HTML2text(nsString& inString, nsString& inType, nsString& outType,
          int flags, int wrapCol, nsString& compareAgainst)
{
  nsresult rv = NS_OK;

  nsString outString;

  // Create a parser
  nsIParser* parser;
  rv = CallCreateInstance(kParserCID, &parser);
  if (NS_FAILED(rv))
  {
    printf("Unable to create a parser : 0x%x\n", rv);
    return NS_ERROR_FAILURE;
  }

  // Create the appropriate output sink
#ifdef USE_SERIALIZER
  nsCAutoString progId(NS_CONTENTSERIALIZER_CONTRACTID_PREFIX);
  progId.AppendWithConversion(outType);

  // The syntax used here doesn't work
  nsCOMPtr<nsIContentSerializer> mSerializer;
  mSerializer = do_CreateInstance(NS_STATIC_CAST(const char *, progId));
  NS_ENSURE_TRUE(mSerializer, NS_ERROR_NOT_IMPLEMENTED);

  mSerializer->Init(flags, wrapCol);

  nsCOMPtr<nsIHTMLContentSink> sink (do_QueryInterface(mSerializer));
  if (!sink)
  {
    printf("Couldn't get content sink!\n");
    return NS_ERROR_UNEXPECTED;
  }
#else /* USE_SERIALIZER */
  nsCOMPtr<nsIContentSink> sink;
  if (!inType.EqualsLiteral("text/html")
      || !outType.EqualsLiteral("text/plain"))
  {
    char* in = ToNewCString(inType);
    char* out = ToNewCString(outType);
    printf("Don't know how to convert from %s to %s\n", in, out);
    Recycle(in);
    Recycle(out);
    return NS_ERROR_FAILURE;
  }

  sink = do_CreateInstance(NS_PLAINTEXTSINK_CONTRACTID);
  NS_ENSURE_TRUE(sink, NS_ERROR_FAILURE);

  nsCOMPtr<nsIHTMLToTextSink> textSink(do_QueryInterface(sink));
  NS_ENSURE_TRUE(textSink, NS_ERROR_FAILURE);

  textSink->Initialize(&outString, flags, wrapCol);
#endif /* USE_SERIALIZER */

  parser->SetContentSink(sink);
   nsCOMPtr<nsIDTD> dtd;
  if (inType.EqualsLiteral("text/html")) {
    static NS_DEFINE_CID(kNavDTDCID, NS_CNAVDTD_CID);
    dtd = do_CreateInstance(kNavDTDCID, &rv);
  }
  else
  {
    printf("Don't know how to deal with non-html input!\n");
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  if (NS_FAILED(rv))
  {
    printf("Couldn't create new HTML DTD: 0x%x\n", rv);
    return rv;
  }

  parser->RegisterDTD(dtd);

  rv = parser->Parse(inString, 0, NS_LossyConvertUCS2toASCII(inType), PR_FALSE, PR_TRUE);
  if (NS_FAILED(rv))
  {
    printf("Parse() failed! 0x%x\n", rv);
    return rv;
  }
  NS_RELEASE(parser);

  if (compareAgainst.Length() > 0)
    return Compare(outString, compareAgainst);

  char* charstar = ToNewUTF8String(outString);
  printf("Output string is:\n--------------------\n%s--------------------\n",
         charstar);
  delete[] charstar;

  return NS_OK;
}

//----------------------------------------------------------------------

int main(int argc, char** argv)
{
  nsString inType(NS_LITERAL_STRING("text/html"));
  nsString outType(NS_LITERAL_STRING("text/plain"));
  int wrapCol = 72;
  int flags = 0;
  nsString compareAgainst;


  // Skip over progname arg:
  const char* progname = argv[0];
  --argc; ++argv;

  // Process flags
  while (argc > 0 && argv[0][0] == '-')
  {
    switch (argv[0][1])
    {
      case 'h':
        printf("\
Usage: %s [-i intype] [-o outtype] [-f flags] [-w wrapcol] [-c comparison_file] infile\n\
\tIn/out types are mime types (e.g. text/html)\n\
\tcomparison_file is a file against which to compare the output\n\
\n\
\tDefaults are -i text/html -o text/plain -f 0 -w 72 [stdin]\n",
               progname);
        exit(0);

        case 'i':
        if (argv[0][2] != '\0')
          inType.AssignWithConversion(argv[0]+2);
        else {
          inType.AssignWithConversion(argv[1]);
          --argc;
          ++argv;
        }
        break;

      case 'o':
        if (argv[0][2] != '\0')
          outType.AssignWithConversion(argv[0]+2);
        else {
          outType.AssignWithConversion(argv[1]);
          --argc;
          ++argv;
        }
        break;

      case 'w':
        if (isdigit(argv[0][2]))
          wrapCol = atoi(argv[0]+2);
        else {
          wrapCol = atoi(argv[1]);
          --argc;
          ++argv;
        }
        break;

      case 'f':
        if (isdigit(argv[0][2]))
          flags = atoi(argv[0]+2);
        else {
          flags = atoi(argv[1]);
          --argc;
          ++argv;
        }
        break;

      case 'c':
        if (argv[0][2] != '\0')
          compareAgainst.AssignWithConversion(argv[0]+2);
        else {
          compareAgainst.AssignWithConversion(argv[1]);
          --argc;
          ++argv;
        }
        break;
    }
    ++argv;
    --argc;
  }

  FILE* file = 0;
  if (argc > 0)         // read from a file
  {
    // Open the file in a Unix-centric way,
    // until I find out how to use nsFileSpec:
    file = fopen(argv[0], "r");
    if (!file)
    {
      fprintf(stderr, "Can't open file %s", argv[0]);
      perror(" ");
      exit(1);
    }
  }
  else
    file = stdin;

  nsresult ret;
  {
    nsCOMPtr<nsIServiceManager> servMan;
    NS_InitXPCOM2(getter_AddRefs(servMan), nsnull, nsnull);
    nsCOMPtr<nsIComponentRegistrar> registrar = do_QueryInterface(servMan);
    NS_ASSERTION(registrar, "Null nsIComponentRegistrar");
    registrar->AutoRegister(nsnull);

    // Read in the string: very inefficient, but who cares?
    nsString inString;
    int c;
    while ((c = getc(file)) != EOF)
      inString.Append(PRUnichar(c));

    if (file != stdin)
      fclose(file);

    ret = HTML2text(inString, inType, outType, flags, wrapCol, compareAgainst);
  } // this scopes the nsCOMPtrs
  // no nsCOMPtrs are allowed to be alive when you call NS_ShutdownXPCOM
  nsresult rv = NS_ShutdownXPCOM( NULL );
  NS_ASSERTION(NS_SUCCEEDED(rv), "NS_ShutdownXPCOM failed");
  return ret;
}
