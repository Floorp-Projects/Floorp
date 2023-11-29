/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* This parsing code originally lived in xpfe/components/directory/ - bbaetz */

#include "nsDirIndexParser.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Encoding.h"
#include "mozilla/StaticPtr.h"
#include "prprf.h"
#include "nsCRT.h"
#include "nsDirIndex.h"
#include "nsEscape.h"
#include "nsIDirIndex.h"
#include "nsIInputStream.h"

using namespace mozilla;

NS_IMPL_ISUPPORTS(nsDirIndexParser, nsIRequestObserver, nsIStreamListener,
                  nsIDirIndexParser)

void nsDirIndexParser::Init() {
  mLineStart = 0;
  mFormat[0] = -1;
}

NS_IMETHODIMP
nsDirIndexParser::SetListener(nsIDirIndexListener* aListener) {
  mListener = aListener;
  return NS_OK;
}

NS_IMETHODIMP
nsDirIndexParser::GetListener(nsIDirIndexListener** aListener) {
  *aListener = do_AddRef(mListener).take();
  return NS_OK;
}

NS_IMETHODIMP
nsDirIndexParser::OnStartRequest(nsIRequest* aRequest) { return NS_OK; }

NS_IMETHODIMP
nsDirIndexParser::OnStopRequest(nsIRequest* aRequest, nsresult aStatusCode) {
  // Finish up
  if (mBuf.Length() > (uint32_t)mLineStart) {
    ProcessData(aRequest);
  }

  return NS_OK;
}

nsDirIndexParser::Field nsDirIndexParser::gFieldTable[] = {
    {"Filename", FIELD_FILENAME},
    {"Content-Length", FIELD_CONTENTLENGTH},
    {"Last-Modified", FIELD_LASTMODIFIED},
    {"File-Type", FIELD_FILETYPE},
    {nullptr, FIELD_UNKNOWN}};

void nsDirIndexParser::ParseFormat(const char* aFormatStr) {
  // Parse a "200" format line, and remember the fields and their
  // ordering in mFormat. Multiple 200 lines stomp on each other.
  unsigned int formatNum = 0;
  mFormat[0] = -1;

  do {
    while (*aFormatStr && nsCRT::IsAsciiSpace(char16_t(*aFormatStr))) {
      ++aFormatStr;
    }

    if (!*aFormatStr) break;

    nsAutoCString name;
    int32_t len = 0;
    while (aFormatStr[len] && !nsCRT::IsAsciiSpace(char16_t(aFormatStr[len]))) {
      ++len;
    }
    name.Append(aFormatStr, len);
    aFormatStr += len;

    // Okay, we're gonna monkey with the nsStr. Bold!
    name.SetLength(nsUnescapeCount(name.BeginWriting()));

    for (Field* i = gFieldTable; i->mName; ++i) {
      if (name.EqualsIgnoreCase(i->mName)) {
        mFormat[formatNum] = i->mType;
        mFormat[++formatNum] = -1;
        break;
      }
    }

  } while (*aFormatStr && (formatNum < (ArrayLength(mFormat) - 1)));
}

