//* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ProtocolParser.h"
#include "LookupCache.h"
#include "nsIKeyModule.h"
#include "nsNetCID.h"
#include "prlog.h"
#include "prnetdb.h"
#include "prprf.h"

#include "nsUrlClassifierUtils.h"

// NSPR_LOG_MODULES=UrlClassifierDbService:5
extern PRLogModuleInfo *gUrlClassifierDbServiceLog;
#if defined(PR_LOGGING)
#define LOG(args) PR_LOG(gUrlClassifierDbServiceLog, PR_LOG_DEBUG, args)
#define LOG_ENABLED() PR_LOG_TEST(gUrlClassifierDbServiceLog, 4)
#else
#define LOG(args)
#define LOG_ENABLED() (false)
#endif

namespace mozilla {
namespace safebrowsing {

// Updates will fail if fed chunks larger than this
const uint32_t MAX_CHUNK_SIZE = (1024 * 1024);

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
  , mRekeyRequested(false)
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

/**
 * Initialize HMAC for the stream.
 *
 * If serverMAC is empty, the update stream will need to provide a
 * server MAC.
 */
nsresult
ProtocolParser::InitHMAC(const nsACString& aClientKey,
                         const nsACString& aServerMAC)
{
  mServerMAC = aServerMAC;

  nsresult rv;
  nsCOMPtr<nsIKeyObjectFactory> keyObjectFactory(
    do_GetService("@mozilla.org/security/keyobjectfactory;1", &rv));

  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to get nsIKeyObjectFactory service");
    mUpdateStatus = rv;
    return mUpdateStatus;
  }

  nsCOMPtr<nsIKeyObject> keyObject;
  rv = keyObjectFactory->KeyFromString(nsIKeyObject::HMAC, aClientKey,
                                       getter_AddRefs(keyObject));
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to create key object, maybe not FIPS compliant?");
    mUpdateStatus = rv;
    return mUpdateStatus;
  }

  mHMAC = do_CreateInstance(NS_CRYPTO_HMAC_CONTRACTID, &rv);
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to create nsICryptoHMAC instance");
    mUpdateStatus = rv;
    return mUpdateStatus;
  }

  rv = mHMAC->Init(nsICryptoHMAC::SHA1, keyObject);
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to initialize nsICryptoHMAC instance");
    mUpdateStatus = rv;
    return mUpdateStatus;
  }
  return NS_OK;
}

