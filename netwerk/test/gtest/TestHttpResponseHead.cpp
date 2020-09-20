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
  // Only create atom table when it's not already created.
  if (!nsHttp::ResolveAtom("content-type")) {
    Unused << nsHttp::CreateAtomTable();
  }

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
  // Only create atom table when it's not already created.
  if (!nsHttp::ResolveAtom("content-type")) {
    Unused << nsHttp::CreateAtomTable();
  }

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
  // Only create atom table when it's not already created.
  if (!nsHttp::ResolveAtom("content-type")) {
    Unused << nsHttp::CreateAtomTable();
  }

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

}  // namespace net
}  // namespace mozilla
