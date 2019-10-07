/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/***********************************************************************
**
** Contact:     AOF<mailto:freier@netscape.com>
**
** Name: ranfile.c
**
** Description: Test to hammer on various components of NSPR
** Modification History:
** 20-May-97 AGarcia- Converted the test to accomodate the debug_mode flag.
**           The debug mode will print all of the printfs associated with this test.
**           The regress mode will be the default mode. Since the regress tool limits
**           the output to a one line status:PASS or FAIL,all of the printf statements
**           have been handled with an if (debug_mode) statement.
** 04-June-97 AGarcia removed the Test_Result function. Regress tool has been updated to
**          recognize the return code from tha main program.
***********************************************************************/


/***********************************************************************
** Includes
***********************************************************************/
/* Used to get the command line option */
#include <plgetopt.h>
#include <prprf.h>
#include <prio.h>

#include "rccv.h"
#include "rcthread.h"
#include "rcfileio.h"
#include "rclock.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static PRFileDesc *output;
static PRIntn debug_mode = 0;
static PRIntn failed_already = 0;

class HammerData
{
public:
    typedef enum {
        sg_go, sg_stop, sg_done
    } Action;
    typedef enum {
        sg_okay, sg_open, sg_close, sg_delete, sg_write, sg_seek
    } Problem;

    virtual ~HammerData();
    HammerData(RCLock* lock, RCCondition *cond, PRUint32 clip);
    virtual PRUint32 Random();

    Action action;
    Problem problem;
    PRUint32 writes;
    RCInterval timein;
    friend class Hammer;
private:
    RCLock *ml;
    RCCondition *cv;
    PRUint32 limit;

    PRFloat64 seed;
};  /* HammerData */

class Hammer: public HammerData, public RCThread
{
public:
    virtual ~Hammer();
    Hammer(RCThread::Scope scope, RCLock* lock, RCCondition *cond, PRUint32 clip);

private:
    void RootFunction();

};

static PRInt32 pageSize = 1024;
static const char* baseName = "./";
static const char *programName = "Random File";

/***********************************************************************
** PRIVATE FUNCTION:    Random
** DESCRIPTION:
**   Generate a pseudo-random number
** INPUTS:      None
** OUTPUTS:     None
** RETURN:      A pseudo-random unsigned number, 32-bits wide
** SIDE EFFECTS:
**      Updates random seed (a static)
** RESTRICTIONS:
**      None
** MEMORY:      NA
** ALGORITHM:
**      Uses the current interval timer value, promoted to a 64 bit
**      float as a multiplier for a static residue (which begins
**      as an uninitialized variable). The result is bits [16..48)
**      of the product. Seed is then updated with the return value
**      promoted to a float-64.
***********************************************************************/
PRUint32 HammerData::Random()
{
    PRUint32 rv;
    PRUint64 shift;
    RCInterval now = RCInterval(RCInterval::now);
    PRFloat64 random = seed * (PRFloat64)((PRIntervalTime)now);
    LL_USHR(shift, *((PRUint64*)&random), 16);
    LL_L2UI(rv, shift);
    seed = (PRFloat64)rv;
    return rv;
}  /* HammerData::Random */

Hammer::~Hammer() { }

Hammer::Hammer(
    RCThread::Scope scope, RCLock* lock, RCCondition *cond, PRUint32 clip):
    HammerData(lock, cond, clip), RCThread(scope, RCThread::joinable, 0) { }

HammerData::~HammerData() { }

HammerData::HammerData(RCLock* lock, RCCondition *cond, PRUint32 clip)
{
    ml = lock;
    cv = cond;
    writes = 0;
    limit = clip;
    seed = 0x58a9382;
    action = HammerData::sg_go;
    problem = HammerData::sg_okay;
    timein = RCInterval(RCInterval::now);
}  /* HammerData::HammerData */


