/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=4 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsURLHelper.h"

#include "mozilla/Encoding.h"
#include "mozilla/RangedPtr.h"
#include "mozilla/TextUtils.h"

#include <algorithm>
#include <iterator>

#include "nsASCIIMask.h"
#include "nsIFile.h"
#include "nsIURLParser.h"
#include "nsCOMPtr.h"
#include "nsCRT.h"
#include "nsNetCID.h"
#include "mozilla/Preferences.h"
#include "prnetdb.h"
#include "mozilla/StaticPrefs_network.h"
#include "mozilla/Tokenizer.h"
#include "nsEscape.h"
#include "nsDOMString.h"
#include "mozilla/net/rust_helper.h"
#include "mozilla/net/DNS.h"

using namespace mozilla;

//----------------------------------------------------------------------------
// Init/Shutdown
//----------------------------------------------------------------------------

static bool gInitialized = false;
static StaticRefPtr<nsIURLParser> gNoAuthURLParser;
static StaticRefPtr<nsIURLParser> gAuthURLParser;
static StaticRefPtr<nsIURLParser> gStdURLParser;

static void InitGlobals() {
  nsCOMPtr<nsIURLParser> parser;

  parser = do_GetService(NS_NOAUTHURLPARSER_CONTRACTID);
  NS_ASSERTION(parser, "failed getting 'noauth' url parser");
  if (parser) {
    gNoAuthURLParser = parser;
  }

  parser = do_GetService(NS_AUTHURLPARSER_CONTRACTID);
  NS_ASSERTION(parser, "failed getting 'auth' url parser");
  if (parser) {
    gAuthURLParser = parser;
  }

  parser = do_GetService(NS_STDURLPARSER_CONTRACTID);
  NS_ASSERTION(parser, "failed getting 'std' url parser");
  if (parser) {
    gStdURLParser = parser;
  }

  gInitialized = true;
}

void net_ShutdownURLHelper() {
  if (gInitialized) {
    gInitialized = false;
  }
  gNoAuthURLParser = nullptr;
  gAuthURLParser = nullptr;
  gStdURLParser = nullptr;
}

//----------------------------------------------------------------------------
// nsIURLParser getters
//----------------------------------------------------------------------------

nsIURLParser* net_GetAuthURLParser() {
  if (!gInitialized) InitGlobals();
  return gAuthURLParser;
}

nsIURLParser* net_GetNoAuthURLParser() {
  if (!gInitialized) InitGlobals();
  return gNoAuthURLParser;
}

nsIURLParser* net_GetStdURLParser() {
  if (!gInitialized) InitGlobals();
  return gStdURLParser;
}

//---------------------------------------------------------------------------
// GetFileFromURLSpec implementations
//---------------------------------------------------------------------------
nsresult net_GetURLSpecFromDir(nsIFile* aFile, nsACString& result) {
  nsAutoCString escPath;
  nsresult rv = net_GetURLSpecFromActualFile(aFile, escPath);
  if (NS_FAILED(rv)) return rv;

  if (escPath.Last() != '/') {
    escPath += '/';
  }

  result = escPath;
  return NS_OK;
}

nsresult net_GetURLSpecFromFile(nsIFile* aFile, nsACString& result) {
  nsAutoCString escPath;
  nsresult rv = net_GetURLSpecFromActualFile(aFile, escPath);
  if (NS_FAILED(rv)) return rv;

  // if this file references a directory, then we need to ensure that the
  // URL ends with a slash.  this is important since it affects the rules
  // for relative URL resolution when this URL is used as a base URL.
  // if the file does not exist, then we make no assumption about its type,
  // and simply leave the URL unmodified.
  if (escPath.Last() != '/') {
    bool dir;
    rv = aFile->IsDirectory(&dir);
    if (NS_SUCCEEDED(rv) && dir) escPath += '/';
  }

  result = escPath;
  return NS_OK;
}

//----------------------------------------------------------------------------
// file:// URL parsing
//----------------------------------------------------------------------------

