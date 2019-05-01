/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsFTPDirListingConv.h"
#include "nsMemory.h"
#include "plstr.h"
#include "mozilla/Logging.h"
#include "nsCOMPtr.h"
#include "nsEscape.h"
#include "nsStringStream.h"
#include "nsIStreamListener.h"
#include "nsCRT.h"
#include "nsIChannel.h"
#include "nsIURI.h"
#include "nsIURIMutator.h"

#include "ParseFTPList.h"
#include <algorithm>

#include "mozilla/UniquePtrExtensions.h"
#include "mozilla/Unused.h"

//
// Log module for FTP dir listing stream converter logging...
//
// To enable logging (see prlog.h for full details):
//
//    set MOZ_LOG=nsFTPDirListConv:5
//    set MOZ_LOG_FILE=network.log
//
// This enables LogLevel::Debug level information and places all output in
// the file network.log.
//
static mozilla::LazyLogModule gFTPDirListConvLog("nsFTPDirListingConv");
using namespace mozilla;

// nsISupports implementation
NS_IMPL_ISUPPORTS(nsFTPDirListingConv, nsIStreamConverter, nsIStreamListener,
                  nsIRequestObserver)

// nsIStreamConverter implementation
NS_IMETHODIMP
nsFTPDirListingConv::Convert(nsIInputStream* aFromStream, const char* aFromType,
                             const char* aToType, nsISupports* aCtxt,
                             nsIInputStream** _retval) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

// Stream converter service calls this to initialize the actual stream converter
// (us).
NS_IMETHODIMP
nsFTPDirListingConv::AsyncConvertData(const char* aFromType,
                                      const char* aToType,
                                      nsIStreamListener* aListener,
                                      nsISupports* aCtxt) {
  NS_ASSERTION(aListener && aFromType && aToType,
               "null pointer passed into FTP dir listing converter");

  // hook up our final listener. this guy gets the various On*() calls we want
  // to throw at him.
  mFinalListener = aListener;
  NS_ADDREF(mFinalListener);

  MOZ_LOG(gFTPDirListConvLog, LogLevel::Debug,
          ("nsFTPDirListingConv::AsyncConvertData() converting FROM raw, TO "
           "application/http-index-format\n"));

  return NS_OK;
}

