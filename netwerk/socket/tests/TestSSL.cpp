/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
*/

#include "nspr.h"

#include "nsIProfile.h"
#include "nsIServiceManager.h"
#include "nsSpecialSystemDirectory.h"

#include "nsISocketProviderService.h"
#include "nsISocketProvider.h"
#include "nsIPSMComponent.h"

static NS_DEFINE_CID(kSocketProviderService, NS_SOCKETPROVIDERSERVICE_CID);


PRUint32
TestSSL_syncprintf(const char *fmt, ...)
{
  static PRFileDesc *logFile=0;
  va_list marker;

  if (!logFile)
    logFile = PR_GetSpecialFD(PR_StandardError);

  PR_ASSERT(logFile);

  va_start(marker, fmt);
  PR_vfprintf(logFile, fmt, marker);
  va_end(marker);

  /* PR_Sync(logFile); */

  return 0;
}



/* TODO: deal with bad arg, error cases */
PRStatus
GetPage(PRFileDesc *sock, char *sName, PRUint16 sPort, char *path)
{
  PRNetAddr  netAddr;
  PRHostEnt  hostEntry;
  char       netBuf[2*PR_NETDB_BUF_SIZE];
  char       outBuf[2048];  /* TODO: worry about max sizes */
  char       inBuf[32768];
  PRIntn     cmdSize;
  PRInt32    cnt;
  PRStatus   rv;

  memset(&netAddr, 0, sizeof(PRNetAddr));

  rv = PR_GetHostByName(sName, netBuf, sizeof(netBuf), &hostEntry);

  if (PR_SUCCESS == rv)
    {
    netAddr.inet.family = PR_AF_INET;
    netAddr.inet.port   = PR_htons(sPort);
    netAddr.inet.ip     = *((PRUint32 *)hostEntry.h_addr_list[0]);

    rv = PR_Connect(sock, &netAddr, PR_INTERVAL_NO_TIMEOUT);
    }

  if (PR_SUCCESS == rv)
    {
    TestSSL_syncprintf("Connected to %s on port %d\n", sName, sPort);
    cmdSize = PR_snprintf(outBuf,
                          sizeof(outBuf), "GET %s HTTP/1.0\r\n\r\n", path);
    outBuf[cmdSize] = 0; /* TODO: shouldn't assume positive value */

    cnt = PR_Send(sock, outBuf, cmdSize, 0, PR_INTERVAL_NO_TIMEOUT);
    TestSSL_syncprintf("Send command cmdSize=%d, cnt=%d\n", cmdSize, cnt);
    TestSSL_syncprintf("Send command cmd\n%s", outBuf);
    if (cnt < 0)
      rv = PR_FAILURE;
    }

  if (PR_SUCCESS == rv)
    {
    PR_Sleep(PR_SecondsToInterval(1));
    memset(inBuf, 0, sizeof(inBuf));

    cnt = PR_Recv(sock, inBuf, sizeof(inBuf), 0, PR_INTERVAL_NO_TIMEOUT);

    if (cnt < 0)
      rv = PR_FAILURE;
    }    

  if (PR_SUCCESS == rv)
    {
    inBuf[cnt] = 0;
    TestSSL_syncprintf("Receive data cnt=%d\n", cnt);
    TestSSL_syncprintf("----\n");
    TestSSL_syncprintf("%s\n", inBuf);
    TestSSL_syncprintf("----\n");
    }

  if (PR_SUCCESS == rv)
    {
    rv = PR_Shutdown(sock, PR_SHUTDOWN_BOTH);
    }

  return rv;
}


#define HTTPS_SERVER "omen"
#define HTTPS_PORT   443

#define HTTP_SERVER  "omen"
#define HTTP_PORT    80

static NS_DEFINE_CID(kProfileCID, NS_PROFILE_CID);

nsresult
StartupServices(void)
{
  nsresult   result;

  NS_WITH_SERVICE(nsIProfile, profile, kProfileCID, &result);
  if (NS_SUCCEEDED(result))
    result = profile->Startup(nsnull);

  if (NS_SUCCEEDED(result))
    {
    NS_WITH_SERVICE(nsIPSMComponent, psm, PSM_COMPONENT_PROGID, &result);
//    psm->Initialize(nsnull, nsnull);
    }   
  return result;
}

