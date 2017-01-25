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

#include "nsUrlClassifierDBService.h"
#include "nsUrlClassifierUtils.h"
#include "nsPrintfCString.h"
#include "mozilla/Base64.h"
#include "RiceDeltaDecoder.h"
#include "mozilla/EndianUtils.h"

// MOZ_LOG=UrlClassifierProtocolParser:5
mozilla::LazyLogModule gUrlClassifierProtocolParserLog("UrlClassifierProtocolParser");
#define PARSER_LOG(args) MOZ_LOG(gUrlClassifierProtocolParserLog, mozilla::LogLevel::Debug, args)

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

///////////////////////////////////////////////////////////////
// ProtocolParser implementation

ProtocolParser::ProtocolParser()
  : mUpdateStatus(NS_OK)
  , mUpdateWaitSec(0)
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
  TableUpdate *update = CreateTableUpdate(aTable);
  mTableUpdates.AppendElement(update);
  return update;
}

///////////////////////////////////////////////////////////////////////
// ProtocolParserV2

ProtocolParserV2::ProtocolParserV2()
  : mState(PROTOCOL_STATE_CONTROL)
  , mResetRequested(false)
  , mTableUpdate(nullptr)
{
}

ProtocolParserV2::~ProtocolParserV2()
{
}

void
ProtocolParserV2::SetCurrentTable(const nsACString& aTable)
{
  auto update = GetTableUpdate(aTable);
  mTableUpdate = TableUpdate::Cast<TableUpdateV2>(update);
}

