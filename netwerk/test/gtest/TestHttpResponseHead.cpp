#include "gtest/gtest.h"

#include "chrome/common/ipc_message.h"
#include "mozilla/net/PHttpChannelParams.h"
#include "mozilla/Unused.h"
#include "nsHttp.h"

namespace mozilla {
namespace net {

void AssertRoundTrips(const nsHttpResponseHead& aHead) {
  {
    // Assert it round-trips via IPC.
    UniquePtr<IPC::Message> msg(new IPC::Message(MSG_ROUTING_NONE, 0));
    IPC::ParamTraits<nsHttpResponseHead>::Write(msg.get(), aHead);

    nsHttpResponseHead deserializedHead;
    PickleIterator iter(*msg);
    bool res = IPC::ParamTraits<mozilla::net::nsHttpResponseHead>::Read(
        msg.get(), &iter, &deserializedHead);
    ASSERT_TRUE(res);
    ASSERT_EQ(aHead, deserializedHead);
  }

  {
    // Assert it round-trips through copy-ctor.
    nsHttpResponseHead copied(aHead);
    ASSERT_EQ(aHead, copied);
  }

  {
    // Assert it round-trips through operator=
    nsHttpResponseHead copied;
    copied = aHead;
    ASSERT_EQ(aHead, copied);
  }
}

TEST(TestHttpResponseHead, Bug1636930)
{
  nsHttpResponseHead head;

  head.ParseStatusLine("HTTP/1.1 200 OK"_ns);
  Unused << head.ParseHeaderLine("content-type: text/plain"_ns);
  Unused << head.ParseHeaderLine("etag: Just testing"_ns);
  Unused << head.ParseHeaderLine("cache-control: max-age=99999"_ns);
  Unused << head.ParseHeaderLine("accept-ranges: bytes"_ns);
  Unused << head.ParseHeaderLine("content-length: 1408"_ns);
  Unused << head.ParseHeaderLine("connection: close"_ns);
  Unused << head.ParseHeaderLine("server: httpd.js"_ns);
  Unused << head.ParseHeaderLine("date: Tue, 12 May 2020 09:24:23 GMT"_ns);

  AssertRoundTrips(head);
}

TEST(TestHttpResponseHead, bug1649807)
{
  nsHttpResponseHead head;

  head.ParseStatusLine("HTTP/1.1 200 OK"_ns);
  Unused << head.ParseHeaderLine("content-type: text/plain"_ns);
  Unused << head.ParseHeaderLine("etag: Just testing"_ns);
  Unused << head.ParseHeaderLine("cache-control: age=99999"_ns);
  Unused << head.ParseHeaderLine("accept-ranges: bytes"_ns);
  Unused << head.ParseHeaderLine("content-length: 1408"_ns);
  Unused << head.ParseHeaderLine("connection: close"_ns);
  Unused << head.ParseHeaderLine("server: httpd.js"_ns);
  Unused << head.ParseHeaderLine("pragma: no-cache"_ns);
  Unused << head.ParseHeaderLine("date: Tue, 12 May 2020 09:24:23 GMT"_ns);

  ASSERT_FALSE(head.NoCache())
  << "Cache-Control wins over Pragma: no-cache";
  AssertRoundTrips(head);
}

TEST(TestHttpResponseHead, bug1660200)
{
  nsHttpResponseHead head;

  head.ParseStatusLine("HTTP/1.1 200 OK"_ns);
  Unused << head.ParseHeaderLine("content-type: text/plain"_ns);
  Unused << head.ParseHeaderLine("etag: Just testing"_ns);
  Unused << head.ParseHeaderLine("cache-control: no-cache"_ns);
  Unused << head.ParseHeaderLine("accept-ranges: bytes"_ns);
  Unused << head.ParseHeaderLine("content-length: 1408"_ns);
  Unused << head.ParseHeaderLine("connection: close"_ns);
  Unused << head.ParseHeaderLine("server: httpd.js"_ns);
  Unused << head.ParseHeaderLine("date: Tue, 12 May 2020 09:24:23 GMT"_ns);

  AssertRoundTrips(head);
}

TEST(TestHttpResponseHead, atoms)
{
  // Test that the resolving the content-type atom returns the initial static
  ASSERT_EQ(nsHttp::Content_Type, nsHttp::ResolveAtom("content-type"_ns));
  // Check that they're case insensitive
  ASSERT_EQ(nsHttp::ResolveAtom("Content-Type"_ns),
            nsHttp::ResolveAtom("content-type"_ns));
  // This string literal should be the backing of the atom when resolved first
  auto header1 = "CustomHeaderXXX1"_ns;
  auto atom1 = nsHttp::ResolveAtom(header1);
  auto header2 = "customheaderxxx1"_ns;
  auto atom2 = nsHttp::ResolveAtom(header2);
  ASSERT_EQ(atom1, atom2);
  ASSERT_EQ(atom1.get(), atom2.get());
  // Check that we get the expected pointer back.
  ASSERT_EQ(atom2.get(), header1.BeginReading());
}

}  // namespace net
}  // namespace mozilla