nsresult net_ParseFileURL(const nsACString& inURL, nsACString& outDirectory,
                          nsACString& outFileBaseName,
                          nsACString& outFileExtension) {
  nsresult rv;

  if (inURL.Length() >
      (uint32_t)StaticPrefs::network_standard_url_max_length()) {
    return NS_ERROR_MALFORMED_URI;
  }

  outDirectory.Truncate();
  outFileBaseName.Truncate();
  outFileExtension.Truncate();

  const nsPromiseFlatCString& flatURL = PromiseFlatCString(inURL);
  const char* url = flatURL.get();

  nsAutoCString scheme;
  rv = net_ExtractURLScheme(flatURL, scheme);
  if (NS_FAILED(rv)) return rv;

  if (!scheme.EqualsLiteral("file")) {
    NS_ERROR("must be a file:// url");
    return NS_ERROR_UNEXPECTED;
  }

  nsIURLParser* parser = net_GetNoAuthURLParser();
  NS_ENSURE_TRUE(parser, NS_ERROR_UNEXPECTED);

  uint32_t pathPos, filepathPos, directoryPos, basenamePos, extensionPos;
  int32_t pathLen, filepathLen, directoryLen, basenameLen, extensionLen;

  // invoke the parser to extract the URL path
  rv = parser->ParseURL(url, flatURL.Length(), nullptr,
                        nullptr,           // don't care about scheme
                        nullptr, nullptr,  // don't care about authority
                        &pathPos, &pathLen);
  if (NS_FAILED(rv)) return rv;

  // invoke the parser to extract filepath from the path
  rv = parser->ParsePath(url + pathPos, pathLen, &filepathPos, &filepathLen,
                         nullptr, nullptr,   // don't care about query
                         nullptr, nullptr);  // don't care about ref
  if (NS_FAILED(rv)) return rv;

  filepathPos += pathPos;

  // invoke the parser to extract the directory and filename from filepath
  rv = parser->ParseFilePath(url + filepathPos, filepathLen, &directoryPos,
                             &directoryLen, &basenamePos, &basenameLen,
                             &extensionPos, &extensionLen);
  if (NS_FAILED(rv)) return rv;

  if (directoryLen > 0) {
    outDirectory = Substring(inURL, filepathPos + directoryPos, directoryLen);
  }
  if (basenameLen > 0) {
    outFileBaseName = Substring(inURL, filepathPos + basenamePos, basenameLen);
  }
  if (extensionLen > 0) {
    outFileExtension =
        Substring(inURL, filepathPos + extensionPos, extensionLen);
  }
  // since we are using a no-auth url parser, there will never be a host
  // XXX not strictly true... file://localhost/foo/bar.html is a valid URL

  return NS_OK;
}

//----------------------------------------------------------------------------
// path manipulation functions
//----------------------------------------------------------------------------

// Replace all /./ with a / while resolving URLs
// But only till #?
void net_CoalesceDirs(netCoalesceFlags flags, char* path) {
  /* Stolen from the old netlib's mkparse.c.
   *
   * modifies a url of the form   /foo/../foo1  ->  /foo1
   *                       and    /foo/./foo1   ->  /foo/foo1
   *                       and    /foo/foo1/..  ->  /foo/
   */
  char* fwdPtr = path;
  char* urlPtr = path;
  char* lastslash = path;
  uint32_t traversal = 0;
  uint32_t special_ftp_len = 0;

  /* Remember if this url is a special ftp one: */
  if (flags & NET_COALESCE_DOUBLE_SLASH_IS_ROOT) {
    /* some schemes (for example ftp) have the speciality that
       the path can begin // or /%2F to mark the root of the
       servers filesystem, a simple / only marks the root relative
       to the user loging in. We remember the length of the marker */
    if (nsCRT::strncasecmp(path, "/%2F", 4) == 0) {
      special_ftp_len = 4;
    } else if (strncmp(path, "//", 2) == 0) {
      special_ftp_len = 2;
    }
  }

  /* find the last slash before # or ? */
  for (; (*fwdPtr != '\0') && (*fwdPtr != '?') && (*fwdPtr != '#'); ++fwdPtr) {
  }

  /* found nothing, but go back one only */
  /* if there is something to go back to */
  if (fwdPtr != path && *fwdPtr == '\0') {
    --fwdPtr;
  }

  /* search the slash */
  for (; (fwdPtr != path) && (*fwdPtr != '/'); --fwdPtr) {
  }
  lastslash = fwdPtr;
  fwdPtr = path;

  /* replace all %2E or %2e with . in the path */
  /* but stop at lastchar if non null */
  for (; (*fwdPtr != '\0') && (*fwdPtr != '?') && (*fwdPtr != '#') &&
         (*lastslash == '\0' || fwdPtr != lastslash);
       ++fwdPtr) {
    if (*fwdPtr == '%' && *(fwdPtr + 1) == '2' &&
        (*(fwdPtr + 2) == 'E' || *(fwdPtr + 2) == 'e')) {
      *urlPtr++ = '.';
      ++fwdPtr;
      ++fwdPtr;
    } else {
      *urlPtr++ = *fwdPtr;
    }
  }
  // Copy remaining stuff past the #?;
  for (; *fwdPtr != '\0'; ++fwdPtr) {
    *urlPtr++ = *fwdPtr;
  }
  *urlPtr = '\0';  // terminate the url

  // start again, this time for real
  fwdPtr = path;
  urlPtr = path;

  for (; (*fwdPtr != '\0') && (*fwdPtr != '?') && (*fwdPtr != '#'); ++fwdPtr) {
    if (*fwdPtr == '/' && *(fwdPtr + 1) == '.' && *(fwdPtr + 2) == '/') {
      // remove . followed by slash
      ++fwdPtr;
    } else if (*fwdPtr == '/' && *(fwdPtr + 1) == '.' && *(fwdPtr + 2) == '.' &&
               (*(fwdPtr + 3) == '/' ||
                *(fwdPtr + 3) == '\0' ||  // This will take care of
                *(fwdPtr + 3) == '?' ||   // something like foo/bar/..#sometag
                *(fwdPtr + 3) == '#')) {
      // remove foo/..
      // reverse the urlPtr to the previous slash if possible
      // if url does not allow relative root then drop .. above root
      // otherwise retain them in the path
      if (traversal > 0 || !(flags & NET_COALESCE_ALLOW_RELATIVE_ROOT)) {
        if (urlPtr != path) urlPtr--;  // we must be going back at least by one
        for (; *urlPtr != '/' && urlPtr != path; urlPtr--) {
          ;  // null body
        }
        --traversal;  // count back
        // forward the fwdPtr past the ../
        fwdPtr += 2;
        // if we have reached the beginning of the path
        // while searching for the previous / and we remember
        // that it is an url that begins with /%2F then
        // advance urlPtr again by 3 chars because /%2F already
        // marks the root of the path
        if (urlPtr == path && special_ftp_len > 3) {
          ++urlPtr;
          ++urlPtr;
          ++urlPtr;
        }
        // special case if we have reached the end
        // to preserve the last /
        if (*fwdPtr == '.' && *(fwdPtr + 1) == '\0') ++urlPtr;
      } else {
        // there are to much /.. in this path, just copy them instead.
        // forward the urlPtr past the /.. and copying it

        // However if we remember it is an url that starts with
        // /%2F and urlPtr just points at the "F" of "/%2F" then do
        // not overwrite it with the /, just copy .. and move forward
        // urlPtr.
        if (special_ftp_len > 3 && urlPtr == path + special_ftp_len - 1) {
          ++urlPtr;
        } else {
          *urlPtr++ = *fwdPtr;
        }
        ++fwdPtr;
        *urlPtr++ = *fwdPtr;
        ++fwdPtr;
        *urlPtr++ = *fwdPtr;
      }
    } else {
      // count the hierachie, but only if we do not have reached
      // the root of some special urls with a special root marker
      if (*fwdPtr == '/' && *(fwdPtr + 1) != '.' &&
          (special_ftp_len != 2 || *(fwdPtr + 1) != '/')) {
        traversal++;
      }
      // copy the url incrementaly
      *urlPtr++ = *fwdPtr;
    }
  }

  /*
   *  Now lets remove trailing . case
   *     /foo/foo1/.   ->  /foo/foo1/
   */

  if ((urlPtr > (path + 1)) && (*(urlPtr - 1) == '.') &&
      (*(urlPtr - 2) == '/')) {
    urlPtr--;
  }

  // Copy remaining stuff past the #?;
  for (; *fwdPtr != '\0'; ++fwdPtr) {
    *urlPtr++ = *fwdPtr;
  }
  *urlPtr = '\0';  // terminate the url
}

