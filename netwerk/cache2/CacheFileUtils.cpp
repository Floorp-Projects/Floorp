/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CacheLog.h"
#include "CacheFileUtils.h"
#include "LoadContextInfo.h"
#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "nsString.h"
#include <algorithm>


namespace mozilla {
namespace net {
namespace CacheFileUtils {

namespace { // anon

/**
 * A simple recursive descent parser for the mapping key.
 */
class KeyParser
{
public:
  KeyParser(nsACString::const_iterator aCaret, nsACString::const_iterator aEnd)
    : caret(aCaret)
    , end(aEnd)
    // Initialize attributes to their default values
    , appId(nsILoadContextInfo::NO_APP_ID)
    , isPrivate(false)
    , isInBrowser(false)
    , isAnonymous(false)
    // Initialize the cache key to a zero length by default
    , cacheKey(aEnd)
    , lastTag(0)
  {
  }

private:
  // Current character being parsed
  nsACString::const_iterator caret;
  // The end of the buffer
  nsACString::const_iterator const end;

  // Results
  uint32_t appId;
  bool isPrivate;
  bool isInBrowser;
  bool isAnonymous;
  nsCString idEnhance;
  // Position of the cache key, if present
  nsACString::const_iterator cacheKey;

  // Keeps the last tag name, used for alphabetical sort checking
  char lastTag;

  bool ParseTags()
  {
    // Expects to be at the tag name or at the end
    if (caret == end)
      return true;

    // 'Read' the tag name and move to the next char
    char const tag = *caret++;
    // Check the alphabetical order, hard-fail on disobedience
    if (!(lastTag < tag || tag == ':'))
      return false;

    lastTag = tag;

    switch (tag) {
    case ':':
      // last possible tag, when present there is the cacheKey following,
      // not terminated with ',' and no need to unescape.
      cacheKey = caret;
      caret = end;
      return true;
    case 'p':
      isPrivate = true;
      break;
    case 'b':
      isInBrowser = true;
      break;
    case 'a':
      isAnonymous = true;
      break;
    case 'i': {
      nsAutoCString appIdString;
      if (!ParseValue(&appIdString))
        return false;

      nsresult rv;
      int64_t appId64 = appIdString.ToInteger64(&rv);
      if (NS_FAILED(rv))
        return false; // appid value is mandatory
      if (appId64 < 0 || appId64 > PR_UINT32_MAX)
        return false; // not in the range
      appId = static_cast<uint32_t>(appId64);

      break;
    }
    case '~':
      if (!ParseValue(&idEnhance))
        return false;
      break;
    default:
      if (!ParseValue()) // skip any tag values, optional
        return false;
      break;
    }

    // Recurse to the next tag
    return ParseNextTagOrEnd();
  }

  bool ParseNextTagOrEnd()
  {
    // We expect a comma after every tag
    if (caret == end || *caret++ != ',')
      return false;

    // Go to another tag
    return ParseTags();
  }

  bool ParseValue(nsACString * result = nullptr)
  {
    // If at the end, fail since we expect a comma ; value may be empty tho
    if (caret == end)
      return false;

    // Remeber where the value starts
    nsACString::const_iterator val = caret;
    nsACString::const_iterator comma = end;
    bool escape = false;
    while (caret != end) {
      nsACString::const_iterator at = caret;
      ++caret; // we can safely break/continue the loop now

      if (*at == ',') {
        if (comma != end) {
          // another comma (we have found ",," -> escape)
          comma = end;
          escape = true;
        } else {
          comma = at;
        }
        continue;
      }

      if (comma != end) {
        // after a single comma
        break;
      }
    }

    // At this point |comma| points to the last and lone ',' we've hit.
    // If a lone comma was not found, |comma| is at the end of the buffer,
    // that is not expected and we return failure.

    caret = comma;
    if (result) {
      if (escape) {
        // No ReplaceSubstring on nsACString..
        nsAutoCString _result(Substring(val, caret));
        _result.ReplaceSubstring(NS_LITERAL_CSTRING(",,"), NS_LITERAL_CSTRING(","));
        result->Assign(_result);
      } else {
        result->Assign(Substring(val, caret));
      }
    }

    return caret != end;
  }

public:
  already_AddRefed<LoadContextInfo> Parse()
  {
    nsRefPtr<LoadContextInfo> info;
    if (ParseTags())
      info = GetLoadContextInfo(isPrivate, appId, isInBrowser, isAnonymous);

    return info.forget();
  }

  void URISpec(nsACString &result)
  {
    // cacheKey is either pointing to end or the position where the cache key is.
    result.Assign(Substring(cacheKey, end));
  }

