/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
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
 * The Original Code is spacetrace.h/spacetrace.c code, released
 * Nov 6, 2001.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *    Garrett Arch Blythe, 31-October-2001
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the MPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the MPL or the GPL.
 */

/*
** spacetrace.c
**
** SpaceTrace is meant to take the output of trace-malloc and present
**   a picture of allocations over the run of the application.
*/

/*
** Required include files.
*/
#include "spacetrace.h"

#include <ctype.h>
#include <math.h>
#include <string.h>
#include <time.h>
#if defined(XP_WIN32)
#include <malloc.h> /* _heapMin */
#endif

#if defined(HAVE_BOUTELL_GD)
/*
** See http://www.boutell.com/gd for the GD graphics library.
** Ports for many platorms exist.
** Your box may already have the lib (mine did, redhat 7.1 workstation).
*/
#include <gd.h>
#include <gdfontt.h>
#include <gdfonts.h>
#include <gdfontmb.h>
#endif /* HAVE_BOUTELL_GD */

/*
** Ugh, MSVC6's qsort is too slow...
*/
#include "nsQuickSort.h"

/*
** Turn on to attempt adding support for graphs on your platform.
*/
#if defined(HAVE_BOUTELL_GD)
#define WANT_GRAPHS 1
#endif /* HAVE_BOUTELL_GD */
#if !defined(WANT_GRAPHS)
#define WANT_GRAPHS 0
#endif

/*
** Turn on to add the ability to quit the server from the client.
** A dubious feature at best.
*/
#define WANT_QUIT 0

/*
** the globals variables.  happy joy.
*/
static STGlobals globals;

/*
** have the heap cleanup at opportune times, if possible.
*/
void heapCompact(void)
{
#if defined(XP_WIN32)
    _heapmin();
#endif
}

/*
** showHelp
**
** Give simple command line help.
** Returns !0 if the help was showed.
*/
int showHelp(void)
{
    int retval = 0;

    if(0 != globals.mOptions.mShowHelp)
    {
        PR_fprintf(PR_STDOUT,
"Usage:\t%s [OPTION]... [-|filename]\n\n",
                   globals.mOptions.mProgramName);

        PR_fprintf(PR_STDOUT, "%s",
"OPTIONS:\n"
" -h                                                         Show this help.\n"
                   "\n");

        PR_fprintf(PR_STDOUT, "%s",
" -p<port>                 Listen for http requests on the specified <port>.\n"
"                                                    Default port is '1969'.\n"
                   "\n");

        PR_fprintf(PR_STDOUT, "%s",
" -d<dir>                                          Place -b output in <dir>.\n"
"                                                  The directory must exist.\n"
"                                              The default directory is '.'.\n"
"                               Very important to not have a trailing slash!\n"
                   "\n");

        PR_fprintf(PR_STDOUT, "%s",
" -b<filepath>                 Execute in batch mode, multiple -b's allowed.\n"
"                                   Save <filepath> into -d<dir>, then exit.\n"
                   "\n");

        PR_fprintf(PR_STDOUT, "%s",
" -l<max>              Set the maximum number of items to display in a list.\n"
"                                                The default <max> is '500'.\n"
                   "\n");

        PR_fprintf(PR_STDOUT, "%s",
" -o<num>           Sets the order in which lists are sorted when displayed.\n"
"                                   '0' is by weight (lifespan * byte size).\n"
"                                                       '1' is by byte size.\n"
"                                                 '2' is by time (lifetime).\n"
                   "");
        PR_fprintf(PR_STDOUT, "%s",
"                                         '3' is by allocation object count.\n"
"                                     '4' is by heap operation runtime cost.\n"
"                                                  By default, <num> is '0'.\n"
                   "\n");

        PR_fprintf(PR_STDOUT, "%s",
" -smin<num>       Set the minimum byte size to exclude smaller allocations.\n"
"                                                  The default <num> is '0'.\n"
                   "\n");

        PR_fprintf(PR_STDOUT, "%s",
" -smax<num>        Set the maximum byte size to exclude larger allocations.\n"
"                                           By default, there is no maximum.\n"
                   "\n");

        PR_fprintf(PR_STDOUT,
" -tmin<num>                 Set the minimum allocation lifetime in seconds.\n"
"              Excludes allocations which do not live at least said seconds.\n"
"                                         The default <num> is '%u' seconds.\n"
                   "\n", ST_DEFAULT_LIFETIME_MIN);

        PR_fprintf(PR_STDOUT, "%s",
" -tmax<num>                 Set the maximum allocation lifetime in seconds.\n"
"              Excludes allocations which live longer than the said seconds.\n"
"                                           By default, there is no maximum.\n"
                   "\n");

        PR_fprintf(PR_STDOUT, "%s",
" -wmin<num>                              Set the minimum allocation weight.\n"
"                                            Weight is lifespan * byte size.\n"
"                         Excludes allocations which do not have the weight.\n"
"                                                  The default <num> is '0'.\n"
                   "\n");

        PR_fprintf(PR_STDOUT, "%s",
" -wmax<num>                              Set the maximum allocation weight.\n"
"                                            Weight is lifespan * byte size.\n"
"                                Excludes allocations which are over weight.\n"
"                                           By default, there is no maximum.\n"
                   "\n");

        PR_fprintf(PR_STDOUT, "%s",
" -imin<num>                                     Set the minimum in seconds.\n"
"                   Excludes allocations existing solely before said second.\n"
"                                                  The default <num> is '0'.\n"
                   "\n");

        PR_fprintf(PR_STDOUT, "%s",
" -imax<num>                                     Set the maximum in seconds.\n"
"                    Excludes allocations existing solely after said second.\n"
"                                           By default, there is no maximum.\n"
                   "\n");

        PR_fprintf(PR_STDOUT, "%s",
" -amin<num>                          Set the allocation minimum in seconds.\n"
"                           Excludes allocations created before said second.\n"
"                                                  The default <num> is '0'.\n"
                   "\n");

        PR_fprintf(PR_STDOUT, "%s",
" -amax<num>                          Set the allocation maximum in seconds.\n"
"                            Excludes allocations created after said second.\n"
"                                           By default, there is no maximum.\n"
                   "\n");

#if WANT_GRAPHS
        PR_fprintf(PR_STDOUT, "%s",
" -gmin<num>                               Set the graph minimum in seconds.\n"
"                  Excludes representing graph intervals before said second.\n"
"                                                  The default <num> is '0'.\n"
                   "\n");

        PR_fprintf(PR_STDOUT, "%s",
" -gmax<num>                               Set the graph maximum in seconds.\n"
"                   Excludes representing graph intervals after said second.\n"
"                                           By default, there is no maximum.\n"
                   "\n");
#endif /* WANT_GRAPHS */

        PR_fprintf(PR_STDOUT,
" -a<num>                               Set an allocation alignment boundry.\n"
"                                     All allocations are a factor of <num>.\n"
"      Meaning, an allocation of 1 byte would actually count as <num> bytes.\n"
            );
        PR_fprintf(PR_STDOUT,
"              Set <num> to '1' in order to see the actual allocation sizes.\n"
"              Alignment is taken into account prior to allocation overhead.\n"
"                                                   By default, <num> is %u.\n"
                   "\n", ST_DEFAULT_ALIGNMENT_SIZE);

        PR_fprintf(PR_STDOUT,
" -h<num>                                           Set allocation overhead.\n"
"                            All allocations cost an additional <num> bytes.\n"
"                 Overhead is taken into account after allocation alignment.\n"
"              Set <num> to '0' in order to see the actual allocation sizes.\n"
"                                                    By default, <num> is %u.\n"
                   "\n", ST_DEFAULT_OVERHEAD_SIZE);

        PR_fprintf(PR_STDOUT,
" -c<text>     Restrict callsite backtraces to only those containing <text>.\n"
"                      Allows targeting of specific object creation methods.\n"
"                     A maximum of %d multiple restrictions can be specified.\n"
"                                By default, there is no <text> restriction.\n"
                   "\n", ST_SUBSTRING_MATCH_MAX);

        /*
        ** Showed something.
        */
        retval = __LINE__;
    }

    return retval;
}

/*
** ticks2xsec
**
** Convert platform specific ticks to second units
** Returns 0 on success.
*/
PRUint32 ticks2xsec(tmreader* aReader, PRUint32 aTicks, PRUint32 aResolution)
{
    PRUint32 retval = 0;
    PRUint64 bigone;
    PRUint64 tmp64;

    LL_UI2L(bigone, aResolution);
    LL_UI2L(tmp64, aTicks);
    LL_MUL(bigone, bigone, tmp64);
    LL_UI2L(tmp64, aReader->ticksPerSec);
    LL_DIV(bigone, bigone, tmp64);
    LL_L2UI(retval, bigone);
    return retval;
}
#define ticks2msec(reader, ticks) ticks2xsec((reader), (ticks), 1000)
#define ticks2usec(reader, ticks) ticks2xsec((reader), (ticks), 1000000)

/*
** initOptions
**
** Determine global settings for the application.
** Returns 0 on success.
*/
int initOptions(int aArgCount, char** aArgArray)
{
    int retval = 0;
    int traverse = 0;
    const char* stdinDash = "-";
    const char* outputDir = ".";
    const PRUint32 httpdPort = 1969;
    const PRUint32 listItemMax = 500;
    PRStatus prStatus = PR_SUCCESS;

    /*
    ** Set the program name.
    */
    globals.mOptions.mProgramName = aArgArray[0];

    /*
    ** As a default, stdin is the input.
    */
    globals.mOptions.mFileName = stdinDash;

    /*
    ** As a default, this directory is the output.
    */
    globals.mOptions.mOutputDir = outputDir;

    /*
    ** As a default, this is the port to listen for http requests.
    */
    globals.mOptions.mHttpdPort = httpdPort;

    /*
    ** As a default, the number of list items to limit display to.
    */
    globals.mOptions.mListItemMax = listItemMax;

    /*
    ** As a default, there is no maximum timeval on the dataset.
    */
    globals.mOptions.mTimevalMax = ST_TIMEVAL_MAX;

    /*
    ** As a default, there is no maximum allocation size on the dataset.
    */
    globals.mOptions.mSizeMax = (PRUint32)-1;

    /*
    ** As a default, want to look at allocations which live at least...
    */
    globals.mOptions.mLifetimeMin = ST_DEFAULT_LIFETIME_MIN * ST_TIMEVAL_RESOLUTION;

    /*
    ** As a default, there is no maximum allocation lifetime timeval.
    */
    globals.mOptions.mLifetimeMax = ST_TIMEVAL_MAX;

    /*
    ** As a default, there is no maximum weight allowed.
    */
    globals.mOptions.mWeightMax64 = LL_INIT(0xFFFFFFFF, 0xFFFFFFFF);

    /*
    ** As a default, there is no maximum allocation timeval.
    */
    globals.mOptions.mAllocationTimevalMax = ST_TIMEVAL_MAX;

    /*
    ** As a default, there is no maximum graph timeval.
    */
    globals.mOptions.mGraphTimevalMax = ST_TIMEVAL_MAX;

    /*
    ** As a default, we align byte sizes to a particular size.
    */
    globals.mOptions.mAlignBy = ST_DEFAULT_ALIGNMENT_SIZE;
    globals.mOptions.mOverhead = ST_DEFAULT_OVERHEAD_SIZE;

    /*
    ** Go through all arguments.
    ** If argument does not being with a dash it is a file name.
    ** If argument does begin with a dash but is only a dash
    **  it means input comes from stdin.
    ** If argument begins with a dash and does not end, then it
    **  maybe an option.
    */
    for(traverse = 1; traverse < aArgCount; traverse++)
    {
        if('-' == aArgArray[traverse][0])
        {
            /*
            ** Regular dash options.
            ** Detect what to do.
            */
            switch(tolower(aArgArray[traverse][1]))
            {
                case '\0':
                {
                    /*
                    ** If the entire option is a dash,
                    ** then input is stdin.
                    */
                    globals.mOptions.mFileName = stdinDash;
                }
                break;

                case 'h':
                {
                    if('\0' != aArgArray[traverse][2])
                    {
                        PRInt32 scanRes = 0;
                        
                        /*
                        ** Allocation overhead.
                        */
                        scanRes = PR_sscanf(&aArgArray[traverse][2], "%u", &globals.mOptions.mOverhead);
                        if(1 != scanRes)
                        {
                            retval = __LINE__;
                            globals.mOptions.mShowHelp = __LINE__;
                        }
                    }
                    else
                    {
                        /*
                        ** Help.
                        */
                        globals.mOptions.mShowHelp = __LINE__;
                    }
                }
                break;

                case 'p':
                {
                    PRInt32 scanRes = 0;

                    /*
                    ** Port.
                    */
                    scanRes = PR_sscanf(&aArgArray[traverse][2], "%u", &globals.mOptions.mHttpdPort);
                    if(1 != scanRes)
                    {
                        retval = __LINE__;
                        globals.mOptions.mShowHelp = __LINE__;
                    }
                }
                break;

                case 'o':
                {
                    PRInt32 scanRes = 0;

                    /*
                    ** Sort Order.
                    */
                    scanRes = PR_sscanf(&aArgArray[traverse][2], "%u", &globals.mOptions.mOrderBy);
                    if(1 != scanRes)
                    {
                        retval = __LINE__;
                        globals.mOptions.mShowHelp = __LINE__;
                    }
                }
                break;

                case 'd':
                {
                    /*
                    ** Where to stick the '-b' output.
                    */
                    if('\0' != aArgArray[traverse][2])
                    {
                        globals.mOptions.mOutputDir = &aArgArray[traverse][2];
                    }
                    else
                    {
                        retval = __LINE__;
                        globals.mOptions.mShowHelp = __LINE__;
                    }
                }
                break;

                case 'b':
                {
                    /*
                    ** Batch mode request.
                    */
                    if('\0' != aArgArray[traverse][2])
                    {
                        const char** expand = NULL;

                        /*
                        ** Increase size of batch buffer.
                        */
                        expand = (const char**)realloc((void*)globals.mOptions.mBatchRequests, sizeof(const char*) * (globals.mOptions.mBatchRequestCount + 1));
                        if(NULL != expand)
                        {
                            /*
                            ** Reassign in case of pointer move.
                            */
                            globals.mOptions.mBatchRequests = expand;

                            /*
                            ** Add new entry, increase the count.
                            */
                            globals.mOptions.mBatchRequests[globals.mOptions.mBatchRequestCount] = &aArgArray[traverse][2];
                            globals.mOptions.mBatchRequestCount++;
                        }
                        else
                        {
                            retval = __LINE__;
                            REPORT_ERROR(__LINE__, realloc);
                        }
                    }
                    else
                    {
                        retval = __LINE__;
                        globals.mOptions.mShowHelp = __LINE__;
                    }
                }
                break;

                case 'l':
                {
                    PRInt32 scanRes = 0;

                    /*
                    ** List item max.
                    */
                    scanRes = PR_sscanf(&aArgArray[traverse][2], "%u", &globals.mOptions.mListItemMax);
                    if(1 != scanRes)
                    {
                        retval = __LINE__;
                        globals.mOptions.mShowHelp = __LINE__;
                    }
                }
                break;

                case 'i':
                {
                    if(0 == strncmp(&aArgArray[traverse][2], "min", 3))
                    {
                        PRInt32 scanRes = 0;

                        /*
                        ** Timeval min.
                        */
                        scanRes = PR_sscanf(&aArgArray[traverse][5], "%u", &globals.mOptions.mTimevalMin);
                        if(1 != scanRes)
                        {
                            retval = __LINE__;
                            globals.mOptions.mShowHelp = __LINE__;
                        }
                        else
                        {
                            globals.mOptions.mTimevalMin *= ST_TIMEVAL_RESOLUTION;
                        }
                    }
                    else if(0 == strncmp(&aArgArray[traverse][2], "max", 3))
                    {
                        PRInt32 scanRes = 0;

                        /*
                        ** Timeval max
                        */
                        scanRes = PR_sscanf(&aArgArray[traverse][5], "%u", &globals.mOptions.mTimevalMax);
                        if(1 != scanRes)
                        {
                            retval = __LINE__;
                            globals.mOptions.mShowHelp = __LINE__;
                        }
                        else
                        {
                            globals.mOptions.mTimevalMax *= ST_TIMEVAL_RESOLUTION;
                        }
                    }
                    else
                    {
                        retval = __LINE__;
                        globals.mOptions.mShowHelp = __LINE__;
                    }
                }
                break;

                case 'a':
                {
                    if(0 == strncmp(&aArgArray[traverse][2], "min", 3))
                    {
                        PRInt32 scanRes = 0;

                        /*
                        ** Allocation Timeval min.
                        */
                        scanRes = PR_sscanf(&aArgArray[traverse][5], "%u", &globals.mOptions.mAllocationTimevalMin);
                        if(1 != scanRes)
                        {
                            retval = __LINE__;
                            globals.mOptions.mShowHelp = __LINE__;
                        }
                        else
                        {
                            globals.mOptions.mAllocationTimevalMin *= ST_TIMEVAL_RESOLUTION;
                        }
                    }
                    else if(0 == strncmp(&aArgArray[traverse][2], "max", 3))
                    {
                        PRInt32 scanRes = 0;

                        /*
                        ** Allocation timeval max
                        */
                        scanRes = PR_sscanf(&aArgArray[traverse][5], "%u", &globals.mOptions.mAllocationTimevalMax);
                        if(1 != scanRes)
                        {
                            retval = __LINE__;
                            globals.mOptions.mShowHelp = __LINE__;
                        }
                        else
                        {
                            globals.mOptions.mAllocationTimevalMax *= ST_TIMEVAL_RESOLUTION;
                        }
                    }
                    else
                    {
                        PRInt32 scanRes = 0;
                        
                        /*
                        ** Align by.
                        */
                        scanRes = PR_sscanf(&aArgArray[traverse][2], "%u", &globals.mOptions.mAlignBy);
                        if(1 != scanRes)
                        {
                            retval = __LINE__;
                            globals.mOptions.mShowHelp = __LINE__;
                        }
                    }
                }
                break;

#if WANT_GRAPHS
                case 'g':
                {
                    if(0 == strncmp(&aArgArray[traverse][2], "min", 3))
                    {
                        PRInt32 scanRes = 0;

                        /*
                        ** Graph Timeval min.
                        */
                        scanRes = PR_sscanf(&aArgArray[traverse][5], "%u", &globals.mOptions.mGraphTimevalMin);
                        if(1 != scanRes)
                        {
                            retval = __LINE__;
                            globals.mOptions.mShowHelp = __LINE__;
                        }
                        else
                        {
                            globals.mOptions.mGraphTimevalMin *= ST_TIMEVAL_RESOLUTION;
                        }
                    }
                    else if(0 == strncmp(&aArgArray[traverse][2], "max", 3))
                    {
                        PRInt32 scanRes = 0;

                        /*
                        ** Graph Timeval max
                        */
                        scanRes = PR_sscanf(&aArgArray[traverse][5], "%u", &globals.mOptions.mGraphTimevalMax);
                        if(1 != scanRes)
                        {
                            retval = __LINE__;
                            globals.mOptions.mShowHelp = __LINE__;
                        }
                        else
                        {
                            globals.mOptions.mGraphTimevalMax *= ST_TIMEVAL_RESOLUTION;
                        }
                    }
                    else
                    {
                        retval = __LINE__;
                        globals.mOptions.mShowHelp = __LINE__;
                    }
                }
                break;
#endif /* WANT_GRAPHS */

                case 's':
                {
                    if(0 == strncmp(&aArgArray[traverse][2], "min", 3))
                    {
                        PRInt32 scanRes = 0;

                        /*
                        ** Size min.
                        */
                        scanRes = PR_sscanf(&aArgArray[traverse][5], "%u", &globals.mOptions.mSizeMin);
                        if(1 != scanRes)
                        {
                            retval = __LINE__;
                            globals.mOptions.mShowHelp = __LINE__;
                        }
                    }
                    else if(0 == strncmp(&aArgArray[traverse][2], "max", 3))
                    {
                        PRInt32 scanRes = 0;

                        /*
                        ** Size max.
                        */
                        scanRes = PR_sscanf(&aArgArray[traverse][5], "%u", &globals.mOptions.mSizeMax);
                        if(1 != scanRes)
                        {
                            retval = __LINE__;
                            globals.mOptions.mShowHelp = __LINE__;
                        }
                    }
                    else
                    {
                        retval = __LINE__;
                        globals.mOptions.mShowHelp = __LINE__;
                    }
                }
                break;

                case 't':
                {
                    if(0 == strncmp(&aArgArray[traverse][2], "min", 3))
                    {
                        PRInt32 scanRes = 0;

                        /*
                        ** Lifetime min.
                        */
                        scanRes = PR_sscanf(&aArgArray[traverse][5], "%u", &globals.mOptions.mLifetimeMin);
                        if(1 != scanRes)
                        {
                            retval = __LINE__;
                            globals.mOptions.mShowHelp = __LINE__;
                        }
                        else
                        {
                            globals.mOptions.mLifetimeMin *= ST_TIMEVAL_RESOLUTION;
                        }
                    }
                    else if(0 == strncmp(&aArgArray[traverse][2], "max", 3))
                    {
                        PRInt32 scanRes = 0;

                        /*
                        ** Lifetime max.
                        */
                        scanRes = PR_sscanf(&aArgArray[traverse][5], "%u", &globals.mOptions.mLifetimeMax);
                        if(1 != scanRes)
                        {
                            retval = __LINE__;
                            globals.mOptions.mShowHelp = __LINE__;
                        }
                        else
                        {
                            globals.mOptions.mLifetimeMax *= ST_TIMEVAL_RESOLUTION;
                        }
                    }
                    else
                    {
                        retval = __LINE__;
                        globals.mOptions.mShowHelp = __LINE__;
                    }
                }
                break;

                case 'w':
                {
                    if(0 == strncmp(&aArgArray[traverse][2], "min", 3))
                    {
                        PRInt32 scanRes = 0;

                        /*
                        ** Weight min.
                        */
                        scanRes = PR_sscanf(&aArgArray[traverse][5], "%llu", &globals.mOptions.mWeightMin64);
                        if(1 != scanRes)
                        {
                            retval = __LINE__;
                            globals.mOptions.mShowHelp = __LINE__;
                        }
                    }
                    else if(0 == strncmp(&aArgArray[traverse][2], "max", 3))
                    {
                        PRInt32 scanRes = 0;

                        /*
                        ** Weight max.
                        */
                        scanRes = PR_sscanf(&aArgArray[traverse][5], "%llu", &globals.mOptions.mWeightMax64);
                        if(1 != scanRes)
                        {
                            retval = __LINE__;
                            globals.mOptions.mShowHelp = __LINE__;
                        }
                    }
                    else
                    {
                        retval = __LINE__;
                        globals.mOptions.mShowHelp = __LINE__;
                    }
                }
                break;

                case 'c':
                {
                    /*
                    ** Restrict callsite text.
                    */
                    if('\0' != aArgArray[traverse][2])
                    {
                        int looper = 0;

                        /*
                        ** Figure out if we have any free space.
                        ** If so, copy it.
                        */
                        for(looper = 0; ST_SUBSTRING_MATCH_MAX > looper; looper++)
                        {
                            if(NULL != globals.mOptions.mRestrictText[looper])
                            {
                                continue;
                            }

                            globals.mOptions.mRestrictText[looper] = strdup(&aArgArray[traverse][2]);
                            if(NULL == globals.mOptions.mRestrictText[looper])
                            {
                                retval = __LINE__;
                            }
                            break;
                        }

                        /*
                        ** Error on no more space left.
                        */
                        if(ST_SUBSTRING_MATCH_MAX == looper)
                        {
                            retval = __LINE__;
                        }
                    }
                    else
                    {
                        retval = __LINE__;
                        globals.mOptions.mShowHelp = __LINE__;
                    }
                }
                break;

                default:
                {
                    /*
                    ** Unknown option.
                    ** Error and show help.
                    */
                    retval = __LINE__;
                    globals.mOptions.mShowHelp = __LINE__;
                }
                break;
            }

            /*
            ** Check for some type of error, so we know to break the
            **  loop if need be.
            */
            if(0 != retval)
            {
                break;
            }
        }
        else
        {
            /*
            ** File to process.
            */
            globals.mOptions.mFileName = aArgArray[traverse];
        }
    }

    return retval;
}

