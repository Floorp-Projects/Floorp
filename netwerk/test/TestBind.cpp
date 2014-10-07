/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TestCommon.h"
#include "TestHarness.h"
#include "nsISocketTransportService.h"
#include "nsISocketTransport.h"
#include "nsIServerSocket.h"
#include "nsIAsyncInputStream.h"
#include "nsINetAddr.h"
#include "mozilla/net/DNS.h"
#include "prerror.h"

using namespace mozilla::net;
using namespace mozilla;

class ServerListener: public nsIServerSocketListener
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISERVERSOCKETLISTENER

  ServerListener();

  // Port that is got from server side will be store here.
  uint32_t mClientPort;
  bool mFailed;
private:
  virtual ~ServerListener();
};

NS_IMPL_ISUPPORTS(ServerListener, nsIServerSocketListener)

ServerListener::ServerListener()
  : mClientPort(-1)
  , mFailed(false)
{
}

ServerListener::~ServerListener()
{
}

NS_IMETHODIMP
ServerListener::OnSocketAccepted(nsIServerSocket *aServ,
                                 nsISocketTransport *aTransport)
{
  // Run on STS thread.
  NetAddr peerAddr;
  nsresult rv = aTransport->GetPeerAddr(&peerAddr);
  if (NS_FAILED(rv)) {
    mFailed = true;
    fail("Server: not able to get peer address.");
    QuitPumpingEvents();
    return NS_OK;
  }
  mClientPort = PR_ntohs(peerAddr.inet.port);
  passed("Server: received connection");
  QuitPumpingEvents();
  return NS_OK;
}

NS_IMETHODIMP
ServerListener::OnStopListening(nsIServerSocket *aServ,
                                nsresult aStatus)
{
  return NS_OK;
}

class ClientInputCallback : public nsIInputStreamCallback
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIINPUTSTREAMCALLBACK

  ClientInputCallback();

  bool mFailed;
private:
  virtual ~ClientInputCallback();
};

NS_IMPL_ISUPPORTS(ClientInputCallback, nsIInputStreamCallback)

ClientInputCallback::ClientInputCallback()
  : mFailed(false)
{
}

ClientInputCallback::~ClientInputCallback()
{
}

NS_IMETHODIMP
ClientInputCallback::OnInputStreamReady(nsIAsyncInputStream *aStream)
{
  // Server doesn't send. That means if we are here, we probably have run into
  // an error.
  uint64_t avail;
  nsresult rv = aStream->Available(&avail);
  if (NS_FAILED(rv)) {
    mFailed = true;
  }
  QuitPumpingEvents();
  return NS_OK;
}

int
main(int32_t argc, char *argv[])
{
  ScopedXPCOM xpcom("SocketTransport");
  if (xpcom.failed()) {
    fail("Unable to initalize XPCOM.");
    return -1;
  }

  //
  // Server side.
  //
  nsCOMPtr<nsIServerSocket> server = do_CreateInstance("@mozilla.org/network/server-socket;1");
  if (!server) {
    fail("Failed to create server socket.");
    return -1;
  }

  nsresult rv = server->Init(-1, true, -1);
  if (NS_FAILED(rv)) {
    fail("Failed to initialize server.");
    return -1;
  }

  int32_t serverPort;
  rv = server->GetPort(&serverPort);
  if (NS_FAILED(rv)) {
    fail("Unable to get server port.");
    return -1;
  }

  // Listening.
  nsRefPtr<ServerListener> serverListener = new ServerListener();
  rv = server->AsyncListen(serverListener);
  if (NS_FAILED(rv)) {
    fail("Server fail to start listening.");
    return -1;
  }

  //
  // Client side
  //
  uint32_t bindingPort = 20000;
  nsCOMPtr<nsISocketTransportService> sts =
    do_GetService("@mozilla.org/network/socket-transport-service;1", &rv);
  if (NS_FAILED(rv)) {
    fail("Unable to get socket transport service.");
    return -1;
  }

  for (int32_t tried = 0; tried < 100; tried++) {
    nsCOMPtr<nsISocketTransport> client;
    rv = sts->CreateTransport(nullptr, 0, NS_LITERAL_CSTRING("127.0.0.1"),
                              serverPort, nullptr, getter_AddRefs(client));
    if (NS_FAILED(rv)) {
      fail("Unable to create transport.");
      return -1;
    }

    // Bind to a port. It's possible that we are binding to a port that is
    // currently in use. If we failed to bind, we try next port.
    NetAddr bindingAddr;
    bindingAddr.inet.family = AF_INET;
    bindingAddr.inet.ip = 0;
    bindingAddr.inet.port = PR_htons(bindingPort);
    rv = client->Bind(&bindingAddr);
    if (NS_FAILED(rv)) {
      fail("Unable to bind a port.");
      return -1;
    }

    // Open IO streams, to make client SocketTransport connect to server.
    nsRefPtr<ClientInputCallback> clientCallback = new ClientInputCallback();
    nsCOMPtr<nsIInputStream> inputStream;
    rv = client->OpenInputStream(nsITransport::OPEN_UNBUFFERED,
                                 0, 0, getter_AddRefs(inputStream));
    if (NS_FAILED(rv)) {
      fail("Failed to open an input stream.");
      return -1;
    }
    nsCOMPtr<nsIAsyncInputStream> asyncInputStream = do_QueryInterface(inputStream);
    rv = asyncInputStream->AsyncWait(clientCallback, 0, 0, nullptr);

    // Wait for server's response or callback of input stream.
    PumpEvents();
    if (clientCallback->mFailed) {
      // if client received error, we likely have bound a port that is in use.
      // we can try another port.
      bindingPort++;
    } else {
      // We are unlocked by server side, leave the loop and check result.
      break;
    }
  }

  if (serverListener->mFailed) {
    fail("Server failure.");
    return -1;
  }
  if (serverListener->mClientPort != bindingPort) {
    fail("Port that server got doesn't match what we are expecting.");
    return -1;
  }
  passed("Port matched");
  return 0;
}
