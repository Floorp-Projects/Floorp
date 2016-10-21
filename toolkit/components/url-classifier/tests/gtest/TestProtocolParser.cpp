/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

#include "gtest/gtest.h"
#include "ProtocolParser.h"
#include "mozilla/EndianUtils.h"

using namespace mozilla;
using namespace mozilla::safebrowsing;

typedef FetchThreatListUpdatesResponse_ListUpdateResponse ListUpdateResponse;

static void
InitUpdateResponse(ListUpdateResponse* aUpdateResponse,
                   ThreatType aThreatType,
                   const nsACString& aState,
                   const nsACString& aChecksum,
                   bool isFullUpdate,
                   const nsTArray<uint32_t>& aFixedLengthPrefixes);

static void
DumpBinary(const nsACString& aBinary);

TEST(ProtocolParser, UpdateWait)
{
  // Top level response which contains a list of update response
  // for different lists.
  FetchThreatListUpdatesResponse response;

  auto r = response.mutable_list_update_responses()->Add();
  InitUpdateResponse(r, SOCIAL_ENGINEERING_PUBLIC,
                        nsCString("sta\x00te", 6),
                        nsCString("check\x0sum", 9),
                        true,
                        {0, 1, 2, 3});

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

static void
InitUpdateResponse(ListUpdateResponse* aUpdateResponse,
                   ThreatType aThreatType,
                   const nsACString& aState,
                   const nsACString& aChecksum,
                   bool isFullUpdate,
                   const nsTArray<uint32_t>& aFixedLengthPrefixes)
{
  aUpdateResponse->set_threat_type(aThreatType);
  aUpdateResponse->set_new_client_state(aState.BeginReading(), aState.Length());
  aUpdateResponse->mutable_checksum()->set_sha256(aChecksum.BeginReading(), aChecksum.Length());
  aUpdateResponse->set_response_type(isFullUpdate ? ListUpdateResponse::FULL_UPDATE
                                                  : ListUpdateResponse::PARTIAL_UPDATE);

  auto additions = aUpdateResponse->mutable_additions()->Add();
  additions->set_compression_type(RAW);
  auto rawHashes = additions->mutable_raw_hashes();
  rawHashes->set_prefix_size(4);
  auto prefixes = rawHashes->mutable_raw_hashes();
  for (auto p : aFixedLengthPrefixes) {
    char buffer[4];
    NativeEndian::copyAndSwapToBigEndian(buffer, &p, 1);
    prefixes->append(buffer, 4);
  }
}

static void DumpBinary(const nsACString& aBinary)
{
  nsCString s;
  for (size_t i = 0; i < aBinary.Length(); i++) {
    s.AppendPrintf("\\x%.2X", (uint8_t)aBinary[i]);
  }
  printf("%s\n", s.get());
}