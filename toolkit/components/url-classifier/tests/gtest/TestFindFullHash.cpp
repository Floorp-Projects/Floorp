/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Base64.h"
#include "nsUrlClassifierUtils.h"
#include "safebrowsing.pb.h"

#include "Common.h"

template <size_t N>
static void ToBase64EncodedStringArray(nsCString (&aInput)[N],
                                       nsTArray<nsCString>& aEncodedArray) {
  for (size_t i = 0; i < N; i++) {
    nsCString encoded;
    nsresult rv = mozilla::Base64Encode(aInput[i], encoded);
    NS_ENSURE_SUCCESS_VOID(rv);
    aEncodedArray.AppendElement(encoded);
  }
}

TEST(UrlClassifierFindFullHash, Request)
{
  nsUrlClassifierUtils* urlUtil = nsUrlClassifierUtils::GetInstance();

  nsTArray<nsCString> listNames;
  listNames.AppendElement("moztest-phish-proto");
  listNames.AppendElement("moztest-unwanted-proto");

  nsCString listStates[] = {nsCString("sta\x00te1", 7),
                            nsCString("sta\x00te2", 7)};
  nsTArray<nsCString> listStateArray;
  ToBase64EncodedStringArray(listStates, listStateArray);

  nsCString prefixes[] = {nsCString("\x00\x00\x00\x01", 4),
                          nsCString("\x00\x00\x00\x00\x01", 5),
                          nsCString("\x00\xFF\x00\x01", 4),
                          nsCString("\x00\xFF\x00\x01\x11\x23\xAA\xBC", 8),
                          nsCString("\x00\x00\x00\x01\x00\x01\x98", 7)};
  nsTArray<nsCString> prefixArray;
  ToBase64EncodedStringArray(prefixes, prefixArray);

  nsCString requestBase64;
  nsresult rv;
  rv = urlUtil->MakeFindFullHashRequestV4(listNames, listStateArray,
                                          prefixArray, requestBase64);
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  // Base64 URL decode first.
  FallibleTArray<uint8_t> requestBinary;
  rv = Base64URLDecode(requestBase64, Base64URLDecodePaddingPolicy::Require,
                       requestBinary);
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  // Parse the FindFullHash binary and compare with the expected values.
  FindFullHashesRequest r;
  ASSERT_TRUE(r.ParseFromArray(&requestBinary[0], requestBinary.Length()));

  // Compare client states.
  ASSERT_EQ(r.client_states_size(), (int)ArrayLength(listStates));
  for (int i = 0; i < r.client_states_size(); i++) {
    auto s = r.client_states(i);
    ASSERT_TRUE(listStates[i].Equals(nsCString(s.c_str(), s.size())));
  }

  auto threatInfo = r.threat_info();

  // Compare threat types.
  ASSERT_EQ(threatInfo.threat_types_size(), (int)ArrayLength(listStates));
  for (int i = 0; i < threatInfo.threat_types_size(); i++) {
    uint32_t expectedThreatType;
    rv =
        urlUtil->ConvertListNameToThreatType(listNames[i], &expectedThreatType);
    ASSERT_TRUE(NS_SUCCEEDED(rv));
    ASSERT_EQ(threatInfo.threat_types(i), (int)expectedThreatType);
  }

  // Compare prefixes.
  ASSERT_EQ(threatInfo.threat_entries_size(), (int)ArrayLength(prefixes));
  for (int i = 0; i < threatInfo.threat_entries_size(); i++) {
    auto p = threatInfo.threat_entries(i).hash();
    ASSERT_TRUE(prefixes[i].Equals(nsCString(p.c_str(), p.size())));
  }
}

/////////////////////////////////////////////////////////////
// Following is to test parsing the gethash response.

namespace {

// safebrowsing::Duration manipulation.
struct MyDuration {
  uint32_t mSecs;
  uint32_t mNanos;
};
void PopulateDuration(Duration& aDest, const MyDuration& aSrc) {
  aDest.set_seconds(aSrc.mSecs);
  aDest.set_nanos(aSrc.mNanos);
}

// The expected match data.
static MyDuration EXPECTED_MIN_WAIT_DURATION = {12, 10};
static MyDuration EXPECTED_NEG_CACHE_DURATION = {120, 9};
static const struct ExpectedMatch {
  nsCString mCompleteHash;
  ThreatType mThreatType;
  MyDuration mPerHashCacheDuration;
} EXPECTED_MATCH[] = {
    {nsCString("01234567890123456789012345678901"),
     SOCIAL_ENGINEERING_PUBLIC,
     {8, 500}},
    {nsCString("12345678901234567890123456789012"),
     SOCIAL_ENGINEERING_PUBLIC,
     {7, 100}},
    {nsCString("23456789012345678901234567890123"),
     SOCIAL_ENGINEERING_PUBLIC,
     {1, 20}},
};

class MyParseCallback final : public nsIUrlClassifierParseFindFullHashCallback {
 public:
  NS_DECL_ISUPPORTS

