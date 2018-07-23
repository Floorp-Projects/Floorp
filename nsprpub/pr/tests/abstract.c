/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>

#if defined(LINUX)

#include <string.h>
#include "nspr.h"

static const char abstractSocketName[] = "\0testsocket";

static void
ClientThread(void* aArg)
{
  PRFileDesc* socket;
  PRNetAddr addr;
  PRUint8 buf[1024];
  PRInt32 len;
  PRInt32 total;

  addr.local.family = PR_AF_LOCAL;
  memcpy(addr.local.path, abstractSocketName, sizeof(abstractSocketName));

  socket = PR_OpenTCPSocket(addr.raw.family);
  if (!socket) {
    fprintf(stderr, "PR_OpenTCPSokcet failed\n");
    exit(1);
  }

  if (PR_Connect(socket, &addr, PR_INTERVAL_NO_TIMEOUT) == PR_FAILURE) {
    fprintf(stderr, "PR_Connect failed\n");
    exit(1);
  }

  total = 0;
  while (total < sizeof(buf)) {
    len = PR_Recv(socket, buf + total, sizeof(buf) - total, 0,
                  PR_INTERVAL_NO_TIMEOUT);
    if (len < 1) {
      fprintf(stderr, "PR_Recv failed\n");
      exit(1);
    }
    total += len;
  }

  total = 0;
  while (total < sizeof(buf)) {
    len = PR_Send(socket, buf + total, sizeof(buf) - total, 0,
                  PR_INTERVAL_NO_TIMEOUT);
    if (len < 1) {
      fprintf(stderr, "PR_Send failed\n");
      exit(1);
    }
    total += len;
  }

  if (PR_Close(socket) == PR_FAILURE) {
    fprintf(stderr, "PR_Close failed\n");
    exit(1);
  }
}

int
main()
{
  PRFileDesc* socket;
  PRFileDesc* acceptSocket;
  PRThread* thread;
  PRNetAddr addr;
  PRUint8 buf[1024];
  PRInt32 len;
  PRInt32 total;

  addr.local.family = PR_AF_LOCAL;
  memcpy(addr.local.path, abstractSocketName, sizeof(abstractSocketName));

  socket = PR_OpenTCPSocket(addr.raw.family);
  if (!socket) {
    fprintf(stderr, "PR_OpenTCPSocket failed\n");
    exit(1);
  }
  if (PR_Bind(socket, &addr) == PR_FAILURE) {
    fprintf(stderr, "PR_Bind failed\n");
    exit(1);
  }

  if (PR_Listen(socket, 5) == PR_FAILURE) {
    fprintf(stderr, "PR_Listen failed\n");
    exit(1);
  }

  thread = PR_CreateThread(PR_USER_THREAD, ClientThread, 0, PR_PRIORITY_NORMAL,
                           PR_GLOBAL_THREAD, PR_JOINABLE_THREAD, 0);
  if (!thread) {
    fprintf(stderr, "PR_CreateThread failed");
    exit(1);
  }

  acceptSocket  = PR_Accept(socket, NULL, PR_INTERVAL_NO_TIMEOUT);
  if (!acceptSocket) {
    fprintf(stderr, "PR_Accept failed\n");
    exit(1);
  }

  memset(buf, 'A', sizeof(buf));

  total = 0;
  while (total < sizeof(buf)) {
    len = PR_Send(acceptSocket, buf + total, sizeof(buf) - total, 0,
                  PR_INTERVAL_NO_TIMEOUT);
    if (len < 1) {
      fprintf(stderr, "PR_Send failed\n");
      exit(1);
    }
    total += len;
  }

  total = 0;
  while (total < sizeof(buf)) {
    len = PR_Recv(acceptSocket, buf + total, sizeof(buf) - total, 0,
                  PR_INTERVAL_NO_TIMEOUT);
    if (len < 1) {
      fprintf(stderr, "PR_Recv failed\n");
      exit(1);
    }
    total += len;
  }

  if (PR_Close(acceptSocket) == PR_FAILURE) {
    fprintf(stderr, "PR_Close failed\n");
    exit(1);
  }

  if (PR_JoinThread(thread) == PR_FAILURE) {
    fprintf(stderr, "PR_JoinThread failed\n");
    exit(1);
  }

  if (PR_Close(socket) == PR_FAILURE) {
    fprintf(stderr, "PR_Close failed\n");
    exit(1);
  }
  printf("PASS\n");
  return 0;
}

#else
int
main()
{
  prinf("PASS\n");
  return 0;
}
#endif