nsresult
ProtocolParserV2::AppendStream(const nsACString& aData)
{
  if (NS_FAILED(mUpdateStatus))
    return mUpdateStatus;

  nsresult rv;
  mPending.Append(aData);
#ifdef MOZ_SAFEBROWSING_DUMP_FAILED_UPDATES
  mRawUpdate.Append(aData);
#endif

  bool done = false;
  while (!done) {
    if (nsUrlClassifierDBService::ShutdownHasStarted()) {
      return NS_ERROR_ABORT;
    }

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

void
ProtocolParserV2::End()
{
  // Inbound data has already been processed in every AppendStream() call.
}

nsresult
ProtocolParserV2::ProcessControl(bool* aDone)
{
  nsresult rv;

  nsAutoCString line;
  *aDone = true;
  while (NextLine(line)) {
    PARSER_LOG(("Processing %s\n", line.get()));

    if (StringBeginsWith(line, NS_LITERAL_CSTRING("i:"))) {
      // Set the table name from the table header line.
      SetCurrentTable(Substring(line, 2));
    } else if (StringBeginsWith(line, NS_LITERAL_CSTRING("n:"))) {
      if (PR_sscanf(line.get(), "n:%d", &mUpdateWaitSec) != 1) {
        PARSER_LOG(("Error parsing n: '%s' (%d)", line.get(), mUpdateWaitSec));
        return NS_ERROR_FAILURE;
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
ProtocolParserV2::ProcessExpirations(const nsCString& aLine)
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
ProtocolParserV2::ProcessChunkControl(const nsCString& aLine)
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
    NS_WARNING(("PR_sscanf failed"));
    return NS_ERROR_FAILURE;
  }

  if (mChunkState.length > MAX_CHUNK_SIZE) {
    NS_WARNING("Invalid length specified in update.");
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
ProtocolParserV2::ProcessForward(const nsCString& aLine)
{
  const nsCSubstring &forward = Substring(aLine, 2);
  return AddForward(forward);
}

nsresult
ProtocolParserV2::AddForward(const nsACString& aUrl)
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
ProtocolParserV2::ProcessChunk(bool* aDone)
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
ProtocolParserV2::ProcessPlaintextChunk(const nsACString& aChunk)
{
  if (!mTableUpdate) {
    NS_WARNING("Chunk received with no table.");
    return NS_ERROR_FAILURE;
  }

  PARSER_LOG(("Handling a %d-byte simple chunk", aChunk.Length()));

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
ProtocolParserV2::ProcessShaChunk(const nsACString& aChunk)
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

    PARSER_LOG(("Handling a %d-byte shavar chunk containing %u entries"
                " for domain %X", aChunk.Length(), numEntries,
                domain.ToUint32()));

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
      PARSER_LOG(("Got an unexpected chunk type/hash size: %s:%d",
           mChunkState.type == CHUNK_ADD ? "add" : "sub",
           mChunkState.hashSize));
      return NS_ERROR_FAILURE;
    }
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

nsresult
ProtocolParserV2::ProcessDigestChunk(const nsACString& aChunk)
{
  PARSER_LOG(("Handling a %d-byte digest256 chunk", aChunk.Length()));

  if (mChunkState.type == CHUNK_ADD_DIGEST) {
    return ProcessDigestAdd(aChunk);
  }
  if (mChunkState.type == CHUNK_SUB_DIGEST) {
    return ProcessDigestSub(aChunk);
  }
  return NS_ERROR_UNEXPECTED;
}

nsresult
ProtocolParserV2::ProcessDigestAdd(const nsACString& aChunk)
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
ProtocolParserV2::ProcessDigestSub(const nsACString& aChunk)
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
ProtocolParserV2::ProcessHostAdd(const Prefix& aDomain, uint8_t aNumEntries,
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
    PARSER_LOG(("Add prefix %X", hash.ToUint32()));
    nsresult rv = mTableUpdate->NewAddPrefix(mChunkState.num, hash);
    if (NS_FAILED(rv)) {
      return rv;
    }
    *aStart += PREFIX_SIZE;
  }

  return NS_OK;
}

nsresult
ProtocolParserV2::ProcessHostSub(const Prefix& aDomain, uint8_t aNumEntries,
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

    PARSER_LOG(("Sub prefix (addchunk=%u)", addChunk));
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

    PARSER_LOG(("Sub prefix %X (addchunk=%u)", prefix.ToUint32(), addChunk));
    nsresult rv = mTableUpdate->NewSubPrefix(addChunk, prefix, mChunkState.num);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  return NS_OK;
}

nsresult
ProtocolParserV2::ProcessHostAddComplete(uint8_t aNumEntries,
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
ProtocolParserV2::ProcessHostSubComplete(uint8_t aNumEntries,
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
ProtocolParserV2::NextLine(nsACString& aLine)
{
  int32_t newline = mPending.FindChar('\n');
  if (newline == kNotFound) {
    return false;
  }
  aLine.Assign(Substring(mPending, 0, newline));
  mPending.Cut(0, newline + 1);
  return true;
}

TableUpdate*
ProtocolParserV2::CreateTableUpdate(const nsACString& aTableName) const
{
  return new TableUpdateV2(aTableName);
}

///////////////////////////////////////////////////////////////////////
// ProtocolParserProtobuf

ProtocolParserProtobuf::ProtocolParserProtobuf()
{
}

ProtocolParserProtobuf::~ProtocolParserProtobuf()
{
}

void
ProtocolParserProtobuf::SetCurrentTable(const nsACString& aTable)
{
  // Should never occur.
  MOZ_ASSERT_UNREACHABLE("SetCurrentTable shouldn't be called");
}


TableUpdate*
ProtocolParserProtobuf::CreateTableUpdate(const nsACString& aTableName) const
{
  return new TableUpdateV4(aTableName);
}

nsresult
ProtocolParserProtobuf::AppendStream(const nsACString& aData)
{
  // Protobuf data cannot be parsed progressively. Just save the incoming data.
  mPending.Append(aData);
  return NS_OK;
}

void
ProtocolParserProtobuf::End()
{
  // mUpdateStatus will be updated to success as long as not all
  // the responses are invalid.
  mUpdateStatus = NS_ERROR_FAILURE;

  FetchThreatListUpdatesResponse response;
  if (!response.ParseFromArray(mPending.get(), mPending.Length())) {
    NS_WARNING("ProtocolParserProtobuf failed parsing data.");
    return;
  }

  auto minWaitDuration = response.minimum_wait_duration();
  mUpdateWaitSec = minWaitDuration.seconds() +
                   minWaitDuration.nanos() / 1000000000;

  for (int i = 0; i < response.list_update_responses_size(); i++) {
    auto r = response.list_update_responses(i);
    nsresult rv = ProcessOneResponse(r);
    if (NS_SUCCEEDED(rv)) {
      mUpdateStatus = rv;
    } else {
      NS_WARNING("Failed to process one response.");
    }
  }
}

nsresult
ProtocolParserProtobuf::ProcessOneResponse(const ListUpdateResponse& aResponse)
{
  // A response must have a threat type.
  if (!aResponse.has_threat_type()) {
    NS_WARNING("Threat type not initialized. This seems to be an invalid response.");
    return NS_ERROR_FAILURE;
  }

  // Convert threat type to list name.
  nsCOMPtr<nsIUrlClassifierUtils> urlUtil =
    do_GetService(NS_URLCLASSIFIERUTILS_CONTRACTID);
  nsCString possibleListNames;
  nsresult rv = urlUtil->ConvertThreatTypeToListNames(aResponse.threat_type(),
                                                      possibleListNames);
  if (NS_FAILED(rv)) {
    PARSER_LOG((nsPrintfCString("Threat type to list name conversion error: %d",
                               aResponse.threat_type())).get());
    return NS_ERROR_FAILURE;
  }

  // Match the table name we received with one of the ones we requested.
  // We ignore the case where a threat type matches more than one list
  // per provider and return the first one. See bug 1287059."
  nsCString listName;
  nsTArray<nsCString> possibleListNameArray;
  Classifier::SplitTables(possibleListNames, possibleListNameArray);
  for (auto possibleName : possibleListNameArray) {
    if (mRequestedTables.Contains(possibleName)) {
      listName = possibleName;
      break;
    }
  }

  if (listName.IsEmpty()) {
    PARSER_LOG(("We received an update for a list we didn't ask for. Ignoring it."));
    return NS_ERROR_FAILURE;
  }

  // Test if this is a full update.
  bool isFullUpdate = false;
  if (aResponse.has_response_type()) {
    isFullUpdate =
      aResponse.response_type() == ListUpdateResponse::FULL_UPDATE;
  } else {
    NS_WARNING("Response type not initialized.");
    return NS_ERROR_FAILURE;
  }

  // Warn if there's no new state.
  if (!aResponse.has_new_client_state()) {
    NS_WARNING("New state not initialized.");
    return NS_ERROR_FAILURE;
  }

  auto tu = GetTableUpdate(nsCString(listName.get()));
  auto tuV4 = TableUpdate::Cast<TableUpdateV4>(tu);
  NS_ENSURE_TRUE(tuV4, NS_ERROR_FAILURE);

  nsCString state(aResponse.new_client_state().c_str(),
                  aResponse.new_client_state().size());
  tuV4->SetNewClientState(state);

  if (aResponse.has_checksum()) {
    tuV4->NewChecksum(aResponse.checksum().sha256());
  }

  PARSER_LOG(("==== Update for threat type '%d' ====", aResponse.threat_type()));
  PARSER_LOG(("* listName: %s\n", listName.get()));
  PARSER_LOG(("* newState: %s\n", aResponse.new_client_state().c_str()));
  PARSER_LOG(("* isFullUpdate: %s\n", (isFullUpdate ? "yes" : "no")));
  PARSER_LOG(("* hasChecksum: %s\n", (aResponse.has_checksum() ? "yes" : "no")));

  tuV4->SetFullUpdate(isFullUpdate);

  rv = ProcessAdditionOrRemoval(*tuV4, aResponse.additions(), true /*aIsAddition*/);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = ProcessAdditionOrRemoval(*tuV4, aResponse.removals(), false);
  NS_ENSURE_SUCCESS(rv, rv);

  PARSER_LOG(("\n\n"));

  return NS_OK;
}

nsresult
ProtocolParserProtobuf::ProcessAdditionOrRemoval(TableUpdateV4& aTableUpdate,
                                                 const ThreatEntrySetList& aUpdate,
                                                 bool aIsAddition)
{
  nsresult ret = NS_OK;

  for (int i = 0; i < aUpdate.size(); i++) {
    auto update = aUpdate.Get(i);
    if (!update.has_compression_type()) {
      NS_WARNING(nsPrintfCString("%s with no compression type.",
                                  aIsAddition ? "Addition" : "Removal").get());
      continue;
    }

    switch (update.compression_type()) {
    case COMPRESSION_TYPE_UNSPECIFIED:
      NS_WARNING("Unspecified compression type.");
      break;

    case RAW:
      ret = (aIsAddition ? ProcessRawAddition(aTableUpdate, update)
                         : ProcessRawRemoval(aTableUpdate, update));
      break;

    case RICE:
      ret = (aIsAddition ? ProcessEncodedAddition(aTableUpdate, update)
                         : ProcessEncodedRemoval(aTableUpdate, update));
      break;
    }
  }

  return ret;
}

nsresult
ProtocolParserProtobuf::ProcessRawAddition(TableUpdateV4& aTableUpdate,
                                           const ThreatEntrySet& aAddition)
{
  if (!aAddition.has_raw_hashes()) {
    PARSER_LOG(("* No raw addition."));
    return NS_OK;
  }

  auto rawHashes = aAddition.raw_hashes();
  if (!rawHashes.has_prefix_size()) {
    NS_WARNING("Raw hash has no prefix size");
    return NS_OK;
  }

  auto prefixes = rawHashes.raw_hashes();
  if (4 == rawHashes.prefix_size()) {
    // Process fixed length prefixes separately.
    uint32_t* fixedLengthPrefixes = (uint32_t*)prefixes.c_str();
    size_t numOfFixedLengthPrefixes = prefixes.size() / 4;
    PARSER_LOG(("* Raw addition (4 bytes)"));
    PARSER_LOG(("  - # of prefixes: %d", numOfFixedLengthPrefixes));
    PARSER_LOG(("  - Memory address: 0x%p", fixedLengthPrefixes));
  } else {
    // TODO: Process variable length prefixes including full hashes.
    // See Bug 1283009.
    PARSER_LOG((" Raw addition (%d bytes)", rawHashes.prefix_size()));
  }

  if (!rawHashes.mutable_raw_hashes()) {
    PARSER_LOG(("Unable to get mutable raw hashes. Can't perform a string move."));
    return NS_ERROR_FAILURE;
  }

  aTableUpdate.NewPrefixes(rawHashes.prefix_size(),
                           *rawHashes.mutable_raw_hashes());

  return NS_OK;
}

nsresult
ProtocolParserProtobuf::ProcessRawRemoval(TableUpdateV4& aTableUpdate,
                                          const ThreatEntrySet& aRemoval)
{
  if (!aRemoval.has_raw_indices()) {
    NS_WARNING("A removal has no indices.");
    return NS_OK;
  }

  // indices is an array of int32.
  auto indices = aRemoval.raw_indices().indices();
  PARSER_LOG(("* Raw removal"));
  PARSER_LOG(("  - # of removal: %d", indices.size()));

  aTableUpdate.NewRemovalIndices((const uint32_t*)indices.data(),
                                 indices.size());

  return NS_OK;
}

static nsresult
DoRiceDeltaDecode(const RiceDeltaEncoding& aEncoding,
                  nsTArray<uint32_t>& aDecoded)
{
  if (!aEncoding.has_first_value()) {
    PARSER_LOG(("The encoding info is incomplete."));
    return NS_ERROR_FAILURE;
  }
  if (aEncoding.num_entries() > 0 &&
      (!aEncoding.has_rice_parameter() || !aEncoding.has_encoded_data())) {
    PARSER_LOG(("Rice parameter or encoded data is missing."));
    return NS_ERROR_FAILURE;
  }

  PARSER_LOG(("* Encoding info:"));
  PARSER_LOG(("  - First value: %d", aEncoding.first_value()));
  PARSER_LOG(("  - Num of entries: %d", aEncoding.num_entries()));
  PARSER_LOG(("  - Rice parameter: %d", aEncoding.rice_parameter()));

  // Set up the input buffer. Note that the bits should be read
  // from LSB to MSB so that we in-place reverse the bits before
  // feeding to the decoder.
  auto encoded = const_cast<RiceDeltaEncoding&>(aEncoding).mutable_encoded_data();
  RiceDeltaDecoder decoder((uint8_t*)encoded->c_str(), encoded->size());

  // Setup the output buffer. The "first value" is included in
  // the output buffer.
  aDecoded.SetLength(aEncoding.num_entries() + 1);

  // Decode!
  bool rv = decoder.Decode(aEncoding.rice_parameter(),
                           aEncoding.first_value(), // first value.
                           aEncoding.num_entries(), // # of entries (first value not included).
                           &aDecoded[0]);

  NS_ENSURE_TRUE(rv, NS_ERROR_FAILURE);

  return NS_OK;
}

nsresult
ProtocolParserProtobuf::ProcessEncodedAddition(TableUpdateV4& aTableUpdate,
                                               const ThreatEntrySet& aAddition)
{
  if (!aAddition.has_rice_hashes()) {
    PARSER_LOG(("* No rice encoded addition."));
    return NS_OK;
  }

  nsTArray<uint32_t> decoded;
  nsresult rv = DoRiceDeltaDecode(aAddition.rice_hashes(), decoded);
  if (NS_FAILED(rv)) {
    PARSER_LOG(("Failed to parse encoded prefixes."));
    return rv;
  }

  //  Say we have the following raw prefixes
  //                              BE            LE
  //   00 00 00 01                 1      16777216
  //   00 00 02 00               512        131072
  //   00 03 00 00            196608           768
  //   04 00 00 00          67108864             4
  //
  // which can be treated as uint32 (big-endian) sorted in increasing order:
  //
  // [1, 512, 196608, 67108864]
  //
  // According to https://developers.google.com/safe-browsing/v4/compression,
  // the following should be done prior to compression:
  //
  // 1) re-interpret in little-endian ==> [16777216, 131072, 768, 4]
  // 2) sort in increasing order       ==> [4, 768, 131072, 16777216]
  //
  // In order to get the original byte stream from |decoded|
  // ([4, 768, 131072, 16777216] in this case), we have to:
  //
  // 1) sort in big-endian order      ==> [16777216, 131072, 768, 4]
  // 2) copy each uint32 in little-endian to the result string
  //

  // The 4-byte prefixes have to be re-sorted in Big-endian increasing order.
  struct CompareBigEndian
  {
    bool Equals(const uint32_t& aA, const uint32_t& aB) const
    {
      return aA == aB;
    }

    bool LessThan(const uint32_t& aA, const uint32_t& aB) const
    {
      return NativeEndian::swapToBigEndian(aA) <
             NativeEndian::swapToBigEndian(aB);
    }
  };
  decoded.Sort(CompareBigEndian());

  // The encoded prefixes are always 4 bytes.
  std::string prefixes;
  for (size_t i = 0; i < decoded.Length(); i++) {
    // Note that the third argument is the number of elements we want
    // to copy (and swap) but not the number of bytes we want to copy.
    char p[4];
    NativeEndian::copyAndSwapToLittleEndian(p, &decoded[i], 1);
    prefixes.append(p, 4);
  }

  aTableUpdate.NewPrefixes(4, prefixes);

  return NS_OK;
}

nsresult
ProtocolParserProtobuf::ProcessEncodedRemoval(TableUpdateV4& aTableUpdate,
                                              const ThreatEntrySet& aRemoval)
{
  if (!aRemoval.has_rice_indices()) {
    PARSER_LOG(("* No rice encoded removal."));
    return NS_OK;
  }

  nsTArray<uint32_t> decoded;
  nsresult rv = DoRiceDeltaDecode(aRemoval.rice_indices(), decoded);
  if (NS_FAILED(rv)) {
    PARSER_LOG(("Failed to decode encoded removal indices."));
    return rv;
  }

  // The encoded prefixes are always 4 bytes.
  aTableUpdate.NewRemovalIndices(&decoded[0], decoded.Length());

  return NS_OK;
}

} // namespace safebrowsing
} // namespace mozilla
