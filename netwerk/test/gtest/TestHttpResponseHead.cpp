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

  origHead.ParseStatusLine(NS_LITERAL_CSTRING("HTTP/1.1 200 OK"));
  Unused << origHead.ParseHeaderLine(
      NS_LITERAL_CSTRING("content-type: text/plain"));
  Unused << origHead.ParseHeaderLine(NS_LITERAL_CSTRING("etag: Just testing"));
  Unused << origHead.ParseHeaderLine(
      NS_LITERAL_CSTRING("cache-control: max-age=99999"));
  Unused << origHead.ParseHeaderLine(
      NS_LITERAL_CSTRING("accept-ranges: bytes"));
  Unused << origHead.ParseHeaderLine(
      NS_LITERAL_CSTRING("content-length: 1408"));
  Unused << origHead.ParseHeaderLine(NS_LITERAL_CSTRING("connection: close"));
  Unused << origHead.ParseHeaderLine(NS_LITERAL_CSTRING("server: httpd.js"));
  Unused << origHead.ParseHeaderLine(
      NS_LITERAL_CSTRING("date: Tue, 12 May 2020 09:24:23 GMT"));

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