// nsIStreamListener implementation
NS_IMETHODIMP
nsFTPDirListingConv::OnDataAvailable(nsIRequest* request, nsIInputStream* inStr,
                                     uint64_t sourceOffset, uint32_t count) {
  NS_ASSERTION(request, "FTP dir listing stream converter needs a request");

  nsresult rv;

  nsCOMPtr<nsIChannel> channel = do_QueryInterface(request, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  uint32_t read, streamLen;

  uint64_t streamLen64;
  rv = inStr->Available(&streamLen64);
  NS_ENSURE_SUCCESS(rv, rv);
  streamLen = (uint32_t)std::min(streamLen64, uint64_t(UINT32_MAX - 1));

  auto buffer = MakeUniqueFallible<char[]>(streamLen + 1);
  NS_ENSURE_TRUE(buffer, NS_ERROR_OUT_OF_MEMORY);

  rv = inStr->Read(buffer.get(), streamLen, &read);
  NS_ENSURE_SUCCESS(rv, rv);

  // the dir listings are ascii text, null terminate this sucker.
  buffer[streamLen] = '\0';

  MOZ_LOG(gFTPDirListConvLog, LogLevel::Debug,
          ("nsFTPDirListingConv::OnData(request = %p, inStr = %p, "
           "sourceOffset = %" PRIu64 ", count = %u)\n",
           request, inStr, sourceOffset, count));

  if (!mBuffer.IsEmpty()) {
    // we have data left over from a previous OnDataAvailable() call.
    // combine the buffers so we don't lose any data.
    mBuffer.Append(buffer.get());

    buffer = MakeUniqueFallible<char[]>(mBuffer.Length() + 1);
    NS_ENSURE_TRUE(buffer, NS_ERROR_OUT_OF_MEMORY);

    strncpy(buffer.get(), mBuffer.get(), mBuffer.Length() + 1);
    mBuffer.Truncate();
  }

  MOZ_LOG(gFTPDirListConvLog, LogLevel::Debug,
          ("::OnData() received the following %d bytes...\n\n%s\n\n", streamLen,
           buffer.get()));

  nsAutoCString indexFormat;
  if (!mSentHeading) {
    // build up the 300: line
    nsCOMPtr<nsIURI> uri;
    rv = channel->GetURI(getter_AddRefs(uri));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = GetHeaders(indexFormat, uri);
    NS_ENSURE_SUCCESS(rv, rv);

    mSentHeading = true;
  }

  char* line = buffer.get();
  line = DigestBufferLines(line, indexFormat);

  MOZ_LOG(gFTPDirListConvLog, LogLevel::Debug,
          ("::OnData() sending the following %d bytes...\n\n%s\n\n",
           indexFormat.Length(), indexFormat.get()));

  // if there's any data left over, buffer it.
  if (line && *line) {
    mBuffer.Append(line);
    MOZ_LOG(gFTPDirListConvLog, LogLevel::Debug,
            ("::OnData() buffering the following %zu bytes...\n\n%s\n\n",
             strlen(line), line));
  }

  // send the converted data out.
  nsCOMPtr<nsIInputStream> inputData;

  rv = NS_NewCStringInputStream(getter_AddRefs(inputData), indexFormat);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mFinalListener->OnDataAvailable(request, inputData, 0,
                                       indexFormat.Length());

  return rv;
}

// nsIRequestObserver implementation
NS_IMETHODIMP
nsFTPDirListingConv::OnStartRequest(nsIRequest* request) {
  // we don't care about start. move along... but start masqeurading
  // as the http-index channel now.
  return mFinalListener->OnStartRequest(request);
}

NS_IMETHODIMP
nsFTPDirListingConv::OnStopRequest(nsIRequest* request, nsresult aStatus) {
  // we don't care about stop. move along...

  return mFinalListener->OnStopRequest(request, aStatus);
}

// nsFTPDirListingConv methods
nsFTPDirListingConv::nsFTPDirListingConv() {
  mFinalListener = nullptr;
  mSentHeading = false;
}

nsFTPDirListingConv::~nsFTPDirListingConv() { NS_IF_RELEASE(mFinalListener); }

nsresult nsFTPDirListingConv::GetHeaders(nsACString& headers, nsIURI* uri) {
  nsresult rv = NS_OK;
  // build up 300 line
  headers.AppendLiteral("300: ");

  // Bug 111117 - don't print the password
  nsAutoCString pw;
  nsAutoCString spec;
  uri->GetPassword(pw);
  if (!pw.IsEmpty()) {
    nsCOMPtr<nsIURI> noPassURI;
    rv = NS_MutateURI(uri).SetPassword(EmptyCString()).Finalize(noPassURI);
    if (NS_FAILED(rv)) return rv;
    rv = noPassURI->GetAsciiSpec(spec);
    if (NS_FAILED(rv)) return rv;
    headers.Append(spec);
  } else {
    rv = uri->GetAsciiSpec(spec);
    if (NS_FAILED(rv)) return rv;

    headers.Append(spec);
  }
  headers.Append(char(nsCRT::LF));
  // END 300:

  // build up the column heading; 200:
  headers.AppendLiteral(
      "200: filename content-length last-modified file-type\n");
  // END 200:
  return rv;
}

char* nsFTPDirListingConv::DigestBufferLines(char* aBuffer,
                                             nsCString& aString) {
  char* line = aBuffer;
  char* eol;
  bool cr = false;

  list_state state;

  // while we have new lines, parse 'em into application/http-index-format.
  while (line && (eol = PL_strchr(line, nsCRT::LF))) {
    // yank any carriage returns too.
    if (eol > line && *(eol - 1) == nsCRT::CR) {
      eol--;
      *eol = '\0';
      cr = true;
    } else {
      *eol = '\0';
      cr = false;
    }

    list_result result;

    int type = ParseFTPList(line, &state, &result);

    // if it is other than a directory, file, or link -OR- if it is a
    // directory named . or .., skip over this line.
    if ((type != 'd' && type != 'f' && type != 'l') ||
        (result.fe_type == 'd' && result.fe_fname[0] == '.' &&
         (result.fe_fnlen == 1 ||
          (result.fe_fnlen == 2 && result.fe_fname[1] == '.')))) {
      if (cr)
        line = eol + 2;
      else
        line = eol + 1;

      continue;
    }

    // blast the index entry into the indexFormat buffer as a 201: line.
    aString.AppendLiteral("201: ");
    // FILENAME

    // parsers for styles 'U' and 'W' handle sequence " -> " themself
    if (state.lstyle != 'U' && state.lstyle != 'W') {
      const char* offset = strstr(result.fe_fname, " -> ");
      if (offset) {
        result.fe_fnlen = offset - result.fe_fname;
      }
    }

    nsAutoCString buf;
    aString.Append('\"');
    aString.Append(NS_EscapeURL(
        Substring(result.fe_fname, result.fe_fname + result.fe_fnlen),
        esc_Minimal | esc_OnlyASCII | esc_Forced, buf));
    aString.AppendLiteral("\" ");

    // CONTENT LENGTH

    if (type != 'd') {
      for (char& fe : result.fe_size) {
        if (fe != '\0') aString.Append((const char*)&fe, 1);
      }

      aString.Append(' ');
    } else
      aString.AppendLiteral("0 ");

    // MODIFIED DATE
    char buffer[256] = "";

    // ParseFTPList can return time structure with invalid values.
    // PR_NormalizeTime will set all values into valid limits.
    result.fe_time.tm_params.tp_gmt_offset = 0;
    result.fe_time.tm_params.tp_dst_offset = 0;
    PR_NormalizeTime(&result.fe_time, PR_GMTParameters);

    // Note: The below is the RFC822/1123 format, as required by
    // the application/http-index-format specs
    // viewers of such a format can then reformat this into the
    // current locale (or anything else they choose)
    PR_FormatTimeUSEnglish(buffer, sizeof(buffer), "%a, %d %b %Y %H:%M:%S",
                           &result.fe_time);

    nsAutoCString escaped;
    Unused << NS_WARN_IF(
        !NS_Escape(nsDependentCString(buffer), escaped, url_Path));
    aString.Append(escaped);
    aString.Append(' ');

    // ENTRY TYPE
    if (type == 'd')
      aString.AppendLiteral("DIRECTORY");
    else if (type == 'l')
      aString.AppendLiteral("SYMBOLIC-LINK");
    else
      aString.AppendLiteral("FILE");

    aString.Append(' ');

    aString.Append(char(nsCRT::LF));  // complete this line
    // END 201:

    if (cr)
      line = eol + 2;
    else
      line = eol + 1;
  }  // end while(eol)

  return line;
}

nsresult NS_NewFTPDirListingConv(nsFTPDirListingConv** aFTPDirListingConv) {
  MOZ_ASSERT(aFTPDirListingConv != nullptr, "null ptr");
  if (!aFTPDirListingConv) return NS_ERROR_NULL_POINTER;

  *aFTPDirListingConv = new nsFTPDirListingConv();
  if (!*aFTPDirListingConv) return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*aFTPDirListingConv);
  return NS_OK;
}
