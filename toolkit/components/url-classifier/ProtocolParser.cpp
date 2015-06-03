//* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ProtocolParser.h"
#include "LookupCache.h"
#include "nsNetCID.h"
#include "mozilla/Logging.h"
#include "prnetdb.h"
#include "prprf.h"

#include "nsUrlClassifierUtils.h"

// NSPR_LOG_MODULES=UrlClassifierDbService:5
extern PRLogModuleInfo *gUrlClassifierDbServiceLog;
#define LOG(args) MOZ_LOG(gUrlClassifierDbServiceLog, mozilla::LogLevel::Debug, args)
#define LOG_ENABLED() MOZ_LOG_TEST(gUrlClassifierDbServiceLog, mozilla::LogLevel::Debug)

namespace mozilla {
namespace safebrowsing {

// Updates will fail if fed chunks larger than this
const uint32_t MAX_CHUNK_SIZE = (1024 * 1024);
// Updates will fail if the total number of tocuhed chunks is larger than this
const uint32_t MAX_CHUNK_RANGE = 1000000;

const uint32_t DOMAIN_SIZE = 4;

// Parse one stringified range of chunks of the form "n" or "n-m" from a
// comma-separated list of chunks.  Upon return, 'begin' will point to the
// next range of chunks in the list of chunks.
static bool
ParseChunkRange(nsACString::const_iterator& aBegin,
                const nsACString::const_iterator& aEnd,
                uint32_t* aFirst, uint32_t* aLast)
{
  nsACString::const_iterator iter = aBegin;
  FindCharInReadable(',', iter, aEnd);

  nsAutoCString element(Substring(aBegin, iter));
  aBegin = iter;
  if (aBegin != aEnd)
    aBegin++;

  uint32_t numRead = PR_sscanf(element.get(), "%u-%u", aFirst, aLast);
  if (numRead == 2) {
    if (*aFirst > *aLast) {
      uint32_t tmp = *aFirst;
      *aFirst = *aLast;
      *aLast = tmp;
    }
    return true;
  }

  if (numRead == 1) {
    *aLast = *aFirst;
    return true;
  }

  return false;
}

ProtocolParser::ProtocolParser()
    : mState(PROTOCOL_STATE_CONTROL)
  , mUpdateStatus(NS_OK)
  , mUpdateWait(0)
  , mResetRequested(false)
{
}

ProtocolParser::~ProtocolParser()
{
  CleanupUpdates();
}

nsresult
ProtocolParser::Init(nsICryptoHash* aHasher)
{
  mCryptoHash = aHasher;
  return NS_OK;
}

void
ProtocolParser::SetCurrentTable(const nsACString& aTable)
{
  mTableUpdate = GetTableUpdate(aTable);
}

nsresult
ProtocolParser::AppendStream(const nsACString& aData)
{
  if (NS_FAILED(mUpdateStatus))
    return mUpdateStatus;

  nsresult rv;
  mPending.Append(aData);

  bool done = false;
  while (!done) {
    if (mState == PROTOCOL_STATE_CONTROL) {
      rv = ProcessControl(&done);
    } else if (mState == PROTOCOL_STATE_CHUNK) {
      rv = ProcessChunk(&done);
    } else {
      NS_ERROR("Unexpected protocol state");
      rv = NS_ERROR_FAILURE;
    }
    if (NS_FAILED(rv)) {
      mUpdateStatus = rv;
      return rv;
    }
  }
  return NS_OK;
}

nsresult
ProtocolParser::ProcessControl(bool* aDone)
{
  nsresult rv;

  nsAutoCString line;
  *aDone = true;
  while (NextLine(line)) {
    //LOG(("Processing %s\n", line.get()));

    if (StringBeginsWith(line, NS_LITERAL_CSTRING("i:"))) {
      // Set the table name from the table header line.
      SetCurrentTable(Substring(line, 2));
    } else if (StringBeginsWith(line, NS_LITERAL_CSTRING("n:"))) {
      if (PR_sscanf(line.get(), "n:%d", &mUpdateWait) != 1) {
        LOG(("Error parsing n: '%s' (%d)", line.get(), mUpdateWait));
        mUpdateWait = 0;
      }
    } else if (line.EqualsLiteral("r:pleasereset")) {
      mResetRequested = true;
    } else if (StringBeginsWith(line, NS_LITERAL_CSTRING("u:"))) {
      rv = ProcessForward(line);
      NS_ENSURE_SUCCESS(rv, rv);
    } else if (StringBeginsWith(line, NS_LITERAL_CSTRING("a:")) ||
               StringBeginsWith(line, NS_LITERAL_CSTRING("s:"))) {
      rv = ProcessChunkControl(line);
      NS_ENSURE_SUCCESS(rv, rv);
      *aDone = false;
      return NS_OK;
    } else if (StringBeginsWith(line, NS_LITERAL_CSTRING("ad:")) ||
               StringBeginsWith(line, NS_LITERAL_CSTRING("sd:"))) {
      rv = ProcessExpirations(line);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  *aDone = true;
  return NS_OK;
}

nsresult
ProtocolParser::ProcessExpirations(const nsCString& aLine)
{
  if (!mTableUpdate) {
    NS_WARNING("Got an expiration without a table.");
    return NS_ERROR_FAILURE;
  }
  const nsCSubstring &list = Substring(aLine, 3);
  nsACString::const_iterator begin, end;
  list.BeginReading(begin);
  list.EndReading(end);
  while (begin != end) {
    uint32_t first, last;
    if (ParseChunkRange(begin, end, &first, &last)) {
      if (last < first) return NS_ERROR_FAILURE;
      if (last - first > MAX_CHUNK_RANGE) return NS_ERROR_FAILURE;
      for (uint32_t num = first; num <= last; num++) {
        if (aLine[0] == 'a') {
          nsresult rv = mTableUpdate->NewAddExpiration(num);
          if (NS_FAILED(rv)) {
            return rv;
          }
        } else {
          nsresult rv = mTableUpdate->NewSubExpiration(num);
          if (NS_FAILED(rv)) {
            return rv;
          }
        }
      }
    } else {
      return NS_ERROR_FAILURE;
    }
  }
  return NS_OK;
}

nsresult
ProtocolParser::ProcessChunkControl(const nsCString& aLine)
{
  if (!mTableUpdate) {
    NS_WARNING("Got a chunk before getting a table.");
    return NS_ERROR_FAILURE;
  }

  mState = PROTOCOL_STATE_CHUNK;
  char command;

  mChunkState.Clear();

  if (PR_sscanf(aLine.get(),
                "%c:%d:%d:%d",
                &command,
                &mChunkState.num, &mChunkState.hashSize, &mChunkState.length)
      != 4)
  {
    return NS_ERROR_FAILURE;
  }

  if (mChunkState.length > MAX_CHUNK_SIZE) {
    return NS_ERROR_FAILURE;
  }

  if (!(mChunkState.hashSize == PREFIX_SIZE || mChunkState.hashSize == COMPLETE_SIZE)) {
    NS_WARNING("Invalid hash size specified in update.");
    return NS_ERROR_FAILURE;
  }

  if (StringEndsWith(mTableUpdate->TableName(),
                     NS_LITERAL_CSTRING("-shavar")) ||
      StringEndsWith(mTableUpdate->TableName(),
                     NS_LITERAL_CSTRING("-simple"))) {
    // Accommodate test tables ending in -simple for now.
    mChunkState.type = (command == 'a') ? CHUNK_ADD : CHUNK_SUB;
  } else if (StringEndsWith(mTableUpdate->TableName(),
    NS_LITERAL_CSTRING("-digest256"))) {
    LOG(("Processing digest256 data"));
    mChunkState.type = (command == 'a') ? CHUNK_ADD_DIGEST : CHUNK_SUB_DIGEST;
  }
  nsresult rv;
  switch (mChunkState.type) {
    case CHUNK_ADD:
      rv = mTableUpdate->NewAddChunk(mChunkState.num);
      if (NS_FAILED(rv)) {
        return rv;
      }
      break;
    case CHUNK_SUB:
      rv = mTableUpdate->NewSubChunk(mChunkState.num);
      if (NS_FAILED(rv)) {
        return rv;
      }
      break;
    case CHUNK_ADD_DIGEST:
      rv = mTableUpdate->NewAddChunk(mChunkState.num);
      if (NS_FAILED(rv)) {
        return rv;
      }
      break;
    case CHUNK_SUB_DIGEST:
      rv = mTableUpdate->NewSubChunk(mChunkState.num);
      if (NS_FAILED(rv)) {
        return rv;
      }
      break;
  }

  return NS_OK;
}

nsresult
ProtocolParser::ProcessForward(const nsCString& aLine)
{
  const nsCSubstring &forward = Substring(aLine, 2);
  return AddForward(forward);
}

nsresult
ProtocolParser::AddForward(const nsACString& aUrl)
{
  if (!mTableUpdate) {
    NS_WARNING("Forward without a table name.");
    return NS_ERROR_FAILURE;
  }

  ForwardedUpdate *forward = mForwards.AppendElement();
  forward->table = mTableUpdate->TableName();
  forward->url.Assign(aUrl);

  return NS_OK;
}

nsresult
ProtocolParser::ProcessChunk(bool* aDone)
{
  if (!mTableUpdate) {
    NS_WARNING("Processing chunk without an active table.");
    return NS_ERROR_FAILURE;
  }

  NS_ASSERTION(mChunkState.num != 0, "Must have a chunk number.");

  if (mPending.Length() < mChunkState.length) {
    *aDone = true;
    return NS_OK;
  }

  // Pull the chunk out of the pending stream data.
  nsAutoCString chunk;
  chunk.Assign(Substring(mPending, 0, mChunkState.length));
  mPending.Cut(0, mChunkState.length);

  *aDone = false;
  mState = PROTOCOL_STATE_CONTROL;

  //LOG(("Handling a %d-byte chunk", chunk.Length()));
  if (StringEndsWith(mTableUpdate->TableName(),
                     NS_LITERAL_CSTRING("-shavar"))) {
    return ProcessShaChunk(chunk);
  }
  if (StringEndsWith(mTableUpdate->TableName(),
             NS_LITERAL_CSTRING("-digest256"))) {
    return ProcessDigestChunk(chunk);
  }
  return ProcessPlaintextChunk(chunk);
}

/**
 * Process a plaintext chunk (currently only used in unit tests).
 */
nsresult
ProtocolParser::ProcessPlaintextChunk(const nsACString& aChunk)
{
  if (!mTableUpdate) {
    NS_WARNING("Chunk received with no table.");
    return NS_ERROR_FAILURE;
  }

  nsTArray<nsCString> lines;
  ParseString(PromiseFlatCString(aChunk), '\n', lines);

  // non-hashed tables need to be hashed
  for (uint32_t i = 0; i < lines.Length(); i++) {
    nsCString& line = lines[i];

    if (mChunkState.type == CHUNK_ADD) {
      if (mChunkState.hashSize == COMPLETE_SIZE) {
        Completion hash;
        hash.FromPlaintext(line, mCryptoHash);
        nsresult rv = mTableUpdate->NewAddComplete(mChunkState.num, hash);
        if (NS_FAILED(rv)) {
          return rv;
        }
      } else {
        NS_ASSERTION(mChunkState.hashSize == 4, "Only 32- or 4-byte hashes can be used for add chunks.");
        Prefix hash;
        hash.FromPlaintext(line, mCryptoHash);
        nsresult rv = mTableUpdate->NewAddPrefix(mChunkState.num, hash);
        if (NS_FAILED(rv)) {
          return rv;
        }
      }
    } else {
      nsCString::const_iterator begin, iter, end;
      line.BeginReading(begin);
      line.EndReading(end);
      iter = begin;
      uint32_t addChunk;
      if (!FindCharInReadable(':', iter, end) ||
          PR_sscanf(lines[i].get(), "%d:", &addChunk) != 1) {
        NS_WARNING("Received sub chunk without associated add chunk.");
        return NS_ERROR_FAILURE;
      }
      iter++;

      if (mChunkState.hashSize == COMPLETE_SIZE) {
        Completion hash;
        hash.FromPlaintext(Substring(iter, end), mCryptoHash);
        nsresult rv = mTableUpdate->NewSubComplete(addChunk, hash, mChunkState.num);
        if (NS_FAILED(rv)) {
          return rv;
        }
      } else {
        NS_ASSERTION(mChunkState.hashSize == 4, "Only 32- or 4-byte hashes can be used for add chunks.");
        Prefix hash;
        hash.FromPlaintext(Substring(iter, end), mCryptoHash);
        nsresult rv = mTableUpdate->NewSubPrefix(addChunk, hash, mChunkState.num);
        if (NS_FAILED(rv)) {
          return rv;
        }
      }
    }
  }

  return NS_OK;
}

nsresult
ProtocolParser::ProcessShaChunk(const nsACString& aChunk)
{
  uint32_t start = 0;
  while (start < aChunk.Length()) {
    // First four bytes are the domain key.
    Prefix domain;
    domain.Assign(Substring(aChunk, start, DOMAIN_SIZE));
    start += DOMAIN_SIZE;

    // Then a count of entries.
    uint8_t numEntries = static_cast<uint8_t>(aChunk[start]);
    start++;

    nsresult rv;
    if (mChunkState.type == CHUNK_ADD && mChunkState.hashSize == PREFIX_SIZE) {
      rv = ProcessHostAdd(domain, numEntries, aChunk, &start);
    } else if (mChunkState.type == CHUNK_ADD && mChunkState.hashSize == COMPLETE_SIZE) {
      rv = ProcessHostAddComplete(numEntries, aChunk, &start);
    } else if (mChunkState.type == CHUNK_SUB && mChunkState.hashSize == PREFIX_SIZE) {
      rv = ProcessHostSub(domain, numEntries, aChunk, &start);
    } else if (mChunkState.type == CHUNK_SUB && mChunkState.hashSize == COMPLETE_SIZE) {
      rv = ProcessHostSubComplete(numEntries, aChunk, &start);
    } else {
      NS_WARNING("Unexpected chunk type/hash size!");
      LOG(("Got an unexpected chunk type/hash size: %s:%d",
           mChunkState.type == CHUNK_ADD ? "add" : "sub",
           mChunkState.hashSize));
      return NS_ERROR_FAILURE;
    }
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

nsresult
ProtocolParser::ProcessDigestChunk(const nsACString& aChunk)
{
  if (mChunkState.type == CHUNK_ADD_DIGEST) {
    return ProcessDigestAdd(aChunk);
  }
  if (mChunkState.type == CHUNK_SUB_DIGEST) {
    return ProcessDigestSub(aChunk);
  }
  return NS_ERROR_UNEXPECTED;
}

nsresult
ProtocolParser::ProcessDigestAdd(const nsACString& aChunk)
{
  // The ABNF format for add chunks is (HASH)+, where HASH is 32 bytes.
  MOZ_ASSERT(aChunk.Length() % 32 == 0,
             "Chunk length in bytes must be divisible by 4");
  uint32_t start = 0;
  while (start < aChunk.Length()) {
    Completion hash;
    hash.Assign(Substring(aChunk, start, COMPLETE_SIZE));
    start += COMPLETE_SIZE;
    nsresult rv = mTableUpdate->NewAddComplete(mChunkState.num, hash);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }
  return NS_OK;
}

nsresult
ProtocolParser::ProcessDigestSub(const nsACString& aChunk)
{
  // The ABNF format for sub chunks is (ADDCHUNKNUM HASH)+, where ADDCHUNKNUM
  // is a 4 byte chunk number, and HASH is 32 bytes.
  MOZ_ASSERT(aChunk.Length() % 36 == 0,
             "Chunk length in bytes must be divisible by 36");
  uint32_t start = 0;
  while (start < aChunk.Length()) {
    // Read ADDCHUNKNUM
    const nsCSubstring& addChunkStr = Substring(aChunk, start, 4);
    start += 4;

    uint32_t addChunk;
    memcpy(&addChunk, addChunkStr.BeginReading(), 4);
    addChunk = PR_ntohl(addChunk);

    // Read the hash
    Completion hash;
    hash.Assign(Substring(aChunk, start, COMPLETE_SIZE));
    start += COMPLETE_SIZE;

    nsresult rv = mTableUpdate->NewSubComplete(addChunk, hash, mChunkState.num);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }
  return NS_OK;
}

nsresult
ProtocolParser::ProcessHostAdd(const Prefix& aDomain, uint8_t aNumEntries,
                               const nsACString& aChunk, uint32_t* aStart)
{
  NS_ASSERTION(mChunkState.hashSize == PREFIX_SIZE,
               "ProcessHostAdd should only be called for prefix hashes.");

  if (aNumEntries == 0) {
    nsresult rv = mTableUpdate->NewAddPrefix(mChunkState.num, aDomain);
    if (NS_FAILED(rv)) {
      return rv;
    }
    return NS_OK;
  }

  if (*aStart + (PREFIX_SIZE * aNumEntries) > aChunk.Length()) {
    NS_WARNING("Chunk is not long enough to contain the expected entries.");
    return NS_ERROR_FAILURE;
  }

  for (uint8_t i = 0; i < aNumEntries; i++) {
    Prefix hash;
    hash.Assign(Substring(aChunk, *aStart, PREFIX_SIZE));
    nsresult rv = mTableUpdate->NewAddPrefix(mChunkState.num, hash);
    if (NS_FAILED(rv)) {
      return rv;
    }
    *aStart += PREFIX_SIZE;
  }

  return NS_OK;
}

nsresult
ProtocolParser::ProcessHostSub(const Prefix& aDomain, uint8_t aNumEntries,
                               const nsACString& aChunk, uint32_t *aStart)
{
  NS_ASSERTION(mChunkState.hashSize == PREFIX_SIZE,
               "ProcessHostSub should only be called for prefix hashes.");

  if (aNumEntries == 0) {
    if ((*aStart) + 4 > aChunk.Length()) {
      NS_WARNING("Received a zero-entry sub chunk without an associated add.");
      return NS_ERROR_FAILURE;
    }

    const nsCSubstring& addChunkStr = Substring(aChunk, *aStart, 4);
    *aStart += 4;

    uint32_t addChunk;
    memcpy(&addChunk, addChunkStr.BeginReading(), 4);
    addChunk = PR_ntohl(addChunk);

    nsresult rv = mTableUpdate->NewSubPrefix(addChunk, aDomain, mChunkState.num);
    if (NS_FAILED(rv)) {
      return rv;
    }
    return NS_OK;
  }

  if (*aStart + ((PREFIX_SIZE + 4) * aNumEntries) > aChunk.Length()) {
    NS_WARNING("Chunk is not long enough to contain the expected entries.");
    return NS_ERROR_FAILURE;
  }

  for (uint8_t i = 0; i < aNumEntries; i++) {
    const nsCSubstring& addChunkStr = Substring(aChunk, *aStart, 4);
    *aStart += 4;

    uint32_t addChunk;
    memcpy(&addChunk, addChunkStr.BeginReading(), 4);
    addChunk = PR_ntohl(addChunk);

    Prefix prefix;
    prefix.Assign(Substring(aChunk, *aStart, PREFIX_SIZE));
    *aStart += PREFIX_SIZE;

    nsresult rv = mTableUpdate->NewSubPrefix(addChunk, prefix, mChunkState.num);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  return NS_OK;
}

nsresult
ProtocolParser::ProcessHostAddComplete(uint8_t aNumEntries,
                                       const nsACString& aChunk, uint32_t* aStart)
{
  NS_ASSERTION(mChunkState.hashSize == COMPLETE_SIZE,
               "ProcessHostAddComplete should only be called for complete hashes.");

  if (aNumEntries == 0) {
    // this is totally comprehensible.
    // My sarcasm detector is going off!
    NS_WARNING("Expected > 0 entries for a 32-byte hash add.");
    return NS_OK;
  }

  if (*aStart + (COMPLETE_SIZE * aNumEntries) > aChunk.Length()) {
    NS_WARNING("Chunk is not long enough to contain the expected entries.");
    return NS_ERROR_FAILURE;
  }

  for (uint8_t i = 0; i < aNumEntries; i++) {
    Completion hash;
    hash.Assign(Substring(aChunk, *aStart, COMPLETE_SIZE));
    nsresult rv = mTableUpdate->NewAddComplete(mChunkState.num, hash);
    if (NS_FAILED(rv)) {
      return rv;
    }
    *aStart += COMPLETE_SIZE;
  }

  return NS_OK;
}

nsresult
ProtocolParser::ProcessHostSubComplete(uint8_t aNumEntries,
                                       const nsACString& aChunk, uint32_t* aStart)
{
  NS_ASSERTION(mChunkState.hashSize == COMPLETE_SIZE,
               "ProcessHostSubComplete should only be called for complete hashes.");

  if (aNumEntries == 0) {
    // this is totally comprehensible.
    NS_WARNING("Expected > 0 entries for a 32-byte hash sub.");
    return NS_OK;
  }

  if (*aStart + ((COMPLETE_SIZE + 4) * aNumEntries) > aChunk.Length()) {
    NS_WARNING("Chunk is not long enough to contain the expected entries.");
    return NS_ERROR_FAILURE;
  }

  for (uint8_t i = 0; i < aNumEntries; i++) {
    Completion hash;
    hash.Assign(Substring(aChunk, *aStart, COMPLETE_SIZE));
    *aStart += COMPLETE_SIZE;

    const nsCSubstring& addChunkStr = Substring(aChunk, *aStart, 4);
    *aStart += 4;

    uint32_t addChunk;
    memcpy(&addChunk, addChunkStr.BeginReading(), 4);
    addChunk = PR_ntohl(addChunk);

    nsresult rv = mTableUpdate->NewSubComplete(addChunk, hash, mChunkState.num);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  return NS_OK;
}

bool
ProtocolParser::NextLine(nsACString& aLine)
{
  int32_t newline = mPending.FindChar('\n');
  if (newline == kNotFound) {
    return false;
  }
  aLine.Assign(Substring(mPending, 0, newline));
  mPending.Cut(0, newline + 1);
  return true;
}

void
ProtocolParser::CleanupUpdates()
{
  for (uint32_t i = 0; i < mTableUpdates.Length(); i++) {
    delete mTableUpdates[i];
  }
  mTableUpdates.Clear();
}

TableUpdate *
ProtocolParser::GetTableUpdate(const nsACString& aTable)
{
  for (uint32_t i = 0; i < mTableUpdates.Length(); i++) {
    if (aTable.Equals(mTableUpdates[i]->TableName())) {
      return mTableUpdates[i];
    }
  }

  // We free automatically on destruction, ownership of these
  // updates can be transferred to DBServiceWorker, which passes
  // them back to Classifier when doing the updates, and that
  // will free them.
  TableUpdate *update = new TableUpdate(aTable);
  mTableUpdates.AppendElement(update);
  return update;
}

} // namespace safebrowsing
} // namespace mozilla