void nsDirIndexParser::ParseData(nsIDirIndex* aIdx, char* aDataStr,
                                 int32_t aLineLen) {
  // Parse a "201" data line, using the field ordering specified in
  // mFormat.

  if (mFormat[0] == -1) {
    // Ignore if we haven't seen a format yet.
    return;
  }

  nsAutoCString filename;
  int32_t lineLen = aLineLen;

  for (int32_t i = 0; mFormat[i] != -1; ++i) {
    // If we've exhausted the data before we run out of fields, just bail.
    if (!*aDataStr || (lineLen < 1)) {
      return;
    }

    while ((lineLen > 0) && nsCRT::IsAsciiSpace(*aDataStr)) {
      ++aDataStr;
      --lineLen;
    }

    if (lineLen < 1) {
      // invalid format, bail
      return;
    }

    char* value = aDataStr;
    if (*aDataStr == '"' || *aDataStr == '\'') {
      // it's a quoted string. snarf everything up to the next quote character
      const char quotechar = *(aDataStr++);
      lineLen--;
      ++value;
      while ((lineLen > 0) && *aDataStr != quotechar) {
        ++aDataStr;
        --lineLen;
      }
      if (lineLen > 0) {
        *aDataStr++ = '\0';
        --lineLen;
      }

      if (!lineLen) {
        // invalid format, bail
        return;
      }
    } else {
      // it's unquoted. snarf until we see whitespace.
      value = aDataStr;
      while ((lineLen > 0) && (!nsCRT::IsAsciiSpace(*aDataStr))) {
        ++aDataStr;
        --lineLen;
      }
      if (lineLen > 0) {
        *aDataStr++ = '\0';
        --lineLen;
      }
      // even if we ran out of line length here, there's still a trailing zero
      // byte afterwards
    }

    fieldType t = fieldType(mFormat[i]);
    switch (t) {
      case FIELD_FILENAME: {
        // don't unescape at this point, so that UnEscapeAndConvert() can
        filename = value;
        aIdx->SetLocation(filename);
      } break;
      case FIELD_CONTENTLENGTH: {
        int64_t len;
        int32_t status = PR_sscanf(value, "%lld", &len);
        if (status == 1) {
          aIdx->SetSize(len);
        } else {
          aIdx->SetSize(UINT64_MAX);  // UINT64_MAX means unknown
        }
      } break;
      case FIELD_LASTMODIFIED: {
        PRTime tm;
        nsUnescape(value);
        if (PR_ParseTimeString(value, false, &tm) == PR_SUCCESS) {
          aIdx->SetLastModified(tm);
        }
      } break;
      case FIELD_FILETYPE:
        // unescape in-place
        nsUnescape(value);
        if (!nsCRT::strcasecmp(value, "directory")) {
          aIdx->SetType(nsIDirIndex::TYPE_DIRECTORY);
        } else if (!nsCRT::strcasecmp(value, "file")) {
          aIdx->SetType(nsIDirIndex::TYPE_FILE);
        } else if (!nsCRT::strcasecmp(value, "symbolic-link")) {
          aIdx->SetType(nsIDirIndex::TYPE_SYMLINK);
        } else {
          aIdx->SetType(nsIDirIndex::TYPE_UNKNOWN);
        }
        break;
      case FIELD_UNKNOWN:
        // ignore
        break;
    }
  }
}

NS_IMETHODIMP
nsDirIndexParser::OnDataAvailable(nsIRequest* aRequest, nsIInputStream* aStream,
                                  uint64_t aSourceOffset, uint32_t aCount) {
  if (aCount < 1) return NS_OK;

  uint32_t len = mBuf.Length();

  // Ensure that our mBuf has capacity to hold the data we're about to
  // read.
  // Before adjusting the capacity, guard against any potential overflow
  // resulting from the addition of aCount with len. See Bug 1823551.
  NS_ENSURE_TRUE((UINT32_MAX - aCount) >= len, NS_ERROR_FAILURE);
  if (!mBuf.SetLength(len + aCount, fallible)) return NS_ERROR_OUT_OF_MEMORY;

  // Now read the data into our buffer.
  nsresult rv;
  uint32_t count;
  rv = aStream->Read(mBuf.BeginWriting() + len, aCount, &count);
  if (NS_FAILED(rv)) return rv;

  // Set the string's length according to the amount of data we've read.
  // Note: we know this to work on nsCString. This isn't guaranteed to
  //       work on other strings.
  mBuf.SetLength(len + count);

  return ProcessData(aRequest);
}

nsresult nsDirIndexParser::ProcessData(nsIRequest* aRequest) {
  if (!mListener) return NS_ERROR_FAILURE;

  while (true) {
    int32_t eol = mBuf.FindCharInSet("\n\r", mLineStart);
    if (eol < 0) break;
    mBuf.SetCharAt(char16_t('\0'), eol);

    const char* line = mBuf.get() + mLineStart;

    int32_t lineLen = eol - mLineStart;
    mLineStart = eol + 1;

    if (lineLen >= 4) {
      const char* buf = line;

      if (buf[0] == '1') {
        if (buf[1] == '0') {
          if (buf[2] == '0' && buf[3] == ':') {
            // 100. Human-readable comment line. Ignore
          } else if (buf[2] == '1' && buf[3] == ':') {
            // 101. Human-readable information line.
            char* value = ((char*)buf) + 4;
            nsUnescape(value);
            mListener->OnInformationAvailable(aRequest,
                                              NS_ConvertUTF8toUTF16(value));
          }
        }
      } else if (buf[0] == '2') {
        if (buf[1] == '0') {
          if (buf[2] == '0' && buf[3] == ':') {
            // 200. Define field names
            ParseFormat(buf + 4);
          } else if (buf[2] == '1' && buf[3] == ':') {
            // 201. Field data
            nsCOMPtr<nsIDirIndex> idx = new nsDirIndex();

            ParseData(idx, ((char*)buf) + 4, lineLen - 4);
            mListener->OnIndexAvailable(aRequest, idx);
          }
        }
      }
    }
  }

  return NS_OK;
}
