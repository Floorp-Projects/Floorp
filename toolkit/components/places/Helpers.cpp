/* vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Helpers.h"
#include "mozIStorageError.h"
#include "prio.h"
#include "nsIFile.h"
#include "nsReadableUtils.h"
#include "nsString.h"
#include "nsNavHistory.h"
#include "nsNetUtil.h"
#include "mozilla/Base64.h"
#include "mozilla/HashFunctions.h"
#include "mozilla/RandomNum.h"
#include <algorithm>
#include "mozilla/Services.h"

// The length of guids that are used by history and bookmarks.
#define GUID_LENGTH 12

// Maximum number of chars to use for calculating hashes. This value has been
// picked to ensure low hash collisions on a real world common places.sqlite.
// While collisions are not a big deal for functionality, a low ratio allows
// for slightly more efficient SELECTs.
#define MAX_CHARS_TO_HASH 1500U

extern "C" {

// Generates a new Places GUID. This function uses C linkage because it's
// called from the Rust synced bookmarks mirror, on the storage thread.
nsresult NS_GeneratePlacesGUID(nsACString* _guid) {
  return mozilla::places::GenerateGUID(*_guid);
}

}  // extern "C"

namespace mozilla {
namespace places {

////////////////////////////////////////////////////////////////////////////////
//// AsyncStatementCallback

NS_IMPL_ISUPPORTS(AsyncStatementCallback, mozIStorageStatementCallback)

NS_IMETHODIMP
WeakAsyncStatementCallback::HandleResult(mozIStorageResultSet* aResultSet) {
  MOZ_ASSERT(false, "Was not expecting a resultset, but got it.");
  return NS_OK;
}

NS_IMETHODIMP
WeakAsyncStatementCallback::HandleCompletion(uint16_t aReason) { return NS_OK; }

NS_IMETHODIMP
WeakAsyncStatementCallback::HandleError(mozIStorageError* aError) {
#ifdef DEBUG
  int32_t result;
  nsresult rv = aError->GetResult(&result);
  NS_ENSURE_SUCCESS(rv, rv);
  nsAutoCString message;
  rv = aError->GetMessage(message);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString warnMsg;
  warnMsg.AppendLiteral(
      "An error occurred while executing an async statement: ");
  warnMsg.AppendInt(result);
  warnMsg.Append(' ');
  warnMsg.Append(message);
  NS_WARNING(warnMsg.get());
#endif

  return NS_OK;
}

#define URI_TO_URLCSTRING(uri, spec)    \
  nsAutoCString spec;                   \
  if (NS_FAILED(aURI->GetSpec(spec))) { \
    return NS_ERROR_UNEXPECTED;         \
  }

// Bind URI to statement by index.
nsresult  // static
URIBinder::Bind(mozIStorageStatement* aStatement, int32_t aIndex,
                nsIURI* aURI) {
  NS_ASSERTION(aStatement, "Must have non-null statement");
  NS_ASSERTION(aURI, "Must have non-null uri");

  URI_TO_URLCSTRING(aURI, spec);
  return URIBinder::Bind(aStatement, aIndex, spec);
}

// Statement URLCString to statement by index.
nsresult  // static
URIBinder::Bind(mozIStorageStatement* aStatement, int32_t index,
                const nsACString& aURLString) {
  NS_ASSERTION(aStatement, "Must have non-null statement");
  return aStatement->BindUTF8StringByIndex(
      index, StringHead(aURLString, URI_LENGTH_MAX));
}

// Bind URI to statement by name.
nsresult  // static
URIBinder::Bind(mozIStorageStatement* aStatement, const nsACString& aName,
                nsIURI* aURI) {
  NS_ASSERTION(aStatement, "Must have non-null statement");
  NS_ASSERTION(aURI, "Must have non-null uri");

  URI_TO_URLCSTRING(aURI, spec);
  return URIBinder::Bind(aStatement, aName, spec);
}

// Bind URLCString to statement by name.
nsresult  // static
URIBinder::Bind(mozIStorageStatement* aStatement, const nsACString& aName,
                const nsACString& aURLString) {
  NS_ASSERTION(aStatement, "Must have non-null statement");
  return aStatement->BindUTF8StringByName(
      aName, StringHead(aURLString, URI_LENGTH_MAX));
}

// Bind URI to params by index.
nsresult  // static
URIBinder::Bind(mozIStorageBindingParams* aParams, int32_t aIndex,
                nsIURI* aURI) {
  NS_ASSERTION(aParams, "Must have non-null statement");
  NS_ASSERTION(aURI, "Must have non-null uri");

  URI_TO_URLCSTRING(aURI, spec);
  return URIBinder::Bind(aParams, aIndex, spec);
}

// Bind URLCString to params by index.
nsresult  // static
URIBinder::Bind(mozIStorageBindingParams* aParams, int32_t index,
                const nsACString& aURLString) {
  NS_ASSERTION(aParams, "Must have non-null statement");
  return aParams->BindUTF8StringByIndex(index,
                                        StringHead(aURLString, URI_LENGTH_MAX));
}

// Bind URI to params by name.
nsresult  // static
URIBinder::Bind(mozIStorageBindingParams* aParams, const nsACString& aName,
                nsIURI* aURI) {
  NS_ASSERTION(aParams, "Must have non-null params array");
  NS_ASSERTION(aURI, "Must have non-null uri");

  URI_TO_URLCSTRING(aURI, spec);
  return URIBinder::Bind(aParams, aName, spec);
}

// Bind URLCString to params by name.
nsresult  // static
URIBinder::Bind(mozIStorageBindingParams* aParams, const nsACString& aName,
                const nsACString& aURLString) {
  NS_ASSERTION(aParams, "Must have non-null params array");

  nsresult rv = aParams->BindUTF8StringByName(
      aName, StringHead(aURLString, URI_LENGTH_MAX));
  NS_ENSURE_SUCCESS(rv, rv);
  return NS_OK;
}

#undef URI_TO_URLCSTRING

nsresult GetReversedHostname(nsIURI* aURI, nsString& aRevHost) {
  nsAutoCString forward8;
  nsresult rv = aURI->GetHost(forward8);
  // Not all URIs have a host.
  if (NS_FAILED(rv)) return rv;

  // can't do reversing in UTF8, better use 16-bit chars
  GetReversedHostname(NS_ConvertUTF8toUTF16(forward8), aRevHost);
  return NS_OK;
}

void GetReversedHostname(const nsString& aForward, nsString& aRevHost) {
  ReverseString(aForward, aRevHost);
  aRevHost.Append(char16_t('.'));
}

void ReverseString(const nsString& aInput, nsString& aReversed) {
  aReversed.Truncate(0);
  for (int32_t i = aInput.Length() - 1; i >= 0; i--) {
    aReversed.Append(aInput[i]);
  }
}

nsresult GenerateGUID(nsACString& _guid) {
  _guid.Truncate();

  // Request raw random bytes and base64url encode them.  For each set of three
  // bytes, we get one character.
  const uint32_t kRequiredBytesLength =
      static_cast<uint32_t>(GUID_LENGTH / 4 * 3);

  uint8_t buffer[kRequiredBytesLength];
  if (!mozilla::GenerateRandomBytesFromOS(buffer, kRequiredBytesLength)) {
    return NS_ERROR_FAILURE;
  }

  nsresult rv = Base64URLEncode(kRequiredBytesLength, buffer,
                                Base64URLEncodePaddingPolicy::Omit, _guid);
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ASSERTION(_guid.Length() == GUID_LENGTH, "GUID is not the right size!");
  return NS_OK;
}

bool IsValidGUID(const nsACString& aGUID) {
  nsCString::size_type len = aGUID.Length();
  if (len != GUID_LENGTH) {
    return false;
  }

  for (nsCString::size_type i = 0; i < len; i++) {
    char c = aGUID[i];
    if ((c >= 'a' && c <= 'z') ||  // a-z
        (c >= 'A' && c <= 'Z') ||  // A-Z
        (c >= '0' && c <= '9') ||  // 0-9
        c == '-' || c == '_') {    // - or _
      continue;
    }
    return false;
  }
  return true;
}

void TruncateTitle(const nsACString& aTitle, nsACString& aTrimmed) {
  if (aTitle.IsVoid()) {
    return;
  }
  aTrimmed = aTitle;
  if (aTitle.Length() > TITLE_LENGTH_MAX) {
    aTrimmed = StringHead(aTitle, TITLE_LENGTH_MAX);
  }
}

PRTime RoundToMilliseconds(PRTime aTime) {
  return aTime - (aTime % PR_USEC_PER_MSEC);
}

PRTime RoundedPRNow() { return RoundToMilliseconds(PR_Now()); }

nsresult HashURL(const nsACString& aSpec, const nsACString& aMode,
                 uint64_t* _hash) {
  NS_ENSURE_ARG_POINTER(_hash);

  // HashString doesn't stop at the string boundaries if a length is passed to
  // it, so ensure to pass a proper value.
  const uint32_t maxLenToHash =
      std::min(static_cast<uint32_t>(aSpec.Length()), MAX_CHARS_TO_HASH);

  if (aMode.IsEmpty()) {
    // URI-like strings (having a prefix before a colon), are handled specially,
    // as a 48 bit hash, where first 16 bits are the prefix hash, while the
    // other 32 are the string hash.
    // The 16 bits have been decided based on the fact hashing all of the IANA
    // known schemes, plus "places", does not generate collisions.
    // Since we only care about schemes, we just search in the first 50 chars.
    // The longest known IANA scheme, at this time, is 30 chars.
    const nsDependentCSubstring& strHead = StringHead(aSpec, 50);
    nsACString::const_iterator start, tip, end;
    strHead.BeginReading(tip);
    start = tip;
    strHead.EndReading(end);
    uint32_t strHash = HashString(aSpec.BeginReading(), maxLenToHash);
    if (FindCharInReadable(':', tip, end)) {
      const nsDependentCSubstring& prefix = Substring(start, tip);
      uint64_t prefixHash =
          static_cast<uint64_t>(HashString(prefix) & 0x0000FFFF);
      // The second half of the url is more likely to be unique, so we add it.
      *_hash = (prefixHash << 32) + strHash;
    } else {
      *_hash = strHash;
    }
  } else if (aMode.EqualsLiteral("prefix_lo")) {
    // Keep only 16 bits.
    *_hash = static_cast<uint64_t>(
                 HashString(aSpec.BeginReading(), maxLenToHash) & 0x0000FFFF)
             << 32;
  } else if (aMode.EqualsLiteral("prefix_hi")) {
    // Keep only 16 bits.
    *_hash = static_cast<uint64_t>(
                 HashString(aSpec.BeginReading(), maxLenToHash) & 0x0000FFFF)
             << 32;
    // Make this a prefix upper bound by filling the lowest 32 bits.
    *_hash += 0xFFFFFFFF;
  } else {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

bool GetHiddenState(bool aIsRedirect, uint32_t aTransitionType) {
  return aTransitionType == nsINavHistoryService::TRANSITION_FRAMED_LINK ||
         aTransitionType == nsINavHistoryService::TRANSITION_EMBED ||
         aIsRedirect;
}

nsresult TokenizeQueryString(const nsACString& aQuery,
                             nsTArray<QueryKeyValuePair>* aTokens) {
  // Strip off the "place:" prefix
  const uint32_t prefixlen = 6;  // = strlen("place:");
  nsCString query;
  if (aQuery.Length() >= prefixlen &&
      Substring(aQuery, 0, prefixlen).EqualsLiteral("place:"))
    query = Substring(aQuery, prefixlen);
  else
    query = aQuery;

  int32_t keyFirstIndex = 0;
  int32_t equalsIndex = 0;
  for (uint32_t i = 0; i < query.Length(); i++) {
    if (query[i] == '&') {
      // new clause, save last one
      if (i - keyFirstIndex > 1) {
        // XXX(Bug 1631371) Check if this should use a fallible operation as it
        // pretended earlier, or change the return type to void.
        aTokens->AppendElement(
            QueryKeyValuePair(query, keyFirstIndex, equalsIndex, i));
      }
      keyFirstIndex = equalsIndex = i + 1;
    } else if (query[i] == '=') {
      equalsIndex = i;
    }
  }

  // handle last pair, if any
  if (query.Length() - keyFirstIndex > 1) {
    // XXX(Bug 1631371) Check if this should use a fallible operation as it
    // pretended earlier, or change the return type to void.
    aTokens->AppendElement(
        QueryKeyValuePair(query, keyFirstIndex, equalsIndex, query.Length()));
  }
  return NS_OK;
}

void TokensToQueryString(const nsTArray<QueryKeyValuePair>& aTokens,
                         nsACString& aQuery) {
  aQuery = "place:"_ns;
  StringJoinAppend(aQuery, "&"_ns, aTokens,
                   [](nsACString& dst, const QueryKeyValuePair& token) {
                     dst.Append(token.key);
                     dst.AppendLiteral("=");
                     dst.Append(token.value);
                   });
}

nsresult BackupDatabaseFile(nsIFile* aDBFile, const nsAString& aBackupFileName,
                            nsIFile* aBackupParentDirectory, nsIFile** backup) {
  nsresult rv;
  nsCOMPtr<nsIFile> parentDir = aBackupParentDirectory;
  if (!parentDir) {
    // This argument is optional, and defaults to the same parent directory
    // as the current file.
    rv = aDBFile->GetParent(getter_AddRefs(parentDir));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsCOMPtr<nsIFile> backupDB;
  rv = parentDir->Clone(getter_AddRefs(backupDB));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = backupDB->Append(aBackupFileName);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = backupDB->CreateUnique(nsIFile::NORMAL_FILE_TYPE, 0600);
  NS_ENSURE_SUCCESS(rv, rv);
  nsAutoString fileName;
  rv = backupDB->GetLeafName(fileName);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = backupDB->Remove(false);
  NS_ENSURE_SUCCESS(rv, rv);

  backupDB.forget(backup);
  return aDBFile->CopyTo(parentDir, fileName);
}

already_AddRefed<nsIURI> GetExposableURI(nsIURI* aURI) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aURI);

  nsresult rv;
  nsCOMPtr<nsIIOService> ioService = do_GetIOService(&rv);
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to get nsIIOService");
    return nsCOMPtr<nsIURI>(aURI).forget();
  }

  nsCOMPtr<nsIURI> uri;
  rv = ioService->CreateExposableURI(aURI, getter_AddRefs(uri));
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to create exposable URI");
    return nsCOMPtr<nsIURI>(aURI).forget();
  }

  return uri.forget();
}

}  // namespace places
}  // namespace mozilla
