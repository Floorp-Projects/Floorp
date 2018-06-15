/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* This parsing code originally lived in xpfe/components/directory/ - bbaetz */

#include "nsDirIndexParser.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/dom/FallbackEncoding.h"
#include "mozilla/Encoding.h"
#include "prprf.h"
#include "nsCRT.h"
#include "nsEscape.h"
#include "nsIDirIndex.h"
#include "nsIInputStream.h"
#include "nsITextToSubURI.h"
#include "nsServiceManagerUtils.h"

using namespace mozilla;

NS_IMPL_ISUPPORTS(nsDirIndexParser,
                  nsIRequestObserver,
                  nsIStreamListener,
                  nsIDirIndexParser)

nsDirIndexParser::nsDirIndexParser()
  : mLineStart(0)
  , mHasDescription(false) {
}

nsresult
nsDirIndexParser::Init() {
  mLineStart = 0;
  mHasDescription = false;
  mFormat[0] = -1;
  auto encoding = mozilla::dom::FallbackEncoding::FromLocale();
  encoding->Name(mEncoding);

  nsresult rv;
  // XXX not threadsafe
  if (gRefCntParser++ == 0)
    rv = CallGetService(NS_ITEXTTOSUBURI_CONTRACTID, &gTextToSubURI);
  else
    rv = NS_OK;

  return rv;
}

nsDirIndexParser::~nsDirIndexParser() {
  // XXX not threadsafe
  if (--gRefCntParser == 0) {
    NS_IF_RELEASE(gTextToSubURI);
  }
}

NS_IMETHODIMP
nsDirIndexParser::SetListener(nsIDirIndexListener* aListener) {
  mListener = aListener;
  return NS_OK;
}

NS_IMETHODIMP
nsDirIndexParser::GetListener(nsIDirIndexListener** aListener) {
  NS_IF_ADDREF(*aListener = mListener.get());
  return NS_OK;
}

NS_IMETHODIMP
nsDirIndexParser::GetComment(char** aComment) {
  *aComment = ToNewCString(mComment);

  if (!*aComment)
    return NS_ERROR_OUT_OF_MEMORY;

  return NS_OK;
}

NS_IMETHODIMP
nsDirIndexParser::SetEncoding(const char* aEncoding) {
  mEncoding.Assign(aEncoding);
  return NS_OK;
}

NS_IMETHODIMP
nsDirIndexParser::GetEncoding(char** aEncoding) {
  *aEncoding = ToNewCString(mEncoding);

  if (!*aEncoding)
    return NS_ERROR_OUT_OF_MEMORY;

  return NS_OK;
}

NS_IMETHODIMP
nsDirIndexParser::OnStartRequest(nsIRequest* aRequest, nsISupports* aCtxt) {
  return NS_OK;
}

NS_IMETHODIMP
nsDirIndexParser::OnStopRequest(nsIRequest *aRequest, nsISupports *aCtxt,
                                nsresult aStatusCode) {
  // Finish up
  if (mBuf.Length() > (uint32_t) mLineStart) {
    ProcessData(aRequest, aCtxt);
  }

  return NS_OK;
}

nsDirIndexParser::Field
nsDirIndexParser::gFieldTable[] = {
  { "Filename", FIELD_FILENAME },
  { "Description", FIELD_DESCRIPTION },
  { "Content-Length", FIELD_CONTENTLENGTH },
  { "Last-Modified", FIELD_LASTMODIFIED },
  { "Content-Type", FIELD_CONTENTTYPE },
  { "File-Type", FIELD_FILETYPE },
  { nullptr, FIELD_UNKNOWN }
};

nsrefcnt nsDirIndexParser::gRefCntParser = 0;
nsITextToSubURI *nsDirIndexParser::gTextToSubURI;

