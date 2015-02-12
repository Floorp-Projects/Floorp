/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set ts=4 sw=4 sts=4 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// HttpLog.h should generally be included first
#include "HttpLog.h"

#include "nsHttp.h"
#include "pldhash.h"
#include "mozilla/Mutex.h"
#include "mozilla/HashFunctions.h"
#include "nsCRT.h"

#if defined(PR_LOGGING)
PRLogModuleInfo *gHttpLog = nullptr;
#endif

namespace mozilla {
namespace net {

// define storage for all atoms
#define HTTP_ATOM(_name, _value) nsHttpAtom nsHttp::_name = { _value };
#include "nsHttpAtomList.h"
#undef HTTP_ATOM

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

static PLDHashTable         sAtomTable;
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
StringHash(PLDHashTable *table, const void *key)
{
    PLDHashNumber h = 0;
    for (const char *s = reinterpret_cast<const char*>(key); *s; ++s)
        h = AddToHash(h, nsCRT::ToLower(*s));
    return h;
}

static bool
StringCompare(PLDHashTable *table, const PLDHashEntryHdr *entry,
              const void *testKey)
{
    const void *entryKey =
            reinterpret_cast<const PLDHashEntryStub *>(entry)->key;

    return PL_strcasecmp(reinterpret_cast<const char *>(entryKey),
                         reinterpret_cast<const char *>(testKey)) == 0;
}

static const PLDHashTableOps ops = {
    StringHash,
    StringCompare,
    PL_DHashMoveEntryStub,
    PL_DHashClearEntryStub,
    nullptr
};

// We put the atoms in a hash table for speedy lookup.. see ResolveAtom.
nsresult
nsHttp::CreateAtomTable()
{
    MOZ_ASSERT(!sAtomTable.IsInitialized(), "atom table already initialized");

    if (!sLock) {
        sLock = new Mutex("nsHttp.sLock");
    }

    // The initial length for this table is a value greater than the number of
    // known atoms (NUM_HTTP_ATOMS) because we expect to encounter a few random
    // headers right off the bat.
    if (!PL_DHashTableInit(&sAtomTable, &ops, sizeof(PLDHashEntryStub),
                           fallible, NUM_HTTP_ATOMS + 10)) {
        return NS_ERROR_OUT_OF_MEMORY;
    }

    // fill the table with our known atoms
    const char *const atoms[] = {
#define HTTP_ATOM(_name, _value) nsHttp::_name._val,
#include "nsHttpAtomList.h"
#undef HTTP_ATOM
        nullptr
    };

    for (int i = 0; atoms[i]; ++i) {
        PLDHashEntryStub *stub = reinterpret_cast<PLDHashEntryStub *>
            (PL_DHashTableAdd(&sAtomTable, atoms[i], fallible));
        if (!stub)
            return NS_ERROR_OUT_OF_MEMORY;

        MOZ_ASSERT(!stub->key, "duplicate static atom");
        stub->key = atoms[i];
    }

    return NS_OK;
}

void
nsHttp::DestroyAtomTable()
{
    if (sAtomTable.IsInitialized()) {
        PL_DHashTableFinish(&sAtomTable);
    }

    while (sHeapAtoms) {
        HttpHeapAtom *next = sHeapAtoms->next;
        free(sHeapAtoms);
        sHeapAtoms = next;
    }

    if (sLock) {
        delete sLock;
        sLock = nullptr;
    }
}

Mutex *
nsHttp::GetLock()
{
    return sLock;
}

// this function may be called from multiple threads
nsHttpAtom
nsHttp::ResolveAtom(const char *str)
{
    nsHttpAtom atom = { nullptr };

    if (!str || !sAtomTable.IsInitialized())
        return atom;

    MutexAutoLock lock(*sLock);

    PLDHashEntryStub *stub = reinterpret_cast<PLDHashEntryStub *>
        (PL_DHashTableAdd(&sAtomTable, str, fallible));
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
nsHttp::IsValidToken(const char *start, const char *end)
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

// static
bool
nsHttp::IsReasonableHeaderValue(const nsACString &s)
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
nsHttp::FindToken(const char *input, const char *token, const char *seps)
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
nsHttp::ParseInt64(const char *input, const char **next, int64_t *r)
{
    const char *start = input;
    *r = 0;
    while (*input >= '0' && *input <= '9') {
        int64_t next = 10 * (*r) + (*input - '0');
        if (next < *r) // overflow?
            return false;
        *r = next;
        ++input;
    }
    if (input == start) // nothing parsed?
        return false;
    if (next)
        *next = input;
    return true;
}

bool
nsHttp::IsPermanentRedirect(uint32_t httpStatus)
{
  return httpStatus == 301 || httpStatus == 308;
}


template<typename T> void
localEnsureBuffer(nsAutoArrayPtr<T> &buf, uint32_t newSize,
             uint32_t preserve, uint32_t &objSize)
{
  if (objSize >= newSize)
    return;

  // Leave a little slop on the new allocation - add 2KB to
  // what we need and then round the result up to a 4KB (page)
  // boundary.

  objSize = (newSize + 2048 + 4095) & ~4095;

  static_assert(sizeof(T) == 1, "sizeof(T) must be 1");
  nsAutoArrayPtr<T> tmp(new T[objSize]);
  if (preserve) {
    memcpy(tmp, buf, preserve);
  }
  buf = tmp;
}

void EnsureBuffer(nsAutoArrayPtr<char> &buf, uint32_t newSize,
                  uint32_t preserve, uint32_t &objSize)
{
    localEnsureBuffer<char> (buf, newSize, preserve, objSize);
}

void EnsureBuffer(nsAutoArrayPtr<uint8_t> &buf, uint32_t newSize,
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

} // namespace mozilla::net
} // namespace mozilla
