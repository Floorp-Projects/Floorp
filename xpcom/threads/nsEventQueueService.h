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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsEventQueueService_h__
#define nsEventQueueService_h__

#include "nsIEventQueueService.h"
#include "nsHashtable.h"

class nsIEventQueue;

////////////////////////////////////////////////////////////////////////////////

class nsEventQueueServiceImpl : public nsIEventQueueService
{
public:
  nsEventQueueServiceImpl();
  virtual ~nsEventQueueServiceImpl();

  static NS_METHOD
  Create(nsISupports *aOuter, REFNSIID aIID, void **aResult);

  // nsISupports interface...
  NS_DECL_ISUPPORTS

  // nsIEventQueueService interface...
  NS_DECL_NSIEVENTQUEUESERVICE

private:
             /* Create a queue for the given thread if one does not exist.
                Addref the descriptor in any case. parameter aNative is
                ignored if the queue already exists. */
  NS_IMETHOD CreateEventQueue(PRThread *aThread, PRBool aNative);
  NS_IMETHOD MakeNewQueue(PRThread* thread, PRBool aNative, nsIEventQueue **aQueue);
  inline nsresult GetYoungestEventQueue(nsIEventQueue *queue, nsIEventQueue **aResult);

  nsSupportsHashtable mEventQTable;
  PRMonitor           *mEventQMonitor;
};

////////////////////////////////////////////////////////////////////////////////

#endif // nsEventQueueService_h__