nsresult
nsDirIndexParser::ParseFormat(const char* aFormatStr)
{
  // Parse a "200" format line, and remember the fields and their
  // ordering in mFormat. Multiple 200 lines stomp on each other.
  unsigned int formatNum = 0;
  mFormat[0] = -1;

  do {
    while (*aFormatStr && nsCRT::IsAsciiSpace(char16_t(*aFormatStr)))
      ++aFormatStr;

    if (! *aFormatStr)
      break;

    nsAutoCString name;
    int32_t     len = 0;
    while (aFormatStr[len] && !nsCRT::IsAsciiSpace(char16_t(aFormatStr[len])))
      ++len;
    name.SetCapacity(len + 1);
    name.Append(aFormatStr, len);
    aFormatStr += len;

    // Okay, we're gonna monkey with the nsStr. Bold!
    name.SetLength(nsUnescapeCount(name.BeginWriting()));

    // All tokens are case-insensitive - http://www.mozilla.org/projects/netlib/dirindexformat.html
    if (name.LowerCaseEqualsLiteral("description"))
      mHasDescription = true;

    for (Field* i = gFieldTable; i->mName; ++i) {
      if (name.EqualsIgnoreCase(i->mName)) {
        mFormat[formatNum] = i->mType;
        mFormat[++formatNum] = -1;
        break;
      }
    }

  } while (*aFormatStr && (formatNum < (ArrayLength(mFormat)-1)));

  return NS_OK;
}

nsresult
nsDirIndexParser::ParseData(nsIDirIndex *aIdx, char* aDataStr, int32_t aLineLen)
{
  // Parse a "201" data line, using the field ordering specified in
  // mFormat.

  if(mFormat[0] == -1) {
    // Ignore if we haven't seen a format yet.
    return NS_OK;
  }

  nsresult rv = NS_OK;
  nsAutoCString filename;
  int32_t lineLen = aLineLen;

  for (int32_t i = 0; mFormat[i] != -1; ++i) {
    // If we've exhausted the data before we run out of fields, just bail.
    if (!*aDataStr || (lineLen < 1)) {
      return NS_OK;
    }

    while ((lineLen > 0) && nsCRT::IsAsciiSpace(*aDataStr)) {
      ++aDataStr;
      --lineLen;
    }

    if (lineLen < 1) {
      // invalid format, bail
      return NS_OK;
    }

    char    *value = aDataStr;
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
        return NS_OK;
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

      bool    success = false;

      nsAutoString entryuri;

      if (gTextToSubURI) {
        nsAutoString result;
        if (NS_SUCCEEDED(rv = gTextToSubURI->UnEscapeAndConvert(
                           mEncoding, filename, result))) {
          if (!result.IsEmpty()) {
            aIdx->SetLocation(filename.get());
            if (!mHasDescription)
              aIdx->SetDescription(result.get());
            success = true;
          }
        } else {
          NS_WARNING("UnEscapeAndConvert error");
        }
      }

      if (!success) {
        // if unsuccessfully at charset conversion, then
        // just fallback to unescape'ing in-place
        // XXX - this shouldn't be using UTF8, should it?
        // when can we fail to get the service, anyway? - bbaetz
        aIdx->SetLocation(filename.get());
        if (!mHasDescription) {
          aIdx->SetDescription(NS_ConvertUTF8toUTF16(value).get());
        }
      }
    }
      break;
    case FIELD_DESCRIPTION:
      nsUnescape(value);
      aIdx->SetDescription(NS_ConvertUTF8toUTF16(value).get());
      break;
    case FIELD_CONTENTLENGTH:
      {
        int64_t len;
        int32_t status = PR_sscanf(value, "%lld", &len);
        if (status == 1)
          aIdx->SetSize(len);
        else
          aIdx->SetSize(UINT64_MAX); // UINT64_MAX means unknown
      }
      break;
    case FIELD_LASTMODIFIED:
      {
        PRTime tm;
        nsUnescape(value);
        if (PR_ParseTimeString(value, false, &tm) == PR_SUCCESS) {
          aIdx->SetLastModified(tm);
        }
      }
      break;
    case FIELD_CONTENTTYPE:
      aIdx->SetContentType(value);
      break;
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

  return NS_OK;
}