//----------------------------------------------------------------------------
// scheme fu
//----------------------------------------------------------------------------

static bool net_IsValidSchemeChar(const char aChar) {
  return mozilla::net::rust_net_is_valid_scheme_char(aChar);
}

/* Extract URI-Scheme if possible */
nsresult net_ExtractURLScheme(const nsACString& inURI, nsACString& scheme) {
  nsACString::const_iterator start, end;
  inURI.BeginReading(start);
  inURI.EndReading(end);

  // Strip C0 and space from begining
  while (start != end) {
    if ((uint8_t)*start > 0x20) {
      break;
    }
    start++;
  }

  Tokenizer p(Substring(start, end), "\r\n\t");
  p.Record();
  if (!p.CheckChar(IsAsciiAlpha)) {
    // First char must be alpha
    return NS_ERROR_MALFORMED_URI;
  }

  while (p.CheckChar(net_IsValidSchemeChar) || p.CheckWhite()) {
    // Skip valid scheme characters or \r\n\t
  }

  if (!p.CheckChar(':')) {
    return NS_ERROR_MALFORMED_URI;
  }

  p.Claim(scheme);
  scheme.StripTaggedASCII(ASCIIMask::MaskCRLFTab());
  ToLowerCase(scheme);
  return NS_OK;
}

bool net_IsValidScheme(const nsACString& scheme) {
  return mozilla::net::rust_net_is_valid_scheme(&scheme);
}

bool net_IsAbsoluteURL(const nsACString& uri) {
  nsACString::const_iterator start, end;
  uri.BeginReading(start);
  uri.EndReading(end);

  // Strip C0 and space from begining
  while (start != end) {
    if ((uint8_t)*start > 0x20) {
      break;
    }
    start++;
  }

  Tokenizer p(Substring(start, end), "\r\n\t");

  // First char must be alpha
  if (!p.CheckChar(IsAsciiAlpha)) {
    return false;
  }

  while (p.CheckChar(net_IsValidSchemeChar) || p.CheckWhite()) {
    // Skip valid scheme characters or \r\n\t
  }
  if (!p.CheckChar(':')) {
    return false;
  }
  p.SkipWhites();

  if (!p.CheckChar('/')) {
    return false;
  }
  p.SkipWhites();

  if (p.CheckChar('/')) {
    // aSpec is really absolute. Ignore aBaseURI in this case
    return true;
  }
  return false;
}

