/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* 
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
 * The Original Code is the Netscape Portable Runtime (NSPR).
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1998-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */

/*
** File: ttformat.c
** Description:  ttformat.c reads the file "xxxTTLog". xxxTTLog
** contains fixed length binary data written by nssilock. 
** ttformat formats the data to a human readable form (printf) 
** usable for visual scanning and for processing via a perl script. 
** Output is written to stdout
**
*/

#include <stdio.h>
#include <stdlib.h>
#include <nssilock.h>

/*
** struct maps enum nssILockType to character representation
*/
struct {
    nssILockType ltype;
    char *name;
} ltypeNameT[] = {
    { nssILockArena, "Arena" },
    { nssILockSession, "Session" },
    { nssILockObject, "Object" },
    { nssILockRefLock, "RefLock" },
    { nssILockCert, "Cert", },
    { nssILockCertDB, "CertDB" },
    { nssILockDBM, "DBM" },
    { nssILockCache, "Cache" },
    { nssILockSSL, "SSL" },
    { nssILockList, "List" },
    { nssILockSlot, "Slot" },
    { nssILockFreelist, "Freelist" },
    { nssILockOID, "OID" },
    { nssILockAttribute, "Attribute" },
    { nssILockPK11cxt, "PK11Context" }, 
    { nssILockRWLock, "RWLock" },
    { nssILockOther, "Other" },
    { nssILockSelfServ, "SelfServ" }
}; /* end ltypeNameT */

/*
** struct maps enum nssILockOp to character representation
*/
struct {
   nssILockOp op;
   char *name;
} opNameT[] = {
    { FlushTT, "FlushTT" },
    { NewLock, "NewLock" },
    { Lock, "Lock" },
    { Unlock, "Unlock" },
    { DestroyLock, "DestroyLock" },
    { NewCondVar, "NewCondVar" },
    { WaitCondVar, "WaitCondVar" },
    { NotifyCondVar, "NotifyCondVar" },
    { NotifyAllCondVar, "NotifyAllCondVar" },
    { DestroyCondVar, "DestroyCondVar" },
    { NewMonitor, "NewMonitor" },
    { EnterMonitor, "EnterMonitor" },
    { ExitMonitor, "ExitMonitor" },
    { Notify, "Notify" },
    { NotifyAll, "NotifyAll" },
    { Wait, "Wait" },
    { DestroyMonitor, "DestroyMonitor" }
}; /* end opNameT */


int main(int argc, char *argv[])
{
   FILE	*filea;
   struct pzTrace_s inBuf;
   char   *opName;
   char   *ltypeName;
   int    rCount = 0;
   int    oCount = 0;

   filea = fopen( "xxxTTLog", "r" );
   if ( NULL == filea ) {
       fprintf( stderr, "ttformat: Oh drat! Can't open 'xxxTTLog'\n" );
       exit(1);
   }

   while(1 == (fread( &inBuf, sizeof(inBuf), 1 , filea ))) {
       ++rCount;
       if ( inBuf.op > DestroyMonitor ) continue;
       if ( inBuf.op < FlushTT ) continue;

       opName = opNameT[inBuf.op].name;
       ltypeName = ltypeNameT[inBuf.ltype].name;
       
       ++oCount;
       printf("%8d %18s %18s %6d %6d %12p %6d %20s\n",
          inBuf.threadID, opName, ltypeName, inBuf.callTime, inBuf.heldTime,
          inBuf.lock, inBuf.line, inBuf.file );
   } /* end while() */   
  
   fprintf( stderr, "Read: %d, Wrote: %d\n", rCount, oCount ); 
   return 0;
}  /* main() */
/* end ttformat.c */
