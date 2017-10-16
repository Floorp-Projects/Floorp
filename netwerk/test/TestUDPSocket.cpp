/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TestCommon.h"
#include "gtest/gtest.h"
#include "nsIUDPSocket.h"
#include "nsISocketTransportService.h"
#include "nsISocketTransport.h"
#include "nsIOutputStream.h"
#include "nsIInputStream.h"
#include "nsINetAddr.h"
#include "nsIScriptSecurityManager.h"
#include "nsITimer.h"
#include "nsContentUtils.h"
#include "mozilla/net/DNS.h"
#include "prerror.h"

#define REQUEST  0x68656c6f
#define RESPONSE 0x6f6c6568
#define MULTICAST_TIMEOUT 2000

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
    ADD_FAILURE() << "Raw data length " << rawLen << " does not match String data length " << len;
    return false;
  }

  for (uint32_t i = 0; i < len; i++) {
    if (buffer[i] != rawData[i]) {
      ADD_FAILURE();
      return false;
    }
  }

  uint32_t input = 0;
  for (uint32_t i = 0; i < len; i++) {
    input += buffer[i] << (8 * i);
  }

  if (len != sizeof(uint32_t)) {
    ADD_FAILURE() << "Message length mismatch, expected " << sizeof(uint32_t) <<
      " got " << len;
    return false;
  }
  if (input != aExpectedContent) {
    ADD_FAILURE() << "Message content mismatch, expected 0x" <<
      std::hex << aExpectedContent << " got 0x" << input;
    return false;
  }

  return true;
}

/*
 * UDPClientListener: listens for incomming UDP packets
 */
class UDPClientListener : public nsIUDPSocketListener
{
protected:
  virtual ~UDPClientListener();

public:
  explicit UDPClientListener(WaitForCondition* waiter)
    : mWaiter(waiter)
  { }

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIUDPSOCKETLISTENER
  nsresult mResult = NS_ERROR_FAILURE;
  RefPtr<WaitForCondition> mWaiter;
};

NS_IMPL_ISUPPORTS(UDPClientListener, nsIUDPSocketListener)

UDPClientListener::~UDPClientListener() = default;

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

  if (TEST_SEND_API == phase && CheckMessageContent(message, REQUEST)) {
    uint32_t count;
    const uint32_t data = RESPONSE;
    mResult = socket->SendWithAddr(fromAddr, (const uint8_t*)&data,
                                   sizeof(uint32_t), &count);
    if (mResult == NS_OK && count == sizeof(uint32_t)) {
      SUCCEED();
    } else {
      ADD_FAILURE();
    }
    return NS_OK;
  } else if (TEST_OUTPUT_STREAM != phase || !CheckMessageContent(message, RESPONSE)) {
    mResult = NS_ERROR_FAILURE;
  }

  // Notify thread
  mWaiter->Notify();
  return NS_OK;
}

NS_IMETHODIMP
UDPClientListener::OnStopListening(nsIUDPSocket*, nsresult)
{
  mWaiter->Notify();
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
  explicit UDPServerListener(WaitForCondition* waiter)
    : mWaiter(waiter)
  { }

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIUDPSOCKETLISTENER

  nsresult mResult = NS_ERROR_FAILURE;
  RefPtr<WaitForCondition> mWaiter;
};

NS_IMPL_ISUPPORTS(UDPServerListener, nsIUDPSocketListener)

UDPServerListener::~UDPServerListener() = default;

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
  SUCCEED();

  if (TEST_OUTPUT_STREAM == phase && CheckMessageContent(message, REQUEST))
  {
    nsCOMPtr<nsIOutputStream> outstream;
    message->GetOutputStream(getter_AddRefs(outstream));

    uint32_t count;
    const uint32_t data = RESPONSE;
    mResult = outstream->Write((const char*)&data, sizeof(uint32_t), &count);

    if (mResult == NS_OK && count == sizeof(uint32_t)) {
      SUCCEED();
    } else {
      ADD_FAILURE();
    }
    return NS_OK;
  } else if (TEST_MULTICAST == phase && CheckMessageContent(message, REQUEST)) {
    mResult = NS_OK;
  } else if (TEST_SEND_API != phase || !CheckMessageContent(message, RESPONSE)) {
    mResult = NS_ERROR_FAILURE;
  }

  // Notify thread
  mWaiter->Notify();
  return NS_OK;
}