void net_FilterURIString(const nsACString& input, nsACString& result) {
  result.Truncate();

  const auto* start = input.BeginReading();
  const auto* end = input.EndReading();

  // Trim off leading and trailing invalid chars.
  auto charFilter = [](char c) { return static_cast<uint8_t>(c) > 0x20; };
  const auto* newStart = std::find_if(start, end, charFilter);
  const auto* newEnd =
      std::find_if(std::reverse_iterator<decltype(end)>(end),
                   std::reverse_iterator<decltype(newStart)>(newStart),
                   charFilter)
          .base();

  // Check if chars need to be stripped.
  bool needsStrip = false;
  const ASCIIMaskArray& mask = ASCIIMask::MaskCRLFTab();
  for (const auto* itr = start; itr != end; ++itr) {
    if (ASCIIMask::IsMasked(mask, *itr)) {
      needsStrip = true;
      break;
    }
  }

  // Just use the passed in string rather than creating new copies if no
  // changes are necessary.
  if (newStart == start && newEnd == end && !needsStrip) {
    result = input;
    return;
  }

  result.Assign(Substring(newStart, newEnd));
  if (needsStrip) {
    result.StripTaggedASCII(mask);
  }
}

nsresult net_FilterAndEscapeURI(const nsACString& aInput, uint32_t aFlags,
                                const ASCIIMaskArray& aFilterMask,
                                nsACString& aResult) {
  aResult.Truncate();

  const auto* start = aInput.BeginReading();
  const auto* end = aInput.EndReading();

  // Trim off leading and trailing invalid chars.
  auto charFilter = [](char c) { return static_cast<uint8_t>(c) > 0x20; };
  const auto* newStart = std::find_if(start, end, charFilter);
  const auto* newEnd =
      std::find_if(std::reverse_iterator<decltype(end)>(end),
                   std::reverse_iterator<decltype(newStart)>(newStart),
                   charFilter)
          .base();

  return NS_EscapeAndFilterURL(Substring(newStart, newEnd), aFlags,
                               &aFilterMask, aResult, fallible);
}

#if defined(XP_WIN)
bool net_NormalizeFileURL(const nsACString& aURL, nsCString& aResultBuf) {
  bool writing = false;

  nsACString::const_iterator beginIter, endIter;
  aURL.BeginReading(beginIter);
  aURL.EndReading(endIter);

  const char *s, *begin = beginIter.get();

  for (s = begin; s != endIter.get(); ++s) {
    if (*s == '\\') {
      writing = true;
      if (s > begin) aResultBuf.Append(begin, s - begin);
      aResultBuf += '/';
      begin = s + 1;
    }
  }
  if (writing && s > begin) aResultBuf.Append(begin, s - begin);

  return writing;
}
#endif

//----------------------------------------------------------------------------
// miscellaneous (i.e., stuff that should really be elsewhere)
//----------------------------------------------------------------------------

static inline void ToLower(char& c) {
  if ((unsigned)(c - 'A') <= (unsigned)('Z' - 'A')) c += 'a' - 'A';
}

void net_ToLowerCase(char* str, uint32_t length) {
  for (char* end = str + length; str < end; ++str) ToLower(*str);
}

void net_ToLowerCase(char* str) {
  for (; *str; ++str) ToLower(*str);
}

char* net_FindCharInSet(const char* iter, const char* stop, const char* set) {
  for (; iter != stop && *iter; ++iter) {
    for (const char* s = set; *s; ++s) {
      if (*iter == *s) return (char*)iter;
    }
  }
  return (char*)iter;
}

char* net_FindCharNotInSet(const char* iter, const char* stop,
                           const char* set) {
repeat:
  for (const char* s = set; *s; ++s) {
    if (*iter == *s) {
      if (++iter == stop) break;
      goto repeat;
    }
  }
  return (char*)iter;
}

char* net_RFindCharNotInSet(const char* stop, const char* iter,
                            const char* set) {
  --iter;
  --stop;

  if (iter == stop) return (char*)iter;

repeat:
  for (const char* s = set; *s; ++s) {
    if (*iter == *s) {
      if (--iter == stop) break;
      goto repeat;
    }
  }
  return (char*)iter;
}

#define HTTP_LWS " \t"