#if WANT_GRAPHS
/*
** createGraph
**
** Create a GD image with the common properties of a graph.
** Upon return, you normally allocate legend colors,
**  draw your graph inside the region
**  STGD_MARGIN,STGD_MARGIN,STGD_WIDTH-STGD_MARGIN,STGD_HEIGH-STGD_MARGIN,
**  and then call drawGraph to format the surrounding information.
**
** You should use the normal GD image release function, gdImageDestroy
**  when done with it.
**
** Image attributes:
**   STGD_WIDTHxSTGD_HEIGHT
**   trasparent (white) background
**   incremental display
*/
gdImagePtr createGraph(int* aTransparencyColor)
{
    gdImagePtr retval = NULL;

    if(NULL != aTransparencyColor)
    {
        *aTransparencyColor = -1;

        retval = gdImageCreate(STGD_WIDTH, STGD_HEIGHT);
        if(NULL != retval)
        {
            /*
            ** Background color (first one).
            */
            *aTransparencyColor = gdImageColorAllocate(retval, 255, 255, 255);
            if(-1 != *aTransparencyColor)
            {
                /*
                ** As transparency.
                */
                gdImageColorTransparent(retval, *aTransparencyColor);
            }

            /*
            ** And to set interlacing.
            */
            gdImageInterlace(retval, 1);
        }
        else
        {
            REPORT_ERROR(__LINE__, gdImageCreate);
        }
    }
    else
    {
        REPORT_ERROR(__LINE__, createGraph);
    }

    return retval;
}
#endif /* WANT_GRAPHS */

#if WANT_GRAPHS
/*
** drawGraph
**
** This function mainly exists to simplify putitng all the pretty lace
**  around a home made graph.
*/
void drawGraph(gdImagePtr aImage, int aColor, const char* aGraphTitle, const char* aXAxisTitle, const char* aYAxisTitle, PRUint32 aXMarkCount, PRUint32* aXMarkPercents, const char** aXMarkTexts, PRUint32 aYMarkCount, PRUint32* aYMarkPercents, const char** aYMarkTexts, PRUint32 aLegendCount, int* aLegendColors, const char** aLegendTexts)
{
    if(NULL != aImage && NULL != aGraphTitle && NULL != aXAxisTitle && NULL != aYAxisTitle && (0 == aXMarkCount || (NULL != aXMarkPercents && NULL != aXMarkTexts)) && (0 == aYMarkCount || (NULL != aYMarkPercents && NULL != aYMarkTexts)) && (0 == aLegendCount || (NULL != aLegendColors && NULL != aLegendTexts)))
    {
        int margin = 1;
        PRUint32 traverse = 0;
        PRUint32 target = 0;
        const int markSize = 2;
        int x1 = 0;
        int y1 = 0;
        int x2 = 0;
        int y2 = 0;
        time_t theTimeT = time(NULL);
        char* theTime = ctime(&theTimeT);
        const char* logo = "SpaceTrace";
        gdFontPtr titleFont = gdFontMediumBold;
        gdFontPtr markFont = gdFontTiny;
        gdFontPtr dateFont = gdFontTiny;
        gdFontPtr axisFont = gdFontSmall;
        gdFontPtr legendFont = gdFontTiny;
        gdFontPtr logoFont = gdFontTiny;

        /*
        ** Fixup the color.
        ** Black by default.
        */
        if(-1 == aColor)
        {
            aColor = gdImageColorAllocate(aImage, 0, 0, 0);
        }
        if(-1 == aColor)
        {
            aColor = gdImageColorClosest(aImage, 0, 0, 0);
        }

        /*
        ** Output the box.
        */
        x1 = STGD_MARGIN - margin;
        y1 = STGD_MARGIN - margin;
        x2 = STGD_WIDTH - x1;
        y2 = STGD_HEIGHT - y1;
        gdImageRectangle(aImage, x1, y1, x2, y2, aColor);
        margin++;

        /*
        ** Need to make small markings on the graph to indicate where the
        **  labels line up exactly.
        ** While we're at it, draw the label text.
        */
        for(traverse = 0; traverse < aXMarkCount; traverse++)
        {
            target = ((STGD_WIDTH - (STGD_MARGIN * 2)) * aXMarkPercents[traverse]) / 100;

            x1 = STGD_MARGIN + target;
            y1 = STGD_MARGIN - margin;
            x2 = x1;
            y2 = y1 - markSize;
            gdImageLine(aImage, x1, y1, x2, y2, aColor);

            y1 = STGD_HEIGHT - y1;
            y2 = STGD_HEIGHT - y2;
            gdImageLine(aImage, x1, y1, x2, y2, aColor);

            if(NULL != aXMarkTexts[traverse])
            {
                x1 = STGD_MARGIN + target - (markFont->h / 2);
                y1 = STGD_HEIGHT - STGD_MARGIN + margin + markSize + (strlen(aXMarkTexts[traverse]) * markFont->w);
                gdImageStringUp(aImage, markFont, x1, y1, (unsigned char*)aXMarkTexts[traverse], aColor);
            }
        }
        for(traverse = 0; traverse < aYMarkCount; traverse++)
        {
            target = ((STGD_HEIGHT - (STGD_MARGIN * 2)) * (100 - aYMarkPercents[traverse])) / 100;

            x1 = STGD_MARGIN - margin;
            y1 = STGD_MARGIN + target;
            x2 = x1 - markSize;
            y2 = y1;
            gdImageLine(aImage, x1, y1, x2, y2, aColor);

            x1 = STGD_WIDTH - x1;
            x2 = STGD_WIDTH - x2;
            gdImageLine(aImage, x1, y1, x2, y2, aColor);

            if(NULL != aYMarkTexts[traverse])
            {
                x1 = STGD_MARGIN - margin - markSize - (strlen(aYMarkTexts[traverse]) * markFont->w);
                y1 = STGD_MARGIN + target - (markFont->h / 2);
                gdImageString(aImage, markFont, x1, y1, (unsigned char*)aYMarkTexts[traverse], aColor);
            }
        }
        margin += markSize;

        /*
        ** Title will be centered above the image.
        */
        x1 = (STGD_WIDTH / 2) - ((strlen(aGraphTitle) * titleFont->w) / 2);
        y1 = ((STGD_MARGIN - margin) / 2) - (titleFont->h / 2);
        gdImageString(aImage, titleFont, x1, y1, (unsigned char*)aGraphTitle, aColor);

        /*
        ** Upper left will be the date.
        */
        x1 = 0;
        y1 = 0;
        traverse = strlen(theTime) - 1;
        if(isspace(theTime[traverse]))
        {
            theTime[traverse] = '\0';
        }
        gdImageString(aImage, dateFont, x1, y1, (unsigned char*)theTime, aColor);

        /*
        ** Lower right will be the logo.
        */
        x1 = STGD_WIDTH - (strlen(logo) * logoFont->w);
        y1 = STGD_HEIGHT - logoFont->h;
        gdImageString(aImage, logoFont, x1, y1, (unsigned char*)logo, aColor);

        /*
        ** X and Y axis titles
        */
        x1 = (STGD_WIDTH / 2) - ((strlen(aXAxisTitle) * axisFont->w) / 2);
        y1 = STGD_HEIGHT - axisFont->h;
        gdImageString(aImage, axisFont, x1, y1, (unsigned char*)aXAxisTitle, aColor);
        x1 = 0;
        y1 = (STGD_HEIGHT / 2) + ((strlen(aYAxisTitle) * axisFont->w) / 2);
        gdImageStringUp(aImage, axisFont, x1, y1, (unsigned char*)aYAxisTitle, aColor);

        /*
        ** The legend.
        ** Centered on the right hand side, going up.
        */
        x1 = STGD_WIDTH - STGD_MARGIN + margin + (aLegendCount * legendFont->h) / 2;
        x2 = STGD_WIDTH - (aLegendCount * legendFont->h);
        if(x1 > x2)
        {
            x1 = x2;
        }

        y1 = 0;
        for(traverse = 0; traverse < aLegendCount; traverse++)
        {
            y2 = (STGD_HEIGHT / 2) + ((strlen(aLegendTexts[traverse]) * legendFont->w) / 2);
            if(y2 > y1)
            {
                y1 = y2;
            }
        }
        for(traverse = 0; traverse < aLegendCount; traverse++)
        {
            gdImageStringUp(aImage, legendFont, x1, y1, (unsigned char*)aLegendTexts[traverse], aLegendColors[traverse]);
            x1 += legendFont->h;
        }
    }
}

#endif /* WANT_GRAPHS */

#if defined(HAVE_BOUTELL_GD)
/*
** pngSink
**
** GD callback, used to write out the png.
*/
int pngSink(void* aContext, const char* aBuffer, int aLen)
{
    return PR_Write((PRFileDesc*)aContext, aBuffer, aLen);
}
#endif /* HAVE_BOUTELL_GD */

/*
** FormatNumber
**
** Formats a number with thousands separator. Dont free the result. Returns
** static data.
*/
char *FormatNumber(PRInt32 num)
{
    static char buf[64];
    char tmpbuf[64];
    int len = 0;
    int bufindex = sizeof(buf) - 1;
    int mod3;

    PR_snprintf(tmpbuf, sizeof(tmpbuf), "%d", num);

    /* now insert the thousands separator */
    mod3 = 0;
    len = strlen(tmpbuf);
    while (len >= 0)
    {
        if (tmpbuf[len] >= '0' && tmpbuf[len] <= '9')
        {
            if (mod3 == 3)
            {
                buf[bufindex--] = ',';
                mod3 = 0;
            }
            mod3++;
        }
        buf[bufindex--] = tmpbuf[len--];
    }
    return buf+bufindex+1;
}

/*
** actualByteSize
**
** Apply alignment and overhead to size to figure out actual byte size
*/
PRUint32 actualByteSize(PRUint32 retval)
{
    /*
    ** Need to bump the result by our alignment and overhead.
    ** The idea here is that an allocation actually costs you more than you
    **  thought.
    **
    ** The msvcrt malloc has an alignment of 16 with an overhead of 8.
    ** The win32 HeapAlloc has an alignment of 8 with an overhead of 8.
    */
    if(0 != retval)
    {
        PRUint32 eval = 0;
        PRUint32 over = 0;

        eval = retval - 1;
        if(0 != globals.mOptions.mAlignBy)
        {
            over = eval % globals.mOptions.mAlignBy;
        }
        retval = eval + globals.mOptions.mOverhead + globals.mOptions.mAlignBy - over;
    }

    return retval;
}

/*
** byteSize
**
** Figuring the byte size of an allocation.
** Might expand in the future to report size at a given time.
** For now, just use last relevant event.
*/
PRUint32 byteSize(STAllocation* aAlloc)
{
    PRUint32 retval = 0;

    if(NULL != aAlloc && 0 != aAlloc->mEventCount)
    {
        PRUint32 index = aAlloc->mEventCount;

        /*
        ** Generally, the size is the last event's size.
        */
        do
        {
            index--;
            retval = aAlloc->mEvents[index].mHeapSize;
        }
        while(0 == retval && 0 != index);
    }
    return actualByteSize(retval);
}


/*
** appendAllocation
**
** Given a run, append the allocation to it.
** No DUP checks are done.
** Also, we might want to update the parent callsites with stats.
** We decide to do this heavy duty work only if the run we are appending
**  to has a non ZERO mStats.mStamp, meaning that it is asking to track
**  such information when it was created.
** Returns !0 on success.
*/
int appendAllocation(STRun* aRun, STAllocation* aAllocation)
{
    int retval = 0;

    if(NULL != aRun && NULL != aAllocation)
    {
        STAllocation** expand = NULL;

        /*
        ** Expand the size of the array if needed.
        */
        expand = (STAllocation**)realloc(aRun->mAllocations, sizeof(STAllocation*) * (aRun->mAllocationCount + 1));
        if(NULL != expand)
        {
            /*
            ** Reassign in case of pointer move.
            */
            aRun->mAllocations = expand;

            /*
            ** Stick the allocation in.
            */
            aRun->mAllocations[aRun->mAllocationCount] = aAllocation;

            /*
            ** If this is the global run, we need to let the allocation
            **  track the index back to us.
            */
            if(&globals.mRun == aRun)
            {
                aAllocation->mRunIndex = aRun->mAllocationCount;
            }

            /*
            ** Increase the count.
            */
            aRun->mAllocationCount++;

            /*
            ** We're good.
            */
            retval = __LINE__;

            /*
            ** Now, see if they desire a callsite update.
            ** As mentioned previously, we decide if the run desires us to
            **  manipulate the callsite data only if it's stamp is set.
            ** We change all callsites and parent callsites to have that
            **  stamp as well, so as to mark them as being relevant to
            **  the current run in question.
            */
            if(0 != aRun->mStats.mStamp)
            {
                PRUint32 timeval = aAllocation->mMaxTimeval - aAllocation->mMinTimeval;
                PRUint32 size = byteSize(aAllocation);
                PRUint64 weight64 = LL_INIT(0, 0);
                PRUint32 heapCost = aAllocation->mHeapRuntimeCost;
                PRUint64 timeval64 = LL_INIT(0, 0);
                PRUint64 size64 = LL_INIT(0, 0);

                LL_UI2L(timeval64, timeval);
                LL_UI2L(size64, size);
                LL_MUL(weight64, timeval64, size64);

                /*
                ** First, update this run.
                */
                aRun->mStats.mCompositeCount++;
                aRun->mStats.mHeapRuntimeCost += heapCost;
                aRun->mStats.mSize += size;
                LL_ADD(aRun->mStats.mTimeval64, aRun->mStats.mTimeval64, timeval64);
                LL_ADD(aRun->mStats.mWeight64, aRun->mStats.mWeight64, weight64);

                /*
                ** Use the first event of the allocation to update the parent
                **  callsites.
                ** This has positive effect of not updating realloc callsites
                **  with the same data over and over again.
                */
                if(0 < aAllocation->mEventCount)
                {
                    tmcallsite* callsite = aAllocation->mEvents[0].mCallsite;
                    STRun* callsiteRun = NULL;

                    /*
                    ** Go up parents till we drop.
                    */
                    while(NULL != callsite && NULL != callsite->method)
                    {
                        callsiteRun = CALLSITE_RUN(callsite);
                        if(NULL != callsiteRun)
                        {
                            /*
                            ** Do we init it?
                            */
                            if(callsiteRun->mStats.mStamp != aRun->mStats.mStamp)
                            {
                                memset(&callsiteRun->mStats, 0, sizeof(STCallsiteStats));
                                callsiteRun->mStats.mStamp = aRun->mStats.mStamp;
                            }
                            
                            /*
                            ** Add the values.
                            ** Note that if the allocation was ever realloced,
                            **  we are actually recording the final size.
                            ** Also, the composite count does not include
                            **  calls to realloc (or free for that matter),
                            **  but rather is simply a count of actual heap
                            **  allocation objects, from which someone will
                            **  draw conclusions regarding number of malloc
                            **  and free calls.
                            ** It is possible to generate the exact number
                            **  of calls to free/malloc/realloc should the
                            **  absolute need arise to count them individually,
                            **  but I fear it will take mucho memory and this
                            **  is perhaps good enough for now.
                            */
                            callsiteRun->mStats.mCompositeCount++;
                            callsiteRun->mStats.mHeapRuntimeCost += heapCost;
                            callsiteRun->mStats.mSize += size;
                            LL_ADD(callsiteRun->mStats.mTimeval64, callsiteRun->mStats.mTimeval64, timeval64);
                            LL_ADD(callsiteRun->mStats.mWeight64, callsiteRun->mStats.mWeight64, weight64);
                        }
                        
                        callsite = callsite->parent;
                    }
                }
            }
        }
        else
        {
            REPORT_ERROR(__LINE__, appendAllocation);
        }
    }
    else
    {
        REPORT_ERROR(__LINE__, appendAllocation);
    }

    return retval;
}

/*
** hasCallsiteMatch
**
** Determine if the callsite or the other callsites has the matching text.
**
** Returns 0 if there is no match.
*/
int hasCallsiteMatch(tmcallsite* aCallsite, const char* aMatch, int aDirection)
{
    int retval = 0;

    if(NULL != aCallsite && NULL != aCallsite->method && NULL != aMatch && '\0' != *aMatch)
    {
        const char* methodName = NULL;

        do
        {
            methodName = tmmethodnode_name(aCallsite->method);
            if(NULL != methodName && NULL != strstr(methodName, aMatch))
            {
                /*
                ** Contains the text.
                */
                retval = __LINE__;
                break;
            }
            else
            {
                switch(aDirection)
                {
                    case ST_FOLLOW_SIBLINGS:
                        aCallsite = aCallsite->siblings;
                        break;
                    case ST_FOLLOW_PARENTS:
                        aCallsite = aCallsite->parent;
                        break;
                    default:
                        aCallsite = NULL;
                        REPORT_ERROR(__LINE__, hasCallsiteMatch);
                        break;
                }
            }
        }
        while(NULL != aCallsite && NULL != aCallsite->method);
    }
    else
    {
        REPORT_ERROR(__LINE__, hasCallsiteMatch);
    }

    return retval;
}

/*
** harvestRun
**
** Provide a simply way to go over a run, and yield the relevant allocations.
** The restrictions are easily set via the options page or the command
**  line switches.
**
** On any match, add the allocation to the provided run.
**
** This makes it much easier for all the code to respect the options in
**  force.
**
** You can override the global options by passing in your own options
**  pointer if you need a custom harvest.
**
** Returns !0 on error, though aOutRun may contain a partial data set.
*/
int harvestRun(const STRun* aInRun, STRun* aOutRun, STOptions* aOptions)
{
    int retval = 0;

    if(NULL != aInRun && NULL != aOutRun && aInRun != aOutRun)
    {
        PRUint32 traverse = 0;
        STAllocation* current = NULL;

        /*
        ** Fixup options if not provided.
        */
        if(NULL == aOptions)
        {
            aOptions = &globals.mOptions;
        }

        for(traverse = 0; 0 == retval && traverse < aInRun->mAllocationCount; traverse++)
        {
            current = aInRun->mAllocations[traverse];
            if(NULL != current)
            {
                PRUint32 lifetime = 0;
                PRUint32 bytesize = 0;
                PRUint64 weight64 = LL_INIT(0, 0);
                PRUint64 bytesize64 = LL_INIT(0, 0);
                PRUint64 lifetime64 = LL_INIT(0, 0);
                int appendRes = 0;
                int looper = 0;
                PRBool matched = PR_FALSE;

                /*
                ** Use this as an opportune time to fixup a memory
                **  leaked timeval, so as to not completely skew
                **  the weights.
                */
                if(ST_TIMEVAL_MAX == current->mMaxTimeval)
                {
                    current->mMaxTimeval = globals.mMaxTimeval;
                }

                /*
                ** Check allocation timeval restrictions.
                ** We have to slide the recorded timevals to be zero
                **  based, so that the comparisons make sense.
                */
                if(aOptions->mAllocationTimevalMin > (current->mMinTimeval - globals.mMinTimeval))
                {
                    continue;
                }
                else if(aOptions->mAllocationTimevalMax < (current->mMinTimeval - globals.mMinTimeval))
                {
                    continue;
                }

                /*
                ** Check timeval restrictions.
                ** We have to slide the recorded timevals to be zero
                **  based, so that the comparisons make sense.
                */
                if(aOptions->mTimevalMin > (current->mMaxTimeval - globals.mMinTimeval))
                {
                    continue;
                }
                else if(aOptions->mTimevalMax < (current->mMinTimeval - globals.mMinTimeval))
                {
                    continue;
                }

                /*
                ** Check lifetime restrictions.
                */
                lifetime = current->mMaxTimeval - current->mMinTimeval;
                if(lifetime < aOptions->mLifetimeMin)
                {
                    continue;
                }
                else if(lifetime > aOptions->mLifetimeMax)
                {
                    continue;
                }

                /*
                ** Check byte size restrictions.
                */
                bytesize = byteSize(current);
                if(bytesize < aOptions->mSizeMin)
                {
                    continue;
                }
                else if(bytesize > aOptions->mSizeMax)
                {
                    continue;
                }

                /*
                ** Check weight restrictions.
                */
                LL_UI2L(bytesize64, bytesize);
                LL_UI2L(lifetime64, lifetime);
                LL_MUL(weight64, bytesize64, lifetime64);
                if(LL_UCMP(weight64, <, aOptions->mWeightMin64))
                {
                    continue;
                }
                else if(LL_UCMP(weight64, >, aOptions->mWeightMax64))
                {
                    continue;
                }

                /*
                ** Possibly restrict the callsite by text.
                ** Do this last, as it is a heavier check.
                **
                ** One day, we may need to expand the logic to check for
                **  events beyond the initial allocation event.
                */
                for(looper = 0; ST_SUBSTRING_MATCH_MAX > looper; looper++)
                {
                    if(NULL != globals.mOptions.mRestrictText[looper] && '\0' != globals.mOptions.mRestrictText[looper][0])
                    {
                        if(0 == hasCallsiteMatch(current->mEvents[0].mCallsite, globals.mOptions.mRestrictText[looper], ST_FOLLOW_PARENTS))
                        {
                            break;
                        }
                    }
                    else
                    {
                        matched = PR_TRUE;
                        break;
                    }
                }
                if(ST_SUBSTRING_MATCH_MAX == looper)
                {
                    matched = PR_TRUE;
                }
                if(PR_FALSE == matched)
                {
                    continue;
                }

                /*
                ** You get here, we add to the run.
                */
                appendRes = appendAllocation(aOutRun, current);
                if(0 == appendRes)
                {
                    retval = __LINE__;
                    REPORT_ERROR(__LINE__, appendAllocation);
                }
            }
        }
    }

    return retval;
}

