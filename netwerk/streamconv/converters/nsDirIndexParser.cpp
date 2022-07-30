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
#include "nsITextToSubURI.h"
#include "nsServiceManagerUtils.h"
#include "mozilla/intl/LocaleService.h"

using namespace mozilla;

struct EncodingProp {
  const char* const mKey;
  NotNull<const Encoding*> mValue;
};

static StaticRefPtr<nsITextToSubURI> gTextToSubURI;

static const EncodingProp localesFallbacks[] = {
    {"ar", WINDOWS_1256_ENCODING}, {"ba", WINDOWS_1251_ENCODING},
    {"be", WINDOWS_1251_ENCODING}, {"bg", WINDOWS_1251_ENCODING},
    {"cs", WINDOWS_1250_ENCODING}, {"el", ISO_8859_7_ENCODING},
    {"et", WINDOWS_1257_ENCODING}, {"fa", WINDOWS_1256_ENCODING},
    {"he", WINDOWS_1255_ENCODING}, {"hr", WINDOWS_1250_ENCODING},
    {"hu", ISO_8859_2_ENCODING},   {"ja", SHIFT_JIS_ENCODING},
    {"kk", WINDOWS_1251_ENCODING}, {"ko", EUC_KR_ENCODING},
    {"ku", WINDOWS_1254_ENCODING}, {"ky", WINDOWS_1251_ENCODING},
    {"lt", WINDOWS_1257_ENCODING}, {"lv", WINDOWS_1257_ENCODING},
    {"mk", WINDOWS_1251_ENCODING}, {"pl", ISO_8859_2_ENCODING},
    {"ru", WINDOWS_1251_ENCODING}, {"sah", WINDOWS_1251_ENCODING},
    {"sk", WINDOWS_1250_ENCODING}, {"sl", ISO_8859_2_ENCODING},
    {"sr", WINDOWS_1251_ENCODING}, {"tg", WINDOWS_1251_ENCODING},
    {"th", WINDOWS_874_ENCODING},  {"tr", WINDOWS_1254_ENCODING},
    {"tt", WINDOWS_1251_ENCODING}, {"uk", WINDOWS_1251_ENCODING},
    {"vi", WINDOWS_1258_ENCODING}, {"zh", GBK_ENCODING}};

static NotNull<const Encoding*>
GetFTPFallbackEncodingDoNotAddNewCallersToThisFunction() {
  nsAutoCString locale;
  mozilla::intl::LocaleService::GetInstance()->GetAppLocaleAsBCP47(locale);

  // Let's lower case the string just in case unofficial language packs
  // don't stick to conventions.
  ToLowerCase(locale);  // ASCII lowercasing with CString input!

  // Special case Traditional Chinese before throwing away stuff after the
  // language itself. Today we only ship zh-TW, but be defensive about
  // possible future values.
  if (locale.EqualsLiteral("zh-tw") || locale.EqualsLiteral("zh-hk") ||
      locale.EqualsLiteral("zh-mo") || locale.EqualsLiteral("zh-hant")) {
    return BIG5_ENCODING;
  }

  // Throw away regions and other variants to accommodate weird stuff seen
  // in telemetry--apparently unofficial language packs.
  int32_t hyphenIndex = locale.FindChar('-');
  if (hyphenIndex >= 0) {
    locale.Truncate(hyphenIndex);
  }

  size_t index;
  if (BinarySearchIf(
          localesFallbacks, 0, ArrayLength(localesFallbacks),
          [&locale](const EncodingProp& aProperty) {
            return Compare(locale, nsDependentCString(aProperty.mKey));
          },
          &index)) {
    return localesFallbacks[index].mValue;
  }
  return WINDOWS_1252_ENCODING;
}

NS_IMPL_ISUPPORTS(nsDirIndexParser, nsIRequestObserver, nsIStreamListener,
                  nsIDirIndexParser)