// Return the index of the closing quote of the string, if any
static uint32_t net_FindStringEnd(const nsCString& flatStr,
                                  uint32_t stringStart, char stringDelim) {
  NS_ASSERTION(stringStart < flatStr.Length() &&
                   flatStr.CharAt(stringStart) == stringDelim &&
                   (stringDelim == '"' || stringDelim == '\''),
               "Invalid stringStart");

  const char set[] = {stringDelim, '\\', '\0'};
  do {
    // stringStart points to either the start quote or the last
    // escaped char (the char following a '\\')

    // Write to searchStart here, so that when we get back to the
    // top of the loop right outside this one we search from the
    // right place.
    uint32_t stringEnd = flatStr.FindCharInSet(set, stringStart + 1);
    if (stringEnd == uint32_t(kNotFound)) return flatStr.Length();

    if (flatStr.CharAt(stringEnd) == '\\') {
      // Hit a backslash-escaped char.  Need to skip over it.
      stringStart = stringEnd + 1;
      if (stringStart == flatStr.Length()) return stringStart;

      // Go back to looking for the next escape or the string end
      continue;
    }

    return stringEnd;

  } while (true);

  MOZ_ASSERT_UNREACHABLE("How did we get here?");
  return flatStr.Length();
}

static uint32_t net_FindMediaDelimiter(const nsCString& flatStr,
                                       uint32_t searchStart, char delimiter) {
  do {
    // searchStart points to the spot from which we should start looking
    // for the delimiter.
    const char delimStr[] = {delimiter, '"', '\0'};
    uint32_t curDelimPos = flatStr.FindCharInSet(delimStr, searchStart);
    if (curDelimPos == uint32_t(kNotFound)) return flatStr.Length();

    char ch = flatStr.CharAt(curDelimPos);
    if (ch == delimiter) {
      // Found delimiter
      return curDelimPos;
    }

    // We hit the start of a quoted string.  Look for its end.
    searchStart = net_FindStringEnd(flatStr, curDelimPos, ch);
    if (searchStart == flatStr.Length()) return searchStart;

    ++searchStart;

    // searchStart now points to the first char after the end of the
    // string, so just go back to the top of the loop and look for
    // |delimiter| again.
  } while (true);

  MOZ_ASSERT_UNREACHABLE("How did we get here?");
  return flatStr.Length();
}

// aOffset should be added to aCharsetStart and aCharsetEnd if this
// function sets them.
static void net_ParseMediaType(const nsACString& aMediaTypeStr,
                               nsACString& aContentType,
                               nsACString& aContentCharset, int32_t aOffset,
                               bool* aHadCharset, int32_t* aCharsetStart,
                               int32_t* aCharsetEnd, bool aStrict) {
  const nsCString& flatStr = PromiseFlatCString(aMediaTypeStr);
  const char* start = flatStr.get();
  const char* end = start + flatStr.Length();

  // Trim LWS leading and trailing whitespace from type.  We include '(' in
  // the trailing trim set to catch media-type comments, which are not at all
  // standard, but may occur in rare cases.
  const char* type = net_FindCharNotInSet(start, end, HTTP_LWS);
  const char* typeEnd = net_FindCharInSet(type, end, HTTP_LWS ";(");

  const char* charset = "";
  const char* charsetEnd = charset;
  int32_t charsetParamStart = 0;
  int32_t charsetParamEnd = 0;

  uint32_t consumed = typeEnd - type;

  // Iterate over parameters
  bool typeHasCharset = false;
  uint32_t paramStart = flatStr.FindChar(';', typeEnd - start);
  if (paramStart != uint32_t(kNotFound)) {
    // We have parameters.  Iterate over them.
    uint32_t curParamStart = paramStart + 1;
    do {
      uint32_t curParamEnd =
          net_FindMediaDelimiter(flatStr, curParamStart, ';');

      const char* paramName = net_FindCharNotInSet(
          start + curParamStart, start + curParamEnd, HTTP_LWS);
      static const char charsetStr[] = "charset=";
      if (nsCRT::strncasecmp(paramName, charsetStr, sizeof(charsetStr) - 1) ==
          0) {
        charset = paramName + sizeof(charsetStr) - 1;
        charsetEnd = start + curParamEnd;
        typeHasCharset = true;
        charsetParamStart = curParamStart - 1;
        charsetParamEnd = curParamEnd;
      }

      consumed = curParamEnd;
      curParamStart = curParamEnd + 1;
    } while (curParamStart < flatStr.Length());
  }

  bool charsetNeedsQuotedStringUnescaping = false;
  if (typeHasCharset) {
    // Trim LWS leading and trailing whitespace from charset.  We include
    // '(' in the trailing trim set to catch media-type comments, which are
    // not at all standard, but may occur in rare cases.
    charset = net_FindCharNotInSet(charset, charsetEnd, HTTP_LWS);
    if (*charset == '"') {
      charsetNeedsQuotedStringUnescaping = true;
      charsetEnd =
          start + net_FindStringEnd(flatStr, charset - start, *charset);
      charset++;
      NS_ASSERTION(charsetEnd >= charset, "Bad charset parsing");
    } else {
      charsetEnd = net_FindCharInSet(charset, charsetEnd, HTTP_LWS ";(");
    }
  }

  // if the server sent "*/*", it is meaningless, so do not store it.
  // also, if type is the same as aContentType, then just update the
  // charset.  however, if charset is empty and aContentType hasn't
  // changed, then don't wipe-out an existing aContentCharset.  We
  // also want to reject a mime-type if it does not include a slash.
  // some servers give junk after the charset parameter, which may
  // include a comma, so this check makes us a bit more tolerant.

  if (type != typeEnd && memchr(type, '/', typeEnd - type) != nullptr &&
      (aStrict ? (net_FindCharNotInSet(start + consumed, end, HTTP_LWS) == end)
               : (strncmp(type, "*/*", typeEnd - type) != 0))) {
    // Common case here is that aContentType is empty
    bool eq = !aContentType.IsEmpty() &&
              aContentType.Equals(Substring(type, typeEnd),
                                  nsCaseInsensitiveCStringComparator);
    if (!eq) {
      aContentType.Assign(type, typeEnd - type);
      ToLowerCase(aContentType);
    }

    if ((!eq && *aHadCharset) || typeHasCharset) {
      *aHadCharset = true;
      if (charsetNeedsQuotedStringUnescaping) {
        // parameters using the "quoted-string" syntax need
        // backslash-escapes to be unescaped (see RFC 2616 Section 2.2)
        aContentCharset.Truncate();
        for (const char* c = charset; c != charsetEnd; c++) {
          if (*c == '\\' && c + 1 != charsetEnd) {
            // eat escape
            c++;
          }
          aContentCharset.Append(*c);
        }
      } else {
        aContentCharset.Assign(charset, charsetEnd - charset);
      }
      if (typeHasCharset) {
        *aCharsetStart = charsetParamStart + aOffset;
        *aCharsetEnd = charsetParamEnd + aOffset;
      }
    }
    // Only set a new charset position if this is a different type
    // from the last one we had and it doesn't already have a
    // charset param.  If this is the same type, we probably want
    // to leave the charset position on its first occurrence.
    if (!eq && !typeHasCharset) {
      int32_t charsetStart = int32_t(paramStart);
      if (charsetStart == kNotFound) charsetStart = flatStr.Length();

      *aCharsetEnd = *aCharsetStart = charsetStart + aOffset;
    }
  }
}