NS_IMETHODIMP
nsDirIndexParser::OnDataAvailable(nsIRequest *aRequest, nsISupports *aCtxt,
                                  nsIInputStream *aStream,
                                  uint64_t aSourceOffset,
                                  uint32_t aCount) {
  if (aCount < 1)
    return NS_OK;

  int32_t len = mBuf.Length();

  // Ensure that our mBuf has capacity to hold the data we're about to
  // read.
  if (!mBuf.SetLength(len + aCount, fallible))
    return NS_ERROR_OUT_OF_MEMORY;

  // Now read the data into our buffer.
  nsresult rv;
  uint32_t count;
  rv = aStream->Read(mBuf.BeginWriting() + len, aCount, &count);
  if (NS_FAILED(rv)) return rv;

  // Set the string's length according to the amount of data we've read.
  // Note: we know this to work on nsCString. This isn't guaranteed to
  //       work on other strings.
  mBuf.SetLength(len + count);

  return ProcessData(aRequest, aCtxt);
}

nsresult
nsDirIndexParser::ProcessData(nsIRequest *aRequest, nsISupports *aCtxt) {
  if (!mListener)
    return NS_ERROR_FAILURE;

  int32_t     numItems = 0;

  while(true) {
    ++numItems;

    int32_t             eol = mBuf.FindCharInSet("\n\r", mLineStart);
    if (eol < 0)        break;
    mBuf.SetCharAt(char16_t('\0'), eol);

    const char  *line = mBuf.get() + mLineStart;

    int32_t lineLen = eol - mLineStart;
    mLineStart = eol + 1;

    if (lineLen >= 4) {
      nsresult  rv;
      const char        *buf = line;

      if (buf[0] == '1') {
        if (buf[1] == '0') {
          if (buf[2] == '0' && buf[3] == ':') {
            // 100. Human-readable comment line. Ignore
          } else if (buf[2] == '1' && buf[3] == ':') {
            // 101. Human-readable information line.
            mComment.Append(buf + 4);

            char    *value = ((char *)buf) + 4;
            nsUnescape(value);
            mListener->OnInformationAvailable(aRequest, aCtxt, NS_ConvertUTF8toUTF16(value));

          } else if (buf[2] == '2' && buf[3] == ':') {
            // 102. Human-readable information line, HTML.
            mComment.Append(buf + 4);
          }
        }
      } else if (buf[0] == '2') {
        if (buf[1] == '0') {
          if (buf[2] == '0' && buf[3] == ':') {
            // 200. Define field names
            rv = ParseFormat(buf + 4);
            if (NS_FAILED(rv)) {
              return rv;
            }
          } else if (buf[2] == '1' && buf[3] == ':') {
            // 201. Field data
            nsCOMPtr<nsIDirIndex> idx = do_CreateInstance("@mozilla.org/dirIndex;1",&rv);
            if (NS_FAILED(rv))
              return rv;

            rv = ParseData(idx, ((char *)buf) + 4, lineLen - 4);
            if (NS_FAILED(rv)) {
              return rv;
            }

            mListener->OnIndexAvailable(aRequest, aCtxt, idx);
          }
        }
      } else if (buf[0] == '3') {
        if (buf[1] == '0') {
          if (buf[2] == '0' && buf[3] == ':') {
            // 300. Self-referring URL
          } else if (buf[2] == '1' && buf[3] == ':') {
            // 301. OUR EXTENSION - encoding
            int i = 4;
            while (buf[i] && nsCRT::IsAsciiSpace(buf[i]))
              ++i;

            if (buf[i])
              SetEncoding(buf+i);
          }
        }
      }
    }
  }

  return NS_OK;
}