/*
** compareAllocations
**
** qsort callback.
** Compare the allocations as specified by the options.
*/
int compareAllocations(const void* aAlloc1, const void* aAlloc2, void* aContext)
{
    int retval = 0;

    if(NULL != aAlloc1 && NULL != aAlloc2)
    {
        STAllocation* alloc1 = *((STAllocation**)aAlloc1);
        STAllocation* alloc2 = *((STAllocation**)aAlloc2);

        if(NULL != alloc1 && NULL != alloc2)
        {
            /*
            ** Logic determined by pref/option.
            */
            switch(globals.mOptions.mOrderBy)
            {
                case ST_COUNT:
                    /*
                    ** "By count" on a single allocation means nothing,
                    **  fall through to weight.
                    */
                case ST_WEIGHT:
                {
                    PRUint64 weight164 = LL_INIT(0, 0);
                    PRUint64 weight264 = LL_INIT(0, 0);
                    PRUint64 bytesize164 = LL_INIT(0, 0);
                    PRUint64 bytesize264 = LL_INIT(0, 0);
                    PRUint64 timeval164 = LL_INIT(0, 0);
                    PRUint64 timeval264 = LL_INIT(0, 0);

                    LL_UI2L(bytesize164, byteSize(alloc1));
                    LL_UI2L(timeval164, (alloc1->mMaxTimeval - alloc1->mMinTimeval));
                    LL_MUL(weight164, bytesize164, timeval164);
                    LL_UI2L(bytesize264, byteSize(alloc2));
                    LL_UI2L(timeval264, (alloc2->mMaxTimeval - alloc2->mMinTimeval));
                    LL_MUL(weight264, bytesize264, timeval264);

                    if(LL_UCMP(weight164, <, weight264))
                    {
                        retval = __LINE__;
                    }
                    else if(LL_UCMP(weight164, >, weight264))
                    {
                        retval = - __LINE__;
                    }
                }
                break;

                case ST_SIZE:
                {
                    PRUint32 size1 = byteSize(alloc1);
                    PRUint32 size2 = byteSize(alloc2);

                    if(size1 < size2)
                    {
                        retval = __LINE__;
                    }
                    else if(size1 > size2)
                    {
                        retval = - __LINE__;
                    }
                }
                break;

                case ST_TIMEVAL:
                {
                    PRUint32 timeval1 = (alloc1->mMaxTimeval - alloc1->mMinTimeval);
                    PRUint32 timeval2 = (alloc2->mMaxTimeval - alloc2->mMinTimeval);

                    if(timeval1 < timeval2)
                    {
                        retval = __LINE__;
                    }
                    else if(timeval1 > timeval2)
                    {
                        retval = - __LINE__;
                    }
                }
                break;

                case ST_HEAPCOST:
                {
                    PRUint32 cost1 = alloc1->mHeapRuntimeCost;
                    PRUint32 cost2 = alloc2->mHeapRuntimeCost;

                    if(cost1 < cost2)
                    {
                        retval = __LINE__;
                    }
                    else if(cost1 > cost2)
                    {
                        retval = - __LINE__;
                    }
                }
                break;

                default:
                {
                    REPORT_ERROR(__LINE__, compareAllocations);
                }
                break;
            }
        }
    }

    return retval;
}

/*
** sortRun
**
** Given a run, sort it in the manner specified by the options.
** Returns !0 on failure.
*/
int sortRun(STRun* aRun)
{
    int retval = 0;

    if(NULL != aRun)
    {
        if(NULL != aRun->mAllocations && 0 < aRun->mAllocationCount)
        {
            NS_QuickSort(aRun->mAllocations, aRun->mAllocationCount, sizeof(STAllocation*), compareAllocations, NULL);
        }
    }
    else
    {
        retval = __LINE__;
        REPORT_ERROR(__LINE__, sortRun);
    }

    return retval;
}

/*
** createRun
**
** Returns a newly allocated run, properly initialized.
** Must call freeRun() with the new STRun.
**
** ONLY PASS IN A NON_ZERO STAMP IF YOU KNOW WHAT YOU ARE DOING!!!
** A non zero stamp in a run has side effects all over the
**  callsites of the allocations added to the run and their
**  parents.
**
** Returns NULL on failure.
*/
STRun* createRun(PRUint32 aStamp)
{
    STRun* retval = NULL;

    retval = (STRun*)calloc(1, sizeof(STRun));
    if(NULL != retval)
    {
        retval->mStats.mStamp = aStamp;
    }

    return retval;
}

/*
** freeRun
**
** Free off the run and the associated data.
*/
void freeRun(STRun* aRun)
{
    if(NULL != aRun)
    {
        if(NULL != aRun->mAllocations)
        {
            /*
            ** We do not free the allocations themselves.
            ** They are likely pointed to by at least 2 other existing
            **  runs.
            */
            free(aRun->mAllocations);
            aRun->mAllocations = NULL;
        }
        free(aRun);
        aRun = NULL;
    }
}

/*
** createRunFromGlobal
**
** Harvest the global run, then sort it.
** Returns NULL on failure.
** Must call freeRun() with the new STRun.
*/
STRun* createRunFromGlobal(void)
{
    STRun* retval = NULL;

    /*
    ** We stamp the run.
    ** As things are appended to it, it realizes that it should stamp the
    **  callsite backtrace with the information as well.
    ** In this manner, we can provide meaningful callsite data.
    */
    retval = createRun(PR_IntervalNow());

    if(NULL != retval)
    {
        int failure = 0;
        int harvestRes = harvestRun(&globals.mRun, retval, NULL);
        if(0 == harvestRes)
        {
            int sortRes = sortRun(retval);
            if(0 != sortRes)
            {
                failure = __LINE__;
            }
        }
        else
        {
            failure = __LINE__;
        }

        if(0 != failure)
        {
            freeRun(retval);
            retval = NULL;

            REPORT_ERROR(failure, createRunFromGlobal);
        }
    }

    return retval;
}

/*
** getLiveAllocationByHeapID
**
** Go through a run and find the right heap ID.
** At the time of the call to this function, the allocation must be LIVE,
**   meaning that it can not be freed.
** Go through the run backwards, in hopes of finding it near the end.
**
** Returns the allocation on success, otherwise NULL.
*/
STAllocation* getLiveAllocationByHeapID(STRun* aRun, PRUint32 aHeapID)
{
    STAllocation* retval = NULL;

    if(NULL != aRun && 0 != aHeapID)
    {
        PRUint32 traverse = aRun->mAllocationCount;
        STAllocation* eval = NULL;

        /*
        ** Go through in reverse order.
        ** Stop when we have a return value.
        */
        while(0 < traverse && NULL == retval)
        {
            /*
            ** Back up one to align with zero based index.
            */
            traverse--;

            /*
            ** Take the pointer math out of further operations.
            */
            eval = aRun->mAllocations[traverse];

            /*
            ** Take a look at the events in reverse order.
            ** Basically the last event must NOT be a free.
            ** The last event must NOT be a realloc of size zero (free).
            ** Otherwise, try to match up the heapID of the event.
            */
            if(0 != eval->mEventCount)
            {
                STAllocEvent* event = eval->mEvents + (eval->mEventCount - 1);

                switch(event->mEventType)
                {
                    case TM_EVENT_FREE:
                    {
                        /*
                        ** No freed allocation can match.
                        */
                    }
                    break;
                        
                    case TM_EVENT_REALLOC:
                    case TM_EVENT_CALLOC:
                    case TM_EVENT_MALLOC:
                    {
                        /*
                        ** Heap IDs must match.
                        */
                        if(aHeapID == event->mHeapID)
                        {
                            retval = eval;
                        }
                    }
                    break;

                    default:
                    {
                        REPORT_ERROR(__LINE__, getAllocationByHeapID);
                    }
                    break;
                }
            }
        }
    }
    else
    {
        REPORT_ERROR(__LINE__, getAllocationByHeapID);
    }

    return retval;
}

/*
** appendEvent
**
** Given an allocation, append a new event to it's lifetime.
** Returns the new event on success, otherwise NULL.
*/
STAllocEvent* appendEvent(STAllocation* aAllocation, PRUint32 aTimeval, char aEventType, PRUint32 aHeapID, PRUint32 aHeapSize, tmcallsite* aCallsite)
{
    STAllocEvent* retval = NULL;

    if(NULL != aAllocation && NULL != aCallsite)
    {
        STAllocEvent* expand = NULL;

        /*
        ** Expand the allocation's event array.
        */
        expand = (STAllocEvent*)realloc(aAllocation->mEvents, sizeof(STAllocEvent) * (aAllocation->mEventCount + 1));
        if(NULL != expand)
        {
            /*
            ** Reassign in case of pointer move.
            */
            aAllocation->mEvents = expand;

            /*
            ** Remove the pointer math from rest of code.
            */
            retval = aAllocation->mEvents + aAllocation->mEventCount;

            /*
            ** Increase event array count.
            */
            aAllocation->mEventCount++;

            /*
            ** Fill in the event.
            */
            retval->mTimeval = aTimeval;
            retval->mEventType = aEventType;
            retval->mHeapID = aHeapID;
            retval->mHeapSize = aHeapSize;
            retval->mCallsite = aCallsite;

            /*
            ** Allocation may need to update idea of lifetime.
            ** See allocationTracker to see mMinTimeval inited to ST_TIMEVAL_MAX.
            */
            if(aAllocation->mMinTimeval > aTimeval)
            {
                aAllocation->mMinTimeval = aTimeval;
            }

            /*
            ** This a free event?
            ** Can only set max timeval on a free.
            ** Otherwise, mMaxTimeval remains  ST_TIMEVAL_MAX.
            ** Set in allocationTracker.
            */
            if(TM_EVENT_FREE == aEventType)
            {
                aAllocation->mMaxTimeval = aTimeval;
            }
        }
        else
        {
            REPORT_ERROR(__LINE__, appendEvent);
        }
    }
    else
    {
        REPORT_ERROR(__LINE__, appendEvent);
    }

    return retval;
}

/*
** hasAllocation
**
** Determine if a given run has an allocation.
** This is really nothing more than a pointer comparison loop.
** Returns !0 if the run has the allocation.
*/
int hasAllocation(STRun* aRun, STAllocation* aTestFor)
{
    int retval = 0;

    if(NULL != aRun && NULL != aTestFor)
    {
        PRUint32 traverse = aRun->mAllocationCount;

        /*
        ** Go through reverse, in the hopes it exists nearer the end.
        */
        while(0 < traverse)
        {
            /*
            ** Back up.
            */
            traverse--;

            if(aTestFor == aRun->mAllocations[traverse])
            {
                retval = __LINE__;
                break;
            }
        }
    }
    else
    {
        REPORT_ERROR(__LINE__, hasAllocation);
    }

    return retval;
}

/*
** allocationTracker
**
** Important to keep track of all allocations unique so as to determine
**  their lifetimes.
**
** Returns a pointer to the allocation on success.
** Return NULL on failure.
*/
STAllocation* allocationTracker(PRUint32 aTimeval, char aType, PRUint32 aHeapRuntimeCost, tmcallsite* aCallsite, PRUint32 aHeapID, PRUint32 aSize, tmcallsite* aOldCallsite, PRUint32 aOldHeapID, PRUint32 aOldSize)
{
    STAllocation* retval = NULL;
    static int compactor = 1;
    const int frequency = 10000;
    PRUint32 actualSize, actualOldSize = 0;
    actualSize = actualByteSize(aSize);
    if (aOldSize)
        actualOldSize = actualByteSize(aOldSize);

    if(NULL != aCallsite)
    {
        int newAllocation = 0;
        tmcallsite* searchCallsite = NULL;
        PRUint32 searchHeapID = 0;
        STAllocation* allocation = NULL;

        /*
        ** Global operation ID increases.
        */
        globals.mOperationCount++;

        /*
        ** Fix up the timevals if needed.
        */
        if(aTimeval < globals.mMinTimeval)
        {
            globals.mMinTimeval = aTimeval;
        }
        if(aTimeval > globals.mMaxTimeval)
        {
            globals.mMaxTimeval = aTimeval;
        }

        switch(aType)
        {
            case TM_EVENT_FREE:
            {
                /*
                ** Update the global counter.
                */
                globals.mFreeCount++;

                /*
                ** Update our peak memory used counter
                */
                globals.mMemoryUsed -= actualSize;

                /*
                ** Not a new allocation, will need to search passed in site
                **  for the original allocation.
                */
                searchCallsite = aCallsite;
                searchHeapID = aHeapID;
            }
            break;

            case TM_EVENT_MALLOC:
            {
                /*
                ** Update the global counter.
                */
                globals.mMallocCount++;

                /*
                ** Update our peak memory used counter
                */
                globals.mMemoryUsed += actualSize;
                if (globals.mMemoryUsed > globals.mPeakMemoryUsed)
                {
                    globals.mPeakMemoryUsed = globals.mMemoryUsed;
                }

                /*
                ** This will be a new allocation.
                */
                newAllocation = __LINE__;
            }
            break;

            case TM_EVENT_CALLOC:
            {
                /*
                ** Update the global counter.
                */
                globals.mCallocCount++;

                /*
                ** Update our peak memory used counter
                */
                globals.mMemoryUsed += actualSize;
                if (globals.mMemoryUsed > globals.mPeakMemoryUsed)
                {
                    globals.mPeakMemoryUsed = globals.mMemoryUsed;
                }

                /*
                ** This will be a new allocation.
                */
                newAllocation = __LINE__;
            }
            break;

            case TM_EVENT_REALLOC:
            {
                /*
                ** Update the global counter.
                */
                globals.mReallocCount++;

                /*
                ** Update our peak memory used counter
                */
                globals.mMemoryUsed += actualSize - actualOldSize;
                if (globals.mMemoryUsed > globals.mPeakMemoryUsed)
                {
                    globals.mPeakMemoryUsed = globals.mMemoryUsed;
                }

                /*
                ** This might be a new allocation.
                */
                if(NULL == aOldCallsite)
                {
                    newAllocation = __LINE__;
                }
                else
                {
                    /*
                    ** Need to search for the original callsite for the
                    **  index to the allocation.
                    */
                    searchCallsite = aOldCallsite;
                    searchHeapID = aOldHeapID;
                }
            }
            break;

            default:
            {
                REPORT_ERROR(__LINE__, allocationTracker);
            }
            break;
        }

        /*
        ** We are either modifying an existing allocation or we are creating
        **  a new one.
        */
        if(0 != newAllocation)
        {
            allocation = (STAllocation*)calloc(1, sizeof(STAllocation));
            if(NULL != allocation)
            {
                /*
                ** Fixup the min timeval so if logic later will just work.
                */
                allocation->mMinTimeval = ST_TIMEVAL_MAX;
                allocation->mMaxTimeval = ST_TIMEVAL_MAX;
            }
        }
        else if(NULL != searchCallsite && NULL != CALLSITE_RUN(searchCallsite) && 0 != searchHeapID)
        {
            /*
            ** We know what to search for, and we reduce what we search
            **  by only looking for those allocations at a known callsite.
            */
            allocation = getLiveAllocationByHeapID(CALLSITE_RUN(searchCallsite), searchHeapID);
        }
        else
        {
            REPORT_ERROR(__LINE__, allocationTracker);
        }

        if(NULL != allocation)
        {
            STAllocEvent* appendResult = NULL;

            /*
            ** Record the amount of time this allocation event took.
            */
            allocation->mHeapRuntimeCost += aHeapRuntimeCost;

            /*
            ** Now that we have an allocation, we need to make sure it has
            **  the proper event.
            */
            appendResult = appendEvent(allocation, aTimeval, aType, aHeapID, aSize, aCallsite);
            if(NULL != appendResult)
            {
                if(0 != newAllocation)
                {
                    int runAppendResult = 0;
                    int callsiteAppendResult = 0;

                    /*
                    ** A new allocation needs to be added to the global run.
                    ** A new allocation needs to be added to the callsite.
                    */
                    runAppendResult = appendAllocation(&(globals.mRun), allocation);
                    callsiteAppendResult = appendAllocation(CALLSITE_RUN(aCallsite), allocation);
                    if(0 != runAppendResult && 0 != callsiteAppendResult)
                    {
                        /*
                        ** Success.
                        */
                        retval = allocation;
                    }
                    else
                    {
                        REPORT_ERROR(__LINE__, appendAllocation);
                    }
                }
                else
                {
                    /*
                    ** An existing allocation, if a realloc situation,
                    **  may need to be added to the new callsite.
                    ** This can only occur if the new and old callsites
                    **  differ.
                    ** Even then, a brute force check will need to be made
                    **  to ensure the allocation was not added twice; 
                    **  consider a realloc scenario where two different
                    **  call stacks bump the allocation back and forth.
                    */
                    if(aCallsite != searchCallsite)
                    {
                        int found = 0;

                        found = hasAllocation(CALLSITE_RUN(aCallsite), allocation);
                        if(0 == found)
                        {
                            int appendResult = 0;

                            appendResult = appendAllocation(CALLSITE_RUN(aCallsite), allocation);
                            if(0 != appendResult)
                            {
                                /*
                                ** Success.
                                */
                                retval = allocation;
                            }
                            else
                            {
                                REPORT_ERROR(__LINE__, appendAllocation);
                            }
                        }
                        else
                        {
                            /*
                            ** Already there.
                            */
                            retval = allocation;
                        }
                    }
                    else
                    {
                        /*
                        ** Success.
                        */
                        retval = allocation;
                    }
                }
            }
            else
            {
                REPORT_ERROR(__LINE__, appendEvent);
            }
        }
        else
        {
            REPORT_ERROR(__LINE__, allocationTracker);
        }
    }
    else
    {
        REPORT_ERROR(__LINE__, allocationTracker);
    }

    /*
    ** Compact the heap a bit if you can.
    */
    compactor++;
    if(0 == (compactor % frequency))
    {
        heapCompact();
    }

    return retval;
}

/*
** trackEvent
**
** An allocation event has dropped in on us.
** We need to do the right thing and track it.
*/
void trackEvent(PRUint32 aTimeval, char aType, PRUint32 aHeapRuntimeCost, tmcallsite* aCallsite, PRUint32 aHeapID, PRUint32 aSize, tmcallsite* aOldCallsite, PRUint32 aOldHeapID, PRUint32 aOldSize)
{
    if(NULL != aCallsite)
    {
        /*
        ** Verify the old callsite just in case.
        */
        if(NULL != CALLSITE_RUN(aCallsite) && (NULL == aOldCallsite || NULL != CALLSITE_RUN(aOldCallsite)))
        {
            STAllocation* allocation = NULL;

            /*
            ** Add to the allocation tracking code.
            */
            allocation = allocationTracker(aTimeval, aType, aHeapRuntimeCost, aCallsite, aHeapID, aSize, aOldCallsite, aOldHeapID, aOldSize);
        
            if(NULL == allocation)
            {
                REPORT_ERROR(__LINE__, allocationTracker);
            }
        }
        else
        {
            REPORT_ERROR(__LINE__, trackEvent);
        }
    }
    else
    {
        REPORT_ERROR(__LINE__, trackEvent);
    }
}

/*
** tmEventHandler
**
** Callback from the tmreader_eventloop function.
** Simply tries to sort out what we desire to know.
*/
void tmEventHandler(tmreader* aReader, tmevent* aEvent)
{
    if(NULL != aReader && NULL != aEvent)
    {
        switch(aEvent->type)
        {
            /*
            ** Events we ignore.
            */
            case TM_EVENT_LIBRARY:
            case TM_EVENT_METHOD:
            case TM_EVENT_STATS:
            case TM_EVENT_TIMESTAMP:
            case TM_EVENT_FILENAME:
                break;

            /*
            ** Allocation events need to be tracked.
            */
            case TM_EVENT_MALLOC:
            case TM_EVENT_CALLOC:
            case TM_EVENT_REALLOC:
            case TM_EVENT_FREE:
            {
                PRUint32 oldptr = 0;
                PRUint32 oldsize = 0;
                tmcallsite* callsite = NULL;
                tmcallsite* oldcallsite = NULL;

                if(TM_EVENT_REALLOC == aEvent->type)
                {
                    /*
                    ** Only care about old arguments if there were any.
                    */
                    if(0 != aEvent->u.alloc.oldserial)
                    {
                        oldptr = aEvent->u.alloc.oldptr;
                        oldsize = aEvent->u.alloc.oldsize;
                        oldcallsite = tmreader_callsite(aReader, aEvent->u.alloc.oldserial);
                        if(NULL == oldcallsite)
                        {
                            REPORT_ERROR(__LINE__, tmreader_callsite);
                        }
                    }
                }

                callsite = tmreader_callsite(aReader, aEvent->serial);
                if(NULL != callsite)
                {
                    /*
                    ** Verify a callsite run is there.
                    ** If not, we are ignoring this callsite.
                    */
                    if(NULL != CALLSITE_RUN(callsite))
                    {
                        char eventType = aEvent->type;
                        
                        /*
                        ** Play a nasty trick on reallocs of size zero.
                        ** They are to become free events.
                        ** This allows me to avoid all types of special case code.
                        */
                        if(0 == aEvent->u.alloc.size && TM_EVENT_REALLOC == aEvent->type)
                        {
                            eventType = TM_EVENT_FREE;
                        }
                        trackEvent(ticks2msec(aReader, aEvent->u.alloc.interval), eventType, ticks2usec(aReader, aEvent->u.alloc.cost), callsite, aEvent->u.alloc.ptr, aEvent->u.alloc.size, oldcallsite, oldptr, oldsize);
                    }
                }
                else
                {
                    REPORT_ERROR(__LINE__, tmreader_callsite);
                }
            }
            break;

            /*
            ** Callsite, set up the callsite run if it does not exist.
            */
            case TM_EVENT_CALLSITE:
            {
                tmcallsite* callsite = tmreader_callsite(aReader, aEvent->serial);

                if(NULL != callsite)
                {
                    if(NULL == CALLSITE_RUN(callsite))
                    {
                        int createrun = __LINE__;

#if defined(MOZILLA_CLIENT)
                        /*
                        ** For a mozilla spacetrace, ignore this particular
                        **  callsite as it is just noise, and causes us to
                        **  use a lot of memory.
                        **
                        ** This callsite is present on the linux build,
                        **  not sure if the other platforms have it.
                        */
                        if(0 != hasCallsiteMatch(callsite, "g_main_is_running", ST_FOLLOW_PARENTS))
                        {
                            createrun = 0;
                        }
#endif /* MOZILLA_CLIENT */

                        if(0 != createrun)
                        {
                            callsite->data = createRun(0);
                        }
                    }
                }
                else
                {
                    REPORT_ERROR(__LINE__, tmreader_callsite);
                }
            }
            break;

            /*
            ** Unhandled events should not be allowed.
            */
            default:
            {
                REPORT_ERROR(__LINE__, tmEventHandler);
            }
            break;
        }
    }
}

/*
** htmlAnchor
**
** Output an HTML anchor, or just the text depending on the mode.
*/
void htmlAnchor(const char* aHref, const char* aText, const char* aTarget)
{
    if(NULL == aTarget || '\0' == aTarget[0])
    {
        aTarget = "_st_content";
    }

    if(NULL != aHref && '\0' != *aHref && NULL != aText && '\0' != *aText)
    {
        int anchorLive = 1;

        /*
        ** In batch mode, we need to verify the anchor is live.
        */
        if(0 != globals.mOptions.mBatchRequestCount)
        {
            PRUint32 loop = 0;
            int comparison = 1;
            
            for(loop = 0; loop < globals.mOptions.mBatchRequestCount; loop++)
            {
                comparison = strcmp(aHref, globals.mOptions.mBatchRequests[loop]);
                if(0 == comparison)
                {
                    break;
                }
            }
            
            /*
            ** Did we find it?
            */
            if(0 == comparison)
            {
                anchorLive = 0;
            }
        }

        /*
        ** In any mode, don't make an href to the current page.
        */
        if(0 != anchorLive && NULL != globals.mRequest.mFileName)
        {
            if(0 == strcmp(aHref, globals.mRequest.mFileName))
            {
                anchorLive = 0;
            }
        }
        
        /*
        ** Do the right thing.
        */
        if(0 != anchorLive)
        {
            PR_fprintf(globals.mRequest.mFD, "<a target=\"%s\" href=\"./%s\">%s</a>\n", aTarget, aHref, aText);
        }
        else
        {
            PR_fprintf(globals.mRequest.mFD, "%s\n", aText);
        }
    }
    else
    {
        REPORT_ERROR(__LINE__, htmlAnchor);
    }
}

/*
** htmlAllocationAnchor
**
** Output an html achor that will resolve to the allocation in question.
*/
void htmlAllocationAnchor(STAllocation* aAllocation, const char* aText)
{
    if(NULL != aAllocation && NULL != aText && '\0' != *aText)
    {
        char buffer[128];

        /*
        ** This is a total hack.
        ** The filename contains the index of the allocation in globals.mRun.
        ** Safer than using the raw pointer value....
        */
        PR_snprintf(buffer, sizeof(buffer), "allocation_%u.html", aAllocation->mRunIndex);

        htmlAnchor(buffer, aText, NULL);
    }
    else
    {
        REPORT_ERROR(__LINE__, htmlAllocationAnchor);
    }
}

/*
** resolveSourceFile
**
** Easy way to get a readable/short name.
** NULL if not present, not resolvable.
*/
const char* resolveSourceFile(tmmethodnode* aMethod)
{
    const char* retval = NULL;

    if(NULL != aMethod)
    {
        const char* methodSays = NULL;

        methodSays = aMethod->sourcefile;

        if(NULL != methodSays && '\0' != methodSays[0] && 0 != strcmp("noname", methodSays))
        {
            retval = strrchr(methodSays, '/');
            if(NULL != retval)
            {
                retval++;
            }
            else
            {
                retval = methodSays;
            }
        }
    }

    return retval;
}

/*
** htmlCallsiteAnchor
**
** Output an html anchor that will resolve to the callsite in question.
** If no text is provided, we provide our own.
**
** RealName determines wether or not we crawl our parents until the point
**  we no longer match stats.
*/
void htmlCallsiteAnchor(tmcallsite* aCallsite, const char* aText, int aRealName)
{
    if(NULL != aCallsite)
    {
        char textBuf[512];
        char hrefBuf[128];
        tmcallsite* namesite = aCallsite;

        /*
        ** Should we use a different name?
        */
        if(0 == aRealName && NULL != namesite->parent && NULL != namesite->parent->method)
        {
            STRun* myRun = NULL;
            STRun* upRun = NULL;

            do
            {
                myRun = CALLSITE_RUN(namesite);
                upRun = CALLSITE_RUN(namesite->parent);
                
                if(0 != memcmp(&myRun->mStats, &upRun->mStats, sizeof(STCallsiteStats)))
                {
                    /*
                    ** Doesn't match, stop.
                    */
                    break;
                }
                else
                {
                    /*
                    ** Matches, keep going up.
                    */
                    namesite = namesite->parent;
                }
            }
            while(NULL != namesite->parent && NULL != namesite->parent->method);
        }

        /*
        ** If no text, provide our own.
        */
        if(NULL == aText || '\0' == *aText)
        {
            const char* methodName = NULL;
            const char* sourceFile = NULL;

            if(NULL != namesite->method)
            {
                methodName = tmmethodnode_name(namesite->method);
            }
            else
            {
                methodName = "==NONAME==";
            }

            /*
            ** Decide which format to use to identify the callsite.
            ** If we can detect availability, hook up the filename with lxr information.
            */
            sourceFile = resolveSourceFile(namesite->method);
            if(NULL != sourceFile && 0 == strncmp("mozilla/", namesite->method->sourcefile, 8))
            {
                char lxrHREFBuf[512];

                PR_snprintf(lxrHREFBuf, sizeof(lxrHREFBuf), "<a href=\"http://lxr.mozilla.org/mozilla/source/%s#%u\" target=\"_st_lxr\">(%s:%u)</a>", namesite->method->sourcefile + 8, namesite->method->linenumber, sourceFile, namesite->method->linenumber);
                PR_snprintf(textBuf, sizeof(textBuf), "<b>%s</b>%s", methodName, lxrHREFBuf);
            }
            else if(NULL != sourceFile)
            {
                PR_snprintf(textBuf, sizeof(textBuf), "<b>%s</b>(%s:%u)", methodName, sourceFile, namesite->method->linenumber);
            }
            else
            {
                PR_snprintf(textBuf, sizeof(textBuf), "<b>%s</b>+%u(%u)", methodName, namesite->offset, (PRUint32)namesite->entry.key);
            }

            aText = textBuf;
        }

        PR_snprintf(hrefBuf, sizeof(hrefBuf), "callsite_%u.html", (PRUint32)aCallsite->entry.key);

        htmlAnchor(hrefBuf, aText, NULL);
    }
    else
    {
        REPORT_ERROR(__LINE__, htmlCallsiteAnchor);
    }
}

/*
** htmlHeader
**
** Output a standard header in the report files.
*/
void htmlHeader(const char* aTitle)
{
    PR_fprintf(globals.mRequest.mFD,
"<html>\n"
"<head>\n"
"<title>%s</title>\n"
"</head>\n"
"<body>\n"
"<div align=right>\n"
               , aTitle);

    htmlAnchor("index.html", "[Index]", NULL);
    htmlAnchor("options.html", "[Options]", NULL);

    /*
    ** This is a dubious feature at best.
    */
#if WANT_QUIT
    htmlAnchor("quit.html", "[Quit]", NULL);
#endif

    PR_fprintf(globals.mRequest.mFD, "</div>\n<hr>\n");
}

/*
** htmlFooter
**
** Output a standard footer in the report file.
*/
void htmlFooter(void)
{
    PR_fprintf(globals.mRequest.mFD,
"<hr>\n"
"<div align=right>\n"
"<i>SpaceTrace</i>\n"
"</div>\n"
"</body>\n"
"</html>\n"
        );
}

/*
** htmlNotFound
**
** Not found message.
*/
void htmlNotFound(void)
{
    htmlHeader("File Not Found");
    PR_fprintf(globals.mRequest.mFD, "File Not Found\n");
    htmlFooter();
}

/*
** callsiteArrayFromCallsite
**
** Simply return an array of the callsites divulged from the site passed in,
**  including the site passed in.
** Do not worry about dups, or the order of the items.
**
** Returns the number of items in the array.
** If the same as aExistingCount, then nothing happened.
*/
PRUint32 callsiteArrayFromCallsite(tmcallsite*** aArray, PRUint32 aExistingCount, tmcallsite* aSite, int aFollow)
{
    PRUint32 retval = 0;

    if(NULL != aArray && NULL != aSite)
    {
        tmcallsite** expand = NULL;

        /*
        ** If we have an existing count, we just keep expanding this.
        */
        retval = aExistingCount;

        /*
        ** Go through every allocation.
        */
        do
        {
            /*
            ** expand the array.
            */
            expand = (tmcallsite**)realloc(*aArray, sizeof(tmcallsite*) * (retval + 1));
            if(NULL != expand)
            {
                /*
                ** Set the callsite in case of pointer move.
                */
                *aArray = expand;
                
                /*
                ** Assign the value.
                */
                (*aArray)[retval] = aSite;
                retval++;
            }
            else
            {
                REPORT_ERROR(__LINE__, realloc);
                break;
            }
            

            /*
            ** What do we follow?
            */
            switch(aFollow)
            {
                case ST_FOLLOW_SIBLINGS:
                    aSite = aSite->siblings;
                    break;
                case ST_FOLLOW_PARENTS:
                    aSite = aSite->parent;
                    break;
                default:
                    aSite = NULL;
                    REPORT_ERROR(__LINE__, callsiteArrayFromCallsite);
                    break;
            }
        }
        while(NULL != aSite && NULL != aSite->method);
    }

    return retval;
}

/*
** callsiteArrayFromRun
**
** Simply return an array of the callsites from the run allocations.
** We only pay attention to callsites that were not free callsites.
** Do not worry about dups, or the order of the items.
**
** Returns the number of items in the array.
** If the same as aExistingCount, then nothing happened.
*/
PRUint32 callsiteArrayFromRun(tmcallsite*** aArray, PRUint32 aExistingCount, STRun* aRun)
{
    PRUint32 retval = 0;

    if(NULL != aArray && NULL != aRun && 0 < aRun->mAllocationCount)
    {
        PRUint32 allocLoop = 0;
        PRUint32 eventLoop = 0;
        int stopLoops = 0;

        /*
        ** If we have an existing count, we just keep expanding this.
        */
        retval = aExistingCount;

        /*
        ** Go through every allocation.
        */
        for(allocLoop = 0; 0 == stopLoops && allocLoop < aRun->mAllocationCount; allocLoop++)
        {
            /*
            ** Go through every event.
            */
            for(eventLoop = 0; 0 == stopLoops && eventLoop < aRun->mAllocations[allocLoop]->mEventCount; eventLoop++)
            {
                /*
                ** Skip the free events.
                */
                if(TM_EVENT_FREE != aRun->mAllocations[allocLoop]->mEvents[eventLoop].mEventType)
                {
                    tmcallsite** expand = NULL;

                    /*
                    ** expand the array.
                    */
                    expand = (tmcallsite**)realloc(*aArray, sizeof(tmcallsite*) * (retval + 1));
                    if(NULL != expand)
                    {
                        /*
                        ** Set the callsite in case of pointer move.
                        */
                        *aArray = expand;
                        
                        /*
                        ** Assign the value.
                        */
                        (*aArray)[retval] = aRun->mAllocations[allocLoop]->mEvents[eventLoop].mCallsite;
                        retval++;
                    }
                    else
                    {
                        REPORT_ERROR(__LINE__, realloc);
                        stopLoops = __LINE__;
                    }
                }
            }
        }
    }

    return retval;
}

/*
** getDataPRUint*
**
** Helper to avoid cut and paste code.
** Failure to find aCheckFor does not mean failure.
** Returns !0 on failure.
*/
int getDataPRUint32Base(const char* aGetData, const char* aCheckFor, void* aStoreResult, PRUint32 aBits)
{
    int retval = 0;

    if(NULL != aGetData && NULL != aCheckFor && NULL != aStoreResult)
    {
        const char* found = NULL;
        PRInt32 scanRes = 0;
    
        /*
        ** Looking for the presence.
        */
        found = strstr(aGetData, aCheckFor);
        if(NULL != found)
        {
            int length = 0;
            const char* dataPoint = NULL;
            
            /*
            ** Skip the varname.
            ** Skip the '=' sign.
            */
            length = strlen(aCheckFor);
            dataPoint = found + length + 1;
            
            /*
            ** Just attempt to scan from here.
            */
            if(64 == aBits)
            {
                scanRes = PR_sscanf(dataPoint, "%llu", aStoreResult);
            }
            else
            {
                scanRes = PR_sscanf(dataPoint, "%u", aStoreResult);
            }
            if(1 != scanRes)
            {
                retval = __LINE__;
                REPORT_ERROR(__LINE__, PR_sscanf);
            }
        }
    }
    else
    {
        retval = __LINE__;
        REPORT_ERROR(__LINE__, getDataPRUint32);
    }

    return retval;
}
int getDataPRUint32(const char* aGetData, const char* aCheckFor, PRUint32* aStoreResult, int* aChanged, PRUint32 aConversion)
{
    int retval = 0;
    PRUint32 value = *aStoreResult;

    retval =  getDataPRUint32Base(aGetData, aCheckFor, aStoreResult, 32);

    *aStoreResult *= aConversion;
    if(NULL != aChanged && value != *aStoreResult)
    {
        (*aChanged) = (*aChanged) + 1;
    }

    return retval;
}
int getDataPRUint64(const char* aGetData, const char* aCheckFor, PRUint64* aStoreResult64, int* aChanged)
{
    int retval = 0;
    PRUint64 value64 = *aStoreResult64;

    retval = getDataPRUint32Base(aGetData, aCheckFor, aStoreResult64, 64);
    if(NULL != aChanged && LL_NE(value64, *aStoreResult64))
    {
        (*aChanged) = (*aChanged) + 1;
    }

    return retval;
}

/*
** getDataString
**
** Pull out the string data, if specified.
** Return !0 on failure.
*/
int getDataString(const char* aGetData, const char* aCheckFor, char** aStoreResult, int* aChanged)
{
    int retval = 0;

    if(NULL != aGetData && NULL != aCheckFor && NULL != aStoreResult)
    {
        const char* found = NULL;
        PRInt32 scanRes = 0;
    
        /*
        ** Check for presence.
        */
        found = strstr(aGetData, aCheckFor);
        if(NULL != found)
        {
            int length = 0;
            const char* dataPoint = NULL;
            const char* endPoint = NULL;
            char* oldResult = NULL;
            int theLen = 0;
            
            /*
            ** Skip the varname.
            ** Skip the '=' sign.
            */
            length = strlen(aCheckFor);
            dataPoint = found + length + 1;
            
            /*
            ** The length is up to a '&' or until end of string.
            */
            endPoint = strchr(dataPoint, '&');
            if(NULL == endPoint)
            {
                endPoint = dataPoint + strlen(dataPoint);
            }

            /*
            ** Store original value if present.
            */
            if(NULL != *aStoreResult)
            {
                oldResult = *aStoreResult;
            }

            /*
            ** Allocate space for new string.
            */
            theLen = (int)(endPoint - dataPoint);
            *aStoreResult = (char*)malloc((size_t)(theLen + 1));
            if(NULL != *aStoreResult)
            {
                int index1 = 0;
                int index2 = 0;

                strncpy(*aStoreResult, dataPoint, theLen);
                (*aStoreResult)[theLen] = '\0';

                /*
                ** Here's a totally suboptimal and bug prone unhexcaper.
                */
                for(; index1 <= theLen; index1++)
                {
                    if('%' == (*aStoreResult)[index1] && '\0' != (*aStoreResult)[index1 + 1] && '\0' != (*aStoreResult)[index1 + 2])
                    {
                        int unhex = 0;

                        if('9' >= (*aStoreResult)[index1 + 1])
                        {
                            unhex |= (((*aStoreResult)[index1 + 1] - '0') << 4);
                        }
                        else
                        {
                            unhex |= ((toupper((*aStoreResult)[index1 + 1]) - 'A' + 10) << 4);
                        }

                        if('9' >= (*aStoreResult)[index1 + 2])
                        {
                            unhex |= ((*aStoreResult)[index1 + 2] - '0');
                        }
                        else
                        {
                            unhex |= (toupper((*aStoreResult)[index1 + 2]) - 'A' + 10);
                        }

                        index1 += 2;
                        (*aStoreResult)[index1] = unhex;
                    }

                    (*aStoreResult)[index2++] = (*aStoreResult)[index1];
                }

                /*
                ** See if the value actually changed.
                */
                if(NULL != aChanged)
                {
                    if(NULL != oldResult)
                    {
                        if(0 != strcmp(oldResult, *aStoreResult))
                        {
                            (*aChanged) = (*aChanged) + 1;
                        }
                    }
                    else if('\0' != (*aStoreResult)[0])
                    {
                        (*aChanged) = (*aChanged) + 1;
                    }
                }

                /*
                ** Free off the prior value if relevant.
                */
                if(NULL != oldResult)
                {
                    free(oldResult);
                    oldResult = NULL;
                }
            }
            else
            {
                retval = __LINE__;
                REPORT_ERROR(__LINE__, malloc);
            }
        }
    }
    else
    {
        retval = __LINE__;
        REPORT_ERROR(__LINE__, getDataPRUint32);
    }

    return retval;
}

/*
** displayTopAllocations
**
** Present the top allocations.
** The run must be passed in, and it must be pre-sorted.
**
** Returns !0 on failure.
*/
int displayTopAllocations(STRun* aRun, int aWantCallsite)
{
    int retval = 0;

    if(NULL != aRun)
    {
        if(0 < aRun->mAllocationCount)
        {
            PRUint32 loop = 0;
            STAllocation* current = NULL;

            PR_fprintf(globals.mRequest.mFD, "<table border=1>\n");
            PR_fprintf(globals.mRequest.mFD, "<tr>\n");
            PR_fprintf(globals.mRequest.mFD, "<td><b>Rank</b></td>\n");
            PR_fprintf(globals.mRequest.mFD, "<td><b>Index</b></td>\n");
            PR_fprintf(globals.mRequest.mFD, "<td><b>Byte Size</b></td>\n");
            PR_fprintf(globals.mRequest.mFD, "<td><b>Lifespan Seconds</b></td>\n");
            PR_fprintf(globals.mRequest.mFD, "<td><b>Weight</b></td>\n");
            PR_fprintf(globals.mRequest.mFD, "<td><b>Heap Operation Seconds</b></td>\n");
            if(0 != aWantCallsite)
            {
                PR_fprintf(globals.mRequest.mFD, "<td><b>Origin Callsite</b></td>\n");
            }
            PR_fprintf(globals.mRequest.mFD, "</tr>\n");

            /*
            ** Loop over the items, up to some limit or until the end.
            */
            for(loop = 0; loop < globals.mOptions.mListItemMax && loop < aRun->mAllocationCount; loop++)
            {
                current = aRun->mAllocations[loop];
                if(NULL != current)
                {
                    PRUint32 lifespan = current->mMaxTimeval - current->mMinTimeval;
                    PRUint32 size = byteSize(current);
                    PRUint32 heapCost = current->mHeapRuntimeCost;
                    PRUint64 weight64 = LL_INIT(0, 0);
                    PRUint64 size64 = LL_INIT(0, 0);
                    PRUint64 lifespan64 = LL_INIT(0, 0);
                    char buffer[32];

                    LL_UI2L(size64, size);
                    LL_UI2L(lifespan64, lifespan);
                    LL_MUL(weight64, size64, lifespan64);

                    PR_fprintf(globals.mRequest.mFD, "<tr>\n");

                    /*
                    ** Rank.
                    */
                    PR_fprintf(globals.mRequest.mFD, "<td align=right>%u</td>\n", loop + 1);

                    /*
                    ** Index.
                    */
                    PR_snprintf(buffer, sizeof(buffer), "%u", current->mRunIndex);
                    PR_fprintf(globals.mRequest.mFD, "<td align=right>\n");
                    htmlAllocationAnchor(current, buffer);
                    PR_fprintf(globals.mRequest.mFD, "</td>\n");

                    /*
                    ** Byte Size.
                    */
                    PR_fprintf(globals.mRequest.mFD, "<td align=right>%u</td>\n", size);

                    /*
                    ** Lifespan.
                    */
                    PR_fprintf(globals.mRequest.mFD, "<td align=right>" ST_TIMEVAL_FORMAT "</td>\n", ST_TIMEVAL_PRINTABLE(lifespan));

                    /*
                    ** Weight.
                    */
                    PR_fprintf(globals.mRequest.mFD, "<td align=right>%llu</td>\n", weight64);

                    /*
                    ** Heap operation cost.
                    */
                    PR_fprintf(globals.mRequest.mFD, "<td align=right>" ST_MICROVAL_FORMAT "</td>\n", ST_MICROVAL_PRINTABLE(heapCost));

                    if(0 != aWantCallsite)
                    {
                        /*
                        ** Callsite.
                        */
                        PR_fprintf(globals.mRequest.mFD, "<td>");
                        htmlCallsiteAnchor(current->mEvents[0].mCallsite, NULL, 0);
                        PR_fprintf(globals.mRequest.mFD, "</td>\n");
                    }

                    PR_fprintf(globals.mRequest.mFD, "</tr>\n");
                }
            }

            PR_fprintf(globals.mRequest.mFD, "</table>\n");
        }
    }
    else
    {
        retval = __LINE__;
        REPORT_ERROR(__LINE__, displayTopAllocations);
    }

    return retval;
}

/*
** displayMemoryLeaks
**
** Present the top memory leaks.
** The run must be passed in, and it must be pre-sorted.
**
** Returns !0 on failure.
*/
int displayMemoryLeaks(STRun* aRun)
{
    int retval = 0;

    if(NULL != aRun)
    {
        PRUint32 loop = 0;
        PRUint32 displayed = 0;
        STAllocation* current = NULL;

        PR_fprintf(globals.mRequest.mFD, "<table border=1>\n");
        PR_fprintf(globals.mRequest.mFD, "<tr>\n");
        PR_fprintf(globals.mRequest.mFD, "<td><b>Rank</b></td>\n");
        PR_fprintf(globals.mRequest.mFD, "<td><b>Index</b></td>\n");
        PR_fprintf(globals.mRequest.mFD, "<td><b>Byte Size</b></td>\n");
        PR_fprintf(globals.mRequest.mFD, "<td><b>Lifespan Seconds</b></td>\n");
        PR_fprintf(globals.mRequest.mFD, "<td><b>Weight</b></td>\n");
        PR_fprintf(globals.mRequest.mFD, "<td><b>Heap Operation Seconds</b></td>\n");
        PR_fprintf(globals.mRequest.mFD, "<td><b>Origin Callsite</b></td>\n");
        PR_fprintf(globals.mRequest.mFD, "</tr>\n");

        /*
        ** Loop over all of the items, or until we've displayed enough.
        */
        for(loop = 0; displayed < globals.mOptions.mListItemMax && loop < aRun->mAllocationCount; loop++)
        {
            current = aRun->mAllocations[loop];
            if(NULL != current && 0 != current->mEventCount)
            {
                /*
                ** In order to be a leak, the last event of it's life must
                **  NOT be a free operation.
                **
                ** A free operation is just that, a free.
                */
                if(TM_EVENT_FREE != current->mEvents[current->mEventCount - 1].mEventType)
                {
                    PRUint32 lifespan = current->mMaxTimeval - current->mMinTimeval;
                    PRUint32 size = byteSize(current);
                    PRUint32 heapCost = current->mHeapRuntimeCost;
                    PRUint64 weight64 = LL_INIT(0, 0);
                    PRUint64 size64 = LL_INIT(0, 0);
                    PRUint64 lifespan64 = LL_INIT(0, 0);
                    char buffer[32];
                    
                    LL_UI2L(size64, size);
                    LL_UI2L(lifespan64, lifespan);
                    LL_MUL(weight64, size64, lifespan64);

                    /*
                    ** One more shown.
                    */
                    displayed++;
                    
                    PR_fprintf(globals.mRequest.mFD, "<tr>\n");
                    
                    /*
                    ** Rank.
                    */
                    PR_fprintf(globals.mRequest.mFD, "<td align=right>%u</td>\n", displayed);

                    /*
                    ** Index.
                    */
                    PR_snprintf(buffer, sizeof(buffer), "%u", current->mRunIndex);
                    PR_fprintf(globals.mRequest.mFD, "<td align=right>\n");
                    htmlAllocationAnchor(current, buffer);
                    PR_fprintf(globals.mRequest.mFD, "</td>\n");
                    
                    /*
                    ** Byte Size.
                    */
                    PR_fprintf(globals.mRequest.mFD, "<td align=right>%u</td>\n", size);
                    
                    /*
                    ** Lifespan.
                    */
                    PR_fprintf(globals.mRequest.mFD, "<td align=right>" ST_TIMEVAL_FORMAT "</td>\n", ST_TIMEVAL_PRINTABLE(lifespan));
                    
                    /*
                    ** Weight.
                    */
                    PR_fprintf(globals.mRequest.mFD, "<td align=right>%llu</td>\n", weight64);
                    
                    /*
                    ** Heap Operation Seconds.
                    */
                    PR_fprintf(globals.mRequest.mFD, "<td align=right>" ST_MICROVAL_FORMAT "</td>\n", ST_MICROVAL_PRINTABLE(heapCost));

                    /*
                    ** Callsite.
                    */
                    PR_fprintf(globals.mRequest.mFD, "<td>");
                    htmlCallsiteAnchor(current->mEvents[0].mCallsite, NULL, 0);
                    PR_fprintf(globals.mRequest.mFD, "</td>\n");
                    
                    PR_fprintf(globals.mRequest.mFD, "</tr>\n");
                }
            }
        }

        PR_fprintf(globals.mRequest.mFD, "</table>\n");
    }
    else
    {
        retval = __LINE__;
        REPORT_ERROR(__LINE__, displayMemoryLeaks);
    }

    return retval;
}

/*
** displayCallsites
**
** Display a table of callsites.
** If the stamp is non zero, then must match that stamp.
** If the stamp is zero, then must match the global sorted run stamp.
** Return !0 on error.
*/
int displayCallsites(tmcallsite* aCallsite, int aFollow, PRUint32 aStamp, int aRealNames)
{
    int retval = 0;

    if(NULL != aCallsite && NULL != aCallsite->method)
    {
        int headerDisplayed = 0;
        STRun* run = NULL;

        /*
        ** Corrent the stamp if need be.
        */
        if(0 == aStamp && NULL != globals.mCache.mSortedRun)
        {
            aStamp = globals.mCache.mSortedRun->mStats.mStamp;
        }

        /*
        ** Loop over the callsites looking for a stamp match.
        ** A stamp guarantees there is something interesting to look at too.
        ** If found, output it.
        */
        while(NULL != aCallsite && NULL != aCallsite->method)
        {
            run = CALLSITE_RUN(aCallsite);
            if(NULL != run)
            {
                if(aStamp == run->mStats.mStamp)
                {
                    /*
                    ** We got a header?
                    */
                    if(0 == headerDisplayed)
                    {
                        headerDisplayed = __LINE__;

                        PR_fprintf(globals.mRequest.mFD, "<table border=1>\n");
                        PR_fprintf(globals.mRequest.mFD, "<tr>\n");
                        PR_fprintf(globals.mRequest.mFD, "<td><b>Callsite</b></td>\n");
                        PR_fprintf(globals.mRequest.mFD, "<td><b>Composite Byte Size</b></td>\n");
                        PR_fprintf(globals.mRequest.mFD, "<td><b>Composite Seconds</b></td>\n");
                        PR_fprintf(globals.mRequest.mFD, "<td><b>Composite Weight</b></td>\n");
                        PR_fprintf(globals.mRequest.mFD, "<td><b>Heap Object Count</b></td>\n");
                        PR_fprintf(globals.mRequest.mFD, "<td><b>Composite Heap Operation Seconds</b></td>\n");
                        PR_fprintf(globals.mRequest.mFD, "</tr>\n");
                    }

                    /*
                    ** Output the information.
                    */
                    PR_fprintf(globals.mRequest.mFD, "<tr>\n");

                    /*
                    ** Method name.
                    */
                    PR_fprintf(globals.mRequest.mFD, "<td>");
                    htmlCallsiteAnchor(aCallsite, NULL, aRealNames);
                    PR_fprintf(globals.mRequest.mFD, "</td>");

                    /*
                    ** Byte Size.
                    */
                    PR_fprintf(globals.mRequest.mFD, "<td valign=top align=right>%u</td>\n", run->mStats.mSize);

                    /*
                    ** Seconds.
                    */
                    PR_fprintf(globals.mRequest.mFD, "<td valign=top align=right>" ST_TIMEVAL_FORMAT "</td>\n", ST_TIMEVAL_PRINTABLE64(run->mStats.mTimeval64));

                    /*
                    ** Weight.
                    */
                    PR_fprintf(globals.mRequest.mFD, "<td valign=top align=right>%llu</td>\n", run->mStats.mWeight64);
                    
                    /*
                    ** Allocation object count.
                    */
                    PR_fprintf(globals.mRequest.mFD, "<td valign=top align=right>%u</td>\n", run->mStats.mCompositeCount);

                    /*
                    ** Heap Operation Seconds.
                    */
                    PR_fprintf(globals.mRequest.mFD, "<td valign=top align=right>" ST_MICROVAL_FORMAT "</td>\n", ST_MICROVAL_PRINTABLE(run->mStats.mHeapRuntimeCost));

                    PR_fprintf(globals.mRequest.mFD, "</tr>\n");
                }
            }
            else
            {
                retval = __LINE__;
                REPORT_ERROR(__LINE__, displayCallsites);
                break;
            }

            /*
            ** What do we follow?
            */
            switch(aFollow)
            {
                case ST_FOLLOW_SIBLINGS:
                    aCallsite = aCallsite->siblings;
                    break;
                case ST_FOLLOW_PARENTS:
                    aCallsite = aCallsite->parent;
                    break;
                default:
                    aCallsite = NULL;
                    retval = __LINE__;
                    REPORT_ERROR(__LINE__, displayCallsites);
                    break;
            }
        }

        /*
        ** Terminate the table if we should.
        */
        if(0 != headerDisplayed)
        {
            PR_fprintf(globals.mRequest.mFD, "</table>\n");
        }
    }
    else
    {
        retval = __LINE__;
        REPORT_ERROR(__LINE__, displayCallsites);
    }

    return retval;
}

/*
** displayAllocationDetails
**
** Report what we know about the allocation.
**
** Returns !0 on error.
*/
int displayAllocationDetails(STAllocation* aAllocation)
{
    int retval = 0;

    if(NULL != aAllocation)
    {
        PRUint32 traverse = 0;
        PRUint32 bytesize = byteSize(aAllocation);
        PRUint32 timeval = aAllocation->mMaxTimeval - aAllocation->mMinTimeval;
        PRUint32 heapCost = aAllocation->mHeapRuntimeCost;
        PRUint64 weight64 = LL_INIT(0, 0);
        PRUint64 bytesize64 = LL_INIT(0, 0);
        PRUint64 timeval64 = LL_INIT(0, 0);
        PRUint32 cacheval = 0;
        int displayRes = 0;

        LL_UI2L(bytesize64, bytesize);
        LL_UI2L(timeval64, timeval);
        LL_MUL(weight64, bytesize64, timeval64);

        PR_fprintf(globals.mRequest.mFD, "Allocation %u Details:<p>\n", aAllocation->mRunIndex);

        PR_fprintf(globals.mRequest.mFD, "<table>\n");
        PR_fprintf(globals.mRequest.mFD, "<tr><td align=left>Final Size:</td><td align=right>%u</td></tr>\n", bytesize);
        PR_fprintf(globals.mRequest.mFD, "<tr><td align=left>Lifespan Seconds:</td><td align=right>" ST_TIMEVAL_FORMAT "</td></tr>\n", ST_TIMEVAL_PRINTABLE(timeval));
        PR_fprintf(globals.mRequest.mFD, "<tr><td align=left>Weight:</td><td align=right>%llu</td></tr>\n", weight64);
        PR_fprintf(globals.mRequest.mFD, "<tr><td align=left>Heap Operation Seconds:</td><td align=right>" ST_MICROVAL_FORMAT "</td></tr>\n", ST_MICROVAL_PRINTABLE(heapCost));
        PR_fprintf(globals.mRequest.mFD, "</table><p>\n");

        /*
        ** The events.
        */
        PR_fprintf(globals.mRequest.mFD, "%u Life Event(s):<br>\n", aAllocation->mEventCount);
        PR_fprintf(globals.mRequest.mFD, "<table border=1>\n");
        PR_fprintf(globals.mRequest.mFD, "<tr>\n");
        PR_fprintf(globals.mRequest.mFD, "<td></td>\n");
        PR_fprintf(globals.mRequest.mFD, "<td><b>Operation</b></td>\n");
        PR_fprintf(globals.mRequest.mFD, "<td><b>Size</b></td>\n");
        PR_fprintf(globals.mRequest.mFD, "<td><b>Seconds</b></td>\n");
        PR_fprintf(globals.mRequest.mFD, "<td></td>\n");
        PR_fprintf(globals.mRequest.mFD, "</tr>\n");

        for(traverse = 0; traverse < aAllocation->mEventCount && traverse < globals.mOptions.mListItemMax; traverse++)
        {
            PR_fprintf(globals.mRequest.mFD, "<tr>\n");

            /*
            ** count.
            */
            PR_fprintf(globals.mRequest.mFD, "<td valign=top align=right>%u.</td>\n", traverse + 1);

            /*
            ** Operation.
            */
            PR_fprintf(globals.mRequest.mFD, "<td valign=top>");
            switch(aAllocation->mEvents[traverse].mEventType)
            {
                case TM_EVENT_CALLOC:
                    PR_fprintf(globals.mRequest.mFD, "calloc");
                    break;
                case TM_EVENT_FREE:
                    PR_fprintf(globals.mRequest.mFD, "free");
                    break;
                case TM_EVENT_MALLOC:
                    PR_fprintf(globals.mRequest.mFD, "malloc");
                    break;
                case TM_EVENT_REALLOC:
                    PR_fprintf(globals.mRequest.mFD, "realloc");
                    break;
                default:
                    retval = __LINE__;
                    REPORT_ERROR(__LINE__, displayAllocationDetails);
                    break;
            }
            PR_fprintf(globals.mRequest.mFD, "</td>");

            /*
            ** Size.
            */
            PR_fprintf(globals.mRequest.mFD, "<td valign=top align=right>%u</td>\n", aAllocation->mEvents[traverse].mHeapSize);

            /*
            ** Timeval.
            */
            cacheval = aAllocation->mEvents[traverse].mTimeval - globals.mMinTimeval;
            PR_fprintf(globals.mRequest.mFD, "<td valign=top align=right>" ST_TIMEVAL_FORMAT "</td>\n", ST_TIMEVAL_PRINTABLE(cacheval));

            /*
            ** Callsite backtrace.
            ** Only relevant backtrace is for event 0 for now until
            **  trace-malloc outputs proper callsites for all others.
            */
            PR_fprintf(globals.mRequest.mFD, "<td valign=top>\n");
            if(0 == traverse)
            {
                displayRes = displayCallsites(aAllocation->mEvents[traverse].mCallsite, ST_FOLLOW_PARENTS, 0, __LINE__);
                if(0 != displayRes)
                {
                    retval = __LINE__;
                    REPORT_ERROR(__LINE__, displayCallsite);
                }
            }
            PR_fprintf(globals.mRequest.mFD, "</td>\n");

            PR_fprintf(globals.mRequest.mFD, "</tr>\n");
        }
        PR_fprintf(globals.mRequest.mFD, "</table><p>\n");
    }
    else
    {
        retval = __LINE__;
        REPORT_ERROR(__LINE__, displayAllocationDetails);
    }

    return retval;
}

/*
** compareCallsites
**
** qsort callback.
** Compare the callsites as specified by the options.
** There must be NO equal callsites, unless they really are duplicates,
**  this is so that a duplicate detector loop can
**  simply skip sorted items until the callsite is different.
*/
int compareCallsites(const void* aSite1, const void* aSite2, void* aContext)
{
    int retval = 0;

    if(NULL != aSite1 && NULL != aSite2)
    {
        tmcallsite* site1 = *((tmcallsite**)aSite1);
        tmcallsite* site2 = *((tmcallsite**)aSite2);

        if(NULL != site1 && NULL != site2)
        {
            STRun* run1 = CALLSITE_RUN(site1);
            STRun* run2 = CALLSITE_RUN(site2);

            if(NULL != run1 && NULL != run2)
            {
                STCallsiteStats* stats1 = &(run1->mStats);
                STCallsiteStats* stats2 = &(run2->mStats);

                /*
                ** Logic determined by pref/option.
                */
                switch(globals.mOptions.mOrderBy)
                {
                    case ST_WEIGHT:
                    {
                        PRUint64 weight164 = stats1->mWeight64;
                        PRUint64 weight264 = stats2->mWeight64;

                        if(LL_UCMP(weight164, <, weight264))
                        {
                            retval = __LINE__;
                        }
                        else if(LL_UCMP(weight164, >, weight264))
                        {
                            retval = - __LINE__;
                        }
                    }
                    break;

                    case ST_SIZE:
                    {
                        PRUint32 size1 = stats1->mSize;
                        PRUint32 size2 = stats2->mSize;

                        if(size1 < size2)
                        {
                            retval = __LINE__;
                        }
                        else if(size1 > size2)
                        {
                            retval = - __LINE__;
                        }
                    }
                    break;

                    case ST_TIMEVAL:
                    {
                        PRUint64 timeval164 = stats1->mTimeval64;
                        PRUint64 timeval264 = stats2->mTimeval64;

                        if(LL_UCMP(timeval164, <, timeval264))
                        {
                            retval = __LINE__;
                        }
                        else if(LL_UCMP(timeval164, >, timeval264))
                        {
                            retval = - __LINE__;
                        }
                    }
                    break;

                    case ST_COUNT:
                    {
                        PRUint32 count1 = stats1->mCompositeCount;
                        PRUint32 count2 = stats2->mCompositeCount;

                        if(count1 < count2)
                        {
                            retval = __LINE__;
                        }
                        else if(count1 > count2)
                        {
                            retval = - __LINE__;
                        }
                    }
                    break;

                    case ST_HEAPCOST:
                    {
                        PRUint32 cost1 = stats1->mHeapRuntimeCost;
                        PRUint32 cost2 = stats2->mHeapRuntimeCost;

                        if(cost1 < cost2)
                        {
                            retval = __LINE__;
                        }
                        else if(cost1 > cost2)
                        {
                            retval = - __LINE__;
                        }
                    }
                    break;

                    default:
                    {
                        REPORT_ERROR(__LINE__, compareAllocations);
                    }
                    break;
                }

                /*
                ** If the return value is still zero, do a pointer compare.
                ** This makes sure we return zero, only iff the same object.
                */
                if(0 == retval)
                {
                    if(stats1 < stats2)
                    {
                        retval = __LINE__;
                    }
                    else if(stats1 > stats2)
                    {
                        retval = - __LINE__;
                    }
                }
            }
        }
    }

    return retval;
}

/*
** displayTopCallsites
**
** Given a list of callsites, sort it, and output skipping dups.
** The passed in callsite array is side effected, as in that it will come
**  back sorted.  This function will not release the array.
**
** Note:  If the stamp passed in is non zero, then all callsites must match.
**  If the stamp is zero, all callsites must match global sorted run stamp.
**
** Returns !0 on error.
*/
int displayTopCallsites(tmcallsite** aCallsites, PRUint32 aCallsiteCount, PRUint32 aStamp, int aRealName)
{
    int retval = 0;

    if(NULL != aCallsites && 0 < aCallsiteCount)
    {
        PRUint32 traverse = 0;
        STRun* run = NULL;
        tmcallsite* site = NULL;
        int headerDisplayed = 0;
        PRUint32 displayed = 0;

        /*
        ** Fixup the stamp.
        */
        if(0 == aStamp && NULL != globals.mCache.mSortedRun)
        {
            aStamp = globals.mCache.mSortedRun->mStats.mStamp;
        }

        /*
        ** Sort the things.
        */
        NS_QuickSort(aCallsites, aCallsiteCount, sizeof(tmcallsite*), compareCallsites, NULL);

        /*
        ** Time for output.
        */
        for(traverse = 0; traverse < aCallsiteCount && globals.mOptions.mListItemMax > displayed; traverse++)
        {
            site = aCallsites[traverse];
            run = CALLSITE_RUN(site);

            /*
            ** Only if the same stamp....
            */
            if(aStamp == run->mStats.mStamp)
            {
                /*
                ** We got a header yet?
                */
                if(0 == headerDisplayed)
                {
                    headerDisplayed = __LINE__;

                    PR_fprintf(globals.mRequest.mFD, "<table border=1>\n");

                    PR_fprintf(globals.mRequest.mFD, "<tr>\n");
                    PR_fprintf(globals.mRequest.mFD, "<td><b>Rank</b></td>\n");
                    PR_fprintf(globals.mRequest.mFD, "<td><b>Callsite</b></td>\n");
                    PR_fprintf(globals.mRequest.mFD, "<td><b>Composite Size</b></td>\n");
                    PR_fprintf(globals.mRequest.mFD, "<td><b>Composite Seconds</b></td>\n");
                    PR_fprintf(globals.mRequest.mFD, "<td><b>Composite Weight</b></td>\n");
                    PR_fprintf(globals.mRequest.mFD, "<td><b>Heap Object Count</b></td>\n");
                    PR_fprintf(globals.mRequest.mFD, "<td><b>Heap Operation Seconds</b></td>\n");
                    PR_fprintf(globals.mRequest.mFD, "</tr>\n");
                }

                displayed++;

                PR_fprintf(globals.mRequest.mFD, "<tr>\n");

                /*
                ** Rank.
                */
                PR_fprintf(globals.mRequest.mFD, "<td align=right valign=top>%u</td>\n", displayed);

                /*
                ** Method.
                */
                PR_fprintf(globals.mRequest.mFD, "<td>");
                htmlCallsiteAnchor(site, NULL, aRealName);
                PR_fprintf(globals.mRequest.mFD, "</td>\n");

                /*
                ** Size.
                */
                PR_fprintf(globals.mRequest.mFD, "<td align=right valign=top>%u</td>\n", run->mStats.mSize);

                /*
                ** Timeval.
                */
                PR_fprintf(globals.mRequest.mFD, "<td align=right valign=top>" ST_TIMEVAL_FORMAT "</td>\n", ST_TIMEVAL_PRINTABLE64(run->mStats.mTimeval64));

                /*
                ** Weight.
                */
                PR_fprintf(globals.mRequest.mFD, "<td align=right valign=top>%llu</td>\n", run->mStats.mWeight64);

                /*
                ** Allocation object count.
                */
                PR_fprintf(globals.mRequest.mFD, "<td align=right valign=top>%u</td>\n", run->mStats.mCompositeCount);

                /*
                ** Heap operation seconds.
                */
                PR_fprintf(globals.mRequest.mFD, "<td align=right valign=top>" ST_MICROVAL_FORMAT "</td>\n", ST_MICROVAL_PRINTABLE(run->mStats.mHeapRuntimeCost));

                PR_fprintf(globals.mRequest.mFD, "</tr>\n");


                if(globals.mOptions.mListItemMax > displayed)
                {
                    /*
                    ** Skip any dups.
                    */
                    while(((traverse + 1) < aCallsiteCount) && (site == aCallsites[traverse + 1]))
                    {
                        traverse++;
                    }
                }
            }
        }

        /*
        ** We need to terminate anything?
        */
        if(0 != headerDisplayed)
        {
            PR_fprintf(globals.mRequest.mFD, "</table>\n");
        }
    }
    else
    {
        retval = __LINE__;
        REPORT_ERROR(__LINE__, displayTopCallsites);
    }

    return retval;
}

/*
** displayCallsiteDetails
**
** The callsite specific report.
** Try to report what we know.
** This one hits a little harder than the rest.
**
** Returns !0 on error.
*/
int displayCallsiteDetails(tmcallsite* aCallsite)
{
    int retval = 0;

    if(NULL != aCallsite && NULL != aCallsite->method)
    {
        STRun* sortedRun = NULL;
        STRun* thisRun = CALLSITE_RUN(aCallsite);
        const char* sourceFile = NULL;

        sourceFile = resolveSourceFile(aCallsite->method);
        if(NULL != sourceFile)
        {
            PR_fprintf(globals.mRequest.mFD, "<b>%s</b>", tmmethodnode_name(aCallsite->method));
            PR_fprintf(globals.mRequest.mFD, "<a href=\"http://lxr.mozilla.org/mozilla/source/%s#%u\" target=\"_st_lxr\">(%s:%u)</a>", aCallsite->method->sourcefile, aCallsite->method->linenumber, sourceFile, aCallsite->method->linenumber);
            PR_fprintf(globals.mRequest.mFD, " Callsite Details:<p>\n");
        }
        else
        {
            PR_fprintf(globals.mRequest.mFD, "<b>%s</b>+%u(%u) Callsite Details:<p>\n", tmmethodnode_name(aCallsite->method), aCallsite->offset, (PRUint32)aCallsite->entry.key);
        }

        PR_fprintf(globals.mRequest.mFD, "<table border=0>\n");
        PR_fprintf(globals.mRequest.mFD, "<tr><td>Composite Byte Size:</td><td align=right>%u</td></tr>\n", thisRun->mStats.mSize);
        PR_fprintf(globals.mRequest.mFD, "<tr><td>Composite Seconds:</td><td align=right>" ST_TIMEVAL_FORMAT "</td></tr>\n", ST_TIMEVAL_PRINTABLE64(thisRun->mStats.mTimeval64));
        PR_fprintf(globals.mRequest.mFD, "<tr><td>Composite Weight:</td><td align=right>%llu</td></tr>\n", thisRun->mStats.mWeight64);
        PR_fprintf(globals.mRequest.mFD, "<tr><td>Heap Object Count:</td><td align=right>%u</td></tr>\n", thisRun->mStats.mCompositeCount);
        PR_fprintf(globals.mRequest.mFD, "<tr><td>Heap Operation Seconds:</td><td align=right>" ST_MICROVAL_FORMAT "</td></tr>\n", ST_MICROVAL_PRINTABLE(thisRun->mStats.mHeapRuntimeCost));
        PR_fprintf(globals.mRequest.mFD, "</table>\n<p>\n");

        /*
        ** Kids (callsites we call):
        */
        if(NULL != aCallsite->kids && NULL != aCallsite->kids->method)
        {
            int displayRes = 0;
            PRUint32 siteCount = 0;
            tmcallsite** sites = NULL;

            /*
            ** Collect the kid sibling callsites.
            ** Doing it this way sorts them for relevance.
            */
            siteCount = callsiteArrayFromCallsite(&sites, 0, aCallsite->kids, ST_FOLLOW_SIBLINGS);
            if(0 != siteCount && NULL != sites)
            {
                /*
                ** Got something to show.
                */
                PR_fprintf(globals.mRequest.mFD, "Children Callsites:<br>\n");

                displayRes = displayTopCallsites(sites, siteCount, 0, __LINE__);
                if(0 != displayRes)
                {
                    retval = __LINE__;
                    REPORT_ERROR(__LINE__, displayTopCallsites);
                }
                PR_fprintf(globals.mRequest.mFD, "<p>\n");

                /*
                ** Done with array.
                */
                free(sites);
                sites = NULL;
            }
        }

        /*
        ** Parents (those who call us):
        */
        if(NULL != aCallsite->parent && NULL != aCallsite->parent->method)
        {
            int displayRes = 0;

            PR_fprintf(globals.mRequest.mFD, "Parent Callsites:<br>\n");
            displayRes = displayCallsites(aCallsite->parent, ST_FOLLOW_PARENTS, 0, __LINE__);
            if(0 != displayRes)
            {
                retval = __LINE__;
                REPORT_ERROR(__LINE__, displayCallsites);
            }
            PR_fprintf(globals.mRequest.mFD, "<p>\n");
        }

        /*
        ** Allocations we did.
        ** Simply harvest our own run.
        */
        sortedRun = createRun(0);
        if(NULL != sortedRun)
        {
            int harvestRes = 0;

            harvestRes = harvestRun(CALLSITE_RUN(aCallsite), sortedRun, NULL);
            if(0 == harvestRes)
            {
                if(0 != sortedRun->mAllocationCount)
                {
                    int sortRes = 0;

                    sortRes = sortRun(sortedRun);
                    if(0 == sortRes)
                    {
                        int displayRes = 0;

                        PR_fprintf(globals.mRequest.mFD, "Allocations:<br>\n");
                        displayRes = displayTopAllocations(sortedRun, 0);
                        if(0 != displayRes)
                        {
                            retval = __LINE__;
                            REPORT_ERROR(__LINE__, displayTopAllocations);
                        }
                        PR_fprintf(globals.mRequest.mFD, "<p>\n");
                    }
                    else
                    {
                        retval = __LINE__;
                        REPORT_ERROR(__LINE__, sortRun);
                    }
                }
            }
            else
            {
                retval = __LINE__;
                REPORT_ERROR(__LINE__, harvestRun);
            }

            /*
            ** Done with the run.
            */
            freeRun(sortedRun);
            sortedRun = NULL;
        }
        else
        {
            retval = __LINE__;
            REPORT_ERROR(__LINE__, createRun);
        }
    }
    else
    {
        retval = __LINE__;
        REPORT_ERROR(__LINE__, displayCallsiteDetails);
    }

    return retval;
}

#if WANT_GRAPHS
/*
** graphFootprint
**
** Output a PNG graph of the memory usage of the run.
**
** Draw the graph within these boundaries.
**  STGD_MARGIN,STGD_MARGIN,STGD_WIDTH-STGD_MARGIN,STGD_HEIGHT-STGD_MARGIN
**
** Returns !0 on failure.
*/
int graphFootprint(STRun* aRun)
{
    int retval = 0;

    if(NULL != aRun)
    {
        PRUint32 *YData = NULL;
        PRUint32 YDataArray[STGD_SPACE_X];
        PRUint32 traverse = 0;
        PRUint32 timeval = globals.mOptions.mGraphTimevalMin;
        PRUint32 loop = 0;

        /*
        ** Decide if this is custom or we should use the global cache.
        */
        if(aRun == globals.mCache.mSortedRun)
        {
            YData = globals.mCache.mFootprintYData;
        }
        else
        {
            YData = YDataArray;
        }

        /*
        ** Only do the computations if we aren't cached already.
        */
        if(YData != globals.mCache.mFootprintYData || 0 == globals.mCache.mFootprintCached)
        {
            memset(YData, 0, sizeof(PRUint32) * STGD_SPACE_X);

            /*
            ** Initialize our Y data.
            ** Pretty brutal loop here....
            */
            for(traverse = 0; 0 == retval && traverse < STGD_SPACE_X; traverse++)
            {
                /*
                ** Compute what timeval this Y data lands in.
                */
                timeval = ((traverse * (globals.mOptions.mGraphTimevalMax - globals.mOptions.mGraphTimevalMin)) / STGD_SPACE_X) + globals.mMinTimeval + globals.mOptions.mGraphTimevalMin;

                /*
                ** Loop over the run.
                ** Should an allocation contain said Timeval, we're good.
                */
                for(loop = 0; loop < aRun->mAllocationCount; loop++)
                {
                    if(timeval >= aRun->mAllocations[loop]->mMinTimeval && timeval <= aRun->mAllocations[loop]->mMaxTimeval)
                    {
                        YData[traverse] += byteSize(aRun->mAllocations[loop]);
                    }
                }
            }

            /*
            ** Did we cache this?
            */
            if(YData == globals.mCache.mFootprintYData)
            {
                globals.mCache.mFootprintCached = __LINE__;
            }
        }

        if(0 == retval)
        {
            PRUint32 minMemory = (PRUint32)-1;
            PRUint32 maxMemory = 0;
            int transparent = 0;
            gdImagePtr graph = NULL;

            /*
            ** Go through and find the minimum and maximum sizes.
            */
            for(traverse = 0; traverse < STGD_SPACE_X; traverse++)
            {
                if(YData[traverse] < minMemory)
                {
                    minMemory = YData[traverse];
                }
                if(YData[traverse] > maxMemory)
                {
                    maxMemory = YData[traverse];
                }
            }

            /*
            ** We can now draw the graph.
            */
            graph = createGraph(&transparent);
            if(NULL != graph)
            {
                gdSink theSink;
                int red = 0;
                int x1 = 0;
                int y1 = 0;
                int x2 = 0;
                int y2 = 0;
                PRUint32 percents[11] = { 0, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100 };
                char* timevals[11];
                char* bytes[11];
                char timevalSpace[11][32];
                char byteSpace[11][32];
                int legendColors[1];
                const char* legends[1] = { "Memory in Use" };
                PRUint32 cached = 0;

                /*
                ** Figure out what the labels will say.
                */
                for(traverse = 0; traverse < 11; traverse++)
                {
                    timevals[traverse] = timevalSpace[traverse];
                    bytes[traverse] = byteSpace[traverse];

                    cached = (((globals.mOptions.mGraphTimevalMax - globals.mOptions.mGraphTimevalMin) * percents[traverse]) / 100) + globals.mOptions.mGraphTimevalMin;
                    PR_snprintf(timevals[traverse], 32, ST_TIMEVAL_FORMAT, ST_TIMEVAL_PRINTABLE(cached));
                    PR_snprintf(bytes[traverse], 32, "%u", ((maxMemory - minMemory) * percents[traverse]) / 100);
                }
                
                red = gdImageColorAllocate(graph, 255, 0, 0);
                legendColors[0] = red;
                
                drawGraph(graph, -1, "Memory Footprint Over Time", "Seconds", "Bytes", 11, percents, (const char**)timevals, 11, percents, (const char**)bytes, 1, legendColors, legends);

                if(maxMemory != minMemory)
                {
                    PRInt64 in64 = LL_INIT(0, 0);
                    PRInt64 ydata64 = LL_INIT(0, 0);
                    PRInt64 spacey64 = LL_INIT(0, 0);
                    PRInt64 mem64 = LL_INIT(0, 0);
                    PRInt32 in32 = 0;

                    /*
                    ** Go through our Y data and mark it up.
                    */
                    for(traverse = 0; traverse < STGD_SPACE_X; traverse++)
                    {
                        x1 = traverse + STGD_MARGIN;
                        y1 = STGD_HEIGHT - STGD_MARGIN;

                        /*
                        ** Need to do this math in 64 bits.
                        */
                        LL_I2L(ydata64, YData[traverse]);
                        LL_I2L(spacey64, STGD_SPACE_Y);
                        LL_I2L(mem64, (maxMemory - minMemory));

                        LL_MUL(in64, ydata64, spacey64);
                        LL_DIV(in64, in64, mem64);
                        LL_L2I(in32, in64);
                        
                        x2 = x1;
                        y2 = y1 - in32;
                        
                        gdImageLine(graph, x1, y1, x2, y2, red);
                    }
                }


                theSink.context = globals.mRequest.mFD;
                theSink.sink = pngSink;
                gdImagePngToSink(graph, &theSink);
                
                gdImageDestroy(graph);
            }
            else
            {
                retval = __LINE__;
                REPORT_ERROR(__LINE__, createGraph);
            }
        }
    }
    else
    {
        retval = __LINE__;
        REPORT_ERROR(__LINE__, graphFootprint);
    }

    return retval;
}
#endif /* WANT_GRAPHS */

#if WANT_GRAPHS
/*
** graphTimeval
**
** Output a PNG graph of when the memory is allocated.
**
** Draw the graph within these boundaries.
**  STGD_MARGIN,STGD_MARGIN,STGD_WIDTH-STGD_MARGIN,STGD_HEIGHT-STGD_MARGIN
**
** Returns !0 on failure.
*/
int graphTimeval(STRun* aRun)
{
    int retval = 0;

    if(NULL != aRun)
    {
        PRUint32 *YData = NULL;
        PRUint32 YDataArray[STGD_SPACE_X];
        PRUint32 traverse = 0;
        PRUint32 timeval = globals.mOptions.mGraphTimevalMin + globals.mMinTimeval;
        PRUint32 loop = 0;

        /*
        ** Decide if this is custom or we should use the global cache.
        */
        if(aRun == globals.mCache.mSortedRun)
        {
            YData = globals.mCache.mTimevalYData;
        }
        else
        {
            YData = YDataArray;
        }

        /*
        ** Only do the computations if we aren't cached already.
        */
        if(YData != globals.mCache.mTimevalYData || 0 == globals.mCache.mTimevalCached)
        {
            PRUint32 prevTimeval = 0;

            memset(YData, 0, sizeof(PRUint32) * STGD_SPACE_X);

            /*
            ** Initialize our Y data.
            ** Pretty brutal loop here....
            */
            for(traverse = 0; 0 == retval && traverse < STGD_SPACE_X; traverse++)
            {
                /*
                ** Compute what timeval this Y data lands in.
                */
                prevTimeval = timeval;
                timeval = ((traverse * (globals.mOptions.mGraphTimevalMax - globals.mOptions.mGraphTimevalMin)) / STGD_SPACE_X) + globals.mMinTimeval + globals.mOptions.mGraphTimevalMin;

                /*
                ** Loop over the run.
                ** Should an allocation have been allocated between
                **  prevTimeval and timeval....
                */
                for(loop = 0; loop < aRun->mAllocationCount; loop++)
                {
                    if(prevTimeval < aRun->mAllocations[loop]->mMinTimeval && timeval >= aRun->mAllocations[loop]->mMinTimeval)
                    {
                        YData[traverse] += byteSize(aRun->mAllocations[loop]);
                    }
                }
            }

            /*
            ** Did we cache this?
            */
            if(YData == globals.mCache.mTimevalYData)
            {
                globals.mCache.mTimevalCached = __LINE__;
            }
        }

        if(0 == retval)
        {
            PRUint32 minMemory = (PRUint32)-1;
            PRUint32 maxMemory = 0;
            int transparent = 0;
            gdImagePtr graph = NULL;

            /*
            ** Go through and find the minimum and maximum sizes.
            */
            for(traverse = 0; traverse < STGD_SPACE_X; traverse++)
            {
                if(YData[traverse] < minMemory)
                {
                    minMemory = YData[traverse];
                }
                if(YData[traverse] > maxMemory)
                {
                    maxMemory = YData[traverse];
                }
            }

            /*
            ** We can now draw the graph.
            */
            graph = createGraph(&transparent);
            if(NULL != graph)
            {
                gdSink theSink;
                int red = 0;
                int x1 = 0;
                int y1 = 0;
                int x2 = 0;
                int y2 = 0;
                PRUint32 percents[11] = { 0, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100 };
                char* timevals[11];
                char* bytes[11];
                char timevalSpace[11][32];
                char byteSpace[11][32];
                int legendColors[1];
                const char* legends[1] = { "Memory Allocated" };
                PRUint32 cached = 0;

                /*
                ** Figure out what the labels will say.
                */
                for(traverse = 0; traverse < 11; traverse++)
                {
                    timevals[traverse] = timevalSpace[traverse];
                    bytes[traverse] = byteSpace[traverse];

                    cached = (((globals.mOptions.mGraphTimevalMax - globals.mOptions.mGraphTimevalMin) * percents[traverse]) / 100) + globals.mOptions.mGraphTimevalMin;
                    PR_snprintf(timevals[traverse], 32, ST_TIMEVAL_FORMAT, ST_TIMEVAL_PRINTABLE(cached));
                    PR_snprintf(bytes[traverse], 32, "%u", ((maxMemory - minMemory) * percents[traverse]) / 100);
                }
                
                red = gdImageColorAllocate(graph, 255, 0, 0);
                legendColors[0] = red;
                
                drawGraph(graph, -1, "Allocation Times", "Seconds", "Bytes", 11, percents, (const char**)timevals, 11, percents, (const char**)bytes, 1, legendColors, legends);

                if(maxMemory != minMemory)
                {
                    PRInt64 in64 = LL_INIT(0, 0);
                    PRInt64 ydata64 = LL_INIT(0, 0);
                    PRInt64 spacey64 = LL_INIT(0, 0);
                    PRInt64 mem64 = LL_INIT(0, 0);
                    PRInt32 in32 = 0;

                    /*
                    ** Go through our Y data and mark it up.
                    */
                    for(traverse = 0; traverse < STGD_SPACE_X; traverse++)
                    {
                        x1 = traverse + STGD_MARGIN;
                        y1 = STGD_HEIGHT - STGD_MARGIN;

                        /*
                        ** Need to do this math in 64 bits.
                        */
                        LL_I2L(ydata64, YData[traverse]);
                        LL_I2L(spacey64, STGD_SPACE_Y);
                        LL_I2L(mem64, (maxMemory - minMemory));

                        LL_MUL(in64, ydata64, spacey64);
                        LL_DIV(in64, in64, mem64);
                        LL_L2I(in32, in64);
                        
                        x2 = x1;
                        y2 = y1 - in32;
                        
                        gdImageLine(graph, x1, y1, x2, y2, red);
                    }
                }


                theSink.context = globals.mRequest.mFD;
                theSink.sink = pngSink;
                gdImagePngToSink(graph, &theSink);
                
                gdImageDestroy(graph);
            }
            else
            {
                retval = __LINE__;
                REPORT_ERROR(__LINE__, createGraph);
            }
        }
    }
    else
    {
        retval = __LINE__;
        REPORT_ERROR(__LINE__, graphTimeval);
    }

    return retval;
}
#endif /* WANT_GRAPHS */

#if WANT_GRAPHS
/*
** graphLifespan
**
** Output a PNG graph of how long memory lived.
**
** Draw the graph within these boundaries.
**  STGD_MARGIN,STGD_MARGIN,STGD_WIDTH-STGD_MARGIN,STGD_HEIGHT-STGD_MARGIN
**
** Returns !0 on failure.
*/
int graphLifespan(STRun* aRun)
{
    int retval = 0;

    if(NULL != aRun)
    {
        PRUint32 *YData = NULL;
        PRUint32 YDataArray[STGD_SPACE_X];
        PRUint32 traverse = 0;
        PRUint32 timeval = globals.mOptions.mGraphTimevalMin;
        PRUint32 loop = 0;

        /*
        ** Decide if this is custom or we should use the global cache.
        */
        if(aRun == globals.mCache.mSortedRun)
        {
            YData = globals.mCache.mLifespanYData;
        }
        else
        {
            YData = YDataArray;
        }

        /*
        ** Only do the computations if we aren't cached already.
        */
        if(YData != globals.mCache.mLifespanYData || 0 == globals.mCache.mLifespanCached)
        {
            PRUint32 prevTimeval = 0;
            PRUint32 lifespan = 0;

            memset(YData, 0, sizeof(PRUint32) * STGD_SPACE_X);

            /*
            ** Initialize our Y data.
            ** Pretty brutal loop here....
            */
            for(traverse = 0; 0 == retval && traverse < STGD_SPACE_X; traverse++)
            {
                /*
                ** Compute what timeval this Y data lands in.
                */
                prevTimeval = timeval;
                timeval = ((traverse * (globals.mOptions.mGraphTimevalMax - globals.mOptions.mGraphTimevalMin)) / STGD_SPACE_X) + globals.mOptions.mGraphTimevalMin;

                /*
                ** Loop over the run.
                ** Should an allocation have lived between
                **  prevTimeval and timeval....
                */
                for(loop = 0; loop < aRun->mAllocationCount; loop++)
                {
                    lifespan = aRun->mAllocations[loop]->mMaxTimeval - aRun->mAllocations[loop]->mMinTimeval;

                    if(prevTimeval < lifespan && timeval >= lifespan)
                    {
                        YData[traverse] += byteSize(aRun->mAllocations[loop]);
                    }
                }
            }

            /*
            ** Did we cache this?
            */
            if(YData == globals.mCache.mLifespanYData)
            {
                globals.mCache.mLifespanCached = __LINE__;
            }
        }

        if(0 == retval)
        {
            PRUint32 minMemory = (PRUint32)-1;
            PRUint32 maxMemory = 0;
            int transparent = 0;
            gdImagePtr graph = NULL;

            /*
            ** Go through and find the minimum and maximum sizes.
            */
            for(traverse = 0; traverse < STGD_SPACE_X; traverse++)
            {
                if(YData[traverse] < minMemory)
                {
                    minMemory = YData[traverse];
                }
                if(YData[traverse] > maxMemory)
                {
                    maxMemory = YData[traverse];
                }
            }

            /*
            ** We can now draw the graph.
            */
            graph = createGraph(&transparent);
            if(NULL != graph)
            {
                gdSink theSink;
                int red = 0;
                int x1 = 0;
                int y1 = 0;
                int x2 = 0;
                int y2 = 0;
                PRUint32 percents[11] = { 0, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100 };
                char* timevals[11];
                char* bytes[11];
                char timevalSpace[11][32];
                char byteSpace[11][32];
                int legendColors[1];
                const char* legends[1] = { "Live Memory" };
                PRUint32 cached = 0;

                /*
                ** Figure out what the labels will say.
                */
                for(traverse = 0; traverse < 11; traverse++)
                {
                    timevals[traverse] = timevalSpace[traverse];
                    bytes[traverse] = byteSpace[traverse];

                    cached = (((globals.mOptions.mGraphTimevalMax - globals.mOptions.mGraphTimevalMin) * percents[traverse]) / 100) + globals.mOptions.mGraphTimevalMin;
                    PR_snprintf(timevals[traverse], 32, ST_TIMEVAL_FORMAT, ST_TIMEVAL_PRINTABLE(cached));
                    PR_snprintf(bytes[traverse], 32, "%u", ((maxMemory - minMemory) * percents[traverse]) / 100);
                }
                
                red = gdImageColorAllocate(graph, 255, 0, 0);
                legendColors[0] = red;
                
                drawGraph(graph, -1, "Allocation Lifespans", "Lifespan", "Bytes", 11, percents, (const char**)timevals, 11, percents, (const char**)bytes, 1, legendColors, legends);

                if(maxMemory != minMemory)
                {
                    PRInt64 in64 = LL_INIT(0, 0);
                    PRInt64 ydata64 = LL_INIT(0, 0);
                    PRInt64 spacey64 = LL_INIT(0, 0);
                    PRInt64 mem64 = LL_INIT(0, 0);
                    PRInt32 in32 = 0;

                    /*
                    ** Go through our Y data and mark it up.
                    */
                    for(traverse = 0; traverse < STGD_SPACE_X; traverse++)
                    {
                        x1 = traverse + STGD_MARGIN;
                        y1 = STGD_HEIGHT - STGD_MARGIN;

                        /*
                        ** Need to do this math in 64 bits.
                        */
                        LL_I2L(ydata64, YData[traverse]);
                        LL_I2L(spacey64, STGD_SPACE_Y);
                        LL_I2L(mem64, (maxMemory - minMemory));

                        LL_MUL(in64, ydata64, spacey64);
                        LL_DIV(in64, in64, mem64);
                        LL_L2I(in32, in64);
                        
                        x2 = x1;
                        y2 = y1 - in32;
                        
                        gdImageLine(graph, x1, y1, x2, y2, red);
                    }
                }


                theSink.context = globals.mRequest.mFD;
                theSink.sink = pngSink;
                gdImagePngToSink(graph, &theSink);
                
                gdImageDestroy(graph);
            }
            else
            {
                retval = __LINE__;
                REPORT_ERROR(__LINE__, createGraph);
            }
        }
    }
    else
    {
        retval = __LINE__;
        REPORT_ERROR(__LINE__, graphLifespan);
    }

    return retval;
}
#endif /* WANT_GRAPHS */

#if WANT_GRAPHS
/*
** graphWeight
**
** Output a PNG graph of Allocations by Weight
**
** Draw the graph within these boundaries.
**  STGD_MARGIN,STGD_MARGIN,STGD_WIDTH-STGD_MARGIN,STGD_HEIGHT-STGD_MARGIN
**
** Returns !0 on failure.
*/
int graphWeight(STRun* aRun)
{
    int retval = 0;

    if(NULL != aRun)
    {
        PRUint64 *YData64 = NULL;
        PRUint64 YDataArray64[STGD_SPACE_X];
        PRUint32 traverse = 0;
        PRUint32 timeval = globals.mOptions.mGraphTimevalMin + globals.mMinTimeval;
        PRUint32 loop = 0;

        /*
        ** Decide if this is custom or we should use the global cache.
        */
        if(aRun == globals.mCache.mSortedRun)
        {
            YData64 = globals.mCache.mWeightYData64;
        }
        else
        {
            YData64 = YDataArray64;
        }

        /*
        ** Only do the computations if we aren't cached already.
        */
        if(YData64 != globals.mCache.mWeightYData64 || 0 == globals.mCache.mWeightCached)
        {
            PRUint32 prevTimeval = 0;

            memset(YData64, 0, sizeof(PRUint64) * STGD_SPACE_X);

            /*
            ** Initialize our Y data.
            ** Pretty brutal loop here....
            */
            for(traverse = 0; 0 == retval && traverse < STGD_SPACE_X; traverse++)
            {
                /*
                ** Compute what timeval this Y data lands in.
                */
                prevTimeval = timeval;
                timeval = ((traverse * (globals.mOptions.mGraphTimevalMax - globals.mOptions.mGraphTimevalMin)) / STGD_SPACE_X) + globals.mMinTimeval + globals.mOptions.mGraphTimevalMin;

                /*
                ** Loop over the run.
                ** Should an allocation have been allocated between
                **  prevTimeval and timeval....
                */
                for(loop = 0; loop < aRun->mAllocationCount; loop++)
                {
                    if(prevTimeval < aRun->mAllocations[loop]->mMinTimeval && timeval >= aRun->mAllocations[loop]->mMinTimeval)
                    {
                        PRUint64 size64 = LL_INIT(0, 0);
                        PRUint64 lifespan64 = LL_INIT(0, 0);
                        PRUint64 weight64 = LL_INIT(0, 0);

                        LL_UI2L(size64, byteSize(aRun->mAllocations[loop]));
                        LL_UI2L(lifespan64, (aRun->mAllocations[loop]->mMaxTimeval - aRun->mAllocations[loop]->mMinTimeval));
                        LL_MUL(weight64, size64, lifespan64);

                        LL_ADD(YData64[traverse], YData64[traverse], weight64);
                    }
                }
            }

            /*
            ** Did we cache this?
            */
            if(YData64 == globals.mCache.mWeightYData64)
            {
                globals.mCache.mWeightCached = __LINE__;
            }
        }

        if(0 == retval)
        {
            PRUint64 minWeight64 = LL_INIT(0xFFFFFFFF, 0xFFFFFFFF);
            PRUint64 maxWeight64 = LL_INIT(0, 0);
            int transparent = 0;
            gdImagePtr graph = NULL;

            /*
            ** Go through and find the minimum and maximum weights.
            */
            for(traverse = 0; traverse < STGD_SPACE_X; traverse++)
            {
                if(LL_UCMP(YData64[traverse], <, minWeight64))
                {
                    minWeight64 = YData64[traverse];
                }
                if(LL_UCMP(YData64[traverse], >, maxWeight64))
                {
                    maxWeight64 = YData64[traverse];
                }
            }

            /*
            ** We can now draw the graph.
            */
            graph = createGraph(&transparent);
            if(NULL != graph)
            {
                gdSink theSink;
                int red = 0;
                int x1 = 0;
                int y1 = 0;
                int x2 = 0;
                int y2 = 0;
                PRUint32 percents[11] = { 0, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100 };
                char* timevals[11];
                char* bytes[11];
                char timevalSpace[11][32];
                char byteSpace[11][32];
                int legendColors[1];
                const char* legends[1] = { "Memory Weight" };
                PRUint64 percent64 = LL_INIT(0, 0);
                PRUint64 result64 = LL_INIT(0, 0);
                PRUint64 hundred64 = LL_INIT(0, 0);
                PRUint32 cached = 0;

                LL_UI2L(hundred64, 100);

                /*
                ** Figure out what the labels will say.
                */
                for(traverse = 0; traverse < 11; traverse++)
                {
                    timevals[traverse] = timevalSpace[traverse];
                    bytes[traverse] = byteSpace[traverse];

                    cached = (((globals.mOptions.mGraphTimevalMax - globals.mOptions.mGraphTimevalMin) * percents[traverse]) / 100) + globals.mOptions.mGraphTimevalMin;
                    PR_snprintf(timevals[traverse], 32, ST_TIMEVAL_FORMAT, ST_TIMEVAL_PRINTABLE(cached));

                    LL_UI2L(percent64, percents[traverse]);
                    LL_SUB(result64, maxWeight64, minWeight64);
                    LL_MUL(result64, result64, percent64);
                    LL_DIV(result64, result64, hundred64);
                    PR_snprintf(bytes[traverse], 32, "%llu", result64);
                }
                
                red = gdImageColorAllocate(graph, 255, 0, 0);
                legendColors[0] = red;
                
                drawGraph(graph, -1, "Allocation Weights", "Seconds", "Weight", 11, percents, (const char**)timevals, 11, percents, (const char**)bytes, 1, legendColors, legends);

                if(LL_NE(maxWeight64, minWeight64))
                {
                    PRInt64 in64 = LL_INIT(0, 0);
                    PRInt64 spacey64 = LL_INIT(0, 0);
                    PRInt64 weight64 = LL_INIT(0, 0);
                    PRInt32 in32 = 0;

                    /*
                    ** Go through our Y data and mark it up.
                    */
                    for(traverse = 0; traverse < STGD_SPACE_X; traverse++)
                    {
                        x1 = traverse + STGD_MARGIN;
                        y1 = STGD_HEIGHT - STGD_MARGIN;

                        /*
                        ** Need to do this math in 64 bits.
                        */
                        LL_I2L(spacey64, STGD_SPACE_Y);
                        LL_SUB(weight64, maxWeight64, minWeight64);

                        LL_MUL(in64, YData64[traverse], spacey64);
                        LL_DIV(in64, in64, weight64);
                        LL_L2I(in32, in64);
                        
                        x2 = x1;
                        y2 = y1 - in32;
                        
                        gdImageLine(graph, x1, y1, x2, y2, red);
                    }
                }


                theSink.context = globals.mRequest.mFD;
                theSink.sink = pngSink;
                gdImagePngToSink(graph, &theSink);
                
                gdImageDestroy(graph);
            }
            else
            {
                retval = __LINE__;
                REPORT_ERROR(__LINE__, createGraph);
            }
        }
    }
    else
    {
        retval = __LINE__;
        REPORT_ERROR(__LINE__, graphWeight);
    }

    return retval;
}
#endif /* WANT_GRAPHS */

/*
** displaySettings
**
** Present the settings for change during execution.
** Returns !0 on error.
**
** Changes are effected via the get data.
*/
int displaySettings(void)
{
    int retval = 0;
    PRUint32 cached = 0;
    PRIntn looper = 0;

    /*
    ** If we've got get data, we need to attempt to enact the changes.
    ** That way, when we show the page, it will have the new changes.
    */
    if(NULL != globals.mRequest.mGetData && '\0' != *globals.mRequest.mGetData)
    {
        int getRes = 0;
        int changedSet = 0;
        int changedOrder = 0;
        int changedGraph = 0;
        int changedDontCare = 0;
        char looper_buf[32];

        getRes += getDataPRUint32(globals.mRequest.mGetData, "mListItemMax", &globals.mOptions.mListItemMax, &changedDontCare, 1);
        getRes += getDataPRUint32(globals.mRequest.mGetData, "mTimevalMin", &globals.mOptions.mTimevalMin, &changedSet, ST_TIMEVAL_RESOLUTION);
        getRes += getDataPRUint32(globals.mRequest.mGetData, "mTimevalMax", &globals.mOptions.mTimevalMax, &changedSet, ST_TIMEVAL_RESOLUTION);
        getRes += getDataPRUint32(globals.mRequest.mGetData, "mAllocationTimevalMin", &globals.mOptions.mAllocationTimevalMin, &changedSet, ST_TIMEVAL_RESOLUTION);

        getRes += getDataPRUint32(globals.mRequest.mGetData, "mAllocationTimevalMax", &globals.mOptions.mAllocationTimevalMax, &changedSet, ST_TIMEVAL_RESOLUTION);

#if WANT_GRAPHS
        getRes += getDataPRUint32(globals.mRequest.mGetData, "mGraphTimevalMin", &globals.mOptions.mGraphTimevalMin, &changedGraph, ST_TIMEVAL_RESOLUTION);
        getRes += getDataPRUint32(globals.mRequest.mGetData, "mGraphTimevalMax", &globals.mOptions.mGraphTimevalMax, &changedGraph, ST_TIMEVAL_RESOLUTION);
#endif /* WANT_GRAPHS */
        getRes += getDataPRUint32(globals.mRequest.mGetData, "mSizeMin", &globals.mOptions.mSizeMin, &changedSet, 1);
        getRes += getDataPRUint32(globals.mRequest.mGetData, "mSizeMax", &globals.mOptions.mSizeMax, &changedSet, 1);
        getRes += getDataPRUint32(globals.mRequest.mGetData, "mAlignBy", &globals.mOptions.mAlignBy, &changedSet, 1);
        getRes += getDataPRUint32(globals.mRequest.mGetData, "mOverhead", &globals.mOptions.mOverhead, &changedSet, 1);
        getRes += getDataPRUint32(globals.mRequest.mGetData, "mOrderBy", &globals.mOptions.mOrderBy, &changedOrder, 1);
        getRes += getDataPRUint32(globals.mRequest.mGetData, "mLifetimeMin", &globals.mOptions.mLifetimeMin, &changedSet, ST_TIMEVAL_RESOLUTION);
        getRes += getDataPRUint32(globals.mRequest.mGetData, "mLifetimeMax", &globals.mOptions.mLifetimeMax, &changedSet, ST_TIMEVAL_RESOLUTION);
        getRes += getDataPRUint64(globals.mRequest.mGetData, "mWeightMin", &globals.mOptions.mWeightMin64, &changedSet);
        getRes += getDataPRUint64(globals.mRequest.mGetData, "mWeightMax", &globals.mOptions.mWeightMax64, &changedSet);
        for(looper = 0; ST_SUBSTRING_MATCH_MAX > looper; looper++)
        {
            PR_snprintf(looper_buf, sizeof(looper_buf), "mRestrictText%d", looper);
            getRes += getDataString(globals.mRequest.mGetData, looper_buf, &globals.mOptions.mRestrictText[looper], &changedSet);
        }

        /*
        ** Resort the global based on new prefs if needed.
        */
        if(0 != changedSet || 0 != changedOrder)
        {
            if(NULL != globals.mCache.mSortedRun)
            {
                freeRun(globals.mCache.mSortedRun);
            }
            globals.mCache.mSortedRun = createRunFromGlobal();
            if(NULL == globals.mCache.mSortedRun)
            {
                retval = __LINE__;
                REPORT_ERROR(__LINE__, createRunFromGlobal);
            }
        }

#if WANT_GRAPHS
        /*
        ** If any of the set was changed, we need to throw away all our
        **  cached graphs.
        */
        if(0 != changedSet || 0 != changedGraph)
        {
            /*
            ** Automove the graph timeval if required.
            */
            if((globals.mMaxTimeval - globals.mMinTimeval) < globals.mOptions.mGraphTimevalMax)
            {
                globals.mOptions.mGraphTimevalMax = (globals.mMaxTimeval - globals.mMinTimeval);
            }

            globals.mCache.mFootprintCached = 0;
            globals.mCache.mTimevalCached = 0;
            globals.mCache.mLifespanCached = 0;
            globals.mCache.mWeightCached = 0;
        }
#endif /* WANT_GRAPHS */

        /*
        ** Report on the operation.
        */
        if(0 != getRes)
        {
            retval = __LINE__;
            REPORT_ERROR(__LINE__, getDataPRUint32);

            PR_fprintf(globals.mRequest.mFD, "<blink><b>%u: There was a problem.  Some changes may have been applied.</b></blink><br><hr>\n", PR_IntervalNow());
        }
        else
        {
            PR_fprintf(globals.mRequest.mFD, "<b>%u: Your changes have been applied.</b><br><hr>\n", PR_IntervalNow());
        }
    }

    /*
    ** A small blurb regarding the options.
    */
    PR_fprintf(globals.mRequest.mFD, "NOTES:<p>\n");
    cached = globals.mMaxTimeval - globals.mMinTimeval;
    PR_fprintf(globals.mRequest.mFD, "The total seconds in this run is: 0 to " ST_TIMEVAL_FORMAT "<br>\n", ST_TIMEVAL_PRINTABLE(cached));

    PR_fprintf(globals.mRequest.mFD, "All options should have command line equivalents to support batch mode.<br>\n");
    PR_fprintf(globals.mRequest.mFD, "Changes to the options take effect immediately.<br>\n");
    PR_fprintf(globals.mRequest.mFD, "<hr>\n");

    /*
    ** We've got a form to create.
    */
    PR_fprintf(globals.mRequest.mFD, "<form method=get action=\"./options.html\">\n");

    PR_fprintf(globals.mRequest.mFD, "Maximum number of items to display in a list?<br>\n");
    PR_fprintf(globals.mRequest.mFD, "This option exists to control how much information you are willing to accept.<p>\n");
    PR_fprintf(globals.mRequest.mFD, "<input type=text name=\"mListItemMax\" value=\"%u\"><br>\n", globals.mOptions.mListItemMax);
    PR_fprintf(globals.mRequest.mFD, "<hr>\n");

    PR_fprintf(globals.mRequest.mFD, "This option controls the sort order of the lists presented.<br>\n");
    PR_fprintf(globals.mRequest.mFD, "There are several choices:<br>\n");
    PR_fprintf(globals.mRequest.mFD, "<ul><li>0 is by weight (byte size * seconds).<li>1 is by byte size.<li>2 is by seconds (lifetime).<li>3 is by allocation object count.<li>4 is by heap operation runtime cost.</ul><p>\n");
    PR_fprintf(globals.mRequest.mFD, "Desired sort order?<br>\n");
    PR_fprintf(globals.mRequest.mFD, "<input type=text name=\"mOrderBy\" value=\"%u\"><br>\n", globals.mOptions.mOrderBy);
    PR_fprintf(globals.mRequest.mFD, "<hr>\n");

#if WANT_GRAPHS
    PR_fprintf(globals.mRequest.mFD, "Modify the seconds for which the graphs cover;<br>\n");
    PR_fprintf(globals.mRequest.mFD, "meaning that a narrower range will produce a more detailed graph for that timespan.<p>\n");
    PR_fprintf(globals.mRequest.mFD, "Minimum graph second?<br>\n");
    PR_fprintf(globals.mRequest.mFD, "<input type=text name=\"mGraphTimevalMin\" value=\"%u\"><br>\n", globals.mOptions.mGraphTimevalMin / ST_TIMEVAL_RESOLUTION);
    PR_fprintf(globals.mRequest.mFD, "Maximum graph second?<br>\n");
    PR_fprintf(globals.mRequest.mFD, "<input type=text name=\"mGraphTimevalMax\" value=\"%u\"><br>\n", globals.mOptions.mGraphTimevalMax / ST_TIMEVAL_RESOLUTION);
    PR_fprintf(globals.mRequest.mFD, "<hr>\n");
#endif /* WANT_GRAPHS */

    PR_fprintf(globals.mRequest.mFD, "Modify the secondss to target allocations created during a particular timespan;<br>\n");
    PR_fprintf(globals.mRequest.mFD, "meaning that the allocations created only within the timespan are of interest.<p>\n");
    PR_fprintf(globals.mRequest.mFD, "Minimum allocation second?<br>\n");
    PR_fprintf(globals.mRequest.mFD, "<input type=text name=\"mAllocationTimevalMin\" value=\"%u\"><br>\n", globals.mOptions.mAllocationTimevalMin / ST_TIMEVAL_RESOLUTION);
    PR_fprintf(globals.mRequest.mFD, "Maximum allocation second?<br>\n");
    PR_fprintf(globals.mRequest.mFD, "<input type=text name=\"mAllocationTimevalMax\" value=\"%u\"><br>\n", globals.mOptions.mAllocationTimevalMax / ST_TIMEVAL_RESOLUTION);
    PR_fprintf(globals.mRequest.mFD, "<hr>\n");

    PR_fprintf(globals.mRequest.mFD, "Modify the byte sizes to target allocations of a particular byte size.<p>\n");
    PR_fprintf(globals.mRequest.mFD, "Minimum byte size?<br>\n");
    PR_fprintf(globals.mRequest.mFD, "<input type=text name=\"mSizeMin\" value=\"%u\"><br>\n", globals.mOptions.mSizeMin);
    PR_fprintf(globals.mRequest.mFD, "Maximum byte size?<br>\n");
    PR_fprintf(globals.mRequest.mFD, "<input type=text name=\"mSizeMax\" value=\"%u\"><br>\n", globals.mOptions.mSizeMax);
    PR_fprintf(globals.mRequest.mFD, "<hr>\n");

    PR_fprintf(globals.mRequest.mFD, "Modify the alignment boundry and heap overhead of allocations to see the actual impact an allocation has on a heap;<br>\n");
    PR_fprintf(globals.mRequest.mFD, "meaning that normally an allocation of 1 bytes actually costs more bytes depending on your heap implementation.<br>\n");
    PR_fprintf(globals.mRequest.mFD, "Overhead is taken into account after allocation alignment.<br>\n");
    PR_fprintf(globals.mRequest.mFD, "i.e. the msvcrt malloc has an alignment of 16 with an overhead of 8 (1 byte allocation costs 24 heap bytes), the win32 HeapAlloc has an alignment of 8 with an overhead of 8 (1 byte allocation costs 16 heap bytes).<p>\n");
    PR_fprintf(globals.mRequest.mFD, "Align by?<br>\n");
    PR_fprintf(globals.mRequest.mFD, "<input type=text name=\"mAlignBy\" value=\"%u\"><br>\n", globals.mOptions.mAlignBy);
    PR_fprintf(globals.mRequest.mFD, "Overhead?<br>\n");
    PR_fprintf(globals.mRequest.mFD, "<input type=text name=\"mOverhead\" value=\"%u\"><br>\n", globals.mOptions.mOverhead);
    PR_fprintf(globals.mRequest.mFD, "<hr>\n");

    PR_fprintf(globals.mRequest.mFD, "Modify the seconds to target allocations of a particular lifespan/duration;<br>\n");
    PR_fprintf(globals.mRequest.mFD, "meaning that the allocations existed at least or at most the specified span of time.<p>\n");
    PR_fprintf(globals.mRequest.mFD, "Minimum lifetime?<br>\n");
    PR_fprintf(globals.mRequest.mFD, "<input type=text name=\"mLifetimeMin\" value=\"%u\"><br>\n", globals.mOptions.mLifetimeMin / ST_TIMEVAL_RESOLUTION);
    PR_fprintf(globals.mRequest.mFD, "Maximum lifetime?<br>\n");
    PR_fprintf(globals.mRequest.mFD, "<input type=text name=\"mLifetimeMax\" value=\"%u\"><br>\n", globals.mOptions.mLifetimeMax / ST_TIMEVAL_RESOLUTION);
    PR_fprintf(globals.mRequest.mFD, "<hr>\n");

    PR_fprintf(globals.mRequest.mFD, "Modify the numbers to target allocations of particular weights;<br>\n");
    PR_fprintf(globals.mRequest.mFD, "the weight of an allocation is the byte size multiplied by the lifespan.<p>\n");
    PR_fprintf(globals.mRequest.mFD, "Minimum weight?<br>\n");
    PR_fprintf(globals.mRequest.mFD, "<input type=text name=\"mWeightMin\" value=\"%llu\"><br>\n", globals.mOptions.mWeightMin64);
    PR_fprintf(globals.mRequest.mFD, "Maximum weight?<br>\n");
    PR_fprintf(globals.mRequest.mFD, "<input type=text name=\"mWeightMax\" value=\"%llu\"><br>\n", globals.mOptions.mWeightMax64);
    PR_fprintf(globals.mRequest.mFD, "<hr>\n");

    PR_fprintf(globals.mRequest.mFD, "By manipulating the time range, you narrow or widen the set of live allocations evaluated.  Allocations existing solely before the minimum or solely after the maximum will not be considered.<p>\n");
    PR_fprintf(globals.mRequest.mFD, "Minimum second?<br>\n");
    PR_fprintf(globals.mRequest.mFD, "<input type=text name=\"mTimevalMin\" value=\"%u\"><br>\n", globals.mOptions.mTimevalMin / ST_TIMEVAL_RESOLUTION);
    PR_fprintf(globals.mRequest.mFD, "Maximum timeval?<br>\n");
    PR_fprintf(globals.mRequest.mFD, "<input type=text name=\"mTimevalMax\" value=\"%u\"><br>\n", globals.mOptions.mTimevalMax / ST_TIMEVAL_RESOLUTION);
    PR_fprintf(globals.mRequest.mFD, "<hr>\n");

    PR_fprintf(globals.mRequest.mFD, "Restrict callsite backtraces to thost only containing the specified text.\n");
    PR_fprintf(globals.mRequest.mFD, "This allows targeting of specific creation functions.\n");
    PR_fprintf(globals.mRequest.mFD, "Keep all the strings together near the top of this list, as the code will stop trying to match on the first empty string.<br>\n");
    for(looper = 0; ST_SUBSTRING_MATCH_MAX > looper; looper++)
    {
        PR_fprintf(globals.mRequest.mFD, "<input type=text name=\"mRestrictText%d\" value=\"%s\"><br>\n", looper, NULL == globals.mOptions.mRestrictText[looper] ? "" : globals.mOptions.mRestrictText[looper]);
    }
    PR_fprintf(globals.mRequest.mFD, "<hr>\n");

    /*
    ** And last but not least, the submission button.
    */
    PR_fprintf(globals.mRequest.mFD, "<input type=submit value=\"Submit Changes\"><input type=reset value=\"Obligatory Reset Button\"><br>\n");

    PR_fprintf(globals.mRequest.mFD, "</form>\n");

    return retval;
}

/*
** displayIndex
**
** Present a list of the reports you can drill down into.
** Returns !0 on failure.
*/
int displayIndex(void)
{
    int retval = 0;

    /*
    ** Present reports in a list format.
    */
    PR_fprintf(globals.mRequest.mFD, "<ul>");
    
    PR_fprintf(globals.mRequest.mFD, "\n<li>");
    htmlAnchor("root_callsites.html", "Root Callsites", NULL);
    
    PR_fprintf(globals.mRequest.mFD, "\n<li>");
    htmlAnchor("top_callsites.html", "Top Callsites Report", NULL);
    
    PR_fprintf(globals.mRequest.mFD, "\n<li>");
    htmlAnchor("top_allocations.html", "Top Allocations Report", NULL);
    
    PR_fprintf(globals.mRequest.mFD, "\n<li>");
    htmlAnchor("memory_leaks.html", "Memory Leak Report", NULL);

#if WANT_GRAPHS    
    PR_fprintf(globals.mRequest.mFD, "\n<li>Graphs");
    
    PR_fprintf(globals.mRequest.mFD, "<ul>");
    
    PR_fprintf(globals.mRequest.mFD, "\n<li>");
    htmlAnchor("footprint_graph.html", "Footprint", NULL);
    
    PR_fprintf(globals.mRequest.mFD, "\n<li>");
    htmlAnchor("lifespan_graph.html", "Allocation Lifespans", NULL);
    
    PR_fprintf(globals.mRequest.mFD, "\n<li>");
    htmlAnchor("times_graph.html", "Allocation Times", NULL);
    
    PR_fprintf(globals.mRequest.mFD, "\n<li>");
    htmlAnchor("weight_graph.html", "Allocation Weights", NULL);
    
    PR_fprintf(globals.mRequest.mFD, "\n</ul>\n");
#endif /* WANT_GRAPHS */
    
    PR_fprintf(globals.mRequest.mFD, "\n</ul>\n");
    
    return retval;
}

