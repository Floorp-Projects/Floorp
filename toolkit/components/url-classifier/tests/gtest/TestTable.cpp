#include "gtest/gtest.h"
#include "nsUrlClassifierDBService.h"

using namespace mozilla::safebrowsing;

void TestResponseCode(const char* table, nsresult result) {
  nsCString tableName(table);
  ASSERT_EQ(TablesToResponse(tableName), result);
}

TEST(UrlClassifierTable, ResponseCode)
{
  // malware URIs.
  TestResponseCode("goog-malware-shavar", NS_ERROR_MALWARE_URI);
  TestResponseCode("test-malware-simple", NS_ERROR_MALWARE_URI);
  TestResponseCode("goog-phish-shavar,test-malware-simple",
                   NS_ERROR_MALWARE_URI);
  TestResponseCode(
      "test-malware-simple,mozstd-track-digest256,mozplugin-block-digest256",
      NS_ERROR_MALWARE_URI);

  // phish URIs.
  TestResponseCode("goog-phish-shavar", NS_ERROR_PHISHING_URI);
  TestResponseCode("test-phish-simple", NS_ERROR_PHISHING_URI);
  TestResponseCode("test-phish-simple,mozplugin-block-digest256",
                   NS_ERROR_PHISHING_URI);
  TestResponseCode(
      "mozstd-track-digest256,test-phish-simple,goog-unwanted-shavar",
      NS_ERROR_PHISHING_URI);

  // unwanted URIs.
  TestResponseCode("goog-unwanted-shavar", NS_ERROR_UNWANTED_URI);
  TestResponseCode("test-unwanted-simple", NS_ERROR_UNWANTED_URI);
  TestResponseCode("mozplugin-unwanted-digest256,mozfull-track-digest256",
                   NS_ERROR_UNWANTED_URI);
  TestResponseCode(
      "test-block-simple,mozfull-track-digest256,test-unwanted-simple",
      NS_ERROR_UNWANTED_URI);

  // track URIs.
  TestResponseCode("test-track-simple", NS_ERROR_TRACKING_URI);
  TestResponseCode("mozstd-track-digest256", NS_ERROR_TRACKING_URI);
  TestResponseCode("test-block-simple,mozstd-track-digest256",
                   NS_ERROR_TRACKING_URI);

  // block URIs
  TestResponseCode("test-block-simple", NS_ERROR_BLOCKED_URI);
  TestResponseCode("mozplugin-block-digest256", NS_ERROR_BLOCKED_URI);
  TestResponseCode("mozplugin2-block-digest256", NS_ERROR_BLOCKED_URI);

  TestResponseCode("test-trackwhite-simple", NS_OK);
  TestResponseCode("mozstd-trackwhite-digest256", NS_OK);
  TestResponseCode("goog-badbinurl-shavar", NS_OK);
  TestResponseCode("goog-downloadwhite-digest256", NS_OK);
}