/***********************************************************************
** PRIVATE FUNCTION:    Hammer::RootFunction
** DESCRIPTION:
**   Hammer on the file I/O system
** INPUTS:      A pointer to the thread's private data
** OUTPUTS:     None
** RETURN:      None
** SIDE EFFECTS:
**      Creates, accesses and deletes a file
** RESTRICTIONS:
**      (Currently) must have file create permission in "/usr/tmp".
** MEMORY:      NA
** ALGORITHM:
**      This function is a root of a thread
**      1) Creates a (hopefully) unique file in /usr/tmp/
**      2) Writes a zero to a random number of sequential pages
**      3) Closes the file
**      4) Reopens the file
**      5) Seeks to a random page within the file
**      6) Writes a one byte on that page
**      7) Repeat steps [5..6] for each page in the file
**      8) Close and delete the file
**      9) Repeat steps [1..8] until told to stop
**     10) Notify complete and return
***********************************************************************/
void Hammer::RootFunction()
{
    PRUint32 index;
    RCFileIO file;
    char filename[30];
    const char zero = 0;
    PRStatus rv = PR_SUCCESS;

    limit = (Random() % limit) + 1;

    (void)sprintf(filename, "%ssg%04p.dat", baseName, this);

    if (debug_mode) {
        PR_fprintf(output, "Starting work on %s\n", filename);
    }

    while (PR_TRUE)
    {
        PRUint64 bytes;
        PRUint32 minor = (Random() % limit) + 1;
        PRUint32 random = (Random() % limit) + 1;
        PRUint32 pages = (Random() % limit) + 10;
        while (minor-- > 0)
        {
            problem = sg_okay;
            if (action != sg_go) {
                goto finished;
            }
            problem = sg_open;
            rv = file.Open(filename, PR_RDWR|PR_CREATE_FILE, 0666);
            if (PR_FAILURE == rv) {
                goto finished;
            }
            for (index = 0; index < pages; index++)
            {
                problem = sg_okay;
                if (action != sg_go) {
                    goto close;
                }
                problem = sg_seek;
                bytes = file.Seek(pageSize * index, RCFileIO::set);
                if (bytes != pageSize * index) {
                    goto close;
                }
                problem = sg_write;
                bytes = file.Write(&zero, sizeof(zero));
                if (bytes <= 0) {
                    goto close;
                }
                writes += 1;
            }
            problem = sg_close;
            rv = file.Close();
            if (rv != PR_SUCCESS) {
                goto purge;
            }

            problem = sg_okay;
            if (action != sg_go) {
                goto purge;
            }

            problem = sg_open;
            rv = file.Open(filename, PR_RDWR, 0666);
            if (PR_FAILURE == rv) {
                goto finished;
            }
            for (index = 0; index < pages; index++)
            {
                problem = sg_okay;
                if (action != sg_go) {
                    goto close;
                }
                problem = sg_seek;
                bytes = file.Seek(pageSize * index, RCFileIO::set);
                if (bytes != pageSize * index) {
                    goto close;
                }
                problem = sg_write;
                bytes = file.Write(&zero, sizeof(zero));
                if (bytes <= 0) {
                    goto close;
                }
                writes += 1;
                random = (random + 511) % pages;
            }
            problem = sg_close;
            rv = file.Close();
            if (rv != PR_SUCCESS) {
                goto purge;
            }
            problem = sg_delete;
            rv = file.Delete(filename);
            if (rv != PR_SUCCESS) {
                goto finished;
            }
        }
    }

close:
    (void)file.Close();
purge:
    (void)file.Delete(filename);
finished:
    RCEnter scope(ml);
    action = HammerData::sg_done;
    cv->Notify();

    if (debug_mode) {
        PR_fprintf(output, "Ending work on %s\n", filename);
    }

    return;
}  /* Hammer::RootFunction */

static Hammer* hammer[100];
/***********************************************************************
** PRIVATE FUNCTION:    main
** DESCRIPTION:
**   Hammer on the file I/O system
** INPUTS:      The usual argc and argv
**              argv[0] - program name (not used)
**              argv[1] - the number of virtual_procs to execute the major loop
**              argv[2] - the number of threads to toss into the batch
**              argv[3] - the clipping number applied to randoms
**              default values: max_virtual_procs = 2, threads = 10, limit = 57
** OUTPUTS:     None
** RETURN:      None
** SIDE EFFECTS:
**      Creates, accesses and deletes lots of files
** RESTRICTIONS:
**      (Currently) must have file create permission in "/usr/tmp".
** MEMORY:      NA
** ALGORITHM:
**      1) Fork a "Thread()"
**      2) Wait for 'interleave' seconds
**      3) For [0..'threads') repeat [1..2]
**      4) Mark all objects to stop
**      5) Collect the threads, accumulating the results
**      6) For [0..'max_virtual_procs') repeat [1..5]
**      7) Print accumulated results and exit
**
**      Characteristic output (from IRIX)
**          Random File: Using max_virtual_procs = 2, threads = 10, limit = 57
**          Random File: [min [avg] max] writes/sec average
***********************************************************************/
PRIntn main (PRIntn argc, char *argv[])
{
    RCLock ml;
    PLOptStatus os;
    RCCondition cv(&ml);
    PRUint32 writesMax = 0, durationTot = 0;
    RCThread::Scope thread_scope = RCThread::local;
    PRUint32 writes, writesMin = 0x7fffffff, writesTot = 0;
    PRIntn active, poll, limit = 0, max_virtual_procs = 0, threads = 0, virtual_procs;
    RCInterval interleave(RCInterval::FromMilliseconds(10000)), duration(0);

    const char *where[] = {"okay", "open", "close", "delete", "write", "seek"};

    PLOptState *opt = PL_CreateOptState(argc, argv, "Gdl:t:i:");
    while (PL_OPT_EOL != (os = PL_GetNextOpt(opt)))
    {
        if (PL_OPT_BAD == os) {
            continue;
        }
        switch (opt->option)
        {
            case 0:
                baseName = opt->value;
                break;
            case 'G':  /* global threads */
                thread_scope = RCThread::global;
                break;
            case 'd':  /* debug mode */
                debug_mode = 1;
                break;
            case 'l':  /* limiting number */
                limit = atoi(opt->value);
                break;
            case 't':  /* number of threads */
                threads = atoi(opt->value);
                break;
            case 'i':  /* iteration counter */
                max_virtual_procs = atoi(opt->value);
                break;
            default:
                break;
        }
    }
    PL_DestroyOptState(opt);
    output = PR_GetSpecialFD(PR_StandardOutput);

    /* main test */

    cv.SetTimeout(interleave);

    if (max_virtual_procs == 0) {
        max_virtual_procs = 2;
    }
    if (limit == 0) {
        limit = 57;
    }
    if (threads == 0) {
        threads = 10;
    }

    if (debug_mode) PR_fprintf(output,
                                   "%s: Using %d virtual processors, %d threads, limit = %d and %s threads\n",
                                   programName, max_virtual_procs, threads, limit,
                                   (thread_scope == RCThread::local) ? "LOCAL" : "GLOBAL");

    for (virtual_procs = 0; virtual_procs < max_virtual_procs; ++virtual_procs)
    {
        if (debug_mode)
            PR_fprintf(output,
                       "%s: Setting number of virtual processors to %d\n",
                       programName, virtual_procs + 1);
        RCPrimordialThread::SetVirtualProcessors(virtual_procs + 1);
        for (active = 0; active < threads; active++)
        {
            hammer[active] = new Hammer(thread_scope, &ml, &cv, limit);
            hammer[active]->Start();  /* then make it roll */
            RCThread::Sleep(interleave);  /* start them slowly */
        }

        /*
         * The last thread started has had the opportunity to run for
         * 'interleave' seconds. Now gather them all back in.
         */
        {
            RCEnter scope(&ml);
            for (poll = 0; poll < threads; poll++)
            {
                if (hammer[poll]->action == HammerData::sg_go) { /* don't overwrite done */
                    hammer[poll]->action = HammerData::sg_stop;    /* ask him to stop */
                }
            }
        }

        while (active > 0)
        {
            for (poll = 0; poll < threads; poll++)
            {
                ml.Acquire();
                while (hammer[poll]->action < HammerData::sg_done) {
                    cv.Wait();
                }
                ml.Release();

                if (hammer[poll]->problem == HammerData::sg_okay)
                {
                    duration = RCInterval(RCInterval::now) - hammer[poll]->timein;
                    writes = hammer[poll]->writes * 1000 / duration;
                    if (writes < writesMin) {
                        writesMin = writes;
                    }
                    if (writes > writesMax) {
                        writesMax = writes;
                    }
                    writesTot += hammer[poll]->writes;
                    durationTot += duration;
                }
                else
                {
                    if (debug_mode) PR_fprintf(output,
                                                   "%s: test failed %s after %ld seconds\n",
                                                   programName, where[hammer[poll]->problem], duration);
                    else {
                        failed_already=1;
                    }
                }
                active -= 1;  /* this is another one down */
                (void)hammer[poll]->Join();
                hammer[poll] = NULL;
            }
        }
        if (debug_mode) PR_fprintf(output,
                                       "%s: [%ld [%ld] %ld] writes/sec average\n",
                                       programName, writesMin,
                                       writesTot * 1000 / durationTot, writesMax);
    }

    failed_already |= (PR_FAILURE == RCPrimordialThread::Cleanup());
    PR_fprintf(output, "%s\n", (failed_already) ? "FAIL\n" : "PASS\n");
    return failed_already;
}  /* main */