PRIntn
main(PRIntn argc, char **argv)
{
  PRFileDesc    *sockS;
  PRFileDesc    *sock;
  nsresult       rv;

  /* initialize NSPR and logFile */
  PR_STDIO_INIT();

  StartupServices();

  NS_WITH_SERVICE(nsISocketProviderService,
                  pProviderService,
                  kSocketProviderService,
                  &rv);

  nsISocketProvider *pProvider;
  if (NS_SUCCEEDED(rv))
    rv = pProviderService->GetSocketProvider("ssl", &pProvider);
  if (NS_SUCCEEDED(rv))
    rv = pProvider->NewSocket( HTTP_SERVER, &sockS );
  sock  = PR_NewTCPSocket();

  /* grab pages using the PRFileDesc's */
  GetPage(sockS, HTTPS_SERVER, HTTPS_PORT, "/index.html");
  GetPage(sock,  HTTP_SERVER,  HTTP_PORT,  "/index.html");
  PR_Close(sockS);
  PR_Close(sock);

  if (NS_SUCCEEDED(rv))
    rv = pProvider->NewSocket( HTTPS_SERVER, &sockS );
  GetPage(sockS, HTTPS_SERVER, HTTPS_PORT, "/index.html");
  PR_Close(sockS);
  NS_RELEASE(pProvider);

  return 0;
}



#ifdef TESTIOLAYER

static PRDescIdentity  identity;

static PRIOMethods     myMethods;

static PRUint16        default_port = 12273;
static PRNetAddr       server_address;

static PRIntn minor_iterations = 5;
static PRIntn major_iterations = 1;


static PRInt32 PR_CALLBACK
MyRecv(PRFileDesc *fd, void *buf, PRInt32 amount,
       PRIntn flags, PRIntervalTime timeout)
{
  char *b = (char*)buf;
  PRFileDesc *lo = fd->lower;
  PRInt32 rv, readin = 0, request;

  TestSSL_syncprintf(
    "MyRecv start\n");

  TestSSL_syncprintf(
    "MyRecv waiting for permission request\n");
  rv = lo->methods->recv(lo, &request, sizeof(request), flags, timeout);

  if (0 < rv)
    {
    TestSSL_syncprintf(
      "MyRecv received permission request for %d bytes\n", request);


    TestSSL_syncprintf(
      "MyRecv sending permission for %d bytes\n", request);
    rv = lo->methods->send(
        lo, &request, sizeof(request), flags, timeout);

    if (0 < rv)
      {
      TestSSL_syncprintf(
        "MyRecv waiting for data\n");
      while (readin < request)
        {
        rv = lo->methods->recv(
               lo, b + readin, amount - readin, flags, timeout);
        if (rv <= 0) break;
        TestSSL_syncprintf(
          "MyRecv received %d bytes\n", rv);
        readin += rv;
        }
      rv = readin;
      }
    }

  TestSSL_syncprintf(
    "MyRecv end\n");

  return rv;
}

static PRInt32 PR_CALLBACK
MySend(PRFileDesc *fd, const void *buf, PRInt32 amount,
       PRIntn flags, PRIntervalTime timeout)
{
  PRFileDesc *lo = fd->lower;
  const char *b = (const char*)buf;
  PRInt32 rv, wroteout = 0, request;

  TestSSL_syncprintf(
    "MySend start\n");
  TestSSL_syncprintf(
    "MySend asking permission to send %d bytes\n", amount);

  rv = lo->methods->send(lo, &amount, sizeof(amount), flags, timeout);
  if (0 < rv)
    {
    TestSSL_syncprintf(
      "MySend waiting for permission to send %d bytes\n", amount);
    rv = lo->methods->recv(
           lo, &request, sizeof(request), flags, timeout);
    if (0 < rv)
      {
      PR_ASSERT(request == amount);
      TestSSL_syncprintf(
        "MySend got permission to send %d bytes\n", request);
      while (wroteout < request)
        {
        TestSSL_syncprintf("MySend writing %d bytes\n", request-wroteout);
        rv = lo->methods->send(
               lo, b + wroteout, request - wroteout, flags, timeout);
        if (rv <= 0)
          {
          TestSSL_syncprintf("MySend send error %d\n", rv);
          break;
          }
        TestSSL_syncprintf("MySend wrote %d bytes\n", rv);
        wroteout += rv;
        }
      rv = amount;
      }
    }

  TestSSL_syncprintf(
    "MySend end\n");
  return rv;
}