nsresult
ProtocolParser::FinishHMAC()
{
  if (NS_FAILED(mUpdateStatus)) {
    return mUpdateStatus;
  }

  if (mRekeyRequested) {
    mUpdateStatus = NS_ERROR_FAILURE;
    return mUpdateStatus;
  }

  if (!mHMAC) {
    return NS_OK;
  }

  nsAutoCString clientMAC;
  mHMAC->Finish(true, clientMAC);

  if (clientMAC != mServerMAC) {
    NS_WARNING("Invalid update MAC!");
    LOG(("Invalid update MAC: expected %s, got %s",
         clientMAC.get(), mServerMAC.get()));
    mUpdateStatus = NS_ERROR_FAILURE;
  }
  return mUpdateStatus;
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

  // Digest the data if we have a server MAC.
  if (mHMAC && !mServerMAC.IsEmpty()) {
    rv = mHMAC->Update(reinterpret_cast<const uint8_t*>(aData.BeginReading()),
                       aData.Length());
    if (NS_FAILED(rv)) {
      mUpdateStatus = rv;
      return rv;
    }
  }

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

    if (line.EqualsLiteral("e:pleaserekey")) {
      mRekeyRequested = true;
      return NS_OK;
    } else if (mHMAC && mServerMAC.IsEmpty()) {
      rv = ProcessMAC(line);
      NS_ENSURE_SUCCESS(rv, rv);
    } else if (StringBeginsWith(line, NS_LITERAL_CSTRING("i:"))) {
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
ProtocolParser::ProcessMAC(const nsCString& aLine)
{
  nsresult rv;

  LOG(("line: %s", aLine.get()));

  if (StringBeginsWith(aLine, NS_LITERAL_CSTRING("m:"))) {
    mServerMAC = Substring(aLine, 2);
    nsUrlClassifierUtils::UnUrlsafeBase64(mServerMAC);

    // The remainder of the pending update wasn't digested, digest it now.
    rv = mHMAC->Update(reinterpret_cast<const uint8_t*>(mPending.BeginReading()),
                       mPending.Length());
    return rv;
  }

  LOG(("No MAC specified!"));
  return NS_ERROR_FAILURE;
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
      for (uint32_t num = first; num <= last; num++) {
        if (aLine[0] == 'a')
          mTableUpdate->NewAddExpiration(num);
        else
          mTableUpdate->NewSubExpiration(num);
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

  mChunkState.type = (command == 'a') ? CHUNK_ADD : CHUNK_SUB;

  if (mChunkState.type == CHUNK_ADD) {
    mTableUpdate->NewAddChunk(mChunkState.num);
  } else {
    mTableUpdate->NewSubChunk(mChunkState.num);
  }

  return NS_OK;
}

nsresult
ProtocolParser::ProcessForward(const nsCString& aLine)
{
  const nsCSubstring &forward = Substring(aLine, 2);
  if (mHMAC) {
    // We're expecting MACs alongside any url forwards.
    nsCSubstring::const_iterator begin, end, sepBegin, sepEnd;
    forward.BeginReading(begin);
    sepBegin = begin;

    forward.EndReading(end);
    sepEnd = end;

    if (!RFindInReadable(NS_LITERAL_CSTRING(","), sepBegin, sepEnd)) {
      NS_WARNING("No MAC specified for a redirect in a request that expects a MAC");
      return NS_ERROR_FAILURE;
    }

    nsCString serverMAC(Substring(sepEnd, end));
    nsUrlClassifierUtils::UnUrlsafeBase64(serverMAC);
    return AddForward(Substring(begin, sepBegin), serverMAC);
  }
  return AddForward(forward, mServerMAC);
}

nsresult
ProtocolParser::AddForward(const nsACString& aUrl, const nsACString& aMac)
{
  if (!mTableUpdate) {
    NS_WARNING("Forward without a table name.");
    return NS_ERROR_FAILURE;
  }

  ForwardedUpdate *forward = mForwards.AppendElement();
  forward->table = mTableUpdate->TableName();
  forward->url.Assign(aUrl);
  forward->mac.Assign(aMac);

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
  mPending = Substring(mPending, mChunkState.length);

  *aDone = false;
  mState = PROTOCOL_STATE_CONTROL;

  //LOG(("Handling a %d-byte chunk", chunk.Length()));
  if (StringEndsWith(mTableUpdate->TableName(), NS_LITERAL_CSTRING("-shavar"))) {
    return ProcessShaChunk(chunk);
  } else {
    return ProcessPlaintextChunk(chunk);
  }
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
        mTableUpdate->NewAddComplete(mChunkState.num, hash);
      } else {
        NS_ASSERTION(mChunkState.hashSize == 4, "Only 32- or 4-byte hashes can be used for add chunks.");
        Prefix hash;
        hash.FromPlaintext(line, mCryptoHash);
        mTableUpdate->NewAddPrefix(mChunkState.num, hash);
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
        mTableUpdate->NewSubComplete(addChunk, hash, mChunkState.num);
      } else {
        NS_ASSERTION(mChunkState.hashSize == 4, "Only 32- or 4-byte hashes can be used for add chunks.");
        Prefix hash;
        hash.FromPlaintext(Substring(iter, end), mCryptoHash);
        mTableUpdate->NewSubPrefix(addChunk, hash, mChunkState.num);
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
ProtocolParser::ProcessHostAdd(const Prefix& aDomain, uint8_t aNumEntries,
                               const nsACString& aChunk, uint32_t* aStart)
{
  NS_ASSERTION(mChunkState.hashSize == PREFIX_SIZE,
               "ProcessHostAdd should only be called for prefix hashes.");

  if (aNumEntries == 0) {
    mTableUpdate->NewAddPrefix(mChunkState.num, aDomain);
    return NS_OK;
  }

  if (*aStart + (PREFIX_SIZE * aNumEntries) > aChunk.Length()) {
    NS_WARNING("Chunk is not long enough to contain the expected entries.");
    return NS_ERROR_FAILURE;
  }

  for (uint8_t i = 0; i < aNumEntries; i++) {
    Prefix hash;
    hash.Assign(Substring(aChunk, *aStart, PREFIX_SIZE));
    mTableUpdate->NewAddPrefix(mChunkState.num, hash);
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

    mTableUpdate->NewSubPrefix(addChunk, aDomain, mChunkState.num);
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

    mTableUpdate->NewSubPrefix(addChunk, prefix, mChunkState.num);
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
    mTableUpdate->NewAddComplete(mChunkState.num, hash);
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

    mTableUpdate->NewSubComplete(addChunk, hash, mChunkState.num);
  }

  return NS_OK;
}

bool
ProtocolParser::NextLine(nsACString& line)
{
  int32_t newline = mPending.FindChar('\n');
  if (newline == kNotFound) {
    return false;
  }
  line.Assign(Substring(mPending, 0, newline));
  mPending = Substring(mPending, newline + 1);
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

}
}
