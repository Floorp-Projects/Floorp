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
#include "mozilla/net/DNS.h"
#include "prerror.h"

#define REQUEST  0x68656c6f
#define RESPONSE 0x6f6c6568

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
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIUDPSOCKETLISTENER
  virtual ~UDPClientListener();
  nsresult mResult;
};

NS_IMPL_ISUPPORTS1(UDPClientListener, nsIUDPSocketListener)

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
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIUDPSOCKETLISTENER

  virtual ~UDPServerListener();

  nsresult mResult;
};

NS_IMPL_ISUPPORTS1(UDPServerListener, nsIUDPSocketListener)

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

  // Bind server socket to 127.0.0.1
  rv = server->Init(0, true);
  NS_ENSURE_SUCCESS(rv, -1);
  int32_t serverPort;
  server->GetPort(&serverPort);
  server->AsyncListen(serverListener);

  // Bind clinet on arbitrary port
  nsRefPtr<UDPClientListener> clientListener = new UDPClientListener();
  client->Init(0, true);
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

  // Close server
  printf("*** Attempting to close server ...\n");
  server->Close();
  client->Close();
  PumpEvents();
  passed("Server closed");

  return 0; // failure is a non-zero return
}
