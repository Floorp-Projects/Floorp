/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <stdio.h>
#include "nscore.h"
#include "nsIConverterInputStream.h"
#include "nsIURL.h"
#include "nsNetUtil.h"
#include "nsCRT.h"
#include "nsString.h"
#include "prprf.h"
#include "prtime.h"

static nsString* ConvertCharacterSetName(const char* aName)
{
  return new nsString(NS_ConvertASCIItoUTF16(aName));
}

int main(int argc, char** argv)
{
  if (3 != argc) {
    printf("usage: CvtURL url utf8\n");
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
  nsIInputStream* in;
  nsresult ec = NS_OpenURI(&in,
                           url,
                           nsContentUtils::GetSystemPrincipal(),
                           nsILoadInfo::SEC_NORMAL,
                           nsIContentPolicy::TYPE_OTHER);

  if (nullptr == in) {
    printf("open of url('%s') failed: error=%x\n", urlName, ec);
    return -1;
  }

  // Translate the input using the argument character set id into
  // unicode
  nsCOMPtr<nsIConverterInputStream> uin =
    do_CreateInstance("@mozilla.org/intl/converter-input-stream;1", &rv);
  if (NS_SUCCEEDED(rv))
    rv = uin->Init(in, cset->get(), 4096);
  if (NS_FAILED(rv)) {
    printf("can't create converter input stream: %d\n", rv);
    return -1;
  }

  // Read the input and write some output
  PRTime start = PR_Now();
  int32_t count = 0;
  for (;;) {
    char16_t buf[1000];
    uint32_t nb;
    ec = uin->Read(buf, 0, 1000, &nb);
    if (NS_FAILED(ec)) {
      printf("i/o error: %d\n", ec);
      break;
    }
    if (nb == 0) break; // EOF
    count += nb;
  }
  PRTime end = PR_Now();
  PRTime conversion = (end - start) / 1000;
  char buf[500];
  PR_snprintf(buf, sizeof(buf),
              "converting and discarding %d bytes took %lldms",
              count, conversion);
  puts(buf);

  // Release the objects
  in->Release();
  url->Release();

  return 0;
}
