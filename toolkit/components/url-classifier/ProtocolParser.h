//* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ProtocolParser_h__
#define ProtocolParser_h__

#include "HashStore.h"
#include "nsICryptoHMAC.h"
#include "safebrowsing.pb.h"

namespace mozilla {
namespace safebrowsing {

/**
 * Abstract base class for parsing update data in multiple formats.
 */
class ProtocolParser {
public:
  struct ForwardedUpdate {
    nsCString table;
    nsCString url;
  };

  ProtocolParser();
  virtual ~ProtocolParser();

  nsresult Status() const { return mUpdateStatus; }

  nsresult Init(nsICryptoHash* aHasher);

#ifdef MOZ_SAFEBROWSING_DUMP_FAILED_UPDATES
  virtual nsCString GetRawTableUpdates() const { return mPending; }
#endif

  virtual void SetCurrentTable(const nsACString& aTable) = 0;

  void SetRequestedTables(const nsTArray<nsCString>& aRequestTables)
  {
    mRequestedTables = aRequestTables;
  }

  nsresult Begin();
  virtual nsresult AppendStream(const nsACString& aData) = 0;

  uint32_t UpdateWaitSec() { return mUpdateWaitSec; }

  // Notify that the inbound data is ready for parsing if progressive
  // parsing is not supported, for example in V4.
  virtual void End() = 0;

  // Forget the table updates that were created by this pass.  It
  // becomes the caller's responsibility to free them.  This is shitty.
  TableUpdate *GetTableUpdate(const nsACString& aTable);
  void ForgetTableUpdates() { mTableUpdates.Clear(); }
  nsTArray<TableUpdate*> &GetTableUpdates() { return mTableUpdates; }

  // These are only meaningful to V2. Since they were originally public,
  // moving them to ProtocolParserV2 requires a dymamic cast in the call
  // sites. As a result, we will leave them until we remove support
  // for V2 entirely..
  virtual const nsTArray<ForwardedUpdate> &Forwards() const { return mForwards; }
  virtual bool ResetRequested() { return false; }

protected:
  virtual TableUpdate* CreateTableUpdate(const nsACString& aTableName) const = 0;

  nsCString mPending;
  nsresult mUpdateStatus;

  // Keep track of updates to apply before passing them to the DBServiceWorkers.
  nsTArray<TableUpdate*> mTableUpdates;

  nsTArray<ForwardedUpdate> mForwards;
  nsCOMPtr<nsICryptoHash> mCryptoHash;

  // The table names that were requested from the client.
  nsTArray<nsCString> mRequestedTables;

  // How long we should wait until the next update.
  uint32_t mUpdateWaitSec;

private:
  void CleanupUpdates();
};

/**
 * Helpers to parse the "shavar", "digest256" and "simple" list formats.
 */
class ProtocolParserV2 final : public ProtocolParser {
public:
  ProtocolParserV2();
  virtual ~ProtocolParserV2();

  virtual void SetCurrentTable(const nsACString& aTable) override;
  virtual nsresult AppendStream(const nsACString& aData) override;
  virtual void End() override;

  // Update information.
  virtual const nsTArray<ForwardedUpdate> &Forwards() const override { return mForwards; }
  virtual bool ResetRequested() override { return mResetRequested; }

#ifdef MOZ_SAFEBROWSING_DUMP_FAILED_UPDATES
  // Unfortunately we have to override to return mRawUpdate which
  // will not be modified during the parsing, unlike mPending.
  virtual nsCString GetRawTableUpdates() const override { return mRawUpdate; }
#endif

private:
  virtual TableUpdate* CreateTableUpdate(const nsACString& aTableName) const override;

  nsresult ProcessControl(bool* aDone);
  nsresult ProcessExpirations(const nsCString& aLine);
  nsresult ProcessChunkControl(const nsCString& aLine);
  nsresult ProcessForward(const nsCString& aLine);
  nsresult AddForward(const nsACString& aUrl);
  nsresult ProcessChunk(bool* done);
  // Remove this, it's only used for testing
  nsresult ProcessPlaintextChunk(const nsACString& aChunk);
  nsresult ProcessShaChunk(const nsACString& aChunk);
  nsresult ProcessHostAdd(const Prefix& aDomain, uint8_t aNumEntries,
                          const nsACString& aChunk, uint32_t* aStart);
  nsresult ProcessHostSub(const Prefix& aDomain, uint8_t aNumEntries,
                          const nsACString& aChunk, uint32_t* aStart);
  nsresult ProcessHostAddComplete(uint8_t aNumEntries, const nsACString& aChunk,
                                  uint32_t *aStart);
  nsresult ProcessHostSubComplete(uint8_t numEntries, const nsACString& aChunk,
                                  uint32_t* start);
  // Digest chunks are very similar to shavar chunks, except digest chunks
  // always contain the full hash, so there is no need for chunk data to
  // contain prefix sizes.
  nsresult ProcessDigestChunk(const nsACString& aChunk);
  nsresult ProcessDigestAdd(const nsACString& aChunk);
  nsresult ProcessDigestSub(const nsACString& aChunk);
  bool NextLine(nsACString& aLine);

  enum ParserState {
    PROTOCOL_STATE_CONTROL,
    PROTOCOL_STATE_CHUNK
  };
  ParserState mState;

  enum ChunkType {
    // Types for shavar tables.
    CHUNK_ADD,
    CHUNK_SUB,
    // Types for digest256 tables. digest256 tables differ in format from
    // shavar tables since they only contain complete hashes.
    CHUNK_ADD_DIGEST,
    CHUNK_SUB_DIGEST
  };

  struct ChunkState {
    ChunkType type;
    uint32_t num;
    uint32_t hashSize;
    uint32_t length;
    void Clear() { num = 0; hashSize = 0; length = 0; }
  };
  ChunkState mChunkState;

  bool mResetRequested;

  // Updates to apply to the current table being parsed.
  TableUpdateV2 *mTableUpdate;

#ifdef MOZ_SAFEBROWSING_DUMP_FAILED_UPDATES
  nsCString mRawUpdate; // Keep a copy of mPending before it's processed.
#endif
};

// Helpers to parse the "proto" list format.
class ProtocolParserProtobuf final : public ProtocolParser {
public:
  typedef FetchThreatListUpdatesResponse_ListUpdateResponse ListUpdateResponse;
  typedef google::protobuf::RepeatedPtrField<ThreatEntrySet> ThreatEntrySetList;

public:
  ProtocolParserProtobuf();

  virtual void SetCurrentTable(const nsACString& aTable) override;
  virtual nsresult AppendStream(const nsACString& aData) override;
  virtual void End() override;

private:
  virtual ~ProtocolParserProtobuf();

  virtual TableUpdate* CreateTableUpdate(const nsACString& aTableName) const override;

  // For parsing update info.
  nsresult ProcessOneResponse(const ListUpdateResponse& aResponse);

  nsresult ProcessAdditionOrRemoval(TableUpdateV4& aTableUpdate,
                                    const ThreatEntrySetList& aUpdate,
                                    bool aIsAddition);

  nsresult ProcessRawAddition(TableUpdateV4& aTableUpdate,
                              const ThreatEntrySet& aAddition);

  nsresult ProcessRawRemoval(TableUpdateV4& aTableUpdate,
                             const ThreatEntrySet& aRemoval);

  nsresult ProcessEncodedAddition(TableUpdateV4& aTableUpdate,
                                  const ThreatEntrySet& aAddition);

  nsresult ProcessEncodedRemoval(TableUpdateV4& aTableUpdate,
                                 const ThreatEntrySet& aRemoval);
};

} // namespace safebrowsing
} // namespace mozilla

#endif
