/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "prstrms.h"

#include "prinit.h"
#include "prio.h"
#include "prthread.h"

#include <cstring>
#include <iostream>

#ifdef XP_UNIX
#include <sys/stat.h>
#endif

using std::cout;
using std::endl;
using std::ios;

const unsigned int MaxCnt = 1;

typedef struct threadarg {
    const char *mytag;
} threadarg;

void threadwork(threadarg *arg);

void
threadmain(void *mytag)
{
    threadarg arg;

    arg.mytag = static_cast<const char *>(mytag);

    threadwork(&arg);
}

void
threadwork(threadarg *arg)
{
    unsigned int i;

    char fname1[256];
    char fname2[256];

    strcpy(fname1, arg->mytag);
    strcpy(fname2, arg->mytag);
    strcat(fname2, "2");
    PR_Delete(fname1);
    PR_Delete(fname2);

    PRfilebuf *fb[MaxCnt];
    PRifstream *ifs[MaxCnt];
    PRofstream *ofs[MaxCnt];
    int mode = 0;
#ifdef XP_UNIX
    mode = S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH;
#endif

    //
    // Allocate a bunch
    cout << "Testing unused filebufs ----------------" << endl;
    for (i=0; i < MaxCnt; i++) {
        fb[i] = new PRfilebuf;
    }
    // Delete them
    for (i=0; i < MaxCnt; i++) {
        delete fb[i];
    }
    cout << "Unused filebufs complete ---------------" << endl;

    //
    // Allocate a bunch
    cout << "Testing unused ifstream -----------------" << endl;
    for (i=0; i < MaxCnt; i++) {
        ifs[i] = new PRifstream;
    }
    //
    // Delete them
    for (i=0; i < MaxCnt; i++) {
        delete ifs[i];
    }
    cout << "Unused ifstream complete ----------------" << endl;
    //
    // Allocate a bunch
    cout << "Testing unused ofstream -----------------" << endl;
    for (i=0; i < MaxCnt; i++) {
        ofs[i] = new PRofstream;
    }
    for (i=0; i < MaxCnt; i++) {
        *(ofs[i]) << "A"; // Write a bit
        delete ofs[i]; // Delete it.
    }
    cout << "Unused ofstream complete ----------------" << endl;

    cout << "Testing use of ofstream 1 (extra filebuf allocated) ---------" << endl;
    PRofstream *aos = new PRofstream(fname1, ios::out|ios::ate, mode);
    for (i=0; i < MaxCnt; i++) {
        for (int j=0; j < 8192; j++) {
            *aos << "AaBbCcDdEeFfGg" << endl;
        }
        fb[i] = new PRfilebuf; // Allocate as we go to hack at the heap
    }
    //
    // Delete the extra foo we allocated
    for (i=0; i < MaxCnt; i++) {
        delete fb[i];
    }
    aos->flush(); // Explicit flush
    delete aos;
    cout << "Testing use of ofstream 1 complete (extra filebuf deleted) --" << endl;
    cout << "Testing use of ofstream 2 (extra filebuf allocated) ---------" << endl;
    PRofstream *aos2 = new PRofstream(fname2, ios::out, mode);

    for (i=0; i < MaxCnt; i++) {
        *aos2 << "AaBbCcDdEeFfGgHhIiJjKkLlMmNnOoPpQqRrSsTtUuVvWwXxYyZz";
    }
    // Force flushing in the dtor
    delete aos2;
    cout << "Testing use of ofstream 2 complete (extra filebuf deleted) --" << endl;
    char line[1024];
    cout << "Testing use of ifstream 1 (stack allocation) -------------" << endl;
    PRifstream ais(fname1);
    for (i=0; i < MaxCnt; i++) {
        ais >> line;
    }
    cout << "Testing use of ifstream 1 complete -----------------------" << endl;
    cout << "Testing use of ifstream 2 ----------------------" << endl;
    PRifstream *ais2 = new PRifstream(fname2);
    char achar;
    for (i=0; i < MaxCnt*10; i++) {
        *ais2 >> achar;
    }
    delete ais2;
    cout << "Testing use of ifstream 2 complete -------------" << endl;
}

#define STACKSIZE 1024*1024
int
main()
{
    PR_Init(PR_SYSTEM_THREAD, PR_PRIORITY_NORMAL, 256);
    threadmain(const_cast<char *>("TestFile"));
    PRThread *thr1 = PR_CreateThread(PR_SYSTEM_THREAD,
                                     threadmain,
                                     const_cast<char *>("TestFile1"),
                                     PR_PRIORITY_NORMAL,
                                     PR_GLOBAL_THREAD,
                                     PR_JOINABLE_THREAD,
                                     STACKSIZE);
    PRThread *thr2 = PR_CreateThread(PR_SYSTEM_THREAD,
                                     threadmain,
                                     const_cast<char *>("TestFile2"),
                                     PR_PRIORITY_NORMAL,
                                     PR_GLOBAL_THREAD,
                                     PR_JOINABLE_THREAD,
                                     STACKSIZE);
    PRThread *thr3 = PR_CreateThread(PR_SYSTEM_THREAD,
                                     threadmain,
                                     const_cast<char *>("TestFile3"),
                                     PR_PRIORITY_NORMAL,
                                     PR_GLOBAL_THREAD,
                                     PR_JOINABLE_THREAD,
                                     STACKSIZE);
    PR_JoinThread(thr1);
    PR_JoinThread(thr2);
    PR_JoinThread(thr3);
    return 0;
}
