/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TestCommon.h"
#include "TestHarness.h"
#include "nsIUDPSocket.h"
#include "nsISocketTransportService.h"
#include "nsISocketTransport.h"
#include "nsIOutputStream.h"
#include "nsIInputStream.h"
#include "nsINetAddr.h"
#include "nsIScriptSecurityManager.h"
#include "nsITimer.h"
#include "mozilla/net/DNS.h"
#include "prerror.h"

#define REQUEST  0x68656c6f
#define RESPONSE 0x6f6c6568
#define MULTICAST_TIMEOUT 2000

#define EXPECT_SUCCESS(rv, ...) \
  PR_BEGIN_MACRO \
  if (NS_FAILED(rv)) { \
    fail(__VA_ARGS__); \
    return false; \
  } \
  PR_END_MACRO


#define EXPECT_FAILURE(rv, ...) \
  PR_BEGIN_MACRO \
  if (NS_SUCCEEDED(rv)) { \
    fail(__VA_ARGS__); \
    return false; \
  } \
  PR_END_MACRO

#define REQUIRE_EQUAL(a, b, ...) \
  PR_BEGIN_MACRO \
  if (a != b) { \
    fail(__VA_ARGS__); \
    return false; \
  } \
  PR_END_MACRO

enum TestPhase {
  TEST_OUTPUT_STREAM,
  TEST_SEND_API,
  TEST_MULTICAST,
  TEST_NONE
};

static TestPhase phase = TEST_NONE;

static bool CheckMessageContent(nsIUDPMessage *aMessage, uint32_t aExpectedContent)
{
  nsCString data;
  aMessage->GetData(data);

  const char* buffer = data.get();
  uint32_t len = data.Length();

  FallibleTArray<uint8_t>& rawData = aMessage->GetDataAsTArray();
  uint32_t rawLen = rawData.Length();

  if (len != rawLen) {
    fail("Raw data length(%d) do not matches String data length(%d).", rawLen, len);
    return false;
  }

  for (uint32_t i = 0; i < len; i++) {
    if (buffer[i] != rawData[i]) {
      fail("Raw data(%s) do not matches String data(%s)", rawData.Elements() ,buffer);
      return false;
    }
  }

  uint32_t input = 0;
  for (uint32_t i = 0; i < len; i++) {
    input += buffer[i] << (8 * i);
  }

  if (len != sizeof(uint32_t) || input != aExpectedContent)
  {
    fail("Request 0x%x received, expected 0x%x", input, aExpectedContent);
    return false;
  } else {
    passed("Request 0x%x received as expected", input);
    return true;
  }
}

/*
 * UDPClientListener: listens for incomming UDP packets
 */
class UDPClientListener : public nsIUDPSocketListener
{
protected:
  virtual ~UDPClientListener();

public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIUDPSOCKETLISTENER
  nsresult mResult;
};

NS_IMPL_ISUPPORTS(UDPClientListener, nsIUDPSocketListener)

UDPClientListener::~UDPClientListener()
{
}

NS_IMETHODIMP
UDPClientListener::OnPacketReceived(nsIUDPSocket* socket, nsIUDPMessage* message)
{
  mResult = NS_OK;

  uint16_t port;
  nsCString ip;
  nsCOMPtr<nsINetAddr> fromAddr;
  message->GetFromAddr(getter_AddRefs(fromAddr));
  fromAddr->GetPort(&port);
  fromAddr->GetAddress(ip);
  passed("Packet received on client from %s:%d", ip.get(), port);

  if (TEST_SEND_API == phase && CheckMessageContent(message, REQUEST)) {
    uint32_t count;
    const uint32_t data = RESPONSE;
    printf("*** Attempting to write response 0x%x to server by SendWithAddr...\n", RESPONSE);
    mResult = socket->SendWithAddr(fromAddr, (const uint8_t*)&data,
                                   sizeof(uint32_t), &count);
    if (mResult == NS_OK && count == sizeof(uint32_t)) {
      passed("Response written");
    } else {
      fail("Response written");
    }
    return NS_OK;
  } else if (TEST_OUTPUT_STREAM != phase || !CheckMessageContent(message, RESPONSE)) {
    mResult = NS_ERROR_FAILURE;
  }

  // Notify thread
  QuitPumpingEvents();
  return NS_OK;
}

