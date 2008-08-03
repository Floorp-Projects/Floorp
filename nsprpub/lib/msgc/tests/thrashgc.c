/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Netscape Portable Runtime (NSPR).
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/***********************************************************************
** Name: thrashgc
**
** Description: test garbace collection functions.
**
** Modification History:
** 19-May-97 AGarcia- Converted the test to accomodate the debug_mode flag.
**	         The debug mode will print all of the printfs associated with this test.
**			 The regress mode will be the default mode. Since the regress tool limits
**           the output to a one line status:PASS or FAIL,all of the printf statements
**			 have been handled with an if (debug_mode) statement.
** 04-June-97 AGarcia removed the Test_Result function. Regress tool has been updated to
**			recognize the return code from tha main program.
***********************************************************************/
/***********************************************************************
** Includes
***********************************************************************/
#include "prthread.h"
#include "prgc.h"
#include "prprf.h"
#include "prinrval.h"
#include "prlock.h"
#include "prinit.h"
#include "prcvar.h"

#ifndef XP_MAC
#include "private/pprthred.h"
#else
#include "pprthred.h"
#endif

#include <stdio.h>
#include <memory.h>
#include <string.h>


#ifdef XP_MAC
#include "prlog.h"
#define printf PR_LogPrint
extern void SetupMacPrintfLog(char *logFile);
#endif

PRIntn failed_already=0;
PRIntn debug_mode;

static char* progname;
static PRInt32 loops = 1000;
static int tix1, tix2, tix3;
static GCInfo* gcInfo;
static PRLock* stderrLock;

typedef struct Type1 Type1;
typedef struct Type2 Type2;

struct Type1 {
  Type2* atwo;
  Type1* next;
};

struct Type2 {
  void* buf;
};

static void PR_CALLBACK ScanType1(void *obj) {
  gcInfo->livePointer(((Type1 *)obj)->atwo);
  gcInfo->livePointer(((Type1 *)obj)->next);
}

static void PR_CALLBACK ScanType2(void *obj) {
  gcInfo->livePointer(((Type2 *)obj)->buf);
}

static GCType type1 = {
    ScanType1
};

static GCType type2 = {
    ScanType2
/*  (void (*)(void*)) ScanType2 */
};

static GCType type3 = {
  0
};

Type1* NewType1(void) {
    Type1* p = (Type1*) PR_AllocMemory(sizeof(Type1), tix1, PR_ALLOC_DOUBLE);
    PR_ASSERT(p != NULL);
    return p;
}

Type2* NewType2(void) {
    Type2* p = (Type2*) PR_AllocMemory(sizeof(Type2), tix2, PR_ALLOC_DOUBLE);
    PR_ASSERT(p != NULL);
    return p;
}

void* NewBuffer(PRInt32 size) {
    void* p = PR_AllocMemory(size, tix3, PR_ALLOC_DOUBLE);
    PR_ASSERT(p != NULL);
    return p;
}

/* Allocate alot of garbage */
static void PR_CALLBACK AllocStuff(void *unused) {
  PRInt32 i;
  void* danglingRefs[50];
  PRIntervalTime start, end;
  char msg[100];

  start = PR_IntervalNow();
  for (i = 0; i < loops; i++) {
    void* p;
    if (i & 1) {
      Type1* t1 = NewType1();
      t1->atwo = NewType2();
      t1->next = NewType1();
      t1->atwo->buf = NewBuffer(100);
      p = t1;
    } else {
      Type2* t2 = NewType2();
      t2->buf = NewBuffer(i & 16383);
      p = t2;
    }
    if ((i % 10) == 0) {
      memmove(&danglingRefs[0], &danglingRefs[1], 49*sizeof(void*));
      danglingRefs[49] = p;
    }
  }
  end = PR_IntervalNow();
  if (debug_mode) PR_snprintf(msg, sizeof(msg), "Thread %p: %ld allocations took %ld ms",
	      PR_GetCurrentThread(), loops,
	      PR_IntervalToMilliseconds((PRIntervalTime) (end - start)));
  PR_Lock(stderrLock);
#ifndef XP_MAC
  fprintf(stderr, "%s\n", msg);
#else
  if (debug_mode) printf("%s\n", msg);
#endif
  PR_Unlock(stderrLock);
  }

static void usage(char *progname) {
#ifndef XP_MAC
  fprintf(stderr, "Usage: %s [-t threads] [-l loops]\n", progname);
#else
  printf("Usage: %s [-t threads] [-l loops]\n", progname);
#endif
  exit(-1);
}

static int realMain(int argc, char **argv, char *notused) {
  int i;
  int threads = 0;

#ifndef XP_MAC
  progname = strrchr(argv[0], '/');
  if (progname == 0) progname = argv[0];
  for (i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-t") == 0) {
      if (i == argc - 1) {
	usage(progname);
      }
      threads = atoi(argv[++i]);
      if (threads < 0) threads = 0;
      if (threads > 10000) threads = 10000;
      continue;
    }
    if (strcmp(argv[i], "-l") == 0) {
      if (i == argc - 1) {
	usage(progname);
      }
      loops = atoi(argv[++i]);
      continue;
    }
    usage(progname);
  }
#else
	threads = 50;
#endif

  for (i = 0; i < threads; i++) {
    PRThread* thread;

    /* XXXXX */
    thread = PR_CreateThreadGCAble(PR_USER_THREAD,  /* thread type */
			     AllocStuff,  /* start function */
			     NULL,  /* arg */
			     PR_PRIORITY_NORMAL,  /* priority */
			     PR_LOCAL_THREAD,  /* thread scope */
			     PR_UNJOINABLE_THREAD,  /* thread state */
			     0);   /* stack size */
    if (thread == 0) {
#ifndef XP_MAC
      fprintf(stderr, "%s: no more threads (only %d were created)\n",
	      progname, i);
#else
      printf("%s: no more threads (only %d were created)\n",
	      progname, i);
#endif
      break;
    }
  }
  AllocStuff(NULL);
  return 0;
}

static int padMain(int argc, char **argv) {
  char pad[512];
  return realMain(argc, argv, pad);
}

int main(int argc, char **argv) {
  int rv;

  debug_mode = 1;
  
  PR_Init(PR_USER_THREAD, PR_PRIORITY_NORMAL, 0);
  PR_SetThreadGCAble();

#ifdef XP_MAC
  SetupMacPrintfLog("thrashgc.log");
  debug_mode = 1;
#endif

  PR_InitGC(0, 0, 0, PR_GLOBAL_THREAD);
  PR_STDIO_INIT();
  stderrLock = PR_NewLock();
  tix1 = PR_RegisterType(&type1);
  tix2 = PR_RegisterType(&type2);
  tix3 = PR_RegisterType(&type3);
  gcInfo = PR_GetGCInfo();
  rv = padMain(argc, argv);
  printf("PASS\n");
  PR_Cleanup();
  return rv;
}