NS_IMETHODIMP
UDPServerListener::OnStopListening(nsIUDPSocket*, nsresult)
{
  mWaiter->Notify();
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
  explicit MulticastTimerCallback(WaitForCondition* waiter)
    : mWaiter(waiter)
  { }

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSITIMERCALLBACK

  nsresult mResult;
  RefPtr<WaitForCondition> mWaiter;
};

NS_IMPL_ISUPPORTS(MulticastTimerCallback, nsITimerCallback)

MulticastTimerCallback::~MulticastTimerCallback() = default;

NS_IMETHODIMP
MulticastTimerCallback::Notify(nsITimer* timer)
{
  if (TEST_MULTICAST != phase) {
    return NS_OK;
  }
  // Multicast ping failed
  printf("Multicast ping timeout expired\n");
  mResult = NS_ERROR_FAILURE;
  mWaiter->Notify();
  return NS_OK;
}

/**** Main ****/

TEST(TestUDPSocket, TestUDPSocketMain)
{
  nsresult rv;

  // Create UDPSocket
  nsCOMPtr<nsIUDPSocket> server, client;
  server = do_CreateInstance("@mozilla.org/network/udp-socket;1", &rv);
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  client = do_CreateInstance("@mozilla.org/network/udp-socket;1", &rv);
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  RefPtr<WaitForCondition> waiter = new WaitForCondition();

  // Create UDPServerListener to process UDP packets
  RefPtr<UDPServerListener> serverListener = new UDPServerListener(waiter);

  nsCOMPtr<nsIPrincipal> systemPrincipal = nsContentUtils::GetSystemPrincipal();

  // Bind server socket to 0.0.0.0
  rv = server->Init(0, false, systemPrincipal, true, 0);
  ASSERT_TRUE(NS_SUCCEEDED(rv));
  int32_t serverPort;
  server->GetPort(&serverPort);
  server->AsyncListen(serverListener);

  // Bind clinet on arbitrary port
  RefPtr<UDPClientListener> clientListener = new UDPClientListener(waiter);
  client->Init(0, false, systemPrincipal, true, 0);
  client->AsyncListen(clientListener);

  // Write data to server
  uint32_t count;
  const uint32_t data = REQUEST;

  phase = TEST_OUTPUT_STREAM;
  rv = client->Send(NS_LITERAL_CSTRING("127.0.0.1"), serverPort, (uint8_t*)&data, sizeof(uint32_t), &count);
  ASSERT_TRUE(NS_SUCCEEDED(rv));
  EXPECT_EQ(count, sizeof(uint32_t));

  // Wait for server
  waiter->Wait(1);
  ASSERT_TRUE(NS_SUCCEEDED(serverListener->mResult));

  // Read response from server
  ASSERT_TRUE(NS_SUCCEEDED(clientListener->mResult));

  mozilla::net::NetAddr clientAddr;
  rv = client->GetAddress(&clientAddr);
  ASSERT_TRUE(NS_SUCCEEDED(rv));
  // The client address is 0.0.0.0, but Windows won't receive packets there, so
  // use 127.0.0.1 explicitly
  clientAddr.inet.ip = PR_htonl(127 << 24 | 1);

  phase = TEST_SEND_API;
  rv = server->SendWithAddress(&clientAddr, (uint8_t*)&data, sizeof(uint32_t), &count);
  ASSERT_TRUE(NS_SUCCEEDED(rv));
  EXPECT_EQ(count, sizeof(uint32_t));

  // Wait for server
  waiter->Wait(1);
  ASSERT_TRUE(NS_SUCCEEDED(serverListener->mResult));

  // Read response from server
  ASSERT_TRUE(NS_SUCCEEDED(clientListener->mResult));

  // Setup timer to detect multicast failure
  nsCOMPtr<nsITimer> timer = NS_NewTimer();
  ASSERT_TRUE(timer);
  RefPtr<MulticastTimerCallback> timerCb = new MulticastTimerCallback(waiter);

  // Join multicast group
  printf("Joining multicast group\n");
  phase = TEST_MULTICAST;
  mozilla::net::NetAddr multicastAddr;
  multicastAddr.inet.family = AF_INET;
  multicastAddr.inet.ip = PR_htonl(224 << 24 | 255);
  multicastAddr.inet.port = PR_htons(serverPort);
  rv = server->JoinMulticastAddr(multicastAddr, nullptr);
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  // Send multicast ping
  timerCb->mResult = NS_OK;
  timer->InitWithCallback(timerCb, MULTICAST_TIMEOUT, nsITimer::TYPE_ONE_SHOT);
  rv = client->SendWithAddress(&multicastAddr, (uint8_t*)&data, sizeof(uint32_t), &count);
  ASSERT_TRUE(NS_SUCCEEDED(rv));
  EXPECT_EQ(count, sizeof(uint32_t));

  // Wait for server to receive successfully
  waiter->Wait(1);
  ASSERT_TRUE(NS_SUCCEEDED(serverListener->mResult));
  ASSERT_TRUE(NS_SUCCEEDED(timerCb->mResult));
  timer->Cancel();

  // Disable multicast loopback
  printf("Disable multicast loopback\n");
  client->SetMulticastLoopback(false);
  server->SetMulticastLoopback(false);

  // Send multicast ping
  timerCb->mResult = NS_OK;
  timer->InitWithCallback(timerCb, MULTICAST_TIMEOUT, nsITimer::TYPE_ONE_SHOT);
  rv = client->SendWithAddress(&multicastAddr, (uint8_t*)&data, sizeof(uint32_t), &count);
  ASSERT_TRUE(NS_SUCCEEDED(rv));
  EXPECT_EQ(count, sizeof(uint32_t));

  // Wait for server to fail to receive
  waiter->Wait(1);
  ASSERT_FALSE(NS_SUCCEEDED(timerCb->mResult));
  timer->Cancel();

  // Reset state
  client->SetMulticastLoopback(true);
  server->SetMulticastLoopback(true);

  // Change multicast interface
  mozilla::net::NetAddr loopbackAddr;
  loopbackAddr.inet.family = AF_INET;
  loopbackAddr.inet.ip = PR_htonl(INADDR_LOOPBACK);
  client->SetMulticastInterfaceAddr(loopbackAddr);

  // Send multicast ping
  timerCb->mResult = NS_OK;
  timer->InitWithCallback(timerCb, MULTICAST_TIMEOUT, nsITimer::TYPE_ONE_SHOT);
  rv = client->SendWithAddress(&multicastAddr, (uint8_t*)&data, sizeof(uint32_t), &count);
  ASSERT_TRUE(NS_SUCCEEDED(rv));
  EXPECT_EQ(count, sizeof(uint32_t));

  // Wait for server to fail to receive
  waiter->Wait(1);
  ASSERT_FALSE(NS_SUCCEEDED(timerCb->mResult));
  timer->Cancel();

  // Reset state
  mozilla::net::NetAddr anyAddr;
  anyAddr.inet.family = AF_INET;
  anyAddr.inet.ip = PR_htonl(INADDR_ANY);
  client->SetMulticastInterfaceAddr(anyAddr);

  // Leave multicast group
  rv = server->LeaveMulticastAddr(multicastAddr, nullptr);
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  // Send multicast ping
  timerCb->mResult = NS_OK;
  timer->InitWithCallback(timerCb, MULTICAST_TIMEOUT, nsITimer::TYPE_ONE_SHOT);
  rv = client->SendWithAddress(&multicastAddr, (uint8_t*)&data, sizeof(uint32_t), &count);
  ASSERT_TRUE(NS_SUCCEEDED(rv));
  EXPECT_EQ(count, sizeof(uint32_t));

  // Wait for server to fail to receive
  waiter->Wait(1);
  ASSERT_FALSE(NS_SUCCEEDED(timerCb->mResult));
  timer->Cancel();

  goto close; // suppress warning about unused label

close:
  // Close server
  server->Close();
  client->Close();

  // Wait for client and server to see closing
  waiter->Wait(2);
}