NS_IMETHODIMP
UDPClientListener::OnStopListening(nsIUDPSocket*, nsresult)
{
  QuitPumpingEvents();
  return NS_OK;
}

/*
 * UDPServerListener: listens for incomming UDP packets
 */
class UDPServerListener : public nsIUDPSocketListener
{
protected:
  virtual ~UDPServerListener();

public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIUDPSOCKETLISTENER

  nsresult mResult;
};

NS_IMPL_ISUPPORTS(UDPServerListener, nsIUDPSocketListener)

UDPServerListener::~UDPServerListener()
{
}

NS_IMETHODIMP
UDPServerListener::OnPacketReceived(nsIUDPSocket* socket, nsIUDPMessage* message)
{
  mResult = NS_OK;

  uint16_t port;
  nsCString ip;
  nsCOMPtr<nsINetAddr> fromAddr;
  message->GetFromAddr(getter_AddRefs(fromAddr));
  fromAddr->GetPort(&port);
  fromAddr->GetAddress(ip);
  passed("Packet received on server from %s:%d", ip.get(), port);

  if (TEST_OUTPUT_STREAM == phase && CheckMessageContent(message, REQUEST))
  {
    nsCOMPtr<nsIOutputStream> outstream;
    message->GetOutputStream(getter_AddRefs(outstream));

    uint32_t count;
    const uint32_t data = RESPONSE;
    printf("*** Attempting to write response 0x%x to client by OutputStream...\n", RESPONSE);
    mResult = outstream->Write((const char*)&data, sizeof(uint32_t), &count);

    if (mResult == NS_OK && count == sizeof(uint32_t)) {
      passed("Response written");
    } else {
      fail("Response written");
    }
    return NS_OK;
  } else if (TEST_MULTICAST == phase && CheckMessageContent(message, REQUEST)) {
    mResult = NS_OK;
  } else if (TEST_SEND_API != phase || !CheckMessageContent(message, RESPONSE)) {
    mResult = NS_ERROR_FAILURE;
  }

  // Notify thread
  QuitPumpingEvents();
  return NS_OK;
}

NS_IMETHODIMP
UDPServerListener::OnStopListening(nsIUDPSocket*, nsresult)
{
  QuitPumpingEvents();
  return NS_OK;
}

/**
 * Multicast timer callback: detects delivery failure
 */
class MulticastTimerCallback : public nsITimerCallback
{
protected:
  virtual ~MulticastTimerCallback();

public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSITIMERCALLBACK

  nsresult mResult;
};

NS_IMPL_ISUPPORTS(MulticastTimerCallback, nsITimerCallback)

MulticastTimerCallback::~MulticastTimerCallback()
{
}

NS_IMETHODIMP
MulticastTimerCallback::Notify(nsITimer* timer)
{
  if (TEST_MULTICAST != phase) {
    return NS_OK;
  }
  // Multicast ping failed
  printf("Multicast ping timeout expired\n");
  mResult = NS_ERROR_FAILURE;
  QuitPumpingEvents();
  return NS_OK;
}

