/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nsCommandServer.h"
#include "nsxpfcCIID.h"
#include "nsxpfcutil.h"
#include "nsCRT.h"
#include "nsString.h"
#include "nsxpfcstrings.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kCCommandServerCID, NS_XPFC_COMMAND_SERVER_CID);
static NS_DEFINE_IID(kICommandServerIID, NS_IXPFC_COMMAND_SERVER_IID);

static void PR_CALLBACK CommandServerThread(void * arg);
static void PR_CALLBACK CommandServerClientThread(void * arg);

nsIApplicationShell * gApplicationShell = nsnull;

nsCommandServer :: nsCommandServer()
{
  NS_INIT_REFCNT();
  mApplicationShell = nsnull;
  mExitMon = nsnull ;
  mExitCounter = nsnull; 
  mDatalen = 0;
  mNumThreads = 0;
  mServerMon = nsnull;
  nsCRT::memset(&mServerAddr, 0 , sizeof(mServerAddr));
  gApplicationShell = mApplicationShell;
}

nsCommandServer :: ~nsCommandServer()
{
  ExitThread();
  NS_IF_RELEASE(mApplicationShell);
}

NS_IMPL_QUERY_INTERFACE(nsCommandServer, kICommandServerIID)
NS_IMPL_ADDREF(nsCommandServer)
NS_IMPL_RELEASE(nsCommandServer)

nsresult nsCommandServer :: Init(nsIApplicationShell * aApplicationShell)
{
  PRThread * t = nsnull ;

  mApplicationShell = aApplicationShell;
  gApplicationShell = mApplicationShell;
  NS_ADDREF(mApplicationShell);

  /*
   * Let's launch a server on a separate thread here....
   */

  mServerMon = PR_NewMonitor();
  PR_EnterMonitor(mServerMon);

  mExitMon = mServerMon;
  mExitCounter = &mNumThreads;

  t = PR_CreateThread(PR_USER_THREAD,
                      CommandServerThread, 
                      (void *)this, 
                      PR_PRIORITY_NORMAL,
                      PR_LOCAL_THREAD,
                      PR_UNJOINABLE_THREAD,
                      0);
  mNumThreads++;

  /*
   * Note, this is an indefinite wait.  This is probably bad
   * since we are executing on the application thread still!
   * Theoretically, it should happen relatively fast.
   */

  PR_Wait(mServerMon,PR_INTERVAL_NO_TIMEOUT);

  PR_ExitMonitor(mServerMon);

  return NS_OK ;
}


/*
 * Note:  This routine runs on it's own thread
 */

nsresult nsCommandServer :: RunThread()
{
  PRFileDesc * sockfd = nsnull;
  PRNetAddr netaddr;
  PRInt32 i = 0;
  PRFileDesc *newsockfd;    
  PRThread *t;

  sockfd = PR_NewTCPSocket();

  if (sockfd == nsnull)
    return NS_OK;

  nsCRT::memset(&netaddr, 0 , sizeof(netaddr));

  netaddr.inet.family = PR_AF_INET;
  netaddr.inet.port   = PR_htons(TCP_SERVER_PORT);
  netaddr.inet.ip     = PR_htonl(PR_INADDR_ANY);

  while (PR_Bind(sockfd, &netaddr) < 0) 
  {
    if (PR_GetError() == PR_ADDRESS_IN_USE_ERROR) 
    {
      netaddr.inet.port += 2;
      if (i++ < SERVER_MAX_BIND_COUNT)
        continue;
    }
    PR_Close(sockfd);
    return NS_OK;
  }

  if (PR_Listen(sockfd, 32) < 0) 
  {
    PR_Close(sockfd);
    return NS_OK;
  }

  if (PR_GetSockName(sockfd, &netaddr) < 0) 
  {
    PR_Close(sockfd);
    return NS_OK;
  }

  mServerAddr.inet.family = netaddr.inet.family;
  mServerAddr.inet.port   = netaddr.inet.port;
  mServerAddr.inet.ip     = netaddr.inet.ip;


  /*
   * Wake up the parent thread now.
   */

  PR_EnterMonitor(mServerMon);
  PR_Notify(mServerMon);
  PR_ExitMonitor(mServerMon);


  /*
   * Each time a client connects, spawn yet another thread to deal with 
   * the communication
   */

  while(mNumThreads)//for (i = 0; i < (NUM_TCP_CLIENTS * NUM_TCP_CONNECTIONS_PER_CLIENT); i++) 
  {   
    newsockfd = PR_Accept(sockfd, &netaddr, PR_INTERVAL_NO_TIMEOUT);

    if (newsockfd == nsnull)
    {
      PR_Close(sockfd);
      return NS_OK;
    }

    t = PR_CreateThread(PR_USER_THREAD,
                        CommandServerClientThread, 
                        (void *)newsockfd, 
                        PR_PRIORITY_NORMAL,
                        PR_LOCAL_THREAD,
                        PR_UNJOINABLE_THREAD,
                        0);
  }


  
  PR_Close(sockfd);
  return NS_OK;

}