#undef HTTP_LWS

void net_ParseContentType(const nsACString& aHeaderStr,
                          nsACString& aContentType, nsACString& aContentCharset,
                          bool* aHadCharset) {
  int32_t dummy1, dummy2;
  net_ParseContentType(aHeaderStr, aContentType, aContentCharset, aHadCharset,
                       &dummy1, &dummy2);
}

void net_ParseContentType(const nsACString& aHeaderStr,
                          nsACString& aContentType, nsACString& aContentCharset,
                          bool* aHadCharset, int32_t* aCharsetStart,
                          int32_t* aCharsetEnd) {
  //
  // Augmented BNF (from RFC 2616 section 3.7):
  //
  //   header-value = media-type *( LWS "," LWS media-type )
  //   media-type   = type "/" subtype *( LWS ";" LWS parameter )
  //   type         = token
  //   subtype      = token
  //   parameter    = attribute "=" value
  //   attribute    = token
  //   value        = token | quoted-string
  //
  //
  // Examples:
  //
  //   text/html
  //   text/html, text/html
  //   text/html,text/html; charset=ISO-8859-1
  //   text/html,text/html; charset="ISO-8859-1"
  //   text/html;charset=ISO-8859-1, text/html
  //   text/html;charset='ISO-8859-1', text/html
  //   application/octet-stream
  //

  *aHadCharset = false;
  const nsCString& flatStr = PromiseFlatCString(aHeaderStr);

  // iterate over media-types.  Note that ',' characters can happen
  // inside quoted strings, so we need to watch out for that.
  uint32_t curTypeStart = 0;
  do {
    // curTypeStart points to the start of the current media-type.  We want
    // to look for its end.
    uint32_t curTypeEnd = net_FindMediaDelimiter(flatStr, curTypeStart, ',');

    // At this point curTypeEnd points to the spot where the media-type
    // starting at curTypeEnd ends.  Time to parse that!
    net_ParseMediaType(
        Substring(flatStr, curTypeStart, curTypeEnd - curTypeStart),
        aContentType, aContentCharset, curTypeStart, aHadCharset, aCharsetStart,
        aCharsetEnd, false);

    // And let's move on to the next media-type
    curTypeStart = curTypeEnd + 1;
  } while (curTypeStart < flatStr.Length());
}