  void IdEnhance(nsACString &result)
  {
    result.Assign(idEnhance);
  }
};

} // anon

already_AddRefed<nsILoadContextInfo>
ParseKey(const nsCSubstring &aKey,
         nsCSubstring *aIdEnhance,
         nsCSubstring *aURISpec)
{
  nsACString::const_iterator caret, end;
  aKey.BeginReading(caret);
  aKey.EndReading(end);

  KeyParser parser(caret, end);
  nsRefPtr<LoadContextInfo> info = parser.Parse();

  if (info) {
    if (aIdEnhance)
      parser.IdEnhance(*aIdEnhance);
    if (aURISpec)
      parser.URISpec(*aURISpec);
  }

  return info.forget();
}

void
AppendKeyPrefix(nsILoadContextInfo* aInfo, nsACString &_retval)
{
  /**
   * This key is used to salt file hashes.  When form of the key is changed
   * cache entries will fail to find on disk.
   *
   * IMPORTANT NOTE:
   * Keep the attributes list sorted according their ASCII code.
   */

  if (aInfo->IsAnonymous()) {
    _retval.AppendLiteral("a,");
  }

  if (aInfo->IsInBrowserElement()) {
    _retval.AppendLiteral("b,");
  }

  if (aInfo->AppId() != nsILoadContextInfo::NO_APP_ID) {
    _retval.Append('i');
    _retval.AppendInt(aInfo->AppId());
    _retval.Append(',');
  }

  if (aInfo->IsPrivate()) {
    _retval.AppendLiteral("p,");
  }
}

void
AppendTagWithValue(nsACString & aTarget, char const aTag, nsCSubstring const & aValue)
{
  aTarget.Append(aTag);

  // First check the value string to save some memory copying
  // for cases we don't need to escape at all (most likely).
  if (!aValue.IsEmpty()) {
    if (aValue.FindChar(',') == kNotFound) {
      // No need to escape
      aTarget.Append(aValue);
    } else {
      nsAutoCString escapedValue(aValue);
      escapedValue.ReplaceSubstring(
        NS_LITERAL_CSTRING(","), NS_LITERAL_CSTRING(",,"));
      aTarget.Append(escapedValue);
    }
  }

  aTarget.Append(',');
}

nsresult
KeyMatchesLoadContextInfo(const nsACString &aKey, nsILoadContextInfo *aInfo,
                          bool *_retval)
{
  nsCOMPtr<nsILoadContextInfo> info = ParseKey(aKey);

  if (!info) {
    return NS_ERROR_FAILURE;
  }

  *_retval = info->Equals(aInfo);
  return NS_OK;
}

ValidityPair::ValidityPair(uint32_t aOffset, uint32_t aLen)
  : mOffset(aOffset), mLen(aLen)
{}

ValidityPair&
ValidityPair::operator=(const ValidityPair& aOther)
{
  mOffset = aOther.mOffset;
  mLen = aOther.mLen;
  return *this;
}

bool
ValidityPair::CanBeMerged(const ValidityPair& aOther) const
{
  // The pairs can be merged into a single one if the start of one of the pairs
  // is placed anywhere in the validity interval of other pair or exactly after
  // its end.
  return IsInOrFollows(aOther.mOffset) || aOther.IsInOrFollows(mOffset);
}

bool
ValidityPair::IsInOrFollows(uint32_t aOffset) const
{
  return mOffset <= aOffset && mOffset + mLen >= aOffset;
}

bool
ValidityPair::LessThan(const ValidityPair& aOther) const
{
  if (mOffset < aOther.mOffset) {
    return true;
  }

  if (mOffset == aOther.mOffset && mLen < aOther.mLen) {
    return true;
  }

  return false;
}

void
ValidityPair::Merge(const ValidityPair& aOther)
{
  MOZ_ASSERT(CanBeMerged(aOther));

  uint32_t offset = std::min(mOffset, aOther.mOffset);
  uint32_t end = std::max(mOffset + mLen, aOther.mOffset + aOther.mLen);

  mOffset = offset;
  mLen = end - offset;
}

void
ValidityMap::Log() const
{
  LOG(("ValidityMap::Log() - number of pairs: %u", mMap.Length()));
  for (uint32_t i=0; i<mMap.Length(); i++) {
    LOG(("    (%u, %u)", mMap[i].Offset() + 0, mMap[i].Len() + 0));
  }
}

uint32_t
ValidityMap::Length() const
{
  return mMap.Length();
}

void
ValidityMap::AddPair(uint32_t aOffset, uint32_t aLen)
{
  ValidityPair pair(aOffset, aLen);

  if (mMap.Length() == 0) {
    mMap.AppendElement(pair);
    return;
  }

  // Find out where to place this pair into the map, it can overlap only with
  // one preceding pair and all subsequent pairs.
  uint32_t pos = 0;
  for (pos = mMap.Length(); pos > 0; ) {
    --pos;

    if (mMap[pos].LessThan(pair)) {
      // The new pair should be either inserted after pos or merged with it.
      if (mMap[pos].CanBeMerged(pair)) {
        // Merge with the preceding pair
        mMap[pos].Merge(pair);
      } else {
        // They don't overlap, element must be placed after pos element
        ++pos;
        if (pos == mMap.Length()) {
          mMap.AppendElement(pair);
        } else {
          mMap.InsertElementAt(pos, pair);
        }
      }

      break;
    }

    if (pos == 0) {
      // The new pair should be placed in front of all existing pairs.
      mMap.InsertElementAt(0, pair);
    }
  }

  // pos now points to merged or inserted pair, check whether it overlaps with
  // subsequent pairs.
  while (pos + 1 < mMap.Length()) {
    if (mMap[pos].CanBeMerged(mMap[pos + 1])) {
      mMap[pos].Merge(mMap[pos + 1]);
      mMap.RemoveElementAt(pos + 1);
    } else {
      break;
    }
  }
}

void
ValidityMap::Clear()
{
  mMap.Clear();
}

size_t
ValidityMap::SizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf) const
{
  return mMap.SizeOfExcludingThis(mallocSizeOf);
}

ValidityPair&
ValidityMap::operator[](uint32_t aIdx)
{
  return mMap.ElementAt(aIdx);
}

} // CacheFileUtils
} // net
} // mozilla
