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

#include "prinit.h"
#include "prstrms.h"
#include "prio.h"
#include <string.h>
#include <stdio.h>
#if defined(XP_UNIX) || defined(XP_OS2_EMX)
#include <sys/types.h>
#include <sys/stat.h>
#endif

const unsigned int MaxCnt = 1;

void threadwork(void *mytag);


typedef struct threadarg {
    void *mytag;
} threadarg;

void 
#ifdef XP_OS2_VACPP
_Optlink
#endif
threadmain(void *mytag)
{
    threadarg arg;

    arg.mytag = mytag;

    threadwork(&arg);
}


void
threadwork(void *_arg)
{
	threadarg *arg = (threadarg *)_arg;
	unsigned int i;

	char fname1[256];
	char fname2[256];

	strcpy(fname1, (char *)arg->mytag);
	strcpy(fname2, (char *)arg->mytag);
	strcat(fname2, "2");
	PR_Delete(fname1);
	PR_Delete(fname2);

	PRfilebuf *fb[MaxCnt];
	PRifstream *ifs[MaxCnt];
	PRofstream *ofs[MaxCnt];
	int mode = 0;
#ifdef XP_UNIX
	mode = S_IRUSR|S_IWUSR|S_IRGRP|S_IRGRP|S_IWOTH|S_IROTH;
#endif

	//
	// Allocate a bunch
	cout << "Testing unused filebufs ----------------" << endl;
	for (i=0; i < MaxCnt; i++){
		fb[i] = new PRfilebuf;
	}
	// Delete them
	for (i=0; i < MaxCnt; i++){
		delete fb[i];
	}
	cout << "Unused filebufs complete ---------------" << endl;

	//
	// Allocate a bunch
	cout << "Testing unused ifstream -----------------" << endl;
	for (i=0; i < MaxCnt; i++){
	  ifs[i] = new PRifstream;
	}
	//
	// Delete them
	for (i=0; i < MaxCnt; i++){
	  delete ifs[i];
	}
	cout << "Unused ifstream complete ----------------" << endl;
	//
	// Allocate a bunch
	cout << "Testing unused ofstream -----------------" << endl;
	for (i=0; i < MaxCnt; i++){
		ofs[i] = new PRofstream;
	}
	for (i=0; i < MaxCnt; i++){
	  *(ofs[i]) << "A"; // Write a bit
	  delete ofs[i]; // Delete it.
	}
	cout << "Unused ofstream complete ----------------" << endl;

	cout << "Testing use of ofstream 1 (extra filebuf allocated) ---------" << endl;
	PRofstream *aos = new PRofstream(fname1, ios::out|ios::ate, mode);
	for (i=0; i < MaxCnt; i++){
	  for (int j=0; j < 8192; j++)
		*aos << "AaBbCcDdEeFfGg" << endl;
		fb[i] = new PRfilebuf; // Allocate as we go to hack at the heap
	}
	//
	// Delete the extra foo we allocated
	for (i=0; i < MaxCnt; i++){
	  delete fb[i];
	}
	aos->flush(); // Explicit flush
	delete aos;
	cout << "Testing use of ofstream 1 complete (extra filebuf deleted) --" << endl;
	cout << "Testing use of ofstream 2 (extra filebuf allocated) ---------" << endl;
	PRofstream *aos2 = new PRofstream(fname2, ios::out, mode);

	for (i=0; i < MaxCnt; i++){
	    *aos2 << "AaBbCcDdEeFfGgHhIiJjKkLlMmNnOoPpQqRrSsTtUuVvWwXxYyZz";
	}
	// Force flushing in the dtor
	delete aos2;
	cout << "Testing use of ofstream 2 complete (extra filebuf deleted) --" << endl;
	char line[1024];
	cout << "Testing use of ifstream 1 (stack allocation) -------------" << endl;
	PRifstream ais(fname1);
	for (i=0; i < MaxCnt; i++){
		ais >> line;
	}
	cout << "Testing use of ifstream 1 complete -----------------------" << endl;
	cout << "Testing use of ifstream 2 ----------------------" << endl;
	PRifstream *ais2 = new PRifstream(fname2);
	char achar;
	for (i=0; i < MaxCnt*10; i++){
		*ais2 >> achar;
	}
	delete ais2;
	cout << "Testing use of ifstream 2 complete -------------" << endl;
}

#define STACKSIZE 1024*1024
int
main(int argc, char **argv)
{
	PR_Init(PR_SYSTEM_THREAD, PR_PRIORITY_NORMAL, 256);
	threadmain("TestFile");
	PRThread *thr1 = PR_CreateThread(PR_SYSTEM_THREAD, 
					 threadmain, 
					 (void *)"TestFile1",
					 PR_PRIORITY_NORMAL,
					 PR_GLOBAL_THREAD,
					 PR_JOINABLE_THREAD,
					 STACKSIZE);
	PRThread *thr2 = PR_CreateThread(PR_SYSTEM_THREAD, 
					 threadmain, 
					 (void *)"TestFile2",
					 PR_PRIORITY_NORMAL,
					 PR_GLOBAL_THREAD,
					 PR_JOINABLE_THREAD,
					 STACKSIZE);

	PRThread *thr3 = PR_CreateThread(PR_SYSTEM_THREAD, 
					 threadmain, 
					 (void *)"TestFile3",
					 PR_PRIORITY_NORMAL,
					 PR_GLOBAL_THREAD,
					 PR_JOINABLE_THREAD,
					 STACKSIZE);
	PR_JoinThread(thr1);
	PR_JoinThread(thr2);
	PR_JoinThread(thr3);
	return 0;
}