void net_ParseRequestContentType(const nsACString& aHeaderStr,
                                 nsACString& aContentType,
                                 nsACString& aContentCharset,
                                 bool* aHadCharset) {
  //
  // Augmented BNF (from RFC 7231 section 3.1.1.1):
  //
  //   media-type   = type "/" subtype *( OWS ";" OWS parameter )
  //   type         = token
  //   subtype      = token
  //   parameter    = token "=" ( token / quoted-string )
  //
  // Examples:
  //
  //   text/html
  //   text/html; charset=ISO-8859-1
  //   text/html; charset="ISO-8859-1"
  //   application/octet-stream
  //

  aContentType.Truncate();
  aContentCharset.Truncate();
  *aHadCharset = false;
  const nsCString& flatStr = PromiseFlatCString(aHeaderStr);

  // At this point curTypeEnd points to the spot where the media-type
  // starting at curTypeEnd ends.  Time to parse that!
  nsAutoCString contentType, contentCharset;
  bool hadCharset = false;
  int32_t dummy1, dummy2;
  uint32_t typeEnd = net_FindMediaDelimiter(flatStr, 0, ',');
  if (typeEnd != flatStr.Length()) {
    // We have some stuff left at the end, so this is not a valid
    // request Content-Type header.
    return;
  }
  net_ParseMediaType(flatStr, contentType, contentCharset, 0, &hadCharset,
                     &dummy1, &dummy2, true);

  aContentType = contentType;
  aContentCharset = contentCharset;
  *aHadCharset = hadCharset;
}

bool net_IsValidHostName(const nsACString& host) {
  // A DNS name is limited to 255 bytes on the wire.
  // In practice this means the host name is limited to 253 ascii characters.
  if (StaticPrefs::network_dns_limit_253_chars() && host.Length() > 253) {
    return false;
  }

  const char* end = host.EndReading();
  // Use explicit whitelists to select which characters we are
  // willing to send to lower-level DNS logic. This is more
  // self-documenting, and can also be slightly faster than the
  // blacklist approach, since DNS names are the common case, and
  // the commonest characters will tend to be near the start of
  // the list.

  // Whitelist for DNS names (RFC 1035) with extra characters added
  // for pragmatic reasons "$+_"
  // see https://bugzilla.mozilla.org/show_bug.cgi?id=355181#c2
  if (net_FindCharNotInSet(host.BeginReading(), end,
                           "abcdefghijklmnopqrstuvwxyz"
                           ".-0123456789"
                           "ABCDEFGHIJKLMNOPQRSTUVWXYZ$+_") == end) {
    return true;
  }

  // Might be a valid IPv6 link-local address containing a percent sign
  return mozilla::net::HostIsIPLiteral(host);
}

bool net_IsValidIPv4Addr(const nsACString& aAddr) {
  return mozilla::net::rust_net_is_valid_ipv4_addr(&aAddr);
}

bool net_IsValidIPv6Addr(const nsACString& aAddr) {
  return mozilla::net::rust_net_is_valid_ipv6_addr(&aAddr);
}