static void PR_CALLBACK ServerCallback(void *arg)
{
  PRStatus rv;
  PRUint8 buffer[100];
  PRFileDesc *service;
  PRUintn empty_flags = 0;
  PRIntn bytes_read, bytes_sent;
  PRFileDesc *stack = (PRFileDesc*)arg;
  PRNetAddr any_address, client_address;

  TestSSL_syncprintf("Begin ServerCallback\n");

  rv = PR_InitializeNetAddr(PR_IpAddrAny, default_port, &any_address);
  PR_ASSERT(PR_SUCCESS == rv);

  rv = PR_Bind(stack, &any_address); PR_ASSERT(PR_SUCCESS == rv);
  rv = PR_Listen(stack, 10); PR_ASSERT(PR_SUCCESS == rv);

  TestSSL_syncprintf("Server in accept(), waiting for connection\n");
  service = PR_Accept(stack, &client_address, PR_INTERVAL_NO_TIMEOUT);
  TestSSL_syncprintf("Server connection accepted\n");

  do
    {
    TestSSL_syncprintf("Server starting to receive\n");
    bytes_read = PR_Recv(
      service, buffer, sizeof(buffer), empty_flags, PR_INTERVAL_NO_TIMEOUT);
    if (0 != bytes_read)
      {
      TestSSL_syncprintf("Server received %d bytes\n", bytes_read);
      PR_ASSERT(bytes_read > 0);

      TestSSL_syncprintf("Server sending %d bytes\n", bytes_read);
      bytes_sent = PR_Send(
        service, buffer, bytes_read, empty_flags, PR_INTERVAL_NO_TIMEOUT);
      PR_ASSERT(bytes_read == bytes_sent);
      }

    } while (0 != bytes_read);

  TestSSL_syncprintf("Server shutting down and closing stack\n");

  rv = PR_Shutdown(service, PR_SHUTDOWN_BOTH); PR_ASSERT(PR_SUCCESS == rv);
  rv = PR_Close(service); PR_ASSERT(PR_SUCCESS == rv);

  TestSSL_syncprintf("End ServerCallback\n");
  return;
}

static void PR_CALLBACK ClientCallback(void *arg)
{
  PRStatus rv;
  PRUint8 buffer[100];
  PRIntn empty_flags = 0;
  PRIntn bytes_read, bytes_sent;
  PRFileDesc *stack = (PRFileDesc*)arg;

  TestSSL_syncprintf("Begin ClientCallback\n");

  TestSSL_syncprintf("Client connecting to server\n");
  rv = PR_Connect(stack, &server_address, PR_INTERVAL_NO_TIMEOUT);
  PR_ASSERT(PR_SUCCESS == rv);
  TestSSL_syncprintf("Client connected to server\n");

  while (minor_iterations-- > 0)
    {
    TestSSL_syncprintf("Client sending %d bytes\n", sizeof(buffer));
    bytes_sent = PR_Send(
      stack, buffer, sizeof(buffer), empty_flags, PR_INTERVAL_NO_TIMEOUT);
    TestSSL_syncprintf("Client ended up sending %d bytes\n", bytes_sent);

    TestSSL_syncprintf("Client expecting to receive %d bytes\n", bytes_sent);
    PR_ASSERT(sizeof(buffer) == bytes_sent);
    bytes_read = PR_Recv(
      stack, buffer, bytes_sent, empty_flags, PR_INTERVAL_NO_TIMEOUT);
    TestSSL_syncprintf("Client ended up reveiving %d bytes\n", bytes_read);
    PR_ASSERT(bytes_read == bytes_sent);
    }

  TestSSL_syncprintf("Client shutting down stack\n");
   
  rv = PR_Shutdown(stack, PR_SHUTDOWN_BOTH); PR_ASSERT(PR_SUCCESS == rv);

  TestSSL_syncprintf("End ClientCallback\n");

  return;
}