static void PR_CALLBACK CommandServerThread(void * arg)
{
  nsCommandServer * command_server = (nsCommandServer *) arg;

  command_server->RunThread();

#if 0
  command_server->ExitThread();
#endif

}

static void PR_CALLBACK CommandServerClientThread(void * arg)
{
  PRFileDesc * sockfd = (PRFileDesc *) arg;    

  buffer * in_buf;
  PRInt32 bytes = TCP_MESG_SIZE;
  PRInt32 bytes2 ;
  PRInt32 j;

  in_buf = PR_NEW(buffer);

  if (in_buf != nsnull)
  {
    bytes2 = PR_Recv(sockfd, in_buf->data, bytes, 0, PR_INTERVAL_NO_TIMEOUT);

    if (bytes2 > 0)
    {
      nsString string = "BOBO";

      string.SetString(in_buf->data, nsCRT::strlen(in_buf->data));

      // XXX:  We really need an interface for receiving string results
      //       for CommandInvoker queries.  Right now, we can dispatch 
      //       commands (ie SetBackgroundColor) but we have no useful
      //       way of receiving the results of a query (ie GetBackgroundColor)
      //       
      //       I believe we really need to update the nsIXPFCObserver interface
      //       to allow yet a third string to be passed in which is the response
      //       results.....
      //
      //       I believe sman had an idea on how to do these things, check my notes...
      //
#if 0
      if (string.EqualsIgnoreCase(XPFC_STRING_HELP) || string.EqualsIgnoreCase(XPFC_STRING_QUESTIONMARK))
      {

        nsCRT::memset(in_buf->data, '\0', bytes);

        nsCRT::memcpy(in_buf->data, "Help Not Yet Implemented ... Sorry Steve!!!\n", bytes);

      } else {
#endif
        nsString reply("Command Received and Executed!!!\n");

        gApplicationShell->ReceiveCommand(string, reply);

        nsCRT::memset(in_buf->data, '\0', bytes);

        if (reply.Length() == 0)
          reply = "Command Received and Executed!!!\n";

        char * cstring = reply.ToNewCString();

        nsCRT::memcpy(in_buf->data, cstring, reply.Length());

        delete cstring;
#if 0

      }
#endif

      

      bytes2 = PR_Send(sockfd, in_buf->data, bytes, 0, PR_INTERVAL_NO_TIMEOUT);

      if (bytes2 <=0)
        j = 0;
    }

    PR_Shutdown(sockfd, PR_SHUTDOWN_BOTH);

  }

  PR_Close(sockfd);

  if (in_buf) {
      PR_DELETE(in_buf);
  }

}


nsresult nsCommandServer :: ExitThread()
{
  PR_EnterMonitor(mExitMon);
  --(*mExitCounter);
  PR_Notify(mExitMon);
  PR_ExitMonitor(mExitMon);
  mNumThreads--;
  return NS_OK;
}

