/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "NPL"); you may not use this file except in
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

/*
Attached is a test program that uses the nspr1 to demonstrate a bug
under NT4.0. The fix has already been mentioned (add a ResetEvent just
before leaving the critical section in _PR_CondWait in hwmon.c).
*/

#include "prthread.h"
#include "prtypes.h"
#include "prinit.h"
#include "prmon.h"
#include "prlog.h"

typedef struct Arg_s
{
	PRInt32 a, b;
} Arg_t;

PRMonitor*  gMonitor;       // the monitor
PRInt32     gReading;       // number of read locks
PRInt32     gWriteWaiting;  // number of threads waiting for write lock
PRInt32     gReadWaiting;   // number of threads waiting for read lock

PRInt32     gCounter;       // a counter

                            // stats
PRInt32     gReads;         // number of successful reads
PRInt32     gMaxReads;      // max number of simultaneous reads
PRInt32     gMaxWriteWaits; // max number of writes that waited for read
PRInt32     gMaxReadWaits;  // max number of reads that waited for write wait


void spin (PRInt32 aDelay)
{
  PRInt32 index;
  PRInt32 delay = aDelay * 1000;

  PR_Sleep(0);

  // randomize delay a bit
  delay = (delay / 2) + (PRInt32)((float)delay *
	  ((float)rand () / (float)RAND_MAX));

  for (index = 0; index < delay * 10; index++)
	  // consume a bunch of cpu cycles
    ;
  PR_Sleep(0); 
}

void  doWriteThread (void* arg)
{
  PRInt32 last;
  Arg_t *args = (Arg_t*)arg;
  PRInt32 aWorkDelay = args->a, aWaitDelay = args->b;
  PR_Sleep(0);

  while (1)
  {
    // -- enter write lock
    PR_EnterMonitor (gMonitor);

    if (0 < gReading)     // wait for read locks to go away
    {
      PRIntervalTime fiveSecs = PR_SecondsToInterval(5);

      gWriteWaiting++;
      if (gWriteWaiting > gMaxWriteWaits) // stats
        gMaxWriteWaits = gWriteWaiting;
      while (0 < gReading)
        PR_Wait (gMonitor, fiveSecs);
      gWriteWaiting--;
    }
    // -- write lock entered

    last = gCounter;
    gCounter++;

    spin (aWorkDelay);

    PR_ASSERT (gCounter == (last + 1)); // test invariance

    // -- exit write lock        
//    if (0 < gReadWaiting)   // notify waiting reads (do it anyway to show off the CondWait bug)
      PR_NotifyAll (gMonitor);

    PR_ExitMonitor (gMonitor);
    // -- write lock exited

    spin (aWaitDelay);
  }
}

void  doReadThread (void* arg)
{
  PRInt32 last;
  Arg_t *args = (Arg_t*)arg;
  PRInt32 aWorkDelay = args->a, aWaitDelay = args->b;
  PR_Sleep(0);

  while (1)
  {
    // -- enter read lock
    PR_EnterMonitor (gMonitor); 

    if (0 < gWriteWaiting)  // give up the monitor to waiting writes
    {
      PRIntervalTime fiveSecs = PR_SecondsToInterval(5);

      gReadWaiting++;
      if (gReadWaiting > gMaxReadWaits) // stats
        gMaxReadWaits = gReadWaiting;
      while (0 < gWriteWaiting)
        PR_Wait (gMonitor, fiveSecs);
      gReadWaiting--;
    }

    gReading++;

    gReads++;   // stats
    if (gReading > gMaxReads) // stats
      gMaxReads = gReading;

    PR_ExitMonitor (gMonitor);
    // -- read lock entered

    last = gCounter;

    spin (aWorkDelay);

    PR_ASSERT (gCounter == last); // test invariance

    // -- exit read lock
    PR_EnterMonitor (gMonitor);  // read unlock
    gReading--;

//    if ((0 == gReading) && (0 < gWriteWaiting))  // notify waiting writes  (do it anyway to show off the CondWait bug)
      PR_NotifyAll (gMonitor);
    PR_ExitMonitor (gMonitor);
    // -- read lock exited

    spin (aWaitDelay);
  }
}


void fireThread (
    char* aName, void (*aProc)(void *arg), Arg_t *aArg)
{
  PRThread *thread = PR_CreateThread(
	  PR_USER_THREAD, aProc, aArg, PR_PRIORITY_NORMAL,
	  PR_LOCAL_THREAD, PR_UNJOINABLE_THREAD, 0);
}

int pseudoMain (int argc, char** argv, char *pad)
{
  PRInt32 lastWriteCount  = gCounter;
  PRInt32 lastReadCount   = gReads;
  Arg_t a1 = {500, 250};
  Arg_t a2 = {500, 500};
  Arg_t a3 = {250, 500};
  Arg_t a4 = {750, 250};
  Arg_t a5 = {100, 750};
  Arg_t a6 = {100, 500};
  Arg_t a7 = {100, 750};

  gMonitor = PR_NewMonitor ();

  fireThread ("R1", doReadThread,   &a1);
  fireThread ("R2", doReadThread,   &a2);
  fireThread ("R3", doReadThread,   &a3);
  fireThread ("R4", doReadThread,   &a4);

  fireThread ("W1", doWriteThread,  &a5);
  fireThread ("W2", doWriteThread,  &a6);
  fireThread ("W3", doWriteThread,  &a7);

  fireThread ("R5", doReadThread,   &a1);
  fireThread ("R6", doReadThread,   &a2);
  fireThread ("R7", doReadThread,   &a3);
  fireThread ("R8", doReadThread,   &a4);

  fireThread ("W4", doWriteThread,  &a5);
  fireThread ("W5", doWriteThread,  &a6);
  fireThread ("W6", doWriteThread,  &a7);
  
  while (1)
  {
	PRInt32 writeCount, readCount;
    PRIntervalTime fiveSecs = PR_SecondsToInterval(5);
    PR_Sleep (fiveSecs);  // get out of the way

    // print some stats, not threadsafe, informative only
    writeCount = gCounter;
    readCount   = gReads;
    printf ("\ntick %d writes (+%d), %d reads (+%d) [max %d, %d, %d]", 
            writeCount, writeCount - lastWriteCount,
            readCount, readCount - lastReadCount, 
            gMaxReads, gMaxWriteWaits, gMaxReadWaits);
    lastWriteCount = writeCount;
    lastReadCount = readCount;
    gMaxReads = gMaxWriteWaits = gMaxReadWaits = 0;
  }
  return 0;
}


static void padStack (int argc, char** argv)
{
  char pad[512];      /* Work around bug in nspr on windoze */
  pseudoMain (argc, argv, pad);
}

void main (int argc, char **argv)
{
  PR_Init(PR_USER_THREAD, PR_PRIORITY_NORMAL, 0);
  PR_STDIO_INIT();
  padStack (argc, argv);
}


/* bug1test.c */
