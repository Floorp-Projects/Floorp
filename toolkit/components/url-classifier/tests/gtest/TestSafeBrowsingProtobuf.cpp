#include "safebrowsing.pb.h"
#include "gtest/gtest.h"

TEST(SafeBrowsingProtobuf, Empty)
{
  using namespace mozilla::safebrowsing;

  const std::string CLIENT_ID = "firefox";

  // Construct a simple update request.
  FetchThreatListUpdatesRequest r;
  r.set_allocated_client(new ClientInfo());
  r.mutable_client()->set_client_id(CLIENT_ID);

  // Then serialize.
  std::string s;
  r.SerializeToString(&s);

  // De-serialize.
  FetchThreatListUpdatesRequest r2;
  r2.ParseFromString(s);

  ASSERT_EQ(r2.client().client_id(), CLIENT_ID);
}
