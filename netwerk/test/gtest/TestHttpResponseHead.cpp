#include "gtest/gtest.h"

#include "chrome/common/ipc_message.h"
#include "mozilla/net/PHttpChannelParams.h"
#include "mozilla/Unused.h"
#include "nsHttp.h"

namespace mozilla {
namespace net {

TEST(TestHttpResponseHead, Bug1636930)
{
  // Only create atom table when it's not already created.
  if (!nsHttp::ResolveAtom("content-type")) {
    Unused << nsHttp::CreateAtomTable();
  }

  mozilla::UniquePtr<IPC::Message> msg(new IPC::Message(MSG_ROUTING_NONE, 0));
  mozilla::net::nsHttpResponseHead origHead;

  origHead.ParseStatusLine("HTTP/1.1 200 OK"_ns);
  Unused << origHead.ParseHeaderLine("content-type: text/plain"_ns);
  Unused << origHead.ParseHeaderLine("etag: Just testing"_ns);
  Unused << origHead.ParseHeaderLine("cache-control: max-age=99999"_ns);
  Unused << origHead.ParseHeaderLine("accept-ranges: bytes"_ns);
  Unused << origHead.ParseHeaderLine("content-length: 1408"_ns);
  Unused << origHead.ParseHeaderLine("connection: close"_ns);
  Unused << origHead.ParseHeaderLine("server: httpd.js"_ns);
  Unused << origHead.ParseHeaderLine("date: Tue, 12 May 2020 09:24:23 GMT"_ns);

  IPC::ParamTraits<mozilla::net::nsHttpResponseHead>::Write(msg.get(),
                                                            origHead);

  mozilla::net::nsHttpResponseHead deserializedHead;
  PickleIterator iter(*msg);
  bool res = IPC::ParamTraits<mozilla::net::nsHttpResponseHead>::Read(
      msg.get(), &iter, &deserializedHead);
  ASSERT_EQ(res, true);

  bool equal = (origHead == deserializedHead);
  ASSERT_EQ(equal, true);
}

}  // namespace net
}  // namespace mozilla
