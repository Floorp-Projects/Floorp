/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
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

#ifndef nsCommandServer_h___
#define nsCommandServer_h___

#include "nsICommandServer.h"
#include "nspr.h"

#define TCP_MESG_SIZE                     1024
#define TCP_SERVER_PORT                   666
#define NUM_TCP_CLIENTS                   10
#define NUM_TCP_CONNECTIONS_PER_CLIENT    5
#define NUM_TCP_MESGS_PER_CONNECTION      10
#define SERVER_MAX_BIND_COUNT             100

typedef struct buffer {
    char    data[TCP_MESG_SIZE * 2];
} buffer;

class nsCommandServer : public nsICommandServer
{

public:
  nsCommandServer();

  NS_DECL_ISUPPORTS

  NS_IMETHOD                 Init(nsIApplicationShell * aApplicationShell);

protected:
  ~nsCommandServer();

public:
  NS_IMETHOD    RunThread();
  NS_IMETHOD    ExitThread();

private:
  nsIApplicationShell * mApplicationShell;
  PRMonitor *   mExitMon;       /* monitor to signal on exit            */
  PRInt32 *     mExitCounter;   /* counter to decrement, before exit        */
  PRInt32       mDatalen;       /* bytes of data transfered in each read/write    */
  PRInt32       mNumThreads;
  PRMonitor *   mServerMon;
  PRNetAddr     mServerAddr;
};

#endif /* nsCommandServer_h___ */
