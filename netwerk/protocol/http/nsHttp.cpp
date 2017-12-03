/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set ts=4 sw=4 sts=4 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// HttpLog.h should generally be included first
#include "HttpLog.h"

#include "nsHttp.h"
#include "CacheControlParser.h"
#include "PLDHashTable.h"
#include "mozilla/Mutex.h"
#include "mozilla/HashFunctions.h"
#include "nsCRT.h"
#include "nsHttpRequestHead.h"
#include "nsHttpResponseHead.h"
#include "nsHttpHandler.h"
#include "nsICacheEntry.h"
#include "nsIRequest.h"
#include <errno.h>

namespace mozilla {
namespace net {

// define storage for all atoms
namespace nsHttp {
#define HTTP_ATOM(_name, _value) nsHttpAtom _name = { _value };
#include "nsHttpAtomList.h"
#undef HTTP_ATOM
}

// find out how many atoms we have
#define HTTP_ATOM(_name, _value) Unused_ ## _name,
enum {
#include "nsHttpAtomList.h"
    NUM_HTTP_ATOMS
};
#undef HTTP_ATOM

// we keep a linked list of atoms allocated on the heap for easy clean up when
// the atom table is destroyed.  The structure and value string are allocated
// as one contiguous block.

struct HttpHeapAtom {
    struct HttpHeapAtom *next;
    char                 value[1];
};

static PLDHashTable        *sAtomTable;
static struct HttpHeapAtom *sHeapAtoms = nullptr;
static Mutex               *sLock = nullptr;

HttpHeapAtom *
NewHeapAtom(const char *value) {
    int len = strlen(value);

    HttpHeapAtom *a =
        reinterpret_cast<HttpHeapAtom *>(malloc(sizeof(*a) + len));
    if (!a)
        return nullptr;
    memcpy(a->value, value, len + 1);

    // add this heap atom to the list of all heap atoms
    a->next = sHeapAtoms;
    sHeapAtoms = a;

    return a;
}

// Hash string ignore case, based on PL_HashString
static PLDHashNumber
StringHash(const void *key)
{
    PLDHashNumber h = 0;
    for (const char *s = reinterpret_cast<const char*>(key); *s; ++s)
        h = AddToHash(h, nsCRT::ToLower(*s));
    return h;
}

static bool
StringCompare(const PLDHashEntryHdr *entry, const void *testKey)
{
    const void *entryKey =
            reinterpret_cast<const PLDHashEntryStub *>(entry)->key;

    return PL_strcasecmp(reinterpret_cast<const char *>(entryKey),
                         reinterpret_cast<const char *>(testKey)) == 0;
}

static const PLDHashTableOps ops = {
    StringHash,
    StringCompare,
    PLDHashTable::MoveEntryStub,
    PLDHashTable::ClearEntryStub,
    nullptr
};

// We put the atoms in a hash table for speedy lookup.. see ResolveAtom.
namespace nsHttp {
nsresult
CreateAtomTable()
{
    MOZ_ASSERT(!sAtomTable, "atom table already initialized");

    if (!sLock) {
        sLock = new Mutex("nsHttp.sLock");
    }

    // The initial length for this table is a value greater than the number of
    // known atoms (NUM_HTTP_ATOMS) because we expect to encounter a few random
    // headers right off the bat.
    sAtomTable = new PLDHashTable(&ops, sizeof(PLDHashEntryStub),
                                  NUM_HTTP_ATOMS + 10);

    // fill the table with our known atoms
    const char *const atoms[] = {
#define HTTP_ATOM(_name, _value) _name._val,
#include "nsHttpAtomList.h"
#undef HTTP_ATOM
        nullptr
    };

    for (int i = 0; atoms[i]; ++i) {
        auto stub = static_cast<PLDHashEntryStub*>
                               (sAtomTable->Add(atoms[i], fallible));
        if (!stub)
            return NS_ERROR_OUT_OF_MEMORY;

        MOZ_ASSERT(!stub->key, "duplicate static atom");
        stub->key = atoms[i];
    }

    return NS_OK;
}

void
DestroyAtomTable()
{
    delete sAtomTable;
    sAtomTable = nullptr;

    while (sHeapAtoms) {
        HttpHeapAtom *next = sHeapAtoms->next;
        free(sHeapAtoms);
        sHeapAtoms = next;
    }

    delete sLock;
    sLock = nullptr;
}

Mutex *
GetLock()
{
    return sLock;
}

// this function may be called from multiple threads
nsHttpAtom
ResolveAtom(const char *str)
{
    nsHttpAtom atom = { nullptr };

    if (!str || !sAtomTable)
        return atom;

    MutexAutoLock lock(*sLock);

    auto stub = static_cast<PLDHashEntryStub*>(sAtomTable->Add(str, fallible));
    if (!stub)
        return atom;  // out of memory

    if (stub->key) {
        atom._val = reinterpret_cast<const char *>(stub->key);
        return atom;
    }

    // if the atom could not be found in the atom table, then we'll go
    // and allocate a new atom on the heap.
    HttpHeapAtom *heapAtom = NewHeapAtom(str);
    if (!heapAtom)
        return atom;  // out of memory

    stub->key = atom._val = heapAtom->value;
    return atom;
}

//
// From section 2.2 of RFC 2616, a token is defined as:
//
//   token          = 1*<any CHAR except CTLs or separators>
//   CHAR           = <any US-ASCII character (octets 0 - 127)>
//   separators     = "(" | ")" | "<" | ">" | "@"
//                  | "," | ";" | ":" | "\" | <">
//                  | "/" | "[" | "]" | "?" | "="
//                  | "{" | "}" | SP | HT
//   CTL            = <any US-ASCII control character
//                    (octets 0 - 31) and DEL (127)>
//   SP             = <US-ASCII SP, space (32)>
//   HT             = <US-ASCII HT, horizontal-tab (9)>
//
static const char kValidTokenMap[128] = {
    0, 0, 0, 0, 0, 0, 0, 0, //   0
    0, 0, 0, 0, 0, 0, 0, 0, //   8
    0, 0, 0, 0, 0, 0, 0, 0, //  16
    0, 0, 0, 0, 0, 0, 0, 0, //  24

    0, 1, 0, 1, 1, 1, 1, 1, //  32
    0, 0, 1, 1, 0, 1, 1, 0, //  40
    1, 1, 1, 1, 1, 1, 1, 1, //  48
    1, 1, 0, 0, 0, 0, 0, 0, //  56

    0, 1, 1, 1, 1, 1, 1, 1, //  64
    1, 1, 1, 1, 1, 1, 1, 1, //  72
    1, 1, 1, 1, 1, 1, 1, 1, //  80
    1, 1, 1, 0, 0, 0, 1, 1, //  88

    1, 1, 1, 1, 1, 1, 1, 1, //  96
    1, 1, 1, 1, 1, 1, 1, 1, // 104
    1, 1, 1, 1, 1, 1, 1, 1, // 112
    1, 1, 1, 0, 1, 0, 1, 0  // 120
};
bool
IsValidToken(const char *start, const char *end)
{
    if (start == end)
        return false;

    for (; start != end; ++start) {
        const unsigned char idx = *start;
        if (idx > 127 || !kValidTokenMap[idx])
            return false;
    }

    return true;
}

const char*
GetProtocolVersion(uint32_t pv)
{
    switch (pv) {
    case HTTP_VERSION_2:
    case NS_HTTP_VERSION_2_0:
        return "h2";
    case NS_HTTP_VERSION_1_0:
        return "http/1.0";
    case NS_HTTP_VERSION_1_1:
        return "http/1.1";
    default:
        NS_WARNING(nsPrintfCString("Unkown protocol version: 0x%X. "
                                   "Please file a bug", pv).get());
        return "http/1.1";
    }
}

// static
void
TrimHTTPWhitespace(const nsACString& aSource, nsACString& aDest)
{
  nsAutoCString str(aSource);

  // HTTP whitespace 0x09: '\t', 0x0A: '\n', 0x0D: '\r', 0x20: ' '
  static const char kHTTPWhitespace[] = "\t\n\r ";
  str.Trim(kHTTPWhitespace);
  aDest.Assign(str);
}

// static
bool
IsReasonableHeaderValue(const nsACString &s)
{
  // Header values MUST NOT contain line-breaks.  RFC 2616 technically
  // permits CTL characters, including CR and LF, in header values provided
  // they are quoted.  However, this can lead to problems if servers do not
  // interpret quoted strings properly.  Disallowing CR and LF here seems
  // reasonable and keeps things simple.  We also disallow a null byte.
  const nsACString::char_type* end = s.EndReading();
  for (const nsACString::char_type* i = s.BeginReading(); i != end; ++i) {
    if (*i == '\r' || *i == '\n' || *i == '\0') {
      return false;
    }
  }
  return true;
}

const char *
FindToken(const char *input, const char *token, const char *seps)
{
    if (!input)
        return nullptr;

    int inputLen = strlen(input);
    int tokenLen = strlen(token);

    if (inputLen < tokenLen)
        return nullptr;

    const char *inputTop = input;
    const char *inputEnd = input + inputLen - tokenLen;
    for (; input <= inputEnd; ++input) {
        if (PL_strncasecmp(input, token, tokenLen) == 0) {
            if (input > inputTop && !strchr(seps, *(input - 1)))
                continue;
            if (input < inputEnd && !strchr(seps, *(input + tokenLen)))
                continue;
            return input;
        }
    }

    return nullptr;
}

bool
ParseInt64(const char *input, const char **next, int64_t *r)
{
    MOZ_ASSERT(input);
    MOZ_ASSERT(r);

    char *end = nullptr;
    errno = 0; // Clear errno to make sure its value is set by strtoll
    int64_t value = strtoll(input, &end, /* base */ 10);

    // Fail if: - the parsed number overflows.
    //          - the end points to the start of the input string.
    //          - we parsed a negative value. Consumers don't expect that.
    if (errno != 0 || end == input || value < 0) {
        LOG(("nsHttp::ParseInt64 value=%" PRId64 " errno=%d", value, errno));
        return false;
    }

    if (next) {
        *next = end;
    }
    *r = value;
    return true;
}

bool
IsPermanentRedirect(uint32_t httpStatus)
{
  return httpStatus == 301 || httpStatus == 308;
}

bool
ValidationRequired(bool isForcedValid, nsHttpResponseHead *cachedResponseHead,
                   uint32_t loadFlags, bool allowStaleCacheContent,
                   bool isImmutable, bool customConditionalRequest,
                   nsHttpRequestHead &requestHead,
                   nsICacheEntry *entry, CacheControlParser &cacheControlRequest,
                   bool fromPreviousSession)
{
    // Check isForcedValid to see if it is possible to skip validation.
    // Don't skip validation if we have serious reason to believe that this
    // content is invalid (it's expired).
    // See netwerk/cache2/nsICacheEntry.idl for details
    if (isForcedValid &&
        (!cachedResponseHead->ExpiresInPast() ||
         !cachedResponseHead->MustValidateIfExpired())) {
        LOG(("NOT validating based on isForcedValid being true.\n"));
        return false;
    }

    // If the LOAD_FROM_CACHE flag is set, any cached data can simply be used
    if (loadFlags & nsIRequest::LOAD_FROM_CACHE || allowStaleCacheContent) {
        LOG(("NOT validating based on LOAD_FROM_CACHE load flag\n"));
        return false;
    }

    // If the VALIDATE_ALWAYS flag is set, any cached data won't be used until
    // it's revalidated with the server.
    if ((loadFlags & nsIRequest::VALIDATE_ALWAYS) && !isImmutable) {
        LOG(("Validating based on VALIDATE_ALWAYS load flag\n"));
        return true;
    }

    // Even if the VALIDATE_NEVER flag is set, there are still some cases in
    // which we must validate the cached response with the server.
    if (loadFlags & nsIRequest::VALIDATE_NEVER) {
        LOG(("VALIDATE_NEVER set\n"));
        // if no-store validate cached response (see bug 112564)
        if (cachedResponseHead->NoStore()) {
            LOG(("Validating based on no-store logic\n"));
            return true;
        } else {
            LOG(("NOT validating based on VALIDATE_NEVER load flag\n"));
            return false;
        }
    }

    // check if validation is strictly required...
    if (cachedResponseHead->MustValidate()) {
        LOG(("Validating based on MustValidate() returning TRUE\n"));
        return true;
    }

    // possibly serve from cache for a custom If-Match/If-Unmodified-Since
    // conditional request
    if (customConditionalRequest &&
        !requestHead.HasHeader(nsHttp::If_Match) &&
        !requestHead.HasHeader(nsHttp::If_Unmodified_Since)) {
        LOG(("Validating based on a custom conditional request\n"));
        return true;
    }

    // previously we also checked for a query-url w/out expiration
    // and didn't do heuristic on it. but defacto that is allowed now.
    //
    // Check if the cache entry has expired...

    bool doValidation = true;
    uint32_t now = NowInSeconds();

    uint32_t age = 0;
    nsresult rv = cachedResponseHead->ComputeCurrentAge(now, now, &age);
    if (NS_FAILED(rv)) {
        return true;
    }

    uint32_t freshness = 0;
    rv = cachedResponseHead->ComputeFreshnessLifetime(&freshness);
    if (NS_FAILED(rv)) {
        return true;
    }

    uint32_t expiration = 0;
    rv = entry->GetExpirationTime(&expiration);
    if (NS_FAILED(rv)) {
        return true;
    }

    uint32_t maxAgeRequest, maxStaleRequest, minFreshRequest;

    LOG(("  NowInSeconds()=%u, expiration time=%u, freshness lifetime=%u, age=%u",
            now, expiration, freshness, age));

    if (cacheControlRequest.NoCache()) {
        LOG(("  validating, no-cache request"));
        doValidation = true;
    } else if (cacheControlRequest.MaxStale(&maxStaleRequest)) {
        uint32_t staleTime = age > freshness ? age - freshness : 0;
        doValidation = staleTime > maxStaleRequest;
        LOG(("  validating=%d, max-stale=%u requested", doValidation, maxStaleRequest));
    } else if (cacheControlRequest.MaxAge(&maxAgeRequest)) {
        doValidation = age > maxAgeRequest;
        LOG(("  validating=%d, max-age=%u requested", doValidation, maxAgeRequest));
    } else if (cacheControlRequest.MinFresh(&minFreshRequest)) {
        uint32_t freshTime = freshness > age ? freshness - age : 0;
        doValidation = freshTime < minFreshRequest;
        LOG(("  validating=%d, min-fresh=%u requested", doValidation, minFreshRequest));
    } else if (now <= expiration) {
        doValidation = false;
        LOG(("  not validating, expire time not in the past"));
    } else if (cachedResponseHead->MustValidateIfExpired()) {
        doValidation = true;
    } else if (loadFlags & nsIRequest::VALIDATE_ONCE_PER_SESSION) {
        // If the cached response does not include expiration infor-
        // mation, then we must validate the response, despite whether
        // or not this is the first access this session.  This behavior
        // is consistent with existing browsers and is generally expected
        // by web authors.
        if (freshness == 0)
            doValidation = true;
        else
            doValidation = fromPreviousSession;
    } else {
        doValidation = true;
    }

    LOG(("%salidating based on expiration time\n", doValidation ? "V" : "Not v"));
    return doValidation;
}

nsresult
GetHttpResponseHeadFromCacheEntry(nsICacheEntry *entry, nsHttpResponseHead *cachedResponseHead)
{
    nsCString buf;
    // A "original-response-headers" metadata element holds network original headers,
    // i.e. the headers in the form as they arrieved from the network.
    // We need to get the network original headers first, because we need to keep them
    // in order.
    nsresult rv = entry->GetMetaDataElement("original-response-headers", getter_Copies(buf));
    if (NS_SUCCEEDED(rv)) {
        rv = cachedResponseHead->ParseCachedOriginalHeaders((char *) buf.get());
        if (NS_FAILED(rv)) {
            LOG(("  failed to parse original-response-headers\n"));
        }
    }

    buf.Adopt(0);
    // A "response-head" metadata element holds response head, e.g. response status
    // line and headers in the form Firefox uses them internally (no dupicate
    // headers, etc.).
    rv = entry->GetMetaDataElement("response-head", getter_Copies(buf));
    NS_ENSURE_SUCCESS(rv, rv);

    // Parse string stored in a "response-head" metadata element.
    // These response headers will be merged with the orignal headers (i.e. the
    // headers stored in a "original-response-headers" metadata element).
    rv = cachedResponseHead->ParseCachedHead(buf.get());
    NS_ENSURE_SUCCESS(rv, rv);
    buf.Adopt(0);

    return NS_OK;
}

nsresult
CheckPartial(nsICacheEntry* aEntry, int64_t *aSize, int64_t *aContentLength,
             nsHttpResponseHead *responseHead)
{
    nsresult rv;

    rv = aEntry->GetDataSize(aSize);

    if (NS_ERROR_IN_PROGRESS == rv) {
        *aSize = -1;
        rv = NS_OK;
    }

    NS_ENSURE_SUCCESS(rv, rv);

    if (!responseHead) {
        return NS_ERROR_UNEXPECTED;
    }

    *aContentLength = responseHead->ContentLength();

    return NS_OK;
}

void
DetermineFramingAndImmutability(nsICacheEntry *entry,
        nsHttpResponseHead *responseHead, bool isHttps,
        bool *weaklyFramed, bool *isImmutable)
{
    nsCString framedBuf;
    nsresult rv = entry->GetMetaDataElement("strongly-framed", getter_Copies(framedBuf));
    // describe this in terms of explicitly weakly framed so as to be backwards
    // compatible with old cache contents which dont have strongly-framed makers
    *weaklyFramed = NS_SUCCEEDED(rv) && framedBuf.EqualsLiteral("0");
    *isImmutable = !*weaklyFramed && isHttps && responseHead->Immutable();
}

bool
IsBeforeLastActiveTabLoadOptimization(TimeStamp const & when)
{
  return gHttpHandler && gHttpHandler->IsBeforeLastActiveTabLoadOptimization(when);
}

void
NotifyActiveTabLoadOptimization()
{
  if (gHttpHandler) {
    gHttpHandler->NotifyActiveTabLoadOptimization();
  }
}

TimeStamp const GetLastActiveTabLoadOptimizationHit()
{
  return gHttpHandler ? gHttpHandler->GetLastActiveTabLoadOptimizationHit() : TimeStamp();
}

void
SetLastActiveTabLoadOptimizationHit(TimeStamp const &when)
{
  if (gHttpHandler) {
    gHttpHandler->SetLastActiveTabLoadOptimizationHit(when);
  }
}

} // namespace nsHttp


template<typename T> void
localEnsureBuffer(UniquePtr<T[]> &buf, uint32_t newSize,
             uint32_t preserve, uint32_t &objSize)
{
  if (objSize >= newSize)
    return;

  // Leave a little slop on the new allocation - add 2KB to
  // what we need and then round the result up to a 4KB (page)
  // boundary.

  objSize = (newSize + 2048 + 4095) & ~4095;

  static_assert(sizeof(T) == 1, "sizeof(T) must be 1");
  auto tmp = MakeUnique<T[]>(objSize);
  if (preserve) {
    memcpy(tmp.get(), buf.get(), preserve);
  }
  buf = Move(tmp);
}

void EnsureBuffer(UniquePtr<char[]> &buf, uint32_t newSize,
                  uint32_t preserve, uint32_t &objSize)
{
    localEnsureBuffer<char> (buf, newSize, preserve, objSize);
}

void EnsureBuffer(UniquePtr<uint8_t[]> &buf, uint32_t newSize,
                  uint32_t preserve, uint32_t &objSize)
{
    localEnsureBuffer<uint8_t> (buf, newSize, preserve, objSize);
}
///

void
ParsedHeaderValueList::Tokenize(char *input, uint32_t inputLen, char **token,
                                uint32_t *tokenLen, bool *foundEquals, char **next)
{
    if (foundEquals) {
        *foundEquals = false;
    }
    if (next) {
        *next = nullptr;
    }
    if (inputLen < 1 || !input || !token) {
        return;
    }

    bool foundFirst = false;
    bool inQuote = false;
    bool foundToken = false;
    *token = input;
    *tokenLen = inputLen;

    for (uint32_t index = 0; !foundToken && index < inputLen; ++index) {
        // strip leading cruft
        if (!foundFirst &&
            (input[index] == ' ' || input[index] == '"' || input[index] == '\t')) {
            (*token)++;
        } else {
            foundFirst = true;
        }

        if (input[index] == '"') {
            inQuote = !inQuote;
            continue;
        }

        if (inQuote) {
            continue;
        }

        if (input[index] == '=' || input[index] == ';') {
            *tokenLen = (input + index) - *token;
            if (next && ((index + 1) < inputLen)) {
                *next = input + index + 1;
            }
            foundToken = true;
            if (foundEquals && input[index] == '=') {
                *foundEquals = true;
            }
            break;
        }
    }

    if (!foundToken) {
        *tokenLen = (input + inputLen) - *token;
    }

    // strip trailing cruft
    for (char *index = *token + *tokenLen - 1; index >= *token; --index) {
        if (*index != ' ' && *index != '\t' && *index != '"') {
            break;
        }
        --(*tokenLen);
        if (*index == '"') {
            break;
        }
    }
}

ParsedHeaderValueList::ParsedHeaderValueList(char *t, uint32_t len)
{
    char *name = nullptr;
    uint32_t nameLen = 0;
    char *value = nullptr;
    uint32_t valueLen = 0;
    char *next = nullptr;
    bool foundEquals;

    while (t) {
        Tokenize(t, len, &name, &nameLen, &foundEquals, &next);
        if (next) {
            len -= next - t;
        }
        t = next;
        if (foundEquals && t) {
            Tokenize(t, len, &value, &valueLen, nullptr, &next);
            if (next) {
                len -= next - t;
            }
            t = next;
        }
        mValues.AppendElement(ParsedHeaderPair(name, nameLen, value, valueLen));
        value = name = nullptr;
        valueLen = nameLen = 0;
        next = nullptr;
    }
}

ParsedHeaderValueListList::ParsedHeaderValueListList(const nsCString &fullHeader)
    : mFull(fullHeader)
{
    char *t = mFull.BeginWriting();
    uint32_t len = mFull.Length();
    char *last = t;
    bool inQuote = false;
    for (uint32_t index = 0; index < len; ++index) {
        if (t[index] == '"') {
            inQuote = !inQuote;
            continue;
        }
        if (inQuote) {
            continue;
        }
        if (t[index] == ',') {
            mValues.AppendElement(ParsedHeaderValueList(last, (t + index) - last));
            last = t + index + 1;
        }
    }
    if (!inQuote) {
        mValues.AppendElement(ParsedHeaderValueList(last, (t + len) - last));
    }
}

} // namespace net
} // namespace mozilla
