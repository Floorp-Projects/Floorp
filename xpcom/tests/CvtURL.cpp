/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include <stdio.h>
#include "nscore.h"
#include "nsIUnicharInputStream.h"
#include "nsIURL.h"
#include "nsNetUtil.h"
#include "nsCRT.h"
#include "nsString.h"
#include "prprf.h"
#include "prtime.h"

static nsString* ConvertCharacterSetName(const char* aName)
{
  return new nsString(NS_ConvertASCIItoUCS2(aName));
}

int main(int argc, char** argv)
{
  if (3 != argc) {
    printf("usage: CvtURL url character-set-name\n");
    return -1;
  }

  char* characterSetName = argv[2];
  nsString* cset = ConvertCharacterSetName(characterSetName);
  if (NS_PTR_TO_INT32(cset) < 0) {
    printf("illegal character set name: '%s'\n", characterSetName);
    return -1;
  }

  // Create url object
  char* urlName = argv[1];
  nsIURI* url;
  nsresult rv;
  rv = NS_NewURI(&url, urlName);
  if (NS_OK != rv) {
    printf("invalid URL: '%s'\n", urlName);
    return -1;
  }

  // Get an input stream from the url
  nsresult ec;
  nsIInputStream* in;
  ec = NS_OpenURI(&in, url);
  if (nsnull == in) {
    printf("open of url('%s') failed: error=%x\n", urlName, ec);
    return -1;
  }

  // Translate the input using the argument character set id into unicode
  nsIUnicharInputStream* uin;
  rv = NS_NewConverterStream(&uin, nsnull, in, 0, cset);
  if (NS_OK != rv) {
    printf("can't create converter input stream: %d\n", rv);
    return -1;
  }

  // Read the input and write some output
  PRTime start = PR_Now();
  PRInt32 count = 0;
  for (;;) {
    PRUnichar buf[1000];
    PRUint32 nb;
    ec = uin->Read(buf, 0, 1000, &nb);
    if (NS_FAILED(ec)) {
      printf("i/o error: %d\n", ec);
      break;
    }
    if (nb == 0) break; // EOF
    count += nb;
  }
  PRTime end = PR_Now();
  PRTime conversion, ustoms;
  LL_I2L(ustoms, 1000);
  LL_SUB(conversion, end, start);
  LL_DIV(conversion, conversion, ustoms);
  char buf[500];
  PR_snprintf(buf, sizeof(buf),
              "converting and discarding %d bytes took %lldms",
              count, conversion);
  puts(buf);

  // Release the objects
  in->Release();
  uin->Release();
  url->Release();

  return 0;
}