/**** Main ****/
int
main(int32_t argc, char *argv[])
{
  nsresult rv;
  ScopedXPCOM xpcom("UDP ServerSocket");
  if (xpcom.failed())
    return -1;

  // Create UDPSocket
  nsCOMPtr<nsIUDPSocket> server, client;
  server = do_CreateInstance("@mozilla.org/network/udp-socket;1", &rv);
  NS_ENSURE_SUCCESS(rv, -1);
  client = do_CreateInstance("@mozilla.org/network/udp-socket;1", &rv);
  NS_ENSURE_SUCCESS(rv, -1);

  // Create UDPServerListener to process UDP packets
  nsRefPtr<UDPServerListener> serverListener = new UDPServerListener();

  nsCOMPtr<nsIScriptSecurityManager> secman =
    do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, -1);

  nsCOMPtr<nsIPrincipal> systemPrincipal;
  rv = secman->GetSystemPrincipal(getter_AddRefs(systemPrincipal));
  NS_ENSURE_SUCCESS(rv, -1);

  // Bind server socket to 0.0.0.0
  rv = server->Init(0, false, systemPrincipal, true, 0);
  NS_ENSURE_SUCCESS(rv, -1);
  int32_t serverPort;
  server->GetPort(&serverPort);
  server->AsyncListen(serverListener);

  // Bind clinet on arbitrary port
  nsRefPtr<UDPClientListener> clientListener = new UDPClientListener();
  client->Init(0, false, systemPrincipal, true, 0);
  client->AsyncListen(clientListener);

  // Write data to server
  uint32_t count;
  const uint32_t data = REQUEST;

  phase = TEST_OUTPUT_STREAM;
  rv = client->Send(NS_LITERAL_CSTRING("127.0.0.1"), serverPort, (uint8_t*)&data, sizeof(uint32_t), &count);
  NS_ENSURE_SUCCESS(rv, -1);
  REQUIRE_EQUAL(count, sizeof(uint32_t), "Error");
  passed("Request written by Send");

  // Wait for server
  PumpEvents();
  NS_ENSURE_SUCCESS(serverListener->mResult, -1);

  // Read response from server
  NS_ENSURE_SUCCESS(clientListener->mResult, -1);

  mozilla::net::NetAddr clientAddr;
  rv = client->GetAddress(&clientAddr);
  NS_ENSURE_SUCCESS(rv, -1);
  // The client address is 0.0.0.0, but Windows won't receive packets there, so
  // use 127.0.0.1 explicitly
  clientAddr.inet.ip = PR_htonl(127 << 24 | 1);

  phase = TEST_SEND_API;
  rv = server->SendWithAddress(&clientAddr, (uint8_t*)&data, sizeof(uint32_t), &count);
  NS_ENSURE_SUCCESS(rv, -1);
  REQUIRE_EQUAL(count, sizeof(uint32_t), "Error");
  passed("Request written by SendWithAddress");

  // Wait for server
  PumpEvents();
  NS_ENSURE_SUCCESS(serverListener->mResult, -1);

  // Read response from server
  NS_ENSURE_SUCCESS(clientListener->mResult, -1);

  // Setup timer to detect multicast failure
  nsCOMPtr<nsITimer> timer = do_CreateInstance("@mozilla.org/timer;1");
  if (NS_WARN_IF(!timer)) {
    return -1;
  }
  nsRefPtr<MulticastTimerCallback> timerCb = new MulticastTimerCallback();

  // The following multicast tests using multiple sockets require a firewall
  // exception on Windows XP before they pass.  For now, we'll skip them here.
  // Later versions of Windows don't seem to have this issue.
#ifdef XP_WIN
  OSVERSIONINFO OsVersion;
  OsVersion.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
#pragma warning(push)
#pragma warning(disable:4996) // 'GetVersionExA': was declared deprecated
  GetVersionEx(&OsVersion);
#pragma warning(pop)
  if (OsVersion.dwMajorVersion == 5 && OsVersion.dwMinorVersion == 1) {
    goto close;
  }
