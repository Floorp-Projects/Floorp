/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TestCommon.h"
#include "gtest/gtest.h"
#include "nsISocketTransportService.h"
#include "nsISocketTransport.h"
#include "nsIServerSocket.h"
#include "nsIAsyncInputStream.h"
#include "nsINetAddr.h"
#include "mozilla/net/DNS.h"
#include "prerror.h"
#include "nsComponentManagerUtils.h"
#include "nsServiceManagerUtils.h"

using namespace mozilla::net;
using namespace mozilla;

class ServerListener : public nsIServerSocketListener {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISERVERSOCKETLISTENER

  explicit ServerListener(WaitForCondition* waiter);

  // Port that is got from server side will be store here.
  uint32_t mClientPort;
  bool mFailed;
  RefPtr<WaitForCondition> mWaiter;

 private:
  virtual ~ServerListener();
};

NS_IMPL_ISUPPORTS(ServerListener, nsIServerSocketListener)

ServerListener::ServerListener(WaitForCondition* waiter)
    : mClientPort(-1), mFailed(false), mWaiter(waiter) {}

ServerListener::~ServerListener() = default;

NS_IMETHODIMP
ServerListener::OnSocketAccepted(nsIServerSocket* aServ,
                                 nsISocketTransport* aTransport) {
  // Run on STS thread.
  NetAddr peerAddr;
  nsresult rv = aTransport->GetPeerAddr(&peerAddr);
  if (NS_FAILED(rv)) {
    mFailed = true;
    mWaiter->Notify();
    return NS_OK;
  }
  mClientPort = PR_ntohs(peerAddr.inet.port);
  mWaiter->Notify();
  return NS_OK;
}

NS_IMETHODIMP
ServerListener::OnStopListening(nsIServerSocket* aServ, nsresult aStatus) {
  return NS_OK;
}

class ClientInputCallback : public nsIInputStreamCallback {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIINPUTSTREAMCALLBACK

  explicit ClientInputCallback(WaitForCondition* waiter);

  bool mFailed;
  RefPtr<WaitForCondition> mWaiter;

 private:
  virtual ~ClientInputCallback();
};

NS_IMPL_ISUPPORTS(ClientInputCallback, nsIInputStreamCallback)

ClientInputCallback::ClientInputCallback(WaitForCondition* waiter)
    : mFailed(false), mWaiter(waiter) {}

ClientInputCallback::~ClientInputCallback() = default;

NS_IMETHODIMP
ClientInputCallback::OnInputStreamReady(nsIAsyncInputStream* aStream) {
  // Server doesn't send. That means if we are here, we probably have run into
  // an error.
  uint64_t avail;
  nsresult rv = aStream->Available(&avail);
  if (NS_FAILED(rv)) {
    mFailed = true;
  }
  mWaiter->Notify();
  return NS_OK;
}

TEST(TestBind, MainTest)
{
  //
  // Server side.
  //
  nsCOMPtr<nsIServerSocket> server =
      do_CreateInstance("@mozilla.org/network/server-socket;1");
  ASSERT_TRUE(server);

  nsresult rv = server->Init(-1, true, -1);
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  int32_t serverPort;
  rv = server->GetPort(&serverPort);
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  RefPtr<WaitForCondition> waiter = new WaitForCondition();

  // Listening.
  RefPtr<ServerListener> serverListener = new ServerListener(waiter);
  rv = server->AsyncListen(serverListener);
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  //
  // Client side
  //
  uint32_t bindingPort = 20000;
  nsCOMPtr<nsISocketTransportService> sts =
      do_GetService("@mozilla.org/network/socket-transport-service;1", &rv);
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  nsCOMPtr<nsIInputStream> inputStream;
  RefPtr<ClientInputCallback> clientCallback;

  for (int32_t tried = 0; tried < 100; tried++) {
    nsCOMPtr<nsISocketTransport> client;
    rv = sts->CreateTransport(nullptr, 0, NS_LITERAL_CSTRING("127.0.0.1"),
                              serverPort, nullptr, getter_AddRefs(client));
    ASSERT_TRUE(NS_SUCCEEDED(rv));

    // Bind to a port. It's possible that we are binding to a port that is
    // currently in use. If we failed to bind, we try next port.
    NetAddr bindingAddr;
    bindingAddr.inet.family = AF_INET;
    bindingAddr.inet.ip = 0;
    bindingAddr.inet.port = PR_htons(bindingPort);
    rv = client->Bind(&bindingAddr);
    ASSERT_TRUE(NS_SUCCEEDED(rv));

    // Open IO streams, to make client SocketTransport connect to server.
    clientCallback = new ClientInputCallback(waiter);
    rv = client->OpenInputStream(nsITransport::OPEN_UNBUFFERED, 0, 0,
                                 getter_AddRefs(inputStream));
    ASSERT_TRUE(NS_SUCCEEDED(rv));

    nsCOMPtr<nsIAsyncInputStream> asyncInputStream =
        do_QueryInterface(inputStream);
    rv = asyncInputStream->AsyncWait(clientCallback, 0, 0, nullptr);

    // Wait for server's response or callback of input stream.
    waiter->Wait(1);
    if (clientCallback->mFailed) {
      // if client received error, we likely have bound a port that is in use.
      // we can try another port.
      bindingPort++;
    } else {
      // We are unlocked by server side, leave the loop and check result.
      break;
    }
  }

  ASSERT_FALSE(serverListener->mFailed);
  ASSERT_EQ(serverListener->mClientPort, bindingPort);

  inputStream->Close();
  waiter->Wait(1);
  ASSERT_TRUE(clientCallback->mFailed);

  server->Close();
}
