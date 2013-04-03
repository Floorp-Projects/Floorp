/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TestCommon.h"
#include "TestHarness.h"
#include "nsIUDPServerSocket.h"
#include "nsISocketTransportService.h"
#include "nsISocketTransport.h"
#include "nsIOutputStream.h"
#include "nsIInputStream.h"

#define UDP_PORT 1234
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

/*
 * UDPListener: listens for incomming UDP packets
 */
class UDPListener : public nsIUDPServerSocketListener
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIUDPSERVERSOCKETLISTENER

  virtual ~UDPListener();

  nsresult mResult;
};

NS_IMPL_THREADSAFE_ISUPPORTS1(UDPListener,
                              nsIUDPServerSocketListener)

UDPListener::~UDPListener()
{
}

NS_IMETHODIMP
UDPListener::OnPacketReceived(nsIUDPServerSocket* socket, nsIUDPMessage* message)
{
  mResult = NS_OK;

  uint16_t port;
  nsCString ip;
  nsCOMPtr<nsINetAddr> fromAddr;
  message->GetFromAddr(getter_AddRefs(fromAddr));
  fromAddr->GetPort(&port);
  fromAddr->GetAddress(ip);
  passed("Packet received on server from %s:%d", ip.get(), port);

  nsCString data;
  message->GetData(data);

  const char* buffer = data.get();
  uint32_t len = data.Length();

  uint32_t input = 0;
  for (uint32_t i = 0; i < len; i++) {
    input += buffer[i] << (8 * i);
  }

  if (len != sizeof(uint32_t) || input != REQUEST)
  {
    mResult = NS_ERROR_FAILURE;
    fail("Request 0x%x received on server", input);
  } else {
    passed("Request 0x%x received on server", input);
    // Respond with same data

    nsCOMPtr<nsIOutputStream> outstream;
    message->GetOutputStream(getter_AddRefs(outstream));

    uint32_t count;
    const uint32_t data = RESPONSE;
    printf("*** Attempting to write response 0x%x to client ...\n", RESPONSE);
    mResult = outstream->Write((const char*)&data, sizeof(uint32_t), &count);

    if (mResult == NS_OK && count == sizeof(uint32_t)) {
      passed("Response written");
    } else {
      fail("Response written");
    }
  }

  // Notify thread
  QuitPumpingEvents();
  return NS_OK;
}

NS_IMETHODIMP
UDPListener::OnStopListening(nsIUDPServerSocket*, nsresult)
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

  // Create UDPServerSocket
  nsCOMPtr<nsIUDPServerSocket> server;
  server = do_GetService("@mozilla.org/network/server-socket-udp;1", &rv);
  NS_ENSURE_SUCCESS(rv, -1);

  // Create UDPListener to process UDP packets
  nsCOMPtr<UDPListener> listener = new UDPListener();

  // Init async server
  server->Init(UDP_PORT, false);
  server->AsyncListen(listener);

  // Create UDP socket and streams
  nsCOMPtr<nsISocketTransportService> sts =
        do_GetService("@mozilla.org/network/socket-transport-service;1", &rv);
  NS_ENSURE_SUCCESS(rv, -1);

  nsCOMPtr<nsISocketTransport> transport;
  const char *protocol = "udp";
  rv = sts->CreateTransport(&protocol, 1, NS_LITERAL_CSTRING("127.0.0.1"),
                            UDP_PORT, nullptr, getter_AddRefs(transport));
  NS_ENSURE_SUCCESS(rv, -1);

  nsCOMPtr<nsIOutputStream> outstream;
  rv = transport->OpenOutputStream(nsITransport::OPEN_BLOCKING,
                                   0, 0, getter_AddRefs(outstream));
  NS_ENSURE_SUCCESS(rv, -1);

  nsCOMPtr<nsIInputStream> instream;
  rv = transport->OpenInputStream(nsITransport::OPEN_BLOCKING,
                                  0, 0, getter_AddRefs(instream));
  NS_ENSURE_SUCCESS(rv, -1);

  // Write data to server
  uint32_t count, read;
  const uint32_t data = REQUEST;

  printf("*** Attempting to write request 0x%x to server ...\n", REQUEST);
  rv = outstream->Write((const char*)&data, sizeof(uint32_t), &count);
  NS_ENSURE_SUCCESS(rv, -1);
  REQUIRE_EQUAL(count, sizeof(uint32_t), "Error");
  passed("Request written");

  // Wait for server
  PumpEvents();
  NS_ENSURE_SUCCESS(listener->mResult, -1);

  // Read response from server
  printf("*** Attempting to read response from server ...\n");
  rv = instream->Read((char*)&read, sizeof(uint32_t), &count);

  REQUIRE_EQUAL(count, sizeof(uint32_t), "Did not read enough bytes from input stream");
  REQUIRE_EQUAL(read, RESPONSE, "Did not read expected data from stream. Received 0x%x", read);
  passed("Response from server 0x%x", read);

  // Close server
  printf("*** Attempting to close server ...\n");
  server->Close();
  PumpEvents();
  passed("Server closed");

  return 0; // failure is a non-zero return
}
