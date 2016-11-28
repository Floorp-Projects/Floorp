#include "safebrowsing.pb.h"
#include "gtest/gtest.h"
#include "nsUrlClassifierUtils.h"
#include "mozilla/Base64.h"

using namespace mozilla;
using namespace mozilla::safebrowsing;

namespace {

// |Base64EncodedStringArray| and |MakeBase64EncodedStringArray|
// works together to make us able to do things "literally" and easily.

// Given a nsCString array, construct an object which can be implicitly
// casted to |const char**|, where all owning c-style strings have been
// base64 encoded. The memory life cycle of what the "cast operator"
// returns is just as the object itself.
class Base64EncodedStringArray
{
public:
  Base64EncodedStringArray(nsCString aArray[], size_t N);
  operator const char** () const { return (const char**)&mArray[0]; }

private:
  // Since we can't guarantee the layout of nsCString (can we?),
  // an additional nsTArray<nsCString> is required to manage the
  // allocated string.
  nsTArray<const char*> mArray;
  nsTArray<nsCString> mStringStorage;
};

// Simply used to infer the fixed-array size automatically.
template<size_t N>
Base64EncodedStringArray
MakeBase64EncodedStringArray(nsCString (&aArray)[N])
{
  return Base64EncodedStringArray(aArray, N);
}

} // end of unnamed namespace.


TEST(FindFullHash, Request)
{
  nsCOMPtr<nsIUrlClassifierUtils> urlUtil =
    do_GetService("@mozilla.org/url-classifier/utils;1");

  const char* listNames[] = { "test-phish-proto", "test-unwanted-proto" };

  nsCString listStates[] = { nsCString("sta\x00te1", 7),
                             nsCString("sta\x00te2", 7) };

  nsCString prefixes[] = { nsCString("\x00\x00\x00\x01", 4),
                           nsCString("\x00\x00\x00\x00\x01", 5),
                           nsCString("\x00\xFF\x00\x01", 4),
                           nsCString("\x00\xFF\x00\x01\x11\x23\xAA\xBC", 8),
                           nsCString("\x00\x00\x00\x01\x00\x01\x98", 7) };

  nsCString requestBase64;
  nsresult rv;
  rv = urlUtil->MakeFindFullHashRequestV4(listNames,
                                          MakeBase64EncodedStringArray(listStates),
                                          MakeBase64EncodedStringArray(prefixes),
                                          ArrayLength(listNames),
                                          ArrayLength(prefixes),
                                          requestBase64);
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  // Base64 URL decode first.
  FallibleTArray<uint8_t> requestBinary;
  rv = Base64URLDecode(requestBase64,
                       Base64URLDecodePaddingPolicy::Require,
                       requestBinary);
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  // Parse the FindFullHash binary and compare with the expected values.
  FindFullHashesRequest r;
  ASSERT_TRUE(r.ParseFromArray(&requestBinary[0], requestBinary.Length()));

  // Compare client states.
  ASSERT_EQ(r.client_states_size(), (int)ArrayLength(listStates));
  for(int i = 0; i < r.client_states_size(); i++) {
    auto s = r.client_states(i);
    ASSERT_TRUE(listStates[i].Equals(nsCString(s.c_str(), s.size())));
  }

  auto threatInfo = r.threat_info();

  // Compare threat types.
  ASSERT_EQ(threatInfo.threat_types_size(), (int)ArrayLength(listStates));
  for (int i = 0; i < threatInfo.threat_types_size(); i++) {
    uint32_t expectedThreatType;
    rv = urlUtil->ConvertListNameToThreatType(nsCString(listNames[i]),
                                              &expectedThreatType);
    ASSERT_TRUE(NS_SUCCEEDED(rv));
    ASSERT_EQ(threatInfo.threat_types(i), expectedThreatType);
  }

  // Compare prefixes.
  ASSERT_EQ(threatInfo.threat_entries_size(), (int)ArrayLength(prefixes));
  for (int i = 0; i < threatInfo.threat_entries_size(); i++) {
    auto p = threatInfo.threat_entries(i).hash();
    ASSERT_TRUE(prefixes[i].Equals(nsCString(p.c_str(), p.size())));
  }
}

/////////////////////////////////////////////////////////////
namespace {

Base64EncodedStringArray::Base64EncodedStringArray(nsCString aArray[],
                                                   size_t N)
{
  for (size_t i = 0; i < N; i++) {
    nsCString encoded;
    nsresult rv = Base64Encode(aArray[i], encoded);
    NS_ENSURE_SUCCESS_VOID(rv);
    mStringStorage.AppendElement(encoded);
    mArray.AppendElement(encoded.get());
  }
}

}