namespace mozilla {
static auto MakeNameMatcher(const nsAString& aName) {
  return [&aName](const auto& param) { return param.mKey.Equals(aName); };
}

bool URLParams::Has(const nsAString& aName) {
  return std::any_of(mParams.cbegin(), mParams.cend(), MakeNameMatcher(aName));
}

void URLParams::Get(const nsAString& aName, nsString& aRetval) {
  SetDOMStringToNull(aRetval);

  const auto end = mParams.cend();
  const auto it = std::find_if(mParams.cbegin(), end, MakeNameMatcher(aName));
  if (it != end) {
    aRetval.Assign(it->mValue);
  }
}

void URLParams::GetAll(const nsAString& aName, nsTArray<nsString>& aRetval) {
  aRetval.Clear();

  for (uint32_t i = 0, len = mParams.Length(); i < len; ++i) {
    if (mParams[i].mKey.Equals(aName)) {
      aRetval.AppendElement(mParams[i].mValue);
    }
  }
}

void URLParams::Append(const nsAString& aName, const nsAString& aValue) {
  Param* param = mParams.AppendElement();
  param->mKey = aName;
  param->mValue = aValue;
}

void URLParams::Set(const nsAString& aName, const nsAString& aValue) {
  Param* param = nullptr;
  for (uint32_t i = 0, len = mParams.Length(); i < len;) {
    if (!mParams[i].mKey.Equals(aName)) {
      ++i;
      continue;
    }
    if (!param) {
      param = &mParams[i];
      ++i;
      continue;
    }
    // Remove duplicates.
    mParams.RemoveElementAt(i);
    --len;
  }

  if (!param) {
    param = mParams.AppendElement();
    param->mKey = aName;
  }

  param->mValue = aValue;
}

void URLParams::Delete(const nsAString& aName) {
  mParams.RemoveElementsBy(
      [&aName](const auto& param) { return param.mKey.Equals(aName); });
}

/* static */
void URLParams::ConvertString(const nsACString& aInput, nsAString& aOutput) {
  if (NS_FAILED(UTF_8_ENCODING->DecodeWithoutBOMHandling(aInput, aOutput))) {
    MOZ_CRASH("Out of memory when converting URL params.");
  }
}

/* static */
void URLParams::DecodeString(const nsACString& aInput, nsAString& aOutput) {
  const char* const end = aInput.EndReading();

  nsAutoCString unescaped;

  for (const char* iter = aInput.BeginReading(); iter != end;) {
    // replace '+' with U+0020
    if (*iter == '+') {
      unescaped.Append(' ');
      ++iter;
      continue;
    }

    // Percent decode algorithm
    if (*iter == '%') {
      const char* const first = iter + 1;
      const char* const second = first + 1;

      const auto asciiHexDigit = [](char x) {
        return (x >= 0x41 && x <= 0x46) || (x >= 0x61 && x <= 0x66) ||
               (x >= 0x30 && x <= 0x39);
      };

      const auto hexDigit = [](char x) {
        return x >= 0x30 && x <= 0x39
                   ? x - 0x30
                   : (x >= 0x41 && x <= 0x46 ? x - 0x37 : x - 0x57);
      };

      if (first != end && second != end && asciiHexDigit(*first) &&
          asciiHexDigit(*second)) {
        unescaped.Append(hexDigit(*first) * 16 + hexDigit(*second));
        iter = second + 1;
      } else {
        unescaped.Append('%');
        ++iter;
      }

      continue;
    }

    unescaped.Append(*iter);
    ++iter;
  }

  // XXX It seems rather wasteful to first decode into a UTF-8 nsCString and
  // then convert the whole string to UTF-16, at least if we exceed the inline
  // storage size.
  ConvertString(unescaped, aOutput);
}

/* static */
bool URLParams::ParseNextInternal(const char*& aStart, const char* const aEnd,
                                  nsAString* aOutDecodedName,
                                  nsAString* aOutDecodedValue) {
  nsDependentCSubstring string;

  const char* const iter = std::find(aStart, aEnd, '&');
  if (iter != aEnd) {
    string.Rebind(aStart, iter);
    aStart = iter + 1;
  } else {
    string.Rebind(aStart, aEnd);
    aStart = aEnd;
  }

  if (string.IsEmpty()) {
    return false;
  }

  const auto* const eqStart = string.BeginReading();
  const auto* const eqEnd = string.EndReading();
  const auto* const eqIter = std::find(eqStart, eqEnd, '=');

  nsDependentCSubstring name;
  nsDependentCSubstring value;

  if (eqIter != eqEnd) {
    name.Rebind(eqStart, eqIter);
    value.Rebind(eqIter + 1, eqEnd);
  } else {
    name.Rebind(string, 0);
  }

  DecodeString(name, *aOutDecodedName);
  DecodeString(value, *aOutDecodedValue);

  return true;
}

/* static */
bool URLParams::Extract(const nsACString& aInput, const nsAString& aName,
                        nsAString& aValue) {
  aValue.SetIsVoid(true);
  return !URLParams::Parse(
      aInput, [&aName, &aValue](const nsAString& name, nsString&& value) {
        if (aName == name) {
          aValue = std::move(value);
          return false;
        }
        return true;
      });
}

void URLParams::ParseInput(const nsACString& aInput) {
  // Remove all the existing data before parsing a new input.
  DeleteAll();

  URLParams::Parse(aInput, [this](nsString&& name, nsString&& value) {
    mParams.AppendElement(Param{std::move(name), std::move(value)});
    return true;
  });
}

namespace {

void SerializeString(const nsCString& aInput, nsAString& aValue) {
  const unsigned char* p = (const unsigned char*)aInput.get();
  const unsigned char* end = p + aInput.Length();

  while (p != end) {
    // ' ' to '+'
    if (*p == 0x20) {
      aValue.Append(0x2B);
      // Percent Encode algorithm
    } else if (*p == 0x2A || *p == 0x2D || *p == 0x2E ||
               (*p >= 0x30 && *p <= 0x39) || (*p >= 0x41 && *p <= 0x5A) ||
               *p == 0x5F || (*p >= 0x61 && *p <= 0x7A)) {
      aValue.Append(*p);
    } else {
      aValue.AppendPrintf("%%%.2X", *p);
    }

    ++p;
  }
}

}  // namespace

void URLParams::Serialize(nsAString& aValue, bool aEncode) const {
  aValue.Truncate();
  bool first = true;

  for (uint32_t i = 0, len = mParams.Length(); i < len; ++i) {
    if (first) {
      first = false;
    } else {
      aValue.Append('&');
    }

    // XXX Actually, it's not necessary to build a new string object. Generally,
    // such cases could just convert each codepoint one-by-one.
    if (aEncode) {
      SerializeString(NS_ConvertUTF16toUTF8(mParams[i].mKey), aValue);
      aValue.Append('=');
      SerializeString(NS_ConvertUTF16toUTF8(mParams[i].mValue), aValue);
    } else {
      aValue.Append(mParams[i].mKey);
      aValue.Append('=');
      aValue.Append(mParams[i].mValue);
    }
  }
}

void URLParams::Sort() {
  mParams.StableSort([](const Param& lhs, const Param& rhs) {
    return Compare(lhs.mKey, rhs.mKey);
  });
}

}  // namespace mozilla