nsresult nsDirIndexParser::Init() {
  mLineStart = 0;
  mHasDescription = false;
  mFormat[0] = -1;
  auto encoding = GetFTPFallbackEncodingDoNotAddNewCallersToThisFunction();
  encoding->Name(mEncoding);

  nsresult rv = NS_OK;
  if (!gTextToSubURI) {
    nsCOMPtr<nsITextToSubURI> service =
        do_GetService(NS_ITEXTTOSUBURI_CONTRACTID, &rv);
    if (NS_SUCCEEDED(rv)) {
      gTextToSubURI = service;
      ClearOnShutdown(&gTextToSubURI);
    }
  }

  return rv;
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
nsDirIndexParser::GetComment(char** aComment) {
  *aComment = ToNewCString(mComment, mozilla::fallible);

  if (!*aComment) return NS_ERROR_OUT_OF_MEMORY;

  return NS_OK;
}

NS_IMETHODIMP
nsDirIndexParser::SetEncoding(const char* aEncoding) {
  mEncoding.Assign(aEncoding);
  return NS_OK;
}

NS_IMETHODIMP
nsDirIndexParser::GetEncoding(char** aEncoding) {
  *aEncoding = ToNewCString(mEncoding, mozilla::fallible);

  if (!*aEncoding) return NS_ERROR_OUT_OF_MEMORY;

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
    {"Description", FIELD_DESCRIPTION},
    {"Content-Length", FIELD_CONTENTLENGTH},
    {"Last-Modified", FIELD_LASTMODIFIED},
    {"Content-Type", FIELD_CONTENTTYPE},
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

    // All tokens are case-insensitive -
    // http://www.mozilla.org/projects/netlib/dirindexformat.html
    if (name.LowerCaseEqualsLiteral("description")) mHasDescription = true;

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

        bool success = false;

        nsAutoString entryuri;

        if (RefPtr<nsITextToSubURI> textToSub = gTextToSubURI) {
          nsAutoString result;
          if (NS_SUCCEEDED(
                  textToSub->UnEscapeAndConvert(mEncoding, filename, result))) {
            if (!result.IsEmpty()) {
              aIdx->SetLocation(filename);
              if (!mHasDescription) aIdx->SetDescription(result);
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
          aIdx->SetLocation(filename);
          if (!mHasDescription) {
            aIdx->SetDescription(NS_ConvertUTF8toUTF16(value));
          }
        }
      } break;
      case FIELD_DESCRIPTION:
        nsUnescape(value);
        aIdx->SetDescription(NS_ConvertUTF8toUTF16(value));
        break;
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
      case FIELD_CONTENTTYPE:
        aIdx->SetContentType(nsDependentCString(value));
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
}

NS_IMETHODIMP
nsDirIndexParser::OnDataAvailable(nsIRequest* aRequest, nsIInputStream* aStream,
                                  uint64_t aSourceOffset, uint32_t aCount) {
  if (aCount < 1) return NS_OK;

  int32_t len = mBuf.Length();

  // Ensure that our mBuf has capacity to hold the data we're about to
  // read.
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
            mComment.Append(buf + 4);

            char* value = ((char*)buf) + 4;
            nsUnescape(value);
            mListener->OnInformationAvailable(aRequest,
                                              NS_ConvertUTF8toUTF16(value));

          } else if (buf[2] == '2' && buf[3] == ':') {
            // 102. Human-readable information line, HTML.
            mComment.Append(buf + 4);
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
      } else if (buf[0] == '3') {
        if (buf[1] == '0') {
          if (buf[2] == '0' && buf[3] == ':') {
            // 300. Self-referring URL
          } else if (buf[2] == '1' && buf[3] == ':') {
            // 301. OUR EXTENSION - encoding
            int i = 4;
            while (buf[i] && nsCRT::IsAsciiSpace(buf[i])) ++i;

            if (buf[i]) SetEncoding(buf + i);
          }
        }
      }
    }
  }

  return NS_OK;
}