  explicit MyParseCallback(uint32_t& aCallbackCount)
      : mCallbackCount(aCallbackCount) {}

  NS_IMETHOD
  OnCompleteHashFound(const nsACString& aCompleteHash,
                      const nsACString& aTableNames,
                      uint32_t aPerHashCacheDuration) override {
    Verify(aCompleteHash, aTableNames, aPerHashCacheDuration);

    return NS_OK;
  }

  NS_IMETHOD
  OnResponseParsed(uint32_t aMinWaitDuration,
                   uint32_t aNegCacheDuration) override {
    VerifyDuration(aMinWaitDuration / 1000, EXPECTED_MIN_WAIT_DURATION);
    VerifyDuration(aNegCacheDuration, EXPECTED_NEG_CACHE_DURATION);

    return NS_OK;
  }

 private:
  void Verify(const nsACString& aCompleteHash, const nsACString& aTableNames,
              uint32_t aPerHashCacheDuration) {
    auto expected = EXPECTED_MATCH[mCallbackCount];

    ASSERT_TRUE(aCompleteHash.Equals(expected.mCompleteHash));

    // Verify aTableNames
    nsUrlClassifierUtils* urlUtil = nsUrlClassifierUtils::GetInstance();

    nsCString tableNames;
    nsresult rv =
        urlUtil->ConvertThreatTypeToListNames(expected.mThreatType, tableNames);
    ASSERT_TRUE(NS_SUCCEEDED(rv));
    ASSERT_TRUE(aTableNames.Equals(tableNames));

    VerifyDuration(aPerHashCacheDuration, expected.mPerHashCacheDuration);

    mCallbackCount++;
  }

  void VerifyDuration(uint32_t aToVerify, const MyDuration& aExpected) {
    ASSERT_TRUE(aToVerify == aExpected.mSecs);
  }

  ~MyParseCallback() = default;

  uint32_t& mCallbackCount;
};

NS_IMPL_ISUPPORTS(MyParseCallback, nsIUrlClassifierParseFindFullHashCallback)

}  // end of unnamed namespace.

TEST(UrlClassifierFindFullHash, ParseRequest)
{
  // Build response.
  FindFullHashesResponse r;

  // Init response-wise durations.
  auto minWaitDuration = r.mutable_minimum_wait_duration();
  PopulateDuration(*minWaitDuration, EXPECTED_MIN_WAIT_DURATION);
  auto negCacheDuration = r.mutable_negative_cache_duration();
  PopulateDuration(*negCacheDuration, EXPECTED_NEG_CACHE_DURATION);

  // Init matches.
  for (uint32_t i = 0; i < ArrayLength(EXPECTED_MATCH); i++) {
    auto expected = EXPECTED_MATCH[i];
    auto match = r.mutable_matches()->Add();
    match->set_threat_type(expected.mThreatType);
    match->mutable_threat()->set_hash(expected.mCompleteHash.BeginReading(),
                                      expected.mCompleteHash.Length());
    auto perHashCacheDuration = match->mutable_cache_duration();
    PopulateDuration(*perHashCacheDuration, expected.mPerHashCacheDuration);
  }
  std::string s;
  r.SerializeToString(&s);

  uint32_t callbackCount = 0;
  nsCOMPtr<nsIUrlClassifierParseFindFullHashCallback> callback =
      new MyParseCallback(callbackCount);

  nsUrlClassifierUtils* urlUtil = nsUrlClassifierUtils::GetInstance();
  nsresult rv = urlUtil->ParseFindFullHashResponseV4(
      nsCString(s.c_str(), s.size()), callback);
  NS_ENSURE_SUCCESS_VOID(rv);

  ASSERT_EQ(callbackCount, ArrayLength(EXPECTED_MATCH));
}
