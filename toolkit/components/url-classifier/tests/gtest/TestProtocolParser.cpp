/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "mozilla/EndianUtils.h"
#include "ProtocolParser.h"

typedef FetchThreatListUpdatesResponse_ListUpdateResponse ListUpdateResponse;

static bool InitUpdateResponse(ListUpdateResponse* aUpdateResponse,
                               ThreatType aThreatType, const nsACString& aState,
                               const nsACString& aChecksum, bool isFullUpdate,
                               const nsTArray<uint32_t>& aFixedLengthPrefixes,
                               bool aDoPrefixEncoding) {
  aUpdateResponse->set_threat_type(aThreatType);
  aUpdateResponse->set_new_client_state(aState.BeginReading(), aState.Length());
  aUpdateResponse->mutable_checksum()->set_sha256(aChecksum.BeginReading(),
                                                  aChecksum.Length());
  aUpdateResponse->set_response_type(isFullUpdate
                                         ? ListUpdateResponse::FULL_UPDATE
                                         : ListUpdateResponse::PARTIAL_UPDATE);

  auto additions = aUpdateResponse->mutable_additions()->Add();

  if (!aDoPrefixEncoding) {
    additions->set_compression_type(RAW);
    auto rawHashes = additions->mutable_raw_hashes();
    rawHashes->set_prefix_size(4);
    auto prefixes = rawHashes->mutable_raw_hashes();
    for (auto p : aFixedLengthPrefixes) {
      char buffer[4];
      NativeEndian::copyAndSwapToBigEndian(buffer, &p, 1);
      prefixes->append(buffer, 4);
    }
    return true;
  }

  if (1 != aFixedLengthPrefixes.Length()) {
    printf("This function only supports single value encoding.\n");
    return false;
  }

  uint32_t firstValue = aFixedLengthPrefixes[0];
  additions->set_compression_type(RICE);
  auto riceHashes = additions->mutable_rice_hashes();
  riceHashes->set_first_value(firstValue);
  riceHashes->set_num_entries(0);

  return true;
}

static void DumpBinary(const nsACString& aBinary) {
  nsCString s;
  for (size_t i = 0; i < aBinary.Length(); i++) {
    s.AppendPrintf("\\x%.2X", (uint8_t)aBinary[i]);
  }
  printf("%s\n", s.get());
}

TEST(UrlClassifierProtocolParser, UpdateWait)
{
  // Top level response which contains a list of update response
  // for different lists.
  FetchThreatListUpdatesResponse response;

  auto r = response.mutable_list_update_responses()->Add();
  InitUpdateResponse(r, SOCIAL_ENGINEERING_PUBLIC, nsCString("sta\x00te", 6),
                     nsCString("check\x0sum", 9), true, {0, 1, 2, 3},
                     false /* aDoPrefixEncoding */);

  // Set min wait duration.
  auto minWaitDuration = response.mutable_minimum_wait_duration();
  minWaitDuration->set_seconds(8);
  minWaitDuration->set_nanos(1 * 1000000000);

  std::string s;
  response.SerializeToString(&s);

  DumpBinary(nsCString(s.c_str(), s.length()));

  ProtocolParser* p = new ProtocolParserProtobuf();
  p->AppendStream(nsCString(s.c_str(), s.length()));
  p->End();
  ASSERT_EQ(p->UpdateWaitSec(), 9u);
  delete p;
}

TEST(UrlClassifierProtocolParser, SingleValueEncoding)
{
  // Top level response which contains a list of update response
  // for different lists.
  FetchThreatListUpdatesResponse response;

  auto r = response.mutable_list_update_responses()->Add();

  const char* expectedPrefix = "\x00\x01\x02\x00";
  if (!InitUpdateResponse(r, SOCIAL_ENGINEERING_PUBLIC,
                          nsCString("sta\x00te", 6),
                          nsCString("check\x0sum", 9), true,
                          // As per spec, we should interpret the prefix as
                          // uint32 in little endian before encoding.
                          {LittleEndian::readUint32(expectedPrefix)},
                          true /* aDoPrefixEncoding */)) {
    printf("Failed to initialize update response.");
    ASSERT_TRUE(false);
    return;
  }

  // Set min wait duration.
  auto minWaitDuration = response.mutable_minimum_wait_duration();
  minWaitDuration->set_seconds(8);
  minWaitDuration->set_nanos(1 * 1000000000);

  std::string s;
  response.SerializeToString(&s);

  // Feed data to the protocol parser.
  ProtocolParser* p = new ProtocolParserProtobuf();
  p->SetRequestedTables({nsCString("googpub-phish-proto")});
  p->AppendStream(nsCString(s.c_str(), s.length()));
  p->End();

  const TableUpdateArray& tus = p->GetTableUpdates();
  RefPtr<const TableUpdateV4> tuv4 = TableUpdate::Cast<TableUpdateV4>(tus[0]);
  auto& prefixMap = tuv4->Prefixes();
  for (auto iter = prefixMap.ConstIter(); !iter.Done(); iter.Next()) {
    // This prefix map should contain only a single 4-byte prefixe.
    ASSERT_EQ(iter.Key(), 4u);

    // The fixed-length prefix string from ProtocolParser should
    // exactly match the expected prefix string.
    nsCString* prefix = iter.Data();
    ASSERT_TRUE(prefix->Equals(nsCString(expectedPrefix, 4)));
  }

  delete p;
}