static PRFileDesc *PushLayer(PRFileDesc *stack)
{
  PRFileDesc *layer = PR_CreateIOLayerStub(identity, &myMethods);
  PRStatus rv = PR_PushIOLayer(stack, PR_GetLayersIdentity(stack), layer);
  TestSSL_syncprintf("Pushed layer(0x%x) onto stack(0x%x)\n", layer, stack);
  PR_ASSERT(PR_SUCCESS == rv);

  return stack;
}  /* PushLayer */


static PRFileDesc *PopLayer(PRFileDesc *stack)
{
  PRFileDesc *popped = PR_PopIOLayer(stack, identity);
  TestSSL_syncprintf("Popped layer(0x%x) from stack(0x%x)\n", popped, stack);
  popped->dtor(popped);

  return stack;
}  /* PopLayer */


PRIntn
main(PRIntn argc, char **argv)
{
           PRStatus   rv;
         PRFileDesc  *client, *service;
  const PRIOMethods  *stubMethods;
      PRThreadScope   thread_scope = PR_GLOBAL_THREAD;
           PRThread  *client_thread, *server_thread;

  PR_STDIO_INIT();  /* call this to init NSPR stdin/out/err */

  identity    = PR_GetUniqueIdentity("Dummy protocol");
  stubMethods = PR_GetDefaultIOMethods();
                /* are there any differences between file/socket methods? */

  myMethods      = *stubMethods;  /* first get the entire batch */

  myMethods.recv =  MyRecv;       /* then override the ones we care about */
  myMethods.send =  MySend;       /* then override the ones we care about */

  rv = PR_InitializeNetAddr(PR_IpAddrLoopback, default_port, &server_address);


  {
    TestSSL_syncprintf("Beginning non-layered test\n");
    client  = PR_NewTCPSocket(); PR_ASSERT(NULL != client);
    service = PR_NewTCPSocket(); PR_ASSERT(NULL != service);

    server_thread = PR_CreateThread(
      PR_USER_THREAD, ServerCallback, service,
      PR_PRIORITY_HIGH, thread_scope,
      PR_JOINABLE_THREAD, 16 * 1024);
    PR_ASSERT(NULL != server_thread);

    client_thread = PR_CreateThread(
      PR_USER_THREAD, ClientCallback, client,
      PR_PRIORITY_NORMAL, thread_scope,
      PR_JOINABLE_THREAD, 16 * 1024);
    PR_ASSERT(NULL != client_thread);

    rv = PR_JoinThread(client_thread);
    PR_ASSERT(PR_SUCCESS == rv);
    rv = PR_JoinThread(server_thread);
    PR_ASSERT(PR_SUCCESS == rv);

    rv = PR_Close(client); PR_ASSERT(PR_SUCCESS == rv);
    rv = PR_Close(service); PR_ASSERT(PR_SUCCESS == rv);
    TestSSL_syncprintf("Ending non-layered test\n");
  }

  TestSSL_syncprintf("----\n");

  {
    /* with layering */
    minor_iterations = 5;
    TestSSL_syncprintf("Beginning layered test\n");
    client = PR_NewTCPSocket(); PR_ASSERT(NULL != client);
    service = PR_NewTCPSocket(); PR_ASSERT(NULL != service);

    server_thread = PR_CreateThread(
      PR_USER_THREAD, ServerCallback, PushLayer(service),
      PR_PRIORITY_HIGH, thread_scope,
      PR_JOINABLE_THREAD, 16 * 1024);
    PR_ASSERT(NULL != server_thread);

    client_thread = PR_CreateThread(
      PR_USER_THREAD, ClientCallback, PushLayer(client),
      PR_PRIORITY_NORMAL, thread_scope,
      PR_JOINABLE_THREAD, 16 * 1024);
    PR_ASSERT(NULL != client_thread);

    rv = PR_JoinThread(client_thread);
    PR_ASSERT(PR_SUCCESS == rv);
    rv = PR_JoinThread(server_thread);
    PR_ASSERT(PR_SUCCESS == rv);

    rv = PR_Close(client); PR_ASSERT(PR_SUCCESS == rv);
    rv = PR_Close(service); PR_ASSERT(PR_SUCCESS == rv);
    TestSSL_syncprintf("Ending layered test\n");
  }

  return 0;
}

#endif