/*
** handleRequest
**
** Based on what file they are asking for, perform some processing.
** Output the results to aFD.
**
** Returns !0 on error.
*/
int handleRequest(tmreader* aTMR, PRFileDesc* aFD, const char* aFileName, const char* aGetData)
{
    int retval = 0;

    if(NULL != aTMR && NULL != aFD && NULL != aFileName && '\0' != *aFileName)
    {
        /*
        ** Init the global request.
        */
        globals.mRequest.mFD = aFD;
        globals.mRequest.mTMR = aTMR;
        globals.mRequest.mFileName = aFileName;
        globals.mRequest.mGetData = aGetData;

        /*
        ** Attempt to find the file of interest.
        */
        if(0 == strcmp("index.html", aFileName))
        {
            int displayRes = 0;

            htmlHeader("SpaceTrace Index");

            displayRes = displayIndex();
            if(0 != displayRes)
            {
                retval = __LINE__;
                REPORT_ERROR(__LINE__, displayIndex);
            }

            htmlFooter();
        }
        else if(0 == strcmp("settings.html", aFileName) || 0 == strcmp("options.html", aFileName))
        {
            int settingsRes = 0;

            htmlHeader("SpaceTrace Settings");

            settingsRes = displaySettings();
            if(0 != settingsRes)
            {
                retval = __LINE__;
                REPORT_ERROR(__LINE__, displaySettings);
            }

            htmlFooter();
        }
        else if(0 == strcmp("top_allocations.html", aFileName))
        {
            int displayRes = 0;

            htmlHeader("SpaceTrace Top Allocations Report");

            displayRes = displayTopAllocations(globals.mCache.mSortedRun, 1);
            if(0 != displayRes)
            {
                retval = __LINE__;
                REPORT_ERROR(__LINE__, displayTopAllocations);
            }

            htmlFooter();
        }
        else if(0 == strcmp("top_callsites.html", aFileName))
        {
            int displayRes = 0;
            tmcallsite** array = NULL;
            PRUint32 arrayCount = 0;

            htmlHeader("SpaceTrace Top Callsites Report");

            if(0 < globals.mCache.mSortedRun->mAllocationCount)
            {
                arrayCount = callsiteArrayFromRun(&array, 0, globals.mCache.mSortedRun);

                if(0 != arrayCount && NULL != array)
                {
                    displayRes = displayTopCallsites(array, arrayCount, 0, 0);
                    if(0 != displayRes)
                    {
                        retval = __LINE__;
                        REPORT_ERROR(__LINE__, displayTopCallsites);
                    }

                    /*
                    ** Done with the array.
                    */
                    free(array);
                    array = NULL;
                }
            }
            else
            {
                retval = __LINE__;
                REPORT_ERROR(__LINE__, handleRequest);
            }

            htmlFooter();
        }
        else if(0 == strcmp("memory_leaks.html", aFileName))
        {
            int displayRes = 0;

            htmlHeader("SpaceTrace Memory Leaks Report");

            displayRes = displayMemoryLeaks(globals.mCache.mSortedRun);
            if(0 != displayRes)
            {
                retval = __LINE__;
                REPORT_ERROR(__LINE__, displayMemoryLeaks);
            }

            htmlFooter();
        }
        else if(0 == strncmp("allocation_", aFileName, 11))
        {
            int scanRes = 0;
            PRUint32 allocationIndex = 0;

            /*
            ** Oh, what a hack....
            ** The index to the allocation structure in the global run
            **   is in the filename.  Better than the pointer value....
            */
            scanRes = PR_sscanf(aFileName + 11, "%u", &allocationIndex);

            if(1 == scanRes && globals.mRun.mAllocationCount > allocationIndex && NULL != globals.mRun.mAllocations[allocationIndex])
            {
                STAllocation* allocation = globals.mRun.mAllocations[allocationIndex];
                char buffer[128];
                int displayRes = 0;

                PR_snprintf(buffer, sizeof(buffer), "SpaceTrace Allocation %u Details Report", allocationIndex);
                htmlHeader(buffer);

                displayRes = displayAllocationDetails(allocation);
                if(0 != displayRes)
                {
                    retval = __LINE__;
                    REPORT_ERROR(__LINE__, displayAllocationDetails);
                }

                htmlFooter();
            }
            else
            {
                htmlNotFound();
            }
        }
        else if(0 == strncmp("callsite_", aFileName, 9))
        {
            int scanRes = 0;
            PRUint32 callsiteSerial = 0;
            tmcallsite* resolved = NULL;

            /*
            ** Oh, what a hack....
            ** The serial(key) to the callsite structure in the hash table
            **   is in the filename.  Better than the pointer value....
            */
            scanRes = PR_sscanf(aFileName + 9, "%u", &callsiteSerial);

            if(1 == scanRes && 0 != callsiteSerial && NULL != (resolved = tmreader_callsite(aTMR, callsiteSerial)))
            {
                char buffer[128];
                int displayRes = 0;

                PR_snprintf(buffer, sizeof(buffer), "SpaceTrace Callsite %u Details Report", callsiteSerial);
                htmlHeader(buffer);

                displayRes = displayCallsiteDetails(resolved);
                if(0 != displayRes)
                {
                    retval = __LINE__;
                    REPORT_ERROR(__LINE__, displayAllocationDetails);
                }

                htmlFooter();
            }
            else
            {
                htmlNotFound();
            }
        }
        else if(0 == strcmp("root_callsites.html", aFileName))
        {
            int displayRes = 0;

            htmlHeader("SpaceTrace Root Callsites");
            
            displayRes = displayCallsites(aTMR->calltree_root.kids, ST_FOLLOW_SIBLINGS, 0, __LINE__);
            if(0 != displayRes)
            {
                retval = __LINE__;
                REPORT_ERROR(__LINE__, displayCallsites);
            }

            htmlFooter();
        }
#if WANT_GRAPHS
        else if(0 == strcmp("footprint_graph.html", aFileName))
        {
            int displayRes = 0;

            htmlHeader("SpaceTrace Memory Footprint Report");
            
            PR_fprintf(globals.mRequest.mFD, "<div align=center>\n");
            PR_fprintf(globals.mRequest.mFD, "<img src=\"./footprint.png\">\n");
            PR_fprintf(globals.mRequest.mFD, "</div>\n");

            htmlFooter();
        }
#endif /* WANT_GRAPHS */
#if WANT_GRAPHS
        else if(0 == strcmp("times_graph.html", aFileName))
        {
            int displayRes = 0;

            htmlHeader("SpaceTrace Allocation Times Report");
            
            PR_fprintf(globals.mRequest.mFD, "<div align=center>\n");
            PR_fprintf(globals.mRequest.mFD, "<img src=\"./times.png\">\n");
            PR_fprintf(globals.mRequest.mFD, "</div>\n");

            htmlFooter();
        }
#endif /* WANT_GRAPHS */
#if WANT_GRAPHS
        else if(0 == strcmp("lifespan_graph.html", aFileName))
        {
            int displayRes = 0;

            htmlHeader("SpaceTrace Allocation Lifespans Report");
            
            PR_fprintf(globals.mRequest.mFD, "<div align=center>\n");
            PR_fprintf(globals.mRequest.mFD, "<img src=\"./lifespan.png\">\n");
            PR_fprintf(globals.mRequest.mFD, "</div>\n");

            htmlFooter();
        }
#endif /* WANT_GRAPHS */
#if WANT_GRAPHS
        else if(0 == strcmp("weight_graph.html", aFileName))
        {
            int displayRes = 0;

            htmlHeader("SpaceTrace Allocation Weights Report");
            
            PR_fprintf(globals.mRequest.mFD, "<div align=center>\n");
            PR_fprintf(globals.mRequest.mFD, "<img src=\"./weight.png\">\n");
            PR_fprintf(globals.mRequest.mFD, "</div>\n");

            htmlFooter();
        }
#endif /* WANT_GRAPHS */
#if WANT_GRAPHS
        else if(0 == strcmp("footprint.png", aFileName))
        {
            int graphRes = 0;

            graphRes = graphFootprint(globals.mCache.mSortedRun);
            if(0 != graphRes)
            {
                retval = __LINE__;
                REPORT_ERROR(__LINE__, graphFootprint);
            }
        }
#endif /* WANT_GRAPHS */
#if WANT_GRAPHS
        else if(0 == strcmp("times.png", aFileName))
        {
            int graphRes = 0;

            graphRes = graphTimeval(globals.mCache.mSortedRun);
            if(0 != graphRes)
            {
                retval = __LINE__;
                REPORT_ERROR(__LINE__, graphTimeval);
            }
        }
#endif /* WANT_GRAPHS */
#if WANT_GRAPHS
        else if(0 == strcmp("lifespan.png", aFileName))
        {
            int graphRes = 0;

            graphRes = graphLifespan(globals.mCache.mSortedRun);
            if(0 != graphRes)
            {
                retval = __LINE__;
                REPORT_ERROR(__LINE__, graphLifespan);
            }
        }
#endif /* WANT_GRAPHS */
#if WANT_GRAPHS
        else if(0 == strcmp("weight.png", aFileName))
        {
            int graphRes = 0;

            graphRes = graphWeight(globals.mCache.mSortedRun);
            if(0 != graphRes)
            {
                retval = __LINE__;
                REPORT_ERROR(__LINE__, graphWeight);
            }
        }
#endif /* WANT_GRAPHS */
#if WANT_QUIT
        else if(0 == strcmp("quit.html", aFileName) || 0 == strcmp("exit.html", aFileName))
        {
            /*
            ** Request to quit the server.
            */
            globals.mStopHttpd = __LINE__;

            htmlHeader("SpaceTrace Goodbye");
            PR_fprintf(globals.mRequest.mFD, "The server is exiting.\n");
            htmlFooter();
        }
#endif /* WANT_QUIT */
        else
        {
            htmlNotFound();
        }

        /*
        ** Clear out global request.
        */
        memset(&globals.mRequest, 0, sizeof(globals.mRequest));
    }
    else
    {
        retval = __LINE__;
        REPORT_ERROR(__LINE__, handleRequest);
    }

    /*
    ** Compact a little if you can after each request.
    */
    heapCompact();

    return retval;
}

/*
** handleClient
**
** Read the fd for the request.
** Output the results.
** Returns !0 on error.
*/
int handleClient(tmreader* aTMR, PRFileDesc* aFD)
{
    int retval = 0;

    if(NULL != aTMR && NULL != aFD)
    {
        char aBuffer[2048];
        PRInt32 readRes = 0;

        readRes = PR_Read(aFD, aBuffer, sizeof(aBuffer));
        if(0 <= readRes)
        {
            const char* sanityCheck = "GET /";

            if(0 == strncmp(sanityCheck, aBuffer, 5))
            {
                char* eourl = NULL;
                char* start = &aBuffer[5];
                char* getData = NULL;
                int realFun = 0;
                const char* crlf = "\015\012";
                char* eoline = NULL;

                /*
                ** Truncate the line if possible.
                ** Only want first one.
                */
                eoline = strstr(aBuffer, crlf);
                if(NULL != eoline)
                {
                    *eoline = '\0';
                }

                /*
                ** Find the whitespace.
                ** That is either end of line or the " HTTP/1.x" suffix.
                ** We do not care.
                */
                for(eourl = start; 0 == isspace(*eourl) && '\0' != *eourl; eourl++)
                {
                    /*
                    ** No body.
                    */
                }

                /*
                ** Cap it off.
                ** Convert empty '/' to index.html.
                */
                *eourl = '\0';
                if('\0' == *start)
                {
                    strcpy(start, "index.html");
                }

                /*
                ** Have we got any GET form data?
                */
                getData = strchr(start, '?');
                if(NULL != getData)
                {
                    /*
                    ** Whack it off.
                    */
                    *getData = '\0';
                    getData++;
                }

                /*
                ** This is totally a hack, but oh well....
                **
                ** Send that the request was OK, regardless.
                **
                ** Other code will tell the user they were wrong.
                ** If the filename contains a ".png", then send the image
                **  mime type, otherwise, say it is text/html. 
                */
                PR_fprintf(aFD, "HTTP/1.0 200%s", crlf);
                PR_fprintf(aFD, "Server: SpaceTrace/0.1%s", crlf);
                PR_fprintf(aFD, "Content-type: ");
                if(NULL != strstr(start, ".png"))
                {
                    PR_fprintf(aFD, "image/png");
                }
                else if(NULL != strstr(start, ".jpg"))
                {
                    PR_fprintf(aFD, "image/jpeg");
                }
                else if(NULL != strstr(start, ".txt"))
                {
                    PR_fprintf(aFD, "text/plain");
                }
                else
                {
                    PR_fprintf(aFD, "text/html");
                }
                PR_fprintf(aFD, crlf);

                /*
                ** One more to seperate headers from content.
                */
                PR_fprintf(aFD, crlf);

                /*
                ** Ready for the real fun.
                */
                realFun = handleRequest(aTMR, aFD, start, getData);
                if(0 != realFun)
                {
                    retval = __LINE__;
                    REPORT_ERROR(__LINE__, handleRequest);
                }
            }
            else
            {
                retval = __LINE__;
                REPORT_ERROR(__LINE__, handleClient);
            }
        }
        else
        {
            retval = __LINE__;
            REPORT_ERROR(__LINE__, lineReader);
        }
    }
    else
    {
        retval = __LINE__;
        REPORT_ERROR(__LINE__, handleClient);
    }

    return retval;
}

/*
** serverMode
**
** List on a port as a httpd.
** Output results interactively on demand.
**
** Returns !0 on error.
*/
int serverMode(tmreader* aTMR)
{
    int retval = 0;
    PRFileDesc* socket = NULL;

    /*
    ** Create a socket.
    */
    socket = PR_NewTCPSocket();
    if(NULL != socket)
    {
        PRStatus closeRes = PR_SUCCESS;
        PRNetAddr bindAddr;
        PRStatus bindRes = PR_SUCCESS;

        /*
        ** Bind it to an interface/port.
        ** Any interface.
        */
        bindAddr.inet.family = PR_AF_INET;
        bindAddr.inet.port = PR_htons((PRUint16)globals.mOptions.mHttpdPort);
        bindAddr.inet.ip = PR_htonl(PR_INADDR_ANY);

        bindRes = PR_Bind(socket, &bindAddr);
        if(PR_SUCCESS == bindRes)
        {
            PRStatus listenRes = PR_SUCCESS;
            const int backlog = 10;

            /*
            ** Start listening for clients.
            ** Give a decent backlog, some of our processing will take
            **  a bit.
            */
            listenRes = PR_Listen(socket, backlog);
            if(PR_SUCCESS == listenRes)
            {
                PRFileDesc* connection = NULL;
                int handleRes = 0;
                int failureSum = 0;
                char message[80];

                /*
                ** Output a little message saying we are receiving.
                */
                PR_snprintf(message, sizeof(message), "server accepting connections on port %u....", globals.mOptions.mHttpdPort);
                REPORT_INFO(message);

                PR_fprintf(PR_STDOUT, "Peak memory used: %s bytes\n", FormatNumber(globals.mPeakMemoryUsed));
                PR_fprintf(PR_STDOUT, "Total calls     : %s",
                           FormatNumber(globals.mMallocCount + globals.mCallocCount + globals.mReallocCount + globals.mFreeCount));
                PR_fprintf(PR_STDOUT, " [%s", FormatNumber(globals.mMallocCount));
                PR_fprintf(PR_STDOUT, " + %s", FormatNumber(globals.mCallocCount));
                PR_fprintf(PR_STDOUT, " + %s", FormatNumber(globals.mReallocCount));
                PR_fprintf(PR_STDOUT, " + %s]\n", FormatNumber(globals.mFreeCount));

                /*
                ** Keep accepting until we know otherwise.
                ** We stay single threaded, as a result page may output
                **  a URL to an image which requires the same processing,
                **  and we serialize the caching of that common data by
                **  only accepting one connection at a time.
                ** Plus, I can defend I'm still sane by not trying to
                **  write a full on HTTPD if I stay single threaded.
                */
                while(0 == globals.mStopHttpd && 0 == retval)
                {
                    connection = PR_Accept(socket, NULL, PR_INTERVAL_NO_TIMEOUT);
                    if(NULL != connection)
                    {
                        /*
                        ** Hand off the connection.
                        ** However, no error for each connection will cause us
                        **  to stop.
                        ** The load time is too painful to bail out here.
                        */
                        handleRes = handleClient(aTMR, connection);
                        if(0 != handleRes)
                        {
                            failureSum += __LINE__;
                            REPORT_ERROR(__LINE__, handleClient);
                        }
                        
                        /*
                        ** Done with the connection.
                        */
                        closeRes = PR_Close(connection);
                        if(PR_SUCCESS != closeRes)
                        {
                            failureSum += __LINE__;
                            REPORT_ERROR(__LINE__, PR_Close);
                        }
                    }
                    else
                    {
                        failureSum += __LINE__;
                        REPORT_ERROR(__LINE__, PR_Accept);
                    }
                }
             
                if(0 != failureSum)
                {
                    retval = __LINE__;
                }

                /*
                ** Output a little message saying it is all over.
                */
                REPORT_INFO("server no longer accepting connections....");
            }
            else
            {
                retval = __LINE__;
                REPORT_ERROR(__LINE__, PR_Listen);
            }
        }
        else
        {
            retval = __LINE__;
            REPORT_ERROR(__LINE__, PR_Bind);
        }

        /*
        ** Done with socket.
        */
        closeRes = PR_Close(socket);
        if(PR_SUCCESS != closeRes)
        {
            retval = __LINE__;
            REPORT_ERROR(__LINE__, PR_Close);
        }
        socket = NULL;
    }
    else
    {
        retval = __LINE__;
        REPORT_ERROR(__LINE__, PR_NewTCPSocket);
    }

    return retval;
}

/*
** batchMode
**
** Perform whatever batch requests we were asked to do.
*/
int batchMode(tmreader* aTMR)
{
    int retval = 0;

    if(NULL != aTMR && 0 != globals.mOptions.mBatchRequestCount)
    {
        PRUint32 loop = 0;
        int failureSum = 0;
        int handleRes = 0;
        char aFileName[1024];
        PRUint32 sprintfRes = 0;

        /*
        ** Go through and process the various files requested.
        ** We do not stop on failure, as it is too costly to rerun the
        **  batch job.
        */
        for(loop = 0; loop < globals.mOptions.mBatchRequestCount; loop++)
        {
            sprintfRes = PR_snprintf(aFileName, sizeof(aFileName), "%s%c%s", globals.mOptions.mOutputDir, PR_GetDirectorySeparator(), globals.mOptions.mBatchRequests[loop]);
            if((PRUint32)-1 != sprintfRes)
            {
                PRFileDesc* outFile = NULL;
                
                outFile = PR_Open(aFileName, ST_FLAGS, ST_PERMS);
                if(NULL != outFile)
                {
                    PRStatus closeRes = PR_SUCCESS;
                    
                    handleRes = handleRequest(aTMR, outFile, globals.mOptions.mBatchRequests[loop], NULL);
                    if(0 != handleRes)
                    {
                        failureSum += __LINE__;
                        REPORT_ERROR(__LINE__, handleRequest);
                    }
                    
                    closeRes = PR_Close(outFile);
                    if(PR_SUCCESS != closeRes)
                    {
                        failureSum += __LINE__;
                        REPORT_ERROR(__LINE__, PR_Close);
                    }
                }
                else
                {
                    failureSum += __LINE__;
                    REPORT_ERROR(__LINE__, PR_Open);
                }
            }
            else
            {
                failureSum += __LINE__;
                REPORT_ERROR(__LINE__, PR_snprintf);
            }
        }

        if(0 != failureSum)
        {
            retval = __LINE__;
        }
    }
    else
    {
        retval = __LINE__;
        REPORT_ERROR(__LINE__, outputReports);
    }

    return retval;
}

/*
** doRun
**
** Perform the actual processing this program requires.
** Returns !0 on failure.
*/
int doRun(void)
{
    int retval = 0;
    tmreader* tmr = NULL;

    /*
    ** Create the new trace-malloc reader.
    */
    tmr = tmreader_new(globals.mOptions.mProgramName, NULL);
    if(NULL != tmr)
    {
        int tmResult = 0;
        int outputResult = 0;

        tmResult = tmreader_eventloop(tmr, globals.mOptions.mFileName, tmEventHandler);
        if(0 == tmResult)
        {
            REPORT_ERROR(__LINE__, tmreader_eventloop);
            retval = __LINE__;
        }

        if(0 == retval)
        {
#if WANT_GRAPHS
            /*
            ** May want to set the max graph timeval, now that we have it.
            */
            if(ST_TIMEVAL_MAX == globals.mOptions.mGraphTimevalMax)
            {
                globals.mOptions.mGraphTimevalMax = (globals.mMaxTimeval - globals.mMinTimeval);
            }
#endif /* WANT_GRAPHS */

            /*
            ** Create the default sorted run.
            */
            if(NULL != globals.mCache.mSortedRun)
            {
                freeRun(globals.mCache.mSortedRun);
            }
            globals.mCache.mSortedRun = createRunFromGlobal();
            if(NULL != globals.mCache.mSortedRun)
            {
                /*
                ** Decide if we're going into batch mode or server mode.
                */
                if(0 != globals.mOptions.mBatchRequestCount)
                {
                    /*
                    ** Output in one big step while everything still exists.
                    */
                    outputResult = batchMode(tmr);
                    if(0 != outputResult)
                    {
                        REPORT_ERROR(__LINE__, batchMode);
                        retval = __LINE__;
                    }
                }
                else
                {
                    int serverRes = 0;
                            
                    /*
                    ** httpd time.
                    */
                    serverRes = serverMode(tmr);
                    if(0 != serverRes)
                    {
                        REPORT_ERROR(__LINE__, serverMode);
                        retval = __LINE__;
                    }
                }
                    
                /*
                ** Done with global sorted run.
                ** Check for NULL again, may have been realloced at some
                **  point with failure.
                */
                if(NULL != globals.mCache.mSortedRun)
                {
                    freeRun(globals.mCache.mSortedRun);
                    globals.mCache.mSortedRun = NULL;
                }
            }
            else
            {
                retval = __LINE__;
                REPORT_ERROR(__LINE__, createRunFromGlobal);
            }
        }

        /*
        ** All done.
        */
        tmreader_destroy(tmr);
        tmr = NULL;
    }
    else
    {
        REPORT_ERROR(__LINE__, tmreader_new);
        retval = __LINE__;
    }

    return retval;
}

/*
** main
**
** Process entry and exit.
*/
int main(int aArgCount, char** aArgArray)
{
    int retval = 0;
    int optionsResult = 0;
    PRStatus prResult = PR_SUCCESS;
    int showedHelp = 0;
    int looper = 0;

    /*
    ** Set the minimum timeval really high so other code
    **  that checks the timeval will get it right.
    */
    globals.mMinTimeval = ST_TIMEVAL_MAX;

    /*
    ** Initialize globals
    */
    globals.mPeakMemoryUsed = 0;
    globals.mMemoryUsed = 0;

    /*
    ** NSPR init.
    */
    PR_Init(PR_USER_THREAD, PR_PRIORITY_NORMAL, 0);

    /*
    ** Handle initializing options.
    */
    optionsResult = initOptions(aArgCount, aArgArray);
    if(0 != optionsResult)
    {
        REPORT_ERROR(optionsResult, initOptions);
        retval = __LINE__;
    }

    /*
    ** Show help on usage if need be.
    */
    showedHelp = showHelp();

    /*
    ** Only perform the run if everything is checking out.
    */
    if(0 == showedHelp && 0 == retval)
    {
        int runResult = 0;

        runResult = doRun();
        if(0 != runResult)
        {
            REPORT_ERROR(runResult, doRun);
            retval = __LINE__;
        }
    }

    /*
    ** Options cleanup.
    */
    for(looper = 0; ST_SUBSTRING_MATCH_MAX > looper; looper++)
    {
        if(NULL != globals.mOptions.mRestrictText[looper])
        {
            free(globals.mOptions.mRestrictText[looper]);
            globals.mOptions.mRestrictText[looper] = NULL;
        }
    }

    /*
    ** All done.
    */
    if(0 != retval)
    {
        REPORT_ERROR(retval, main);
    }

    prResult = PR_Cleanup();
    if(PR_SUCCESS != prResult)
    {
        REPORT_ERROR(retval, PR_Cleanup);
        retval = __LINE__;
    }

    return retval;
}