#endif

  // Join multicast group
  printf("Joining multicast group\n");
  phase = TEST_MULTICAST;
  mozilla::net::NetAddr multicastAddr;
  multicastAddr.inet.family = AF_INET;
  multicastAddr.inet.ip = PR_htonl(224 << 24 | 255);
  multicastAddr.inet.port = PR_htons(serverPort);
  rv = server->JoinMulticastAddr(multicastAddr, nullptr);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return -1;
  }

  // Send multicast ping
  timerCb->mResult = NS_OK;
  timer->InitWithCallback(timerCb, MULTICAST_TIMEOUT, nsITimer::TYPE_ONE_SHOT);
  rv = client->SendWithAddress(&multicastAddr, (uint8_t*)&data, sizeof(uint32_t), &count);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return -1;
  }
  REQUIRE_EQUAL(count, sizeof(uint32_t), "Error");
  passed("Multicast ping written by SendWithAddress");

  // Wait for server to receive successfully
  PumpEvents();
  if (NS_WARN_IF(NS_FAILED(serverListener->mResult))) {
    return -1;
  }
  if (NS_WARN_IF(NS_FAILED(timerCb->mResult))) {
    return -1;
  }
  timer->Cancel();
  passed("Server received ping successfully");

  // Disable multicast loopback
  printf("Disable multicast loopback\n");
  client->SetMulticastLoopback(false);
  server->SetMulticastLoopback(false);

  // Send multicast ping
  timerCb->mResult = NS_OK;
  timer->InitWithCallback(timerCb, MULTICAST_TIMEOUT, nsITimer::TYPE_ONE_SHOT);
  rv = client->SendWithAddress(&multicastAddr, (uint8_t*)&data, sizeof(uint32_t), &count);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return -1;
  }
  REQUIRE_EQUAL(count, sizeof(uint32_t), "Error");
  passed("Multicast ping written by SendWithAddress");

  // Wait for server to fail to receive
  PumpEvents();
  if (NS_WARN_IF(NS_SUCCEEDED(timerCb->mResult))) {
    return -1;
  }
  timer->Cancel();
  passed("Server failed to receive ping correctly");

  // Reset state
  client->SetMulticastLoopback(true);
  server->SetMulticastLoopback(true);

  // Change multicast interface
  printf("Changing multicast interface\n");
  mozilla::net::NetAddr loopbackAddr;
  loopbackAddr.inet.family = AF_INET;
  loopbackAddr.inet.ip = PR_htonl(INADDR_LOOPBACK);
  client->SetMulticastInterfaceAddr(loopbackAddr);

  // Send multicast ping
  timerCb->mResult = NS_OK;
  timer->InitWithCallback(timerCb, MULTICAST_TIMEOUT, nsITimer::TYPE_ONE_SHOT);
  rv = client->SendWithAddress(&multicastAddr, (uint8_t*)&data, sizeof(uint32_t), &count);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return -1;
  }
  REQUIRE_EQUAL(count, sizeof(uint32_t), "Error");
  passed("Multicast ping written by SendWithAddress");

  // Wait for server to fail to receive
  PumpEvents();
  if (NS_WARN_IF(NS_SUCCEEDED(timerCb->mResult))) {
    return -1;
  }
  timer->Cancel();
  passed("Server failed to receive ping correctly");

  // Reset state
  mozilla::net::NetAddr anyAddr;
  anyAddr.inet.family = AF_INET;
  anyAddr.inet.ip = PR_htonl(INADDR_ANY);
  client->SetMulticastInterfaceAddr(anyAddr);

  // Leave multicast group
  printf("Leave multicast group\n");
  rv = server->LeaveMulticastAddr(multicastAddr, nullptr);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return -1;
  }

  // Send multicast ping
  timerCb->mResult = NS_OK;
  timer->InitWithCallback(timerCb, MULTICAST_TIMEOUT, nsITimer::TYPE_ONE_SHOT);
  rv = client->SendWithAddress(&multicastAddr, (uint8_t*)&data, sizeof(uint32_t), &count);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return -1;
  }
  REQUIRE_EQUAL(count, sizeof(uint32_t), "Error");
  passed("Multicast ping written by SendWithAddress");

  // Wait for server to fail to receive
  PumpEvents();
  if (NS_WARN_IF(NS_SUCCEEDED(timerCb->mResult))) {
    return -1;
  }
  timer->Cancel();
  passed("Server failed to receive ping correctly");
  goto close;

close:
  // Close server
  printf("*** Attempting to close server ...\n");
  server->Close();
  client->Close();
  PumpEvents();
  passed("Server closed");

  return 0; // failure is a non-zero return
}
