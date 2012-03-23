/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is spacetrace.h/spacetrace.c code, released
 * Nov 6, 2001.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Garrett Arch Blythe, 31-October-2001
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
#include <malloc.h>             /* _heapMin */
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

#include "nsQuickSort.h"
#include "prlong.h"
/*
**  strcasecmp API please.
*/
#if defined(_MSC_VER)
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#endif

/*
** the globals variables.  happy joy.
*/
STGlobals globals;

/*
** have the heap cleanup at opportune times, if possible.
*/
void
heapCompact(void)
{
#if defined(XP_WIN32)
    _heapmin();
#endif
}

#define ST_CMD_OPTION_BOOL(option_name, option_genre, option_help) \
    PR_fprintf(PR_STDOUT, "--%s\nDisabled by default.\n%s\n", #option_name, option_help);
#define ST_CMD_OPTION_STRING(option_name, option_genre, default_value, option_help) \
    PR_fprintf(PR_STDOUT, "--%s=<value>\nDefault value is \"%s\".\n%s\n", #option_name, default_value, option_help);
#define ST_CMD_OPTION_STRING_ARRAY(option_name, option_genre, array_size, option_help) \
    PR_fprintf(PR_STDOUT, "--%s=<value>\nUp to %u occurrences allowed.\n%s\n", #option_name, array_size, option_help);
#define ST_CMD_OPTION_STRING_PTR_ARRAY(option_name, option_genre, option_help) \
    PR_fprintf(PR_STDOUT, "--%s=<value>\nUnlimited occurrences allowed.\n%s\n", #option_name, option_help);
#define ST_CMD_OPTION_UINT32(option_name, option_genre, default_value, multiplier, option_help) \
    PR_fprintf(PR_STDOUT, "--%s=<value>\nDefault value is %u.\n%s\n", #option_name, default_value, option_help);
#define ST_CMD_OPTION_UINT64(option_name, option_genre, default_value, multiplier, option_help) \
    PR_fprintf(PR_STDOUT, "--%s=<value>\nDefault value is %llu.\n%s\n", #option_name, default_value, option_help);

/*
** showHelp
**
** Give simple command line help.
** Returns !0 if the help was showed.
*/
int
showHelp(void)
{
    int retval = 0;

    if (PR_FALSE != globals.mCommandLineOptions.mHelp) {
        PR_fprintf(PR_STDOUT, "Usage:\t%s [OPTION]... [-|filename]\n\n",
                   globals.mProgramName);


#include "stoptions.h"

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
PRUint32
ticks2xsec(tmreader * aReader, PRUint32 aTicks, PRUint32 aResolution)
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
int
initOptions(int aArgCount, char **aArgArray)
{
    int retval = 0;
    int traverse = 0;

    /*
     **  Set the initial global default options.
     */
#define ST_CMD_OPTION_BOOL(option_name, option_genre, option_help) globals.mCommandLineOptions.m##option_name = PR_FALSE;
#define ST_CMD_OPTION_STRING(option_name, option_genre, default_value, option_help) PR_snprintf(globals.mCommandLineOptions.m##option_name, sizeof(globals.mCommandLineOptions.m##option_name), "%s", default_value);
#define ST_CMD_OPTION_STRING_ARRAY(option_name, option_genre, array_size, option_help) { int loop; for(loop = 0; loop < array_size; loop++) { globals.mCommandLineOptions.m##option_name[loop][0] = '\0'; } }
#define ST_CMD_OPTION_STRING_PTR_ARRAY(option_name, option_genre, option_help) globals.mCommandLineOptions.m##option_name = NULL; globals.mCommandLineOptions.m##option_name##Count = 0;
#define ST_CMD_OPTION_UINT32(option_name, option_genre, default_value, multiplier, option_help) globals.mCommandLineOptions.m##option_name = default_value * multiplier;
#define ST_CMD_OPTION_UINT64(option_name, option_genre, default_value, multiplier, option_help) { PRUint64 def64 = default_value; PRUint64 mul64 = multiplier; LL_MUL(globals.mCommandLineOptions.m##option_name##64, def64, mul64); }

#include "stoptions.h"

    /*
     ** Go through all arguments.
     ** Two dashes lead off an option.
     ** Any single dash leads off help, unless it is a lone dash (stdin).
     ** Anything else will be attempted as a file to be processed.
     */
    for (traverse = 1; traverse < aArgCount; traverse++) {
        if ('-' == aArgArray[traverse][0] && '-' == aArgArray[traverse][1]) {
            const char *option = &aArgArray[traverse][2];

            /*
             **  Initial if(0) needed to make "else if"s valid.
             */
            if (0) {
            }

#define ST_CMD_OPTION_BOOL(option_name, option_genre, option_help) \
    else if(0 == strcasecmp(option, #option_name)) \
    { \
        globals.mCommandLineOptions.m##option_name = PR_TRUE; \
    }
#define ST_CMD_OPTION_STRING(option_name, option_genre, default_value, option_help) \
    else if(0 == strncasecmp(option, #option_name "=", strlen(#option_name "="))) \
    { \
        PR_snprintf(globals.mCommandLineOptions.m##option_name, sizeof(globals.mCommandLineOptions.m##option_name), "%s", option + strlen(#option_name "=")); \
    }
#define ST_CMD_OPTION_STRING_ARRAY(option_name, option_genre, array_size, option_help) \
    else if(0 == strncasecmp(option, #option_name "=", strlen(#option_name "="))) \
    { \
        int arrLoop = 0; \
        \
        for(arrLoop = 0; arrLoop < array_size; arrLoop++) \
        { \
            if('\0' == globals.mCommandLineOptions.m##option_name[arrLoop][0]) \
            { \
                break; \
            } \
        }\
        \
        if(arrLoop != array_size) \
        { \
            PR_snprintf(globals.mCommandLineOptions.m##option_name[arrLoop], sizeof(globals.mCommandLineOptions.m##option_name[arrLoop]), "%s", option + strlen(#option_name "=")); \
        } \
        else \
        { \
            REPORT_ERROR_MSG(__LINE__, option); \
            retval = __LINE__; \
            globals.mCommandLineOptions.mHelp = PR_TRUE; \
        } \
    }
#define ST_CMD_OPTION_STRING_PTR_ARRAY(option_name, option_genre, option_help) \
    else if(0 == strncasecmp(option, #option_name "=", strlen(#option_name "="))) \
    { \
        const char** expand = NULL; \
        \
        expand = (const char**)realloc((void*)globals.mCommandLineOptions.m##option_name, sizeof(const char*) * (globals.mCommandLineOptions.m##option_name##Count + 1)); \
        if(NULL != expand) \
        { \
            globals.mCommandLineOptions.m##option_name = expand; \
            globals.mCommandLineOptions.m##option_name[globals.mCommandLineOptions.m##option_name##Count] = option + strlen(#option_name "="); \
            globals.mCommandLineOptions.m##option_name##Count++; \
        } \
        else \
        { \
            retval = __LINE__; \
            globals.mCommandLineOptions.mHelp = PR_TRUE; \
        } \
    }
#define ST_CMD_OPTION_UINT32(option_name, option_genre, default_value, multiplier, option_help) \
    else if(0 == strncasecmp(option, #option_name "=", strlen(#option_name "="))) \
    { \
        PRInt32 scanRes = 0; \
        \
        scanRes = PR_sscanf(option + strlen(#option_name "="), "%u", &globals.mCommandLineOptions.m##option_name); \
        if(1 != scanRes) \
        { \
            REPORT_ERROR_MSG(__LINE__, option); \
            retval = __LINE__; \
            globals.mCommandLineOptions.mHelp = PR_TRUE; \
        } \
    }
#define ST_CMD_OPTION_UINT64(option_name, option_genre, default_value, multiplier, option_help) \
    else if(0 == strncasecmp(option, #option_name "=", strlen(#option_name "="))) \
    { \
        PRInt32 scanRes = 0; \
        \
        scanRes = PR_sscanf(option + strlen(#option_name "="), "%llu", &globals.mCommandLineOptions.m##option_name##64); \
        if(1 != scanRes) \
        { \
            REPORT_ERROR_MSG(__LINE__, option); \
            retval = __LINE__; \
            globals.mCommandLineOptions.mHelp = PR_TRUE; \
        } \
    }

#include "stoptions.h"

            /*
             **  If no match on options, this else will get hit.
             */
            else {
                REPORT_ERROR_MSG(__LINE__, option);
                retval = __LINE__;
                globals.mCommandLineOptions.mHelp = PR_TRUE;
            }
        }
        else if ('-' == aArgArray[traverse][0]
                 && '\0' != aArgArray[traverse][1]) {
            /*
             **  Show help, bad/legacy option.
             */
            REPORT_ERROR_MSG(__LINE__, aArgArray[traverse]);
            retval = __LINE__;
            globals.mCommandLineOptions.mHelp = PR_TRUE;
        }
        else {
            /*
             ** Default is same as FileName option, the file to process.
             */
            PR_snprintf(globals.mCommandLineOptions.mFileName,
                        sizeof(globals.mCommandLineOptions.mFileName), "%s",
                        aArgArray[traverse]);
        }
    }

    /*
     ** initialize the categories
     */
    initCategories(&globals);

    return retval;
}

#if ST_WANT_GRAPHS
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
gdImagePtr
createGraph(int *aTransparencyColor)
{
    gdImagePtr retval = NULL;

    if (NULL != aTransparencyColor) {
        *aTransparencyColor = -1;

        retval = gdImageCreate(STGD_WIDTH, STGD_HEIGHT);
        if (NULL != retval) {
            /*
             ** Background color (first one).
             */
            *aTransparencyColor = gdImageColorAllocate(retval, 255, 255, 255);
            if (-1 != *aTransparencyColor) {
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
        else {
            REPORT_ERROR(__LINE__, gdImageCreate);
        }
    }
    else {
        REPORT_ERROR(__LINE__, createGraph);
    }

    return retval;
}
#endif /* ST_WANT_GRAPHS */

#if ST_WANT_GRAPHS
/*
** drawGraph
**
** This function mainly exists to simplify putitng all the pretty lace
**  around a home made graph.
*/
void
drawGraph(gdImagePtr aImage, int aColor,
          const char *aGraphTitle,
          const char *aXAxisTitle,
          const char *aYAxisTitle,
          PRUint32 aXMarkCount,
          PRUint32 * aXMarkPercents,
          const char **aXMarkTexts,
          PRUint32 aYMarkCount,
          PRUint32 * aYMarkPercents,
          const char **aYMarkTexts,
          PRUint32 aLegendCount,
          int *aLegendColors, const char **aLegendTexts)
{
    if (NULL != aImage && NULL != aGraphTitle &&
        NULL != aXAxisTitle && NULL != aYAxisTitle &&
        (0 == aXMarkCount || (NULL != aXMarkPercents && NULL != aXMarkTexts))
        && (0 == aYMarkCount
            || (NULL != aYMarkPercents && NULL != aYMarkTexts))
        && (0 == aLegendCount
            || (NULL != aLegendColors && NULL != aLegendTexts))) {
        int margin = 1;
        PRUint32 traverse = 0;
        PRUint32 target = 0;
        const int markSize = 2;
        int x1 = 0;
        int y1 = 0;
        int x2 = 0;
        int y2 = 0;
        time_t theTimeT = time(NULL);
        char *theTime = ctime(&theTimeT);
        const char *logo = "SpaceTrace";
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
        if (-1 == aColor) {
            aColor = gdImageColorAllocate(aImage, 0, 0, 0);
        }
        if (-1 == aColor) {
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
        for (traverse = 0; traverse < aXMarkCount; traverse++) {
            target =
                ((STGD_WIDTH -
                  (STGD_MARGIN * 2)) * aXMarkPercents[traverse]) / 100;

            x1 = STGD_MARGIN + target;
            y1 = STGD_MARGIN - margin;
            x2 = x1;
            y2 = y1 - markSize;
            gdImageLine(aImage, x1, y1, x2, y2, aColor);

            y1 = STGD_HEIGHT - y1;
            y2 = STGD_HEIGHT - y2;
            gdImageLine(aImage, x1, y1, x2, y2, aColor);

            if (NULL != aXMarkTexts[traverse]) {
                x1 = STGD_MARGIN + target - (markFont->h / 2);
                y1 = STGD_HEIGHT - STGD_MARGIN + margin + markSize +
                    (strlen(aXMarkTexts[traverse]) * markFont->w);
                gdImageStringUp(aImage, markFont, x1, y1,
                                (unsigned char *) aXMarkTexts[traverse],
                                aColor);
            }
        }
        for (traverse = 0; traverse < aYMarkCount; traverse++) {
            target =
                ((STGD_HEIGHT - (STGD_MARGIN * 2)) * (100 -
                                                      aYMarkPercents
                                                      [traverse])) / 100;

            x1 = STGD_MARGIN - margin;
            y1 = STGD_MARGIN + target;
            x2 = x1 - markSize;
            y2 = y1;
            gdImageLine(aImage, x1, y1, x2, y2, aColor);

            x1 = STGD_WIDTH - x1;
            x2 = STGD_WIDTH - x2;
            gdImageLine(aImage, x1, y1, x2, y2, aColor);

            if (NULL != aYMarkTexts[traverse]) {
                x1 = STGD_MARGIN - margin - markSize -
                    (strlen(aYMarkTexts[traverse]) * markFont->w);
                y1 = STGD_MARGIN + target - (markFont->h / 2);
                gdImageString(aImage, markFont, x1, y1,
                              (unsigned char *) aYMarkTexts[traverse],
                              aColor);
            }
        }
        margin += markSize;

        /*
         ** Title will be centered above the image.
         */
        x1 = (STGD_WIDTH / 2) - ((strlen(aGraphTitle) * titleFont->w) / 2);
        y1 = ((STGD_MARGIN - margin) / 2) - (titleFont->h / 2);
        gdImageString(aImage, titleFont, x1, y1,
                      (unsigned char *) aGraphTitle, aColor);

        /*
         ** Upper left will be the date.
         */
        x1 = 0;
        y1 = 0;
        traverse = strlen(theTime) - 1;
        if (isspace(theTime[traverse])) {
            theTime[traverse] = '\0';
        }
        gdImageString(aImage, dateFont, x1, y1, (unsigned char *) theTime,
                      aColor);

        /*
         ** Lower right will be the logo.
         */
        x1 = STGD_WIDTH - (strlen(logo) * logoFont->w);
        y1 = STGD_HEIGHT - logoFont->h;
        gdImageString(aImage, logoFont, x1, y1, (unsigned char *) logo,
                      aColor);

        /*
         ** X and Y axis titles
         */
        x1 = (STGD_WIDTH / 2) - ((strlen(aXAxisTitle) * axisFont->w) / 2);
        y1 = STGD_HEIGHT - axisFont->h;
        gdImageString(aImage, axisFont, x1, y1, (unsigned char *) aXAxisTitle,
                      aColor);
        x1 = 0;
        y1 = (STGD_HEIGHT / 2) + ((strlen(aYAxisTitle) * axisFont->w) / 2);
        gdImageStringUp(aImage, axisFont, x1, y1,
                        (unsigned char *) aYAxisTitle, aColor);

        /*
         ** The legend.
         ** Centered on the right hand side, going up.
         */
        x1 = STGD_WIDTH - STGD_MARGIN + margin +
            (aLegendCount * legendFont->h) / 2;
        x2 = STGD_WIDTH - (aLegendCount * legendFont->h);
        if (x1 > x2) {
            x1 = x2;
        }

        y1 = 0;
        for (traverse = 0; traverse < aLegendCount; traverse++) {
            y2 = (STGD_HEIGHT / 2) +
                ((strlen(aLegendTexts[traverse]) * legendFont->w) / 2);
            if (y2 > y1) {
                y1 = y2;
            }
        }
        for (traverse = 0; traverse < aLegendCount; traverse++) {
            gdImageStringUp(aImage, legendFont, x1, y1,
                            (unsigned char *) aLegendTexts[traverse],
                            aLegendColors[traverse]);
            x1 += legendFont->h;
        }
    }
}

#endif /* ST_WANT_GRAPHS */

#if defined(HAVE_BOUTELL_GD)
/*
** pngSink
**
** GD callback, used to write out the png.
*/
int
pngSink(void *aContext, const char *aBuffer, int aLen)
{
    return PR_Write((PRFileDesc *) aContext, aBuffer, aLen);
}
#endif /* HAVE_BOUTELL_GD */

/*
** FormatNumber
**
** Formats a number with thousands separator. Don't free the result. Returns
** static data.
*/
char *
FormatNumber(PRInt32 num)
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
    while (len >= 0) {
        if (tmpbuf[len] >= '0' && tmpbuf[len] <= '9') {
            if (mod3 == 3) {
                buf[bufindex--] = ',';
                mod3 = 0;
            }
            mod3++;
        }
        buf[bufindex--] = tmpbuf[len--];
    }
    return buf + bufindex + 1;
}

/*
** actualByteSize
**
** Apply alignment and overhead to size to figure out actual byte size
*/
PRUint32
actualByteSize(STOptions * inOptions, PRUint32 retval)
{
    /*
     ** Need to bump the result by our alignment and overhead.
     ** The idea here is that an allocation actually costs you more than you
     **  thought.
     **
     ** The msvcrt malloc has an alignment of 16 with an overhead of 8.
     ** The win32 HeapAlloc has an alignment of 8 with an overhead of 8.
     */
    if (0 != retval) {
        PRUint32 eval = 0;
        PRUint32 over = 0;

        eval = retval - 1;
        if (0 != inOptions->mAlignBy) {
            over = eval % inOptions->mAlignBy;
        }
        retval = eval + inOptions->mOverhead + inOptions->mAlignBy - over;
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
PRUint32
byteSize(STOptions * inOptions, STAllocation * aAlloc)
{
    PRUint32 retval = 0;

    if (NULL != aAlloc && 0 != aAlloc->mEventCount) {
        PRUint32 index = aAlloc->mEventCount;

        /*
         ** Generally, the size is the last event's size.
         */
        do {
            index--;
            retval = aAlloc->mEvents[index].mHeapSize;
        }
        while (0 == retval && 0 != index);
    }
    return actualByteSize(inOptions, retval);
}


/*
** recalculateAllocationCost
**
** Given an allocation, does a recalculation of Cost - weight, heapcount etc.
** and does the right thing to propagate the cost upwards.
*/
int
recalculateAllocationCost(STOptions * inOptions, STContext * inContext,
                          STRun * aRun, STAllocation * aAllocation,
                          PRBool updateParent)
{
    /*
     ** Now, see if they desire a callsite update.
     ** As mentioned previously, we decide if the run desires us to
     **  manipulate the callsite data only if its stamp is set.
     ** We change all callsites and parent callsites to have that
     **  stamp as well, so as to mark them as being relevant to
     **  the current run in question.
     */
    if (NULL != inContext && 0 != aRun->mStats[inContext->mIndex].mStamp) {
        PRUint32 timeval =
            aAllocation->mMaxTimeval - aAllocation->mMinTimeval;
        PRUint32 size = byteSize(inOptions, aAllocation);
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
        aRun->mStats[inContext->mIndex].mCompositeCount++;
        aRun->mStats[inContext->mIndex].mHeapRuntimeCost += heapCost;
        aRun->mStats[inContext->mIndex].mSize += size;
        LL_ADD(aRun->mStats[inContext->mIndex].mTimeval64,
               aRun->mStats[inContext->mIndex].mTimeval64, timeval64);
        LL_ADD(aRun->mStats[inContext->mIndex].mWeight64,
               aRun->mStats[inContext->mIndex].mWeight64, weight64);

        /*
         ** Use the first event of the allocation to update the parent
         **  callsites.
         ** This has positive effect of not updating realloc callsites
         **  with the same data over and over again.
         */
        if (updateParent && 0 < aAllocation->mEventCount) {
            tmcallsite *callsite = aAllocation->mEvents[0].mCallsite;
            STRun *callsiteRun = NULL;

            /*
             ** Go up parents till we drop.
             */
            while (NULL != callsite && NULL != callsite->method) {
                callsiteRun = CALLSITE_RUN(callsite);
                if (NULL != callsiteRun) {
                    /*
                     ** Do we init it?
                     */
                    if (callsiteRun->mStats[inContext->mIndex].mStamp !=
                        aRun->mStats[inContext->mIndex].mStamp) {
                        memset(&callsiteRun->mStats[inContext->mIndex], 0,
                               sizeof(STCallsiteStats));
                        callsiteRun->mStats[inContext->mIndex].mStamp =
                            aRun->mStats[inContext->mIndex].mStamp;
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
                    callsiteRun->mStats[inContext->mIndex].mCompositeCount++;
                    callsiteRun->mStats[inContext->mIndex].mHeapRuntimeCost +=
                        heapCost;
                    callsiteRun->mStats[inContext->mIndex].mSize += size;
                    LL_ADD(callsiteRun->mStats[inContext->mIndex].mTimeval64,
                           callsiteRun->mStats[inContext->mIndex].mTimeval64,
                           timeval64);
                    LL_ADD(callsiteRun->mStats[inContext->mIndex].mWeight64,
                           callsiteRun->mStats[inContext->mIndex].mWeight64,
                           weight64);
                }

                callsite = callsite->parent;
            }
        }
    }

    return 0;
}


/*
** appendAllocation
**
** Given a run, append the allocation to it.
** No DUP checks are done.
** Also, we might want to update the parent callsites with stats.
** We decide to do this heavy duty work only if the run we are appending
**  to has a non ZERO mStats[].mStamp, meaning that it is asking to track
**  such information when it was created.
** Returns !0 on success.
*/
int
appendAllocation(STOptions * inOptions, STContext * inContext,
                 STRun * aRun, STAllocation * aAllocation)
{
    int retval = 0;

    if (NULL != aRun && NULL != aAllocation && NULL != inOptions) {
        STAllocation **expand = NULL;

        /*
         ** Expand the size of the array if needed.
         */
        expand = (STAllocation **) realloc(aRun->mAllocations,
                                           sizeof(STAllocation *) *
                                           (aRun->mAllocationCount + 1));
        if (NULL != expand) {
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
            if (&globals.mRun == aRun) {
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
             ** update allocation cost
             */
            recalculateAllocationCost(inOptions, inContext, aRun, aAllocation,
                                      PR_TRUE);
        }
        else {
            REPORT_ERROR(__LINE__, appendAllocation);
        }
    }
    else {
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
int
hasCallsiteMatch(tmcallsite * aCallsite, const char *aMatch, int aDirection)
{
    int retval = 0;

    if (NULL != aCallsite && NULL != aCallsite->method &&
        NULL != aMatch && '\0' != *aMatch) {
        const char *methodName = NULL;

        do {
            methodName = tmmethodnode_name(aCallsite->method);
            if (NULL != methodName && NULL != strstr(methodName, aMatch)) {
                /*
                 ** Contains the text.
                 */
                retval = __LINE__;
                break;
            }
            else {
                switch (aDirection) {
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
        while (NULL != aCallsite && NULL != aCallsite->method);
    }
    else {
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
** Returns !0 on error, though aOutRun may contain a partial data set.
*/
int
harvestRun(const STRun * aInRun, STRun * aOutRun,
           STOptions * aOptions, STContext * inContext)
{
    int retval = 0;

#if defined(DEBUG_dp)
    PRIntervalTime start = PR_IntervalNow();

    fprintf(stderr, "DEBUG: harvesting run...\n");
#endif

    if (NULL != aInRun && NULL != aOutRun && aInRun != aOutRun
        && NULL != aOptions && NULL != inContext) {
        PRUint32 traverse = 0;
        STAllocation *current = NULL;

        for (traverse = 0;
             0 == retval && traverse < aInRun->mAllocationCount; traverse++) {
            current = aInRun->mAllocations[traverse];
            if (NULL != current) {
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
                if (ST_TIMEVAL_MAX == current->mMaxTimeval) {
                    current->mMaxTimeval = globals.mMaxTimeval;
                }

                /*
                 ** Check allocation timeval restrictions.
                 ** We have to slide the recorded timevals to be zero
                 **  based, so that the comparisons make sense.
                 */
                if ((aOptions->mAllocationTimevalMin >
                     (current->mMinTimeval - globals.mMinTimeval)) ||
                    (aOptions->mAllocationTimevalMax <
                     (current->mMinTimeval - globals.mMinTimeval))) {
                    continue;
                }

                /*
                 ** Check timeval restrictions.
                 ** We have to slide the recorded timevals to be zero
                 **  based, so that the comparisons make sense.
                 */
                if ((aOptions->mTimevalMin >
                     (current->mMinTimeval - globals.mMinTimeval)) ||
                    (aOptions->mTimevalMax <
                     (current->mMinTimeval - globals.mMinTimeval))) {
                    continue;
                }

                /*
                 ** Check lifetime restrictions.
                 */
                lifetime = current->mMaxTimeval - current->mMinTimeval;
                if ((lifetime < aOptions->mLifetimeMin) ||
                    (lifetime > aOptions->mLifetimeMax)) {
                    continue;
                }

                /*
                 ** Check byte size restrictions.
                 */
                bytesize = byteSize(aOptions, current);
                if ((bytesize < aOptions->mSizeMin) ||
                    (bytesize > aOptions->mSizeMax)) {
                    continue;
                }

                /*
                 ** Check weight restrictions.
                 */
                LL_UI2L(bytesize64, bytesize);
                LL_UI2L(lifetime64, lifetime);
                LL_MUL(weight64, bytesize64, lifetime64);
                if (LL_UCMP(weight64, <, aOptions->mWeightMin64) ||
                    LL_UCMP(weight64, >, aOptions->mWeightMax64)) {
                    continue;
                }

                /*
                 ** Possibly restrict the callsite by text.
                 ** Do this last, as it is a heavier check.
                 **
                 ** One day, we may need to expand the logic to check for
                 **  events beyond the initial allocation event.
                 */
                for (looper = 0; ST_SUBSTRING_MATCH_MAX > looper; looper++) {
                    if ('\0' != aOptions->mRestrictText[looper][0]) {
                        if (0 ==
                            hasCallsiteMatch(current->mEvents[0].mCallsite,
                                             aOptions->mRestrictText[looper],
                                             ST_FOLLOW_PARENTS)) {
                            break;
                        }
                    }
                    else {
                        matched = PR_TRUE;
                        break;
                    }
                }
                if (ST_SUBSTRING_MATCH_MAX == looper) {
                    matched = PR_TRUE;
                }
                if (PR_FALSE == matched) {
                    continue;
                }

                /*
                 ** You get here, we add to the run.
                 */
                appendRes =
                    appendAllocation(aOptions, inContext, aOutRun, current);
                if (0 == appendRes) {
                    retval = __LINE__;
                    REPORT_ERROR(__LINE__, appendAllocation);
                }
            }
        }
    }

#if defined(DEBUG_dp)
    fprintf(stderr, "DEBUG: harvesting ends: %dms [%d allocations]\n",
            PR_IntervalToMilliseconds(PR_IntervalNow() - start),
            aInRun->mAllocationCount);
#endif
    return retval;
}

/*
** recalculateRunCost
**
** Goes over all allocations of a run and recalculates and propagates
** the allocation costs - weight, heapcount, size
*/
int
recalculateRunCost(STOptions * inOptions, STContext * inContext, STRun * aRun)
{
    PRUint32 traverse = 0;
    STAllocation *current = NULL;

#if defined(DEBUG_dp)
    PRIntervalTime start = PR_IntervalNow();

    fprintf(stderr, "DEBUG: recalculateRunCost...\n");
#endif

    if (NULL == aRun)
        return -1;

    /* reset stats of this run to 0 to begin recalculation */
    memset(&aRun->mStats[inContext->mIndex], 0, sizeof(STCallsiteStats));

    /* reset timestamp to force propogation of cost */
    aRun->mStats[inContext->mIndex].mStamp = PR_IntervalNow();

    for (traverse = 0; traverse < aRun->mAllocationCount; traverse++) {
        current = aRun->mAllocations[traverse];
        if (NULL != current) {
            recalculateAllocationCost(inOptions, inContext, aRun, current,
                                      PR_TRUE);
        }
    }

#if defined(DEBUG_dp)
    fprintf(stderr, "DEBUG: recalculateRunCost ends: %dms [%d allocations]\n",
            PR_IntervalToMilliseconds(PR_IntervalNow() - start),
            aRun->mAllocationCount);
#endif

    return 0;
}


/*
** compareAllocations
**
** qsort callback.
** Compare the allocations as specified by the options.
*/
int
compareAllocations(const void *aAlloc1, const void *aAlloc2, void *aContext)
{
    int retval = 0;
    STOptions *inOptions = (STOptions *) aContext;

    if (NULL != aAlloc1 && NULL != aAlloc2 && NULL != inOptions) {
        STAllocation *alloc1 = *((STAllocation **) aAlloc1);
        STAllocation *alloc2 = *((STAllocation **) aAlloc2);

        if (NULL != alloc1 && NULL != alloc2) {
            /*
             ** Logic determined by pref/option.
             */
            switch (inOptions->mOrderBy) {
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

                    LL_UI2L(bytesize164, byteSize(inOptions, alloc1));
                    LL_UI2L(timeval164,
                            (alloc1->mMaxTimeval - alloc1->mMinTimeval));
                    LL_MUL(weight164, bytesize164, timeval164);
                    LL_UI2L(bytesize264, byteSize(inOptions, alloc2));
                    LL_UI2L(timeval264,
                            (alloc2->mMaxTimeval - alloc2->mMinTimeval));
                    LL_MUL(weight264, bytesize264, timeval264);

                    if (LL_UCMP(weight164, <, weight264)) {
                        retval = __LINE__;
                    }
                    else if (LL_UCMP(weight164, >, weight264)) {
                        retval = -__LINE__;
                    }
                }
                break;

            case ST_SIZE:
                {
                    PRUint32 size1 = byteSize(inOptions, alloc1);
                    PRUint32 size2 = byteSize(inOptions, alloc2);

                    if (size1 < size2) {
                        retval = __LINE__;
                    }
                    else if (size1 > size2) {
                        retval = -__LINE__;
                    }
                }
                break;

            case ST_TIMEVAL:
                {
                    PRUint32 timeval1 =
                        (alloc1->mMaxTimeval - alloc1->mMinTimeval);
                    PRUint32 timeval2 =
                        (alloc2->mMaxTimeval - alloc2->mMinTimeval);

                    if (timeval1 < timeval2) {
                        retval = __LINE__;
                    }
                    else if (timeval1 > timeval2) {
                        retval = -__LINE__;
                    }
                }
                break;

            case ST_HEAPCOST:
                {
                    PRUint32 cost1 = alloc1->mHeapRuntimeCost;
                    PRUint32 cost2 = alloc2->mHeapRuntimeCost;

                    if (cost1 < cost2) {
                        retval = __LINE__;
                    }
                    else if (cost1 > cost2) {
                        retval = -__LINE__;
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
int
sortRun(STOptions * inOptions, STRun * aRun)
{
    int retval = 0;

    if (NULL != aRun && NULL != inOptions) {
        if (NULL != aRun->mAllocations && 0 < aRun->mAllocationCount) {
            NS_QuickSort(aRun->mAllocations, aRun->mAllocationCount,
                         sizeof(STAllocation *), compareAllocations,
                         inOptions);
        }
    }
    else {
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
STRun *
createRun(STContext * inContext, PRUint32 aStamp)
{
    STRun *retval = NULL;

    retval = (STRun *) calloc(1, sizeof(STRun));
    if (NULL != retval) {
        retval->mStats =
            (STCallsiteStats *) calloc(globals.mCommandLineOptions.mContexts,
                                       sizeof(STCallsiteStats));
        if (NULL != retval->mStats) {
            if (NULL != inContext) {
                retval->mStats[inContext->mIndex].mStamp = aStamp;
            }
        }
        else {
            free(retval);
            retval = NULL;
        }
    }

    return retval;
}

/*
** freeRun
**
** Free off the run and the associated data.
*/
void
freeRun(STRun * aRun)
{
    if (NULL != aRun) {
        if (NULL != aRun->mAllocations) {
            /*
             ** We do not free the allocations themselves.
             ** They are likely pointed to by at least 2 other existing
             **  runs.
             */
            free(aRun->mAllocations);
            aRun->mAllocations = NULL;
        }

        if (NULL != aRun->mStats) {
            free(aRun->mStats);
            aRun->mStats = NULL;
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
STRun *
createRunFromGlobal(STOptions * inOptions, STContext * inContext)
{
    STRun *retval = NULL;

    if (NULL != inOptions && NULL != inContext) {
        /*
         ** We stamp the run.
         ** As things are appended to it, it realizes that it should stamp the
         **  callsite backtrace with the information as well.
         ** In this manner, we can provide meaningful callsite data.
         */
        retval = createRun(inContext, PR_IntervalNow());

        if (NULL != retval) {
            STCategoryNode *node = NULL;
            int failure = 0;
            int harvestRes =
                harvestRun(&globals.mRun, retval, inOptions, inContext);
            if (0 == harvestRes) {
                int sortRes = sortRun(inOptions, retval);

                if (0 != sortRes) {
                    failure = __LINE__;
                }
            }
            else {
                failure = __LINE__;
            }


            if (0 != failure) {
                freeRun(retval);
                retval = NULL;

                REPORT_ERROR(failure, createRunFromGlobal);
            }

            /*
             ** Categorize the run.
             */
            failure = categorizeRun(inOptions, inContext, retval, &globals);
            if (0 != failure) {
                REPORT_ERROR(__LINE__, categorizeRun);
            }

            /*
             ** if we are focussing on a category, return that run instead of
             ** the harvested run. Make sure to recalculate cost.
             */
            node = findCategoryNode(inOptions->mCategoryName, &globals);
            if (node) {
                /* Recalculate cost of run */
                recalculateRunCost(inOptions, inContext,
                                   node->runs[inContext->mIndex]);

                retval = node->runs[inContext->mIndex];
            }
        }
    }
    else {
        REPORT_ERROR(__LINE__, createRunFromGlobal);
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
STAllocation *
getLiveAllocationByHeapID(STRun * aRun, PRUint32 aHeapID)
{
    STAllocation *retval = NULL;

    if (NULL != aRun && 0 != aHeapID) {
        PRUint32 traverse = aRun->mAllocationCount;
        STAllocation *eval = NULL;

        /*
         ** Go through in reverse order.
         ** Stop when we have a return value.
         */
        while (0 < traverse && NULL == retval) {
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
            if (0 != eval->mEventCount) {
                STAllocEvent *event = eval->mEvents + (eval->mEventCount - 1);

                switch (event->mEventType) {
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
                        if (aHeapID == event->mHeapID) {
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
    else {
        REPORT_ERROR(__LINE__, getAllocationByHeapID);
    }

    return retval;
}

/*
** appendEvent
**
** Given an allocation, append a new event to its lifetime.
** Returns the new event on success, otherwise NULL.
*/
STAllocEvent *
appendEvent(STAllocation * aAllocation, PRUint32 aTimeval, char aEventType,
            PRUint32 aHeapID, PRUint32 aHeapSize, tmcallsite * aCallsite)
{
    STAllocEvent *retval = NULL;

    if (NULL != aAllocation && NULL != aCallsite) {
        STAllocEvent *expand = NULL;

        /*
         ** Expand the allocation's event array.
         */
        expand =
            (STAllocEvent *) realloc(aAllocation->mEvents,
                                     sizeof(STAllocEvent) *
                                     (aAllocation->mEventCount + 1));
        if (NULL != expand) {
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
            if (aAllocation->mMinTimeval > aTimeval) {
                aAllocation->mMinTimeval = aTimeval;
            }

            /*
             ** This a free event?
             ** Can only set max timeval on a free.
             ** Otherwise, mMaxTimeval remains  ST_TIMEVAL_MAX.
             ** Set in allocationTracker.
             */
            if (TM_EVENT_FREE == aEventType) {
                aAllocation->mMaxTimeval = aTimeval;
            }
        }
        else {
            REPORT_ERROR(__LINE__, appendEvent);
        }
    }
    else {
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
int
hasAllocation(STRun * aRun, STAllocation * aTestFor)
{
    int retval = 0;

    if (NULL != aRun && NULL != aTestFor) {
        PRUint32 traverse = aRun->mAllocationCount;

        /*
         ** Go through reverse, in the hopes it exists nearer the end.
         */
        while (0 < traverse) {
            /*
             ** Back up.
             */
            traverse--;

            if (aTestFor == aRun->mAllocations[traverse]) {
                retval = __LINE__;
                break;
            }
        }
    }
    else {
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
STAllocation *
allocationTracker(PRUint32 aTimeval, char aType, PRUint32 aHeapRuntimeCost,
                  tmcallsite * aCallsite, PRUint32 aHeapID, PRUint32 aSize,
                  tmcallsite * aOldCallsite, PRUint32 aOldHeapID,
                  PRUint32 aOldSize)
{
    STAllocation *retval = NULL;
    static int compactor = 1;
    const int frequency = 10000;
    PRUint32 actualSize, actualOldSize = 0;

    actualSize = actualByteSize(&globals.mCommandLineOptions, aSize);
    if (aOldSize)
        actualOldSize =
            actualByteSize(&globals.mCommandLineOptions, aOldSize);

    if (NULL != aCallsite) {
        int newAllocation = 0;
        tmcallsite *searchCallsite = NULL;
        PRUint32 searchHeapID = 0;
        STAllocation *allocation = NULL;

        /*
         ** Global operation ID increases.
         */
        globals.mOperationCount++;

        /*
         ** Fix up the timevals if needed.
         */
        if (aTimeval < globals.mMinTimeval) {
            globals.mMinTimeval = aTimeval;
        }
        if (aTimeval > globals.mMaxTimeval) {
            globals.mMaxTimeval = aTimeval;
        }

        switch (aType) {
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
                if (globals.mMemoryUsed > globals.mPeakMemoryUsed) {
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
                if (globals.mMemoryUsed > globals.mPeakMemoryUsed) {
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
                if (globals.mMemoryUsed > globals.mPeakMemoryUsed) {
                    globals.mPeakMemoryUsed = globals.mMemoryUsed;
                }

                /*
                 ** This might be a new allocation.
                 */
                if (NULL == aOldCallsite) {
                    newAllocation = __LINE__;
                }
                else {
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
        if (0 != newAllocation) {
            allocation = (STAllocation *) calloc(1, sizeof(STAllocation));
            if (NULL != allocation) {
                /*
                 ** Fixup the min timeval so if logic later will just work.
                 */
                allocation->mMinTimeval = ST_TIMEVAL_MAX;
                allocation->mMaxTimeval = ST_TIMEVAL_MAX;
            }
        }
        else if (NULL != searchCallsite
                 && NULL != CALLSITE_RUN(searchCallsite)
                 && 0 != searchHeapID) {
            /*
             ** We know what to search for, and we reduce what we search
             **  by only looking for those allocations at a known callsite.
             */
            allocation =
                getLiveAllocationByHeapID(CALLSITE_RUN(searchCallsite),
                                          searchHeapID);
        }
        else {
            REPORT_ERROR(__LINE__, allocationTracker);
        }

        if (NULL != allocation) {
            STAllocEvent *appendResult = NULL;

            /*
             ** Record the amount of time this allocation event took.
             */
            allocation->mHeapRuntimeCost += aHeapRuntimeCost;

            /*
             ** Now that we have an allocation, we need to make sure it has
             **  the proper event.
             */
            appendResult =
                appendEvent(allocation, aTimeval, aType, aHeapID, aSize,
                            aCallsite);
            if (NULL != appendResult) {
                if (0 != newAllocation) {
                    int runAppendResult = 0;
                    int callsiteAppendResult = 0;

                    /*
                     ** A new allocation needs to be added to the global run.
                     ** A new allocation needs to be added to the callsite.
                     */
                    runAppendResult =
                        appendAllocation(&globals.mCommandLineOptions, NULL,
                                         &globals.mRun, allocation);
                    callsiteAppendResult =
                        appendAllocation(&globals.mCommandLineOptions, NULL,
                                         CALLSITE_RUN(aCallsite), allocation);
                    if (0 != runAppendResult && 0 != callsiteAppendResult) {
                        /*
                         ** Success.
                         */
                        retval = allocation;
                    }
                    else {
                        REPORT_ERROR(__LINE__, appendAllocation);
                    }
                }
                else {
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
                    if (aCallsite != searchCallsite) {
                        int found = 0;

                        found =
                            hasAllocation(CALLSITE_RUN(aCallsite),
                                          allocation);
                        if (0 == found) {
                            int appendResult = 0;

                            appendResult =
                                appendAllocation(&globals.mCommandLineOptions,
                                                 NULL,
                                                 CALLSITE_RUN(aCallsite),
                                                 allocation);
                            if (0 != appendResult) {
                                /*
                                 ** Success.
                                 */
                                retval = allocation;
                            }
                            else {
                                REPORT_ERROR(__LINE__, appendAllocation);
                            }
                        }
                        else {
                            /*
                             ** Already there.
                             */
                            retval = allocation;
                        }
                    }
                    else {
                        /*
                         ** Success.
                         */
                        retval = allocation;
                    }
                }
            }
            else {
                REPORT_ERROR(__LINE__, appendEvent);
            }
        }
        else {
            REPORT_ERROR(__LINE__, allocationTracker);
        }
    }
    else {
        REPORT_ERROR(__LINE__, allocationTracker);
    }

    /*
     ** Compact the heap a bit if you can.
     */
    compactor++;
    if (0 == (compactor % frequency)) {
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
void
trackEvent(PRUint32 aTimeval, char aType, PRUint32 aHeapRuntimeCost,
           tmcallsite * aCallsite, PRUint32 aHeapID, PRUint32 aSize,
           tmcallsite * aOldCallsite, PRUint32 aOldHeapID, PRUint32 aOldSize)
{
    if (NULL != aCallsite) {
        /*
         ** Verify the old callsite just in case.
         */
        if (NULL != CALLSITE_RUN(aCallsite)
            && (NULL == aOldCallsite || NULL != CALLSITE_RUN(aOldCallsite))) {
            STAllocation *allocation = NULL;

            /*
             ** Add to the allocation tracking code.
             */
            allocation =
                allocationTracker(aTimeval, aType, aHeapRuntimeCost,
                                  aCallsite, aHeapID, aSize, aOldCallsite,
                                  aOldHeapID, aOldSize);

            if (NULL == allocation) {
                REPORT_ERROR(__LINE__, allocationTracker);
            }
        }
        else {
            REPORT_ERROR(__LINE__, trackEvent);
        }
    }
    else {
        REPORT_ERROR(__LINE__, trackEvent);
    }
}

/*
** tmEventHandler
**
** Callback from the tmreader_eventloop function.
** Simply tries to sort out what we desire to know.
*/

static const char spinner_chars[] = { '/', '-', '\\', '|' };

#define SPINNER_UPDATE_FREQUENCY 4096
#define SPINNER_CHAR_COUNT (sizeof(spinner_chars) / sizeof(spinner_chars[0]))
#define SPINNER_CHAR(_x) spinner_chars[(_x / SPINNER_UPDATE_FREQUENCY) % SPINNER_CHAR_COUNT]

void
tmEventHandler(tmreader * aReader, tmevent * aEvent)
{
    static int event_count = 0;     /* for spinner */
    if ((event_count++ % SPINNER_UPDATE_FREQUENCY) == 0)
        printf("\rReading... %c", SPINNER_CHAR(event_count));
    
    if (NULL != aReader && NULL != aEvent) {
        switch (aEvent->type) {
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
                tmcallsite *callsite = NULL;
                tmcallsite *oldcallsite = NULL;

                if (TM_EVENT_REALLOC == aEvent->type) {
                    /*
                     ** Only care about old arguments if there were any.
                     */
                    if (0 != aEvent->u.alloc.oldserial) {
                        oldptr = aEvent->u.alloc.oldptr;
                        oldsize = aEvent->u.alloc.oldsize;
                        oldcallsite =
                            tmreader_callsite(aReader,
                                              aEvent->u.alloc.oldserial);
                        if (NULL == oldcallsite) {
                            REPORT_ERROR(__LINE__, tmreader_callsite);
                        }
                    }
                }

                callsite = tmreader_callsite(aReader, aEvent->serial);
                if (NULL != callsite) {
                    /*
                     ** Verify a callsite run is there.
                     ** If not, we are ignoring this callsite.
                     */
                    if (NULL != CALLSITE_RUN(callsite)) {
                        char eventType = aEvent->type;
                        PRUint32 eventSize = aEvent->u.alloc.size;

                        /*
                         ** Play a nasty trick on reallocs of size zero.
                         ** They are to become free events, adjust the size accordingly.
                         ** This allows me to avoid all types of special case code.
                         */
                        if (0 == aEvent->u.alloc.size
                            && TM_EVENT_REALLOC == aEvent->type) {
                            eventType = TM_EVENT_FREE;
                            if (0 != aEvent->u.alloc.oldserial) {
                                eventSize = aEvent->u.alloc.oldsize;
                            }
                        }
                        trackEvent(ticks2msec
                                   (aReader, aEvent->u.alloc.interval),
                                   eventType, ticks2usec(aReader,
                                                         aEvent->u.alloc.
                                                         cost), callsite,
                                   aEvent->u.alloc.ptr, eventSize,
                                   oldcallsite, oldptr, oldsize);
                    }
                }
                else {
                    REPORT_ERROR(__LINE__, tmreader_callsite);
                }
            }
            break;

            /*
             ** Callsite, set up the callsite run if it does not exist.
             */
        case TM_EVENT_CALLSITE:
            {
                tmcallsite *callsite =
                    tmreader_callsite(aReader, aEvent->serial);

                if (NULL != callsite) {
                    if (NULL == CALLSITE_RUN(callsite)) {
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
                        if (0 !=
                            hasCallsiteMatch(callsite, "g_main_is_running",
                                             ST_FOLLOW_PARENTS)) {
                            createrun = 0;
                        }
#endif /* MOZILLA_CLIENT */

                        if (0 != createrun) {
                            callsite->data = createRun(NULL, 0);
                        }
                    }
                }
                else {
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
**  optionGetDataOut
**
**  Output option get data.
*/
void
optionGetDataOut(PRFileDesc * inFD, STOptions * inOptions)
{
    if (NULL != inFD && NULL != inOptions) {
        int mark = 0;

#define ST_WEB_OPTION_BOOL(option_name, option_genre, option_help) \
    PR_fprintf(inFD, "%s%s=%d", (0 == mark++) ? "?" : "&", #option_name, inOptions->m##option_name);
#define ST_WEB_OPTION_STRING(option_name, option_genre, default_value, option_help) \
    PR_fprintf(inFD, "%s%s=%s", (0 == mark++) ? "?" : "&", #option_name, inOptions->m##option_name);
#define ST_WEB_OPTION_STRING_ARRAY(option_name, option_genre, array_size, option_help) \
    { \
        PRUint32 loop = 0; \
        \
        for(loop = 0; loop < array_size; loop++) \
        { \
            PR_fprintf(inFD, "%s%s=%s", (0 == mark++) ? "?" : "&", #option_name, inOptions->m##option_name[loop]); \
        } \
    }
#define ST_WEB_OPTION_STRING_PTR_ARRAY(option_name, option_genre, option_help)  /* no implementation */
#define ST_WEB_OPTION_UINT32(option_name, option_genre, default_value, multiplier, option_help) \
    PR_fprintf(inFD, "%s%s=%u", (0 == mark++) ? "?" : "&", #option_name, inOptions->m##option_name / multiplier);
#define ST_WEB_OPTION_UINT64(option_name, option_genre, default_value, multiplier, option_help) \
    { \
        PRUint64 def64 = default_value; \
        PRUint64 mul64 = multiplier; \
        PRUint64 div64; \
        \
        LL_DIV(div64, inOptions->m##option_name##64, mul64); \
        PR_fprintf(inFD, "%s%s=%llu", (0 == mark++) ? "?" : "&", #option_name, div64); \
    }

#include "stoptions.h"
    }
}

/*
** htmlAnchor
**
** Output an HTML anchor, or just the text depending on the mode.
*/
void
htmlAnchor(STRequest * inRequest,
           const char *aHref,
           const char *aText,
           const char *aTarget, const char *aClass, STOptions * inOptions)
{
    if (NULL != aHref && '\0' != *aHref && NULL != aText && '\0' != *aText) {
        int anchorLive = 1;

        /*
         ** In batch mode, we need to verify the anchor is live.
         */
        if (0 != inRequest->mOptions.mBatchRequestCount) {
            PRUint32 loop = 0;
            int comparison = 1;

            for (loop = 0; loop < inRequest->mOptions.mBatchRequestCount;
                 loop++) {
                comparison =
                    strcmp(aHref, inRequest->mOptions.mBatchRequest[loop]);
                if (0 == comparison) {
                    break;
                }
            }

            /*
             ** Did we find it?
             */
            if (0 == comparison) {
                anchorLive = 0;
            }
        }

        /*
         ** In any mode, don't make an href to the current page.
         */
        if (0 != anchorLive && NULL != inRequest->mGetFileName) {
            if (0 == strcmp(aHref, inRequest->mGetFileName)) {
                anchorLive = 0;
            }
        }

        /*
         **  Do the right thing.
         */
        if (0 != anchorLive) {
            PR_fprintf(inRequest->mFD, "<a class=\"%s\" ", aClass);
            if (NULL != aTarget && '\0' != *aTarget) {
                PR_fprintf(inRequest->mFD, "target=\"%s\" ", aTarget);
            }
            PR_fprintf(inRequest->mFD, "href=\"./%s", aHref);

            /*
             **  The options, if desired, get appended as form data.
             */
            optionGetDataOut(inRequest->mFD, inOptions);

            PR_fprintf(inRequest->mFD, "\">%s</a>\n", aText);
        }
        else {
            PR_fprintf(inRequest->mFD, "<span class=\"%s\">%s</span>\n",
                       aClass, aText);
        }
    }
    else {
        REPORT_ERROR(__LINE__, htmlAnchor);
    }
}

/*
** htmlAllocationAnchor
**
** Output an html achor that will resolve to the allocation in question.
*/
void
htmlAllocationAnchor(STRequest * inRequest, STAllocation * aAllocation,
                     const char *aText)
{
    if (NULL != aAllocation && NULL != aText && '\0' != *aText) {
        char buffer[128];

        /*
         ** This is a total hack.
         ** The filename contains the index of the allocation in globals.mRun.
         ** Safer than using the raw pointer value....
         */
        PR_snprintf(buffer, sizeof(buffer), "allocation_%u.html",
                    aAllocation->mRunIndex);

        htmlAnchor(inRequest, buffer, aText, NULL, "allocation",
                   &inRequest->mOptions);
    }
    else {
        REPORT_ERROR(__LINE__, htmlAllocationAnchor);
    }
}

/*
** resolveSourceFile
**
** Easy way to get a readable/short name.
** NULL if not present, not resolvable.
*/
const char *
resolveSourceFile(tmmethodnode * aMethod)
{
    const char *retval = NULL;

    if (NULL != aMethod) {
        const char *methodSays = NULL;

        methodSays = aMethod->sourcefile;

        if (NULL != methodSays && '\0' != methodSays[0]
            && 0 != strcmp("noname", methodSays)) {
            retval = strrchr(methodSays, '/');
            if (NULL != retval) {
                retval++;
            }
            else {
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
** RealName determines whether or not we crawl our parents until the point
**  we no longer match stats.
*/
void
htmlCallsiteAnchor(STRequest * inRequest, tmcallsite * aCallsite,
                   const char *aText, int aRealName)
{
    if (NULL != aCallsite) {
        char textBuf[512];
        char hrefBuf[128];
        tmcallsite *namesite = aCallsite;

        /*
         ** Should we use a different name?
         */
        if (0 == aRealName && NULL != namesite->parent
            && NULL != namesite->parent->method) {
            STRun *myRun = NULL;
            STRun *upRun = NULL;

            do {
                myRun = CALLSITE_RUN(namesite);
                upRun = CALLSITE_RUN(namesite->parent);

                if (0 !=
                    memcmp(&myRun->mStats[inRequest->mContext->mIndex],
                           &upRun->mStats[inRequest->mContext->mIndex],
                           sizeof(STCallsiteStats))) {
                    /*
                     ** Doesn't match, stop.
                     */
                    break;
                }
                else {
                    /*
                     ** Matches, keep going up.
                     */
                    namesite = namesite->parent;
                }
            }
            while (NULL != namesite->parent
                   && NULL != namesite->parent->method);
        }

        /*
         ** If no text, provide our own.
         */
        if (NULL == aText || '\0' == *aText) {
            const char *methodName = NULL;
            const char *sourceFile = NULL;

            if (NULL != namesite->method) {
                methodName = tmmethodnode_name(namesite->method);
            }
            else {
                methodName = "==NONAME==";
            }

            /*
             ** Decide which format to use to identify the callsite.
             ** If we can detect availability, hook up the filename with lxr information.
             */
            sourceFile = resolveSourceFile(namesite->method);
            if (NULL != sourceFile
                && 0 == strncmp("mozilla/", namesite->method->sourcefile,
                                8)) {
                char lxrHREFBuf[512];

                PR_snprintf(lxrHREFBuf, sizeof(lxrHREFBuf),
                            " [<a href=\"http://lxr.mozilla.org/mozilla/source/%s#%u\" class=\"lxr\" target=\"_st_lxr\">%s:%u</a>]",
                            namesite->method->sourcefile + 8,
                            namesite->method->linenumber, sourceFile,
                            namesite->method->linenumber);
                PR_snprintf(textBuf, sizeof(textBuf),
                            "<span class=\"source mozilla-source\">%s</span>%s",
                            methodName, lxrHREFBuf);
            }
            else if (NULL != sourceFile) {
                PR_snprintf(textBuf, sizeof(textBuf),
                            "<span class=\"source external-source\">%s [<span class=\"source-extra\">%s:%u</span>]</span>",
                            methodName, sourceFile,
                            namesite->method->linenumber);
            }
            else {
                PR_snprintf(textBuf, sizeof(textBuf),
                            "<span class=\"source binary-source\">%s [<span class=\"source-extra\">+%u(%u)</span>]</span>",
                            methodName, namesite->offset,
                            (PRUint32) namesite->entry.key);
            }

            aText = textBuf;
        }

        PR_snprintf(hrefBuf, sizeof(hrefBuf), "callsite_%u.html",
                    (PRUint32) aCallsite->entry.key);

        htmlAnchor(inRequest, hrefBuf, aText, NULL, "callsite",
                   &inRequest->mOptions);
    }
    else {
        REPORT_ERROR(__LINE__, htmlCallsiteAnchor);
    }
}

/*
** htmlHeader
**
** Output a standard header in the report files.
*/
void
htmlHeader(STRequest * inRequest, const char *aTitle)
{
    PR_fprintf(inRequest->mFD,
               "<html>\n"
               "<head>\n"
               "<title>%s</title>\n"
               "<link rel=\"stylesheet\" href=\"spacetrace.css\" type=\"text/css\""
               "</head>\n"
               "<body>\n"
               "<div class=spacetrace-header>\n"
               "<span class=spacetrace-title>Spacetrace</span>"
               "<span class=navigate>\n"
               "<span class=\"category-title header-text\">Category:</span>\n"
               "<span class=\"current-category\">%s</span>\n",
               aTitle, inRequest->mOptions.mCategoryName);

    PR_fprintf(inRequest->mFD, "<span class=\"header-item\">");
    htmlAnchor(inRequest, "index.html", "Index", NULL, "header-menuitem",
               &inRequest->mOptions);
    PR_fprintf(inRequest->mFD, "</span>\n");

    PR_fprintf(inRequest->mFD, "<span class=\"header-item\">");
    htmlAnchor(inRequest, "options.html", "Options", NULL, "header-menuitem",
               &inRequest->mOptions);
    PR_fprintf(inRequest->mFD, "</span>\n");

    PR_fprintf(inRequest->mFD, "</span>\n");    /* class=navigate */

    PR_fprintf(inRequest->mFD,
               "</div>\n\n<div class=\"header-separator\"></div>\n\n");
}

/*
** htmlFooter
**
** Output a standard footer in the report file.
*/
void
htmlFooter(STRequest * inRequest)
{
    PR_fprintf(inRequest->mFD,
               "<div class=\"footer-separator\"></div>\n\n"
               "<div class=\"footer\">\n"
               "<span class=\"footer-text\">SpaceTrace</span>\n"
               "</div>\n\n" "</body>\n" "</html>\n");
}

/*
** htmlNotFound
**
** Not found message.
*/
void
htmlNotFound(STRequest * inRequest)
{
    htmlHeader(inRequest, "File Not Found");
    PR_fprintf(inRequest->mFD, "File Not Found\n");
    htmlFooter(inRequest);
}

void
htmlStartTable(STRequest* inRequest,
               const char* table_class,
               const char* id,
               const char* caption,
               const char * const headers[], PRUint32 header_length)
{
        PRUint32 i;
        
        PR_fprintf(inRequest->mFD,
                   "<div id=\"%s\"><table class=\"data %s\">\n"
                   "  <caption>%s</caption>"
                   "  <thead>\n"
                   "    <tr class=\"row-header\">\n", id,
                   table_class ? table_class : "",
                   caption);
        
        for (i=0; i< header_length; i++)
            PR_fprintf(inRequest->mFD,
                       "      <th>%s</th>\n", headers[i]);
        
        PR_fprintf(inRequest->mFD, "    </tr>  </thead>  <tbody>\n");
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
PRUint32
callsiteArrayFromCallsite(tmcallsite *** aArray, PRUint32 aExistingCount,
                          tmcallsite * aSite, int aFollow)
{
    PRUint32 retval = 0;

    if (NULL != aArray && NULL != aSite) {
        tmcallsite **expand = NULL;

        /*
         ** If we have an existing count, we just keep expanding this.
         */
        retval = aExistingCount;

        /*
         ** Go through every allocation.
         */
        do {
            /*
             ** expand the array.
             */
            expand =
                (tmcallsite **) realloc(*aArray,
                                        sizeof(tmcallsite *) * (retval + 1));
            if (NULL != expand) {
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
            else {
                REPORT_ERROR(__LINE__, realloc);
                break;
            }


            /*
             ** What do we follow?
             */
            switch (aFollow) {
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
        while (NULL != aSite && NULL != aSite->method);
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
PRUint32
callsiteArrayFromRun(tmcallsite *** aArray, PRUint32 aExistingCount,
                     STRun * aRun)
{
    PRUint32 retval = 0;

    if (NULL != aArray && NULL != aRun && 0 < aRun->mAllocationCount) {
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
        for (allocLoop = 0;
             0 == stopLoops && allocLoop < aRun->mAllocationCount;
             allocLoop++) {
            /*
             ** Go through every event.
             */
            for (eventLoop = 0;
                 0 == stopLoops
                 && eventLoop < aRun->mAllocations[allocLoop]->mEventCount;
                 eventLoop++) {
                /*
                 ** Skip the free events.
                 */
                if (TM_EVENT_FREE !=
                    aRun->mAllocations[allocLoop]->mEvents[eventLoop].
                    mEventType) {
                    tmcallsite **expand = NULL;

                    /*
                     ** expand the array.
                     */
                    expand =
                        (tmcallsite **) realloc(*aArray,
                                                sizeof(tmcallsite *) *
                                                (retval + 1));
                    if (NULL != expand) {
                        /*
                         ** Set the callsite in case of pointer move.
                         */
                        *aArray = expand;

                        /*
                         ** Assign the value.
                         */
                        (*aArray)[retval] =
                            aRun->mAllocations[allocLoop]->mEvents[eventLoop].
                            mCallsite;
                        retval++;
                    }
                    else {
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
** In case of dups, specify an index on non "1" to get others.
** Do not touch storage space unless a find is made.
** Returns !0 on failure.
*/
int
getDataPRUint32Base(const FormData * aGetData, const char *aCheckFor,
                    int inIndex, void *aStoreResult, PRUint32 aBits)
{
    int retval = 0;

    if (NULL != aGetData && NULL != aCheckFor && 0 != inIndex
        && NULL != aStoreResult) {
        unsigned finder = 0;

        /*
         **  Loop over the names, looking for an exact string match.
         **  Skip over initial finds, decrementing inIndex, until "1".
         */
        for (finder = 0; finder < aGetData->mNVCount; finder++) {
            if (0 == strcmp(aCheckFor, aGetData->mNArray[finder])) {
                inIndex--;

                if (0 == inIndex) {
                    PRInt32 scanRes = 0;

                    if (64 == aBits) {
                        scanRes =
                            PR_sscanf(aGetData->mVArray[finder], "%llu",
                                      aStoreResult);
                    }
                    else {
                        scanRes =
                            PR_sscanf(aGetData->mVArray[finder], "%u",
                                      aStoreResult);
                    }
                    if (1 != scanRes) {
                        retval = __LINE__;
                        REPORT_ERROR(__LINE__, PR_sscanf);
                    }
                    break;
                }
            }
        }
    }
    else {
        retval = __LINE__;
        REPORT_ERROR(__LINE__, getDataPRUint32Base);
    }

    return retval;
}

int
getDataPRUint32(const FormData * aGetData, const char *aCheckFor, int inIndex,
                PRUint32 * aStoreResult, PRUint32 aConversion)
{
    int retval = 0;

    retval =
        getDataPRUint32Base(aGetData, aCheckFor, inIndex, aStoreResult, 32);
    *aStoreResult *= aConversion;

    return retval;
}

int
getDataPRUint64(const FormData * aGetData, const char *aCheckFor, int inIndex,
                PRUint64 * aStoreResult64, PRUint64 aConversion64)
{
    int retval = 0;
    PRUint64 value64 = LL_INIT(0, 0);

    retval = getDataPRUint32Base(aGetData, aCheckFor, inIndex, &value64, 64);
    LL_MUL(*aStoreResult64, value64, aConversion64);

    return retval;
}

/*
** getDataString
**
** Pull out the string data, if specified.
** In case of dups, specify an index on non "1" to get others.
** Do not touch storage space unless a find is made.
** Return !0 on failure.
*/
int
getDataString(const FormData * aGetData, const char *aCheckFor, int inIndex,
              char *aStoreResult, int inStoreResultLength)
{
    int retval = 0;

    if (NULL != aGetData && NULL != aCheckFor && 0 != inIndex
        && NULL != aStoreResult && 0 != inStoreResultLength) {
        unsigned finder = 0;

        /*
         **  Loop over the names, looking for an exact string match.
         **  Skip over initial finds, decrementing inIndex, until "1".
         */
        for (finder = 0; finder < aGetData->mNVCount; finder++) {
            if (0 == strcmp(aCheckFor, aGetData->mNArray[finder])) {
                inIndex--;

                if (0 == inIndex) {
                    PR_snprintf(aStoreResult, inStoreResultLength, "%s",
                                aGetData->mVArray[finder]);
                    break;
                }
            }
        }
    }
    else {
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
int
displayTopAllocations(STRequest * inRequest, STRun * aRun,
                      const char* id,
                      const char* caption,
                      int aWantCallsite)
{
    int retval = 0;

    if (NULL != aRun) {
        if (0 < aRun->mAllocationCount) {
            PRUint32 loop = 0;
            STAllocation *current = NULL;

            static const char* const headers[] = {
                "Rank", "Index", "Byte Size", "Lifespan (sec)",
                "Weight", "Heap Op (sec)"
            };
            
            static const char* const headers_callsite[] = {
                "Rank", "Index", "Byte Size", "Lifespan (sec)",
                "Weight", "Heap Op (sec)", "Origin Callsite"
            };

            if (aWantCallsite)
                htmlStartTable(inRequest, NULL, id,
                               caption,
                               headers_callsite,
                               sizeof(headers_callsite) / sizeof(headers_callsite[0]));
            else
                htmlStartTable(inRequest, NULL, id, caption,
                               headers,
                               sizeof(headers) / sizeof(headers[0]));                           
            /*
             ** Loop over the items, up to some limit or until the end.
             */
            for (loop = 0;
                 loop < inRequest->mOptions.mListItemMax
                 && loop < aRun->mAllocationCount; loop++) {
                current = aRun->mAllocations[loop];
                if (NULL != current) {
                    PRUint32 lifespan =
                        current->mMaxTimeval - current->mMinTimeval;
                    PRUint32 size = byteSize(&inRequest->mOptions, current);
                    PRUint32 heapCost = current->mHeapRuntimeCost;
                    PRUint64 weight64 = LL_INIT(0, 0);
                    PRUint64 size64 = LL_INIT(0, 0);
                    PRUint64 lifespan64 = LL_INIT(0, 0);
                    char buffer[32];

                    LL_UI2L(size64, size);
                    LL_UI2L(lifespan64, lifespan);
                    LL_MUL(weight64, size64, lifespan64);

                    PR_fprintf(inRequest->mFD, "<tr>\n");

                    /*
                     ** Rank.
                     */
                    PR_fprintf(inRequest->mFD, "<td align=right>%u</td>\n",
                               loop + 1);

                    /*
                     ** Index.
                     */
                    PR_snprintf(buffer, sizeof(buffer), "%u",
                                current->mRunIndex);
                    PR_fprintf(inRequest->mFD, "<td align=right>\n");
                    htmlAllocationAnchor(inRequest, current, buffer);
                    PR_fprintf(inRequest->mFD, "</td>\n");

                    /*
                     ** Byte Size.
                     */
                    PR_fprintf(inRequest->mFD, "<td align=right>%u</td>\n",
                               size);

                    /*
                     ** Lifespan.
                     */
                    PR_fprintf(inRequest->mFD,
                               "<td align=right>" ST_TIMEVAL_FORMAT "</td>\n",
                               ST_TIMEVAL_PRINTABLE(lifespan));

                    /*
                     ** Weight.
                     */
                    PR_fprintf(inRequest->mFD, "<td align=right>%llu</td>\n",
                               weight64);

                    /*
                     ** Heap operation cost.
                     */
                    PR_fprintf(inRequest->mFD,
                               "<td align=right>" ST_MICROVAL_FORMAT
                               "</td>\n", ST_MICROVAL_PRINTABLE(heapCost));

                    if (0 != aWantCallsite) {
                        /*
                         ** Callsite.
                         */
                        PR_fprintf(inRequest->mFD, "<td>");
                        htmlCallsiteAnchor(inRequest,
                                           current->mEvents[0].mCallsite,
                                           NULL, 0);
                        PR_fprintf(inRequest->mFD, "</td>\n");
                    }

                    PR_fprintf(inRequest->mFD, "</tr>\n");
                }
            }

            PR_fprintf(inRequest->mFD, "</tbody>\n</table></div>\n\n");
        }
    }
    else {
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
int
displayMemoryLeaks(STRequest * inRequest, STRun * aRun)
{
    int retval = 0;

    if (NULL != aRun) {
        PRUint32 loop = 0;
        PRUint32 displayed = 0;
        STAllocation *current = NULL;

        static const char * headers[] = {
            "Rank", "Index", "Byte Size", "Lifespan (sec)",
            "Weight", "Heap Op (sec)", "Origin Callsite"
        };
        
        htmlStartTable(inRequest, NULL, "memory-leaks", "Memory Leaks", headers,
                       sizeof(headers) / sizeof(headers[0]));

        /*
         ** Loop over all of the items, or until we've displayed enough.
         */
        for (loop = 0;
             displayed < inRequest->mOptions.mListItemMax
             && loop < aRun->mAllocationCount; loop++) {
            current = aRun->mAllocations[loop];
            if (NULL != current && 0 != current->mEventCount) {
                /*
                 ** In order to be a leak, the last event of its life must
                 **  NOT be a free operation.
                 **
                 ** A free operation is just that, a free.
                 */
                if (TM_EVENT_FREE !=
                    current->mEvents[current->mEventCount - 1].mEventType) {
                    PRUint32 lifespan =
                        current->mMaxTimeval - current->mMinTimeval;
                    PRUint32 size = byteSize(&inRequest->mOptions, current);
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

                    PR_fprintf(inRequest->mFD, "<tr>\n");

                    /*
                     ** Rank.
                     */
                    PR_fprintf(inRequest->mFD, "<td align=right>%u</td>\n",
                               displayed);

                    /*
                     ** Index.
                     */
                    PR_snprintf(buffer, sizeof(buffer), "%u",
                                current->mRunIndex);
                    PR_fprintf(inRequest->mFD, "<td align=right>\n");
                    htmlAllocationAnchor(inRequest, current, buffer);
                    PR_fprintf(inRequest->mFD, "</td>\n");

                    /*
                     ** Byte Size.
                     */
                    PR_fprintf(inRequest->mFD, "<td align=right>%u</td>\n",
                               size);

                    /*
                     ** Lifespan.
                     */
                    PR_fprintf(inRequest->mFD,
                               "<td align=right>" ST_TIMEVAL_FORMAT "</td>\n",
                               ST_TIMEVAL_PRINTABLE(lifespan));

                    /*
                     ** Weight.
                     */
                    PR_fprintf(inRequest->mFD, "<td align=right>%llu</td>\n",
                               weight64);

                    /*
                     ** Heap Operation Seconds.
                     */
                    PR_fprintf(inRequest->mFD,
                               "<td align=right>" ST_MICROVAL_FORMAT
                               "</td>\n", ST_MICROVAL_PRINTABLE(heapCost));

                    /*
                     ** Callsite.
                     */
                    PR_fprintf(inRequest->mFD, "<td>");
                    htmlCallsiteAnchor(inRequest,
                                       current->mEvents[0].mCallsite, NULL,
                                       0);
                    PR_fprintf(inRequest->mFD, "</td>\n");

                    PR_fprintf(inRequest->mFD, "</tr>\n");
                }
            }
        }

        PR_fprintf(inRequest->mFD, "</tbody></table></div>\n\n");
    }
    else {
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
int
displayCallsites(STRequest * inRequest, tmcallsite * aCallsite, int aFollow,
                 PRUint32 aStamp,
                 const char* id,
                 const char* caption,
                 int aRealNames)
{
    int retval = 0;

    if (NULL != aCallsite && NULL != aCallsite->method) {
        int headerDisplayed = 0;
        STRun *run = NULL;

        /*
         ** Correct the stamp if need be.
         */
        if (0 == aStamp && NULL != inRequest->mContext->mSortedRun) {
            aStamp =
                inRequest->mContext->mSortedRun->mStats[inRequest->mContext->
                                                        mIndex].mStamp;
        }

        /*
         ** Loop over the callsites looking for a stamp match.
         ** A stamp guarantees there is something interesting to look at too.
         ** If found, output it.
         */
        while (NULL != aCallsite && NULL != aCallsite->method) {
            run = CALLSITE_RUN(aCallsite);
            if (NULL != run) {
                if (aStamp == run->mStats[inRequest->mContext->mIndex].mStamp) {
                    /*
                     ** We got a header?
                     */
                    if (0 == headerDisplayed) {

                        static const char* const headers[] = {
                            "Callsite",
                            "<abbr title=\"Composite Size\">C. Size</abbr>",
                            "<abbr title=\"Composite Seconds\">C. Seconds</abbr>",
                            "<abbr title=\"Composite Weight\">C. Weight</abbr>",
                            "<abbr title=\"Heap Object Count\">H.O. Count</abbr>",
                            "<abbr title=\"Composite Heap Operation Seconds\">C.H. Operation (sec)</abbr>"
                        };
                        headerDisplayed = __LINE__;
                        htmlStartTable(inRequest, NULL, id, caption, headers,
                                       sizeof(headers)/sizeof(headers[0]));
                    }

                    /*
                     ** Output the information.
                     */
                    PR_fprintf(inRequest->mFD, "<tr>\n");

                    /*
                     ** Method name.
                     */
                    PR_fprintf(inRequest->mFD, "<td>");
                    htmlCallsiteAnchor(inRequest, aCallsite, NULL,
                                       aRealNames);
                    PR_fprintf(inRequest->mFD, "</td>");

                    /*
                     ** Byte Size.
                     */
                    PR_fprintf(inRequest->mFD,
                               "<td valign=top align=right>%u</td>\n",
                               run->mStats[inRequest->mContext->mIndex].
                               mSize);

                    /*
                     ** Seconds.
                     */
                    PR_fprintf(inRequest->mFD,
                               "<td valign=top align=right>" ST_TIMEVAL_FORMAT
                               "</td>\n",
                               ST_TIMEVAL_PRINTABLE64(run->
                                                      mStats[inRequest->
                                                             mContext->
                                                             mIndex].
                                                      mTimeval64));

                    /*
                     ** Weight.
                     */
                    PR_fprintf(inRequest->mFD,
                               "<td valign=top align=right>%llu</td>\n",
                               run->mStats[inRequest->mContext->mIndex].
                               mWeight64);

                    /*
                     ** Allocation object count.
                     */
                    PR_fprintf(inRequest->mFD,
                               "<td valign=top align=right>%u</td>\n",
                               run->mStats[inRequest->mContext->mIndex].
                               mCompositeCount);

                    /*
                     ** Heap Operation Seconds.
                     */
                    PR_fprintf(inRequest->mFD,
                               "<td valign=top align=right>"
                               ST_MICROVAL_FORMAT "</td>\n",
                               ST_MICROVAL_PRINTABLE(run->
                                                     mStats[inRequest->
                                                            mContext->mIndex].
                                                     mHeapRuntimeCost));

                    PR_fprintf(inRequest->mFD, "</tr>\n");
                }
            }
            else {
                retval = __LINE__;
                REPORT_ERROR(__LINE__, displayCallsites);
                break;
            }

            /*
             ** What do we follow?
             */
            switch (aFollow) {
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
        if (0 != headerDisplayed) {
            PR_fprintf(inRequest->mFD, "</tbody></table></div>\n\n");
        }
    }
    else {
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
int
displayAllocationDetails(STRequest * inRequest, STAllocation * aAllocation)
{
    int retval = 0;

    if (NULL != aAllocation) {
        PRUint32 traverse = 0;
        PRUint32 bytesize = byteSize(&inRequest->mOptions, aAllocation);
        PRUint32 timeval =
            aAllocation->mMaxTimeval - aAllocation->mMinTimeval;
        PRUint32 heapCost = aAllocation->mHeapRuntimeCost;
        PRUint64 weight64 = LL_INIT(0, 0);
        PRUint64 bytesize64 = LL_INIT(0, 0);
        PRUint64 timeval64 = LL_INIT(0, 0);
        PRUint32 cacheval = 0;
        int displayRes = 0;

        LL_UI2L(bytesize64, bytesize);
        LL_UI2L(timeval64, timeval);
        LL_MUL(weight64, bytesize64, timeval64);

        PR_fprintf(inRequest->mFD, "<p>Allocation %u Details:</p>\n",
                   aAllocation->mRunIndex);

        PR_fprintf(inRequest->mFD, "<div id=\"allocation-details\"><table class=\"data summary\">\n");
        PR_fprintf(inRequest->mFD,
                   "<tr><td align=left>Final Size:</td><td align=right>%u</td></tr>\n",
                   bytesize);
        PR_fprintf(inRequest->mFD,
                   "<tr><td align=left>Lifespan Seconds:</td><td align=right>"
                   ST_TIMEVAL_FORMAT "</td></tr>\n",
                   ST_TIMEVAL_PRINTABLE(timeval));
        PR_fprintf(inRequest->mFD,
                   "<tr><td align=left>Weight:</td><td align=right>%llu</td></tr>\n",
                   weight64);
        PR_fprintf(inRequest->mFD,
                   "<tr><td align=left>Heap Operation Seconds:</td><td align=right>"
                   ST_MICROVAL_FORMAT "</td></tr>\n",
                   ST_MICROVAL_PRINTABLE(heapCost));
        PR_fprintf(inRequest->mFD, "</table></div>\n");

        /*
         ** The events.
         */
        
        {
            static const char* const headers[] = {
                "Operation", "Size", "Seconds", ""
            };

            char caption[100];
            PR_snprintf(caption, sizeof(caption), "%u Life Event(s)",
                       aAllocation->mEventCount);
            htmlStartTable(inRequest, NULL, "allocation-details", caption, headers,
                           sizeof(headers) / sizeof(headers[0]));
        }
        
        for (traverse = 0;
             traverse < aAllocation->mEventCount
             && traverse < inRequest->mOptions.mListItemMax; traverse++) {
            PR_fprintf(inRequest->mFD, "<tr>\n");

            /*
             ** count.
             */
            PR_fprintf(inRequest->mFD,
                       "<td valign=top align=right>%u.</td>\n", traverse + 1);

            /*
             ** Operation.
             */
            PR_fprintf(inRequest->mFD, "<td valign=top>");
            switch (aAllocation->mEvents[traverse].mEventType) {
            case TM_EVENT_CALLOC:
                PR_fprintf(inRequest->mFD, "calloc");
                break;
            case TM_EVENT_FREE:
                PR_fprintf(inRequest->mFD, "free");
                break;
            case TM_EVENT_MALLOC:
                PR_fprintf(inRequest->mFD, "malloc");
                break;
            case TM_EVENT_REALLOC:
                PR_fprintf(inRequest->mFD, "realloc");
                break;
            default:
                retval = __LINE__;
                REPORT_ERROR(__LINE__, displayAllocationDetails);
                break;
            }
            PR_fprintf(inRequest->mFD, "</td>");

            /*
             ** Size.
             */
            PR_fprintf(inRequest->mFD, "<td valign=top align=right>%u</td>\n",
                       aAllocation->mEvents[traverse].mHeapSize);

            /*
             ** Timeval.
             */
            cacheval =
                aAllocation->mEvents[traverse].mTimeval - globals.mMinTimeval;
            PR_fprintf(inRequest->mFD,
                       "<td valign=top align=right>" ST_TIMEVAL_FORMAT
                       "</td>\n", ST_TIMEVAL_PRINTABLE(cacheval));

            /*
             ** Callsite backtrace.
             ** Only relevant backtrace is for event 0 for now until
             **  trace-malloc outputs proper callsites for all others.
             */
            PR_fprintf(inRequest->mFD, "<td valign=top>\n");
            if (0 == traverse) {
                displayRes =
                    displayCallsites(inRequest,
                                     aAllocation->mEvents[traverse].mCallsite,
                                     ST_FOLLOW_PARENTS, 0, "event-stack", "", __LINE__);
                if (0 != displayRes) {
                    retval = __LINE__;
                    REPORT_ERROR(__LINE__, displayCallsite);
                }
            }
            PR_fprintf(inRequest->mFD, "</td>\n");

            PR_fprintf(inRequest->mFD, "</tr>\n");
        }
        PR_fprintf(inRequest->mFD, "</table></div>\n");
    }
    else {
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
int
compareCallsites(const void *aSite1, const void *aSite2, void *aContext)
{
    int retval = 0;
    STRequest *inRequest = (STRequest *) aContext;

    if (NULL != aSite1 && NULL != aSite2) {
        tmcallsite *site1 = *((tmcallsite **) aSite1);
        tmcallsite *site2 = *((tmcallsite **) aSite2);

        if (NULL != site1 && NULL != site2) {
            STRun *run1 = CALLSITE_RUN(site1);
            STRun *run2 = CALLSITE_RUN(site2);

            if (NULL != run1 && NULL != run2) {
                STCallsiteStats *stats1 =
                    &(run1->mStats[inRequest->mContext->mIndex]);
                STCallsiteStats *stats2 =
                    &(run2->mStats[inRequest->mContext->mIndex]);

                /*
                 ** Logic determined by pref/option.
                 */
                switch (inRequest->mOptions.mOrderBy) {
                case ST_WEIGHT:
                    {
                        PRUint64 weight164 = stats1->mWeight64;
                        PRUint64 weight264 = stats2->mWeight64;

                        if (LL_UCMP(weight164, <, weight264)) {
                            retval = __LINE__;
                        }
                        else if (LL_UCMP(weight164, >, weight264)) {
                            retval = -__LINE__;
                        }
                    }
                    break;

                case ST_SIZE:
                    {
                        PRUint32 size1 = stats1->mSize;
                        PRUint32 size2 = stats2->mSize;

                        if (size1 < size2) {
                            retval = __LINE__;
                        }
                        else if (size1 > size2) {
                            retval = -__LINE__;
                        }
                    }
                    break;

                case ST_TIMEVAL:
                    {
                        PRUint64 timeval164 = stats1->mTimeval64;
                        PRUint64 timeval264 = stats2->mTimeval64;

                        if (LL_UCMP(timeval164, <, timeval264)) {
                            retval = __LINE__;
                        }
                        else if (LL_UCMP(timeval164, >, timeval264)) {
                            retval = -__LINE__;
                        }
                    }
                    break;

                case ST_COUNT:
                    {
                        PRUint32 count1 = stats1->mCompositeCount;
                        PRUint32 count2 = stats2->mCompositeCount;

                        if (count1 < count2) {
                            retval = __LINE__;
                        }
                        else if (count1 > count2) {
                            retval = -__LINE__;
                        }
                    }
                    break;

                case ST_HEAPCOST:
                    {
                        PRUint32 cost1 = stats1->mHeapRuntimeCost;
                        PRUint32 cost2 = stats2->mHeapRuntimeCost;

                        if (cost1 < cost2) {
                            retval = __LINE__;
                        }
                        else if (cost1 > cost2) {
                            retval = -__LINE__;
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
                if (0 == retval) {
                    if (stats1 < stats2) {
                        retval = __LINE__;
                    }
                    else if (stats1 > stats2) {
                        retval = -__LINE__;
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
int
displayTopCallsites(STRequest * inRequest, tmcallsite ** aCallsites,
                    PRUint32 aCallsiteCount, PRUint32 aStamp,
                    const char* id,
                    const char* caption,
                    int aRealName)
{
    int retval = 0;

    if (NULL != aCallsites && 0 < aCallsiteCount) {
        PRUint32 traverse = 0;
        STRun *run = NULL;
        tmcallsite *site = NULL;
        int headerDisplayed = 0;
        PRUint32 displayed = 0;

        /*
         ** Fixup the stamp.
         */
        if (0 == aStamp && NULL != inRequest->mContext->mSortedRun) {
            aStamp =
                inRequest->mContext->mSortedRun->mStats[inRequest->mContext->
                                                        mIndex].mStamp;
        }

        /*
         ** Sort the things.
         */
        NS_QuickSort(aCallsites, aCallsiteCount, sizeof(tmcallsite *),
                     compareCallsites, inRequest);

        /*
         ** Time for output.
         */
        for (traverse = 0;
             traverse < aCallsiteCount
             && inRequest->mOptions.mListItemMax > displayed; traverse++) {
            site = aCallsites[traverse];
            run = CALLSITE_RUN(site);

            /*
             ** Only if the same stamp....
             */
            if (aStamp == run->mStats[inRequest->mContext->mIndex].mStamp) {
                /*
                 ** We got a header yet?
                 */
                if (0 == headerDisplayed) {
                    static const char* const headers[] = {
                        "Rank",
                        "Callsite",
                        "<abbr title=\"Composite Size\">Size</abbr>",
                        "<abbr title=\"Composite Seconds\">Seconds</abbr>",
                        "<abbr title=\"Composite Weight\">Weight</abbr>",
                        "<abbr title=\"Heap Object Count\">Object Count</abbr>",
                        "<abbr title=\"Composite Heap Operation Seconds\">C.H. Operation (sec)</abbr>"
                    };
                    headerDisplayed = __LINE__;

                    htmlStartTable(inRequest, NULL, id, caption, headers,
                                   sizeof(headers) / sizeof(headers[0]));
                }

                displayed++;

                PR_fprintf(inRequest->mFD, "<tr>\n");

                /*
                 ** Rank.
                 */
                PR_fprintf(inRequest->mFD,
                           "<td align=right valign=top>%u</td>\n", displayed);

                /*
                 ** Method.
                 */
                PR_fprintf(inRequest->mFD, "<td>");
                htmlCallsiteAnchor(inRequest, site, NULL, aRealName);
                PR_fprintf(inRequest->mFD, "</td>\n");

                /*
                 ** Size.
                 */
                PR_fprintf(inRequest->mFD,
                           "<td align=right valign=top>%u</td>\n",
                           run->mStats[inRequest->mContext->mIndex].mSize);

                /*
                 ** Timeval.
                 */
                PR_fprintf(inRequest->mFD,
                           "<td align=right valign=top>" ST_TIMEVAL_FORMAT
                           "</td>\n",
                           ST_TIMEVAL_PRINTABLE64(run->
                                                  mStats[inRequest->mContext->
                                                         mIndex].mTimeval64));

                /*
                 ** Weight.
                 */
                PR_fprintf(inRequest->mFD,
                           "<td align=right valign=top>%llu</td>\n",
                           run->mStats[inRequest->mContext->mIndex].
                           mWeight64);

                /*
                 ** Allocation object count.
                 */
                PR_fprintf(inRequest->mFD,
                           "<td align=right valign=top>%u</td>\n",
                           run->mStats[inRequest->mContext->mIndex].
                           mCompositeCount);

                /*
                 ** Heap operation seconds.
                 */
                PR_fprintf(inRequest->mFD,
                           "<td align=right valign=top>" ST_MICROVAL_FORMAT
                           "</td>\n",
                           ST_MICROVAL_PRINTABLE(run->
                                                 mStats[inRequest->mContext->
                                                        mIndex].
                                                 mHeapRuntimeCost));

                PR_fprintf(inRequest->mFD, "</tr>\n");


                if (inRequest->mOptions.mListItemMax > displayed) {
                    /*
                     ** Skip any dups.
                     */
                    while (((traverse + 1) < aCallsiteCount)
                           && (site == aCallsites[traverse + 1])) {
                        traverse++;
                    }
                }
            }
        }

        /*
         ** We need to terminate anything?
         */
        if (0 != headerDisplayed) {
            PR_fprintf(inRequest->mFD, "</table></div>\n");
        }
    }
    else {
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
int
displayCallsiteDetails(STRequest * inRequest, tmcallsite * aCallsite)
{
    int retval = 0;

    if (NULL != aCallsite && NULL != aCallsite->method) {
        STRun *sortedRun = NULL;
        STRun *thisRun = CALLSITE_RUN(aCallsite);
        const char *sourceFile = NULL;

        sourceFile = resolveSourceFile(aCallsite->method);

        PR_fprintf(inRequest->mFD, "<div class=\"callsite-header\">\n");
        if (sourceFile) {
            PR_fprintf(inRequest->mFD, "<b>%s</b>",
                       tmmethodnode_name(aCallsite->method));
            PR_fprintf(inRequest->mFD,
                       " [<a href=\"http://lxr.mozilla.org/mozilla/source/%s#%u\" class=\"lxr\" target=\"_st_lxr\">%s:%u</a>]",
                       aCallsite->method->sourcefile,
                       aCallsite->method->linenumber, sourceFile,
                       aCallsite->method->linenumber);
        }
        else {
            PR_fprintf(inRequest->mFD,
                       "<p><b>%s</b>+%u(%u) Callsite Details:</p>\n",
                       tmmethodnode_name(aCallsite->method),
                       aCallsite->offset, (PRUint32) aCallsite->entry.key);
        }

        PR_fprintf(inRequest->mFD, "</div>\n\n");
        PR_fprintf(inRequest->mFD, "<div id=\"callsite-details\"><table class=\"data summary\">\n");
        PR_fprintf(inRequest->mFD,
                   "<tr><td>Composite Byte Size:</td><td align=right>%u</td></tr>\n",
                   thisRun->mStats[inRequest->mContext->mIndex].mSize);
        PR_fprintf(inRequest->mFD,
                   "<tr><td>Composite Seconds:</td><td align=right>"
                   ST_TIMEVAL_FORMAT "</td></tr>\n",
                   ST_TIMEVAL_PRINTABLE64(thisRun->
                                          mStats[inRequest->mContext->mIndex].
                                          mTimeval64));
        PR_fprintf(inRequest->mFD,
                   "<tr><td>Composite Weight:</td><td align=right>%llu</td></tr>\n",
                   thisRun->mStats[inRequest->mContext->mIndex].mWeight64);
        PR_fprintf(inRequest->mFD,
                   "<tr><td>Heap Object Count:</td><td align=right>%u</td></tr>\n",
                   thisRun->mStats[inRequest->mContext->mIndex].
                   mCompositeCount);
        PR_fprintf(inRequest->mFD,
                   "<tr><td>Heap Operation Seconds:</td><td align=right>"
                   ST_MICROVAL_FORMAT "</td></tr>\n",
                   ST_MICROVAL_PRINTABLE(thisRun->
                                         mStats[inRequest->mContext->mIndex].
                                         mHeapRuntimeCost));
        PR_fprintf(inRequest->mFD, "</table></div>\n\n");

        /*
         ** Kids (callsites we call):
         */
        if (NULL != aCallsite->kids && NULL != aCallsite->kids->method) {
            int displayRes = 0;
            PRUint32 siteCount = 0;
            tmcallsite **sites = NULL;

            /*
             ** Collect the kid sibling callsites.
             ** Doing it this way sorts them for relevance.
             */
            siteCount =
                callsiteArrayFromCallsite(&sites, 0, aCallsite->kids,
                                          ST_FOLLOW_SIBLINGS);
            if (0 != siteCount && NULL != sites) {
                /*
                 ** Got something to show.
                 */
                displayRes =
                    displayTopCallsites(inRequest, sites, siteCount, 0,
                                        "callsites",
                                        "Children Callsites",
                                        __LINE__);
                if (0 != displayRes) {
                    retval = __LINE__;
                    REPORT_ERROR(__LINE__, displayTopCallsites);
                }

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
        if (NULL != aCallsite->parent && NULL != aCallsite->parent->method) {
            int displayRes = 0;

            displayRes =
                displayCallsites(inRequest, aCallsite->parent,
                                 ST_FOLLOW_PARENTS, 0, "caller-stack", "Caller stack",
                                 __LINE__);
            if (0 != displayRes) {
                retval = __LINE__;
                REPORT_ERROR(__LINE__, displayCallsites);
            }
        }

        /*
         ** Allocations we did.
         ** Simply harvest our own run.
         */
        sortedRun = createRun(inRequest->mContext, 0);
        if (NULL != sortedRun) {
            int harvestRes = 0;

            harvestRes =
                harvestRun(CALLSITE_RUN(aCallsite), sortedRun,
                           &inRequest->mOptions, inRequest->mContext);
            if (0 == harvestRes) {
                if (0 != sortedRun->mAllocationCount) {
                    int sortRes = 0;

                    sortRes = sortRun(&inRequest->mOptions, sortedRun);
                    if (0 == sortRes) {
                        int displayRes = 0;

                        displayRes =
                            displayTopAllocations(inRequest, sortedRun,
                                                  "allocations",
                                                  "Allocations",
                                                  0);
                        if (0 != displayRes) {
                            retval = __LINE__;
                            REPORT_ERROR(__LINE__, displayTopAllocations);
                        }
                    }
                    else {
                        retval = __LINE__;
                        REPORT_ERROR(__LINE__, sortRun);
                    }
                }
            }
            else {
                retval = __LINE__;
                REPORT_ERROR(__LINE__, harvestRun);
            }

            /*
             ** Done with the run.
             */
            freeRun(sortedRun);
            sortedRun = NULL;
        }
        else {
            retval = __LINE__;
            REPORT_ERROR(__LINE__, createRun);
        }
    }
    else {
        retval = __LINE__;
        REPORT_ERROR(__LINE__, displayCallsiteDetails);
    }

    return retval;
}

#if ST_WANT_GRAPHS
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
int
graphFootprint(STRequest * inRequest, STRun * aRun)
{
    int retval = 0;

    if (NULL != aRun) {
        PRUint32 *YData = NULL;
        PRUint32 YDataArray[STGD_SPACE_X];
        PRUint32 traverse = 0;
        PRUint32 timeval = 0;
        PRUint32 loop = 0;
        PRBool underLock = PR_FALSE;

        /*
         ** Decide if this is custom or we should use the cache.
         */
        if (aRun == inRequest->mContext->mSortedRun) {
            YData = inRequest->mContext->mFootprintYData;
            underLock = PR_TRUE;
        }
        else {
            YData = YDataArray;
        }

        /*
         **  Protect the shared data so that only one client has access to it
         **      at any given time.
         */
        if (PR_FALSE != underLock) {
            PR_Lock(inRequest->mContext->mImageLock);
        }

        /*
         ** Only do the computations if we aren't cached already.
         */
        if (YData != inRequest->mContext->mFootprintYData
            || PR_FALSE == inRequest->mContext->mFootprintCached) {
            memset(YData, 0, sizeof(PRUint32) * STGD_SPACE_X);

            /*
             ** Initialize our Y data.
             ** Pretty brutal loop here....
             */
            for (traverse = 0; 0 == retval && traverse < STGD_SPACE_X;
                 traverse++) {
                /*
                 ** Compute what timeval this Y data lands in.
                 */
                timeval =
                    ((traverse *
                      (globals.mMaxTimeval -
                       globals.mMinTimeval)) / STGD_SPACE_X) +
                    globals.mMinTimeval;

                /*
                 ** Loop over the run.
                 ** Should an allocation contain said Timeval, we're good.
                 */
                for (loop = 0; loop < aRun->mAllocationCount; loop++) {
                    if (timeval >= aRun->mAllocations[loop]->mMinTimeval
                        && timeval <= aRun->mAllocations[loop]->mMaxTimeval) {
                        YData[traverse] +=
                            byteSize(&inRequest->mOptions,
                                     aRun->mAllocations[loop]);
                    }
                }
            }

            /*
             ** Did we cache this?
             */
            if (YData == inRequest->mContext->mFootprintYData) {
                inRequest->mContext->mFootprintCached = PR_TRUE;
            }
        }

        /*
         **  Done with the lock.
         */
        if (PR_FALSE != underLock) {
            PR_Unlock(inRequest->mContext->mImageLock);
        }

        if (0 == retval) {
            PRUint32 minMemory = (PRUint32) - 1;
            PRUint32 maxMemory = 0;
            int transparent = 0;
            gdImagePtr graph = NULL;

            /*
             ** Go through and find the minimum and maximum sizes.
             */
            for (traverse = 0; traverse < STGD_SPACE_X; traverse++) {
                if (YData[traverse] < minMemory) {
                    minMemory = YData[traverse];
                }
                if (YData[traverse] > maxMemory) {
                    maxMemory = YData[traverse];
                }
            }

            /*
             ** We can now draw the graph.
             */
            graph = createGraph(&transparent);
            if (NULL != graph) {
                gdSink theSink;
                int red = 0;
                int x1 = 0;
                int y1 = 0;
                int x2 = 0;
                int y2 = 0;
                PRUint32 percents[11] =
                    { 0, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100 };
                char *timevals[11];
                char *bytes[11];
                char timevalSpace[11][32];
                char byteSpace[11][32];
                int legendColors[1];
                const char *legends[1] = { "Memory in Use" };
                PRUint32 cached = 0;

                /*
                 ** Figure out what the labels will say.
                 */
                for (traverse = 0; traverse < 11; traverse++) {
                    timevals[traverse] = timevalSpace[traverse];
                    bytes[traverse] = byteSpace[traverse];

                    cached =
                        ((globals.mMaxTimeval -
                          globals.mMinTimeval) * percents[traverse]) / 100;
                    PR_snprintf(timevals[traverse], 32, ST_TIMEVAL_FORMAT,
                                ST_TIMEVAL_PRINTABLE(cached));
                    PR_snprintf(bytes[traverse], 32, "%u",
                                ((maxMemory -
                                  minMemory) * percents[traverse]) / 100);
                }

                red = gdImageColorAllocate(graph, 255, 0, 0);
                legendColors[0] = red;

                drawGraph(graph, -1, "Memory Footprint Over Time", "Seconds",
                          "Bytes", 11, percents, (const char **) timevals, 11,
                          percents, (const char **) bytes, 1, legendColors,
                          legends);

                if (maxMemory != minMemory) {
                    PRInt64 in64 = LL_INIT(0, 0);
                    PRInt64 ydata64 = LL_INIT(0, 0);
                    PRInt64 spacey64 = LL_INIT(0, 0);
                    PRInt64 mem64 = LL_INIT(0, 0);
                    PRInt32 in32 = 0;

                    /*
                     ** Go through our Y data and mark it up.
                     */
                    for (traverse = 0; traverse < STGD_SPACE_X; traverse++) {
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


                theSink.context = inRequest->mFD;
                theSink.sink = pngSink;
                gdImagePngToSink(graph, &theSink);

                gdImageDestroy(graph);
            }
            else {
                retval = __LINE__;
                REPORT_ERROR(__LINE__, createGraph);
            }
        }
    }
    else {
        retval = __LINE__;
        REPORT_ERROR(__LINE__, graphFootprint);
    }

    return retval;
}
#endif /* ST_WANT_GRAPHS */

#if ST_WANT_GRAPHS
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
int
graphTimeval(STRequest * inRequest, STRun * aRun)
{
    int retval = 0;

    if (NULL != aRun) {
        PRUint32 *YData = NULL;
        PRUint32 YDataArray[STGD_SPACE_X];
        PRUint32 traverse = 0;
        PRUint32 timeval = globals.mMinTimeval;
        PRUint32 loop = 0;
        PRBool underLock = PR_FALSE;

        /*
         ** Decide if this is custom or we should use the global cache.
         */
        if (aRun == inRequest->mContext->mSortedRun) {
            YData = inRequest->mContext->mTimevalYData;
            underLock = PR_TRUE;
        }
        else {
            YData = YDataArray;
        }

        /*
         **  Protect the shared data so that only one client has access to it
         **      at any given time.
         */
        if (PR_FALSE != underLock) {
            PR_Lock(inRequest->mContext->mImageLock);
        }

        /*
         ** Only do the computations if we aren't cached already.
         */
        if (YData != inRequest->mContext->mTimevalYData
            || PR_FALSE == inRequest->mContext->mTimevalCached) {
            PRUint32 prevTimeval = 0;

            memset(YData, 0, sizeof(PRUint32) * STGD_SPACE_X);

            /*
             ** Initialize our Y data.
             ** Pretty brutal loop here....
             */
            for (traverse = 0; 0 == retval && traverse < STGD_SPACE_X;
                 traverse++) {
                /*
                 ** Compute what timeval this Y data lands in.
                 */
                prevTimeval = timeval;
                timeval =
                    ((traverse *
                      (globals.mMaxTimeval -
                       globals.mMinTimeval)) / STGD_SPACE_X) +
                    globals.mMinTimeval;

                /*
                 ** Loop over the run.
                 ** Should an allocation have been allocated between
                 **  prevTimeval and timeval....
                 */
                for (loop = 0; loop < aRun->mAllocationCount; loop++) {
                    if (prevTimeval < aRun->mAllocations[loop]->mMinTimeval
                        && timeval >= aRun->mAllocations[loop]->mMinTimeval) {
                        YData[traverse] +=
                            byteSize(&inRequest->mOptions,
                                     aRun->mAllocations[loop]);
                    }
                }
            }

            /*
             ** Did we cache this?
             */
            if (YData == inRequest->mContext->mTimevalYData) {
                inRequest->mContext->mTimevalCached = PR_TRUE;
            }
        }

        /*
         **  Done with the lock.
         */
        if (PR_FALSE != underLock) {
            PR_Unlock(inRequest->mContext->mImageLock);
        }

        if (0 == retval) {
            PRUint32 minMemory = (PRUint32) - 1;
            PRUint32 maxMemory = 0;
            int transparent = 0;
            gdImagePtr graph = NULL;

            /*
             ** Go through and find the minimum and maximum sizes.
             */
            for (traverse = 0; traverse < STGD_SPACE_X; traverse++) {
                if (YData[traverse] < minMemory) {
                    minMemory = YData[traverse];
                }
                if (YData[traverse] > maxMemory) {
                    maxMemory = YData[traverse];
                }
            }

            /*
             ** We can now draw the graph.
             */
            graph = createGraph(&transparent);
            if (NULL != graph) {
                gdSink theSink;
                int red = 0;
                int x1 = 0;
                int y1 = 0;
                int x2 = 0;
                int y2 = 0;
                PRUint32 percents[11] =
                    { 0, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100 };
                char *timevals[11];
                char *bytes[11];
                char timevalSpace[11][32];
                char byteSpace[11][32];
                int legendColors[1];
                const char *legends[1] = { "Memory Allocated" };
                PRUint32 cached = 0;

                /*
                 ** Figure out what the labels will say.
                 */
                for (traverse = 0; traverse < 11; traverse++) {
                    timevals[traverse] = timevalSpace[traverse];
                    bytes[traverse] = byteSpace[traverse];

                    cached =
                        ((globals.mMaxTimeval -
                          globals.mMinTimeval) * percents[traverse]) / 100;
                    PR_snprintf(timevals[traverse], 32, ST_TIMEVAL_FORMAT,
                                ST_TIMEVAL_PRINTABLE(cached));
                    PR_snprintf(bytes[traverse], 32, "%u",
                                ((maxMemory -
                                  minMemory) * percents[traverse]) / 100);
                }

                red = gdImageColorAllocate(graph, 255, 0, 0);
                legendColors[0] = red;

                drawGraph(graph, -1, "Allocation Times", "Seconds", "Bytes",
                          11, percents, (const char **) timevals, 11,
                          percents, (const char **) bytes, 1, legendColors,
                          legends);

                if (maxMemory != minMemory) {
                    PRInt64 in64 = LL_INIT(0, 0);
                    PRInt64 ydata64 = LL_INIT(0, 0);
                    PRInt64 spacey64 = LL_INIT(0, 0);
                    PRInt64 mem64 = LL_INIT(0, 0);
                    PRInt32 in32 = 0;

                    /*
                     ** Go through our Y data and mark it up.
                     */
                    for (traverse = 0; traverse < STGD_SPACE_X; traverse++) {
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


                theSink.context = inRequest->mFD;
                theSink.sink = pngSink;
                gdImagePngToSink(graph, &theSink);

                gdImageDestroy(graph);
            }
            else {
                retval = __LINE__;
                REPORT_ERROR(__LINE__, createGraph);
            }
        }
    }
    else {
        retval = __LINE__;
        REPORT_ERROR(__LINE__, graphTimeval);
    }

    return retval;
}
#endif /* ST_WANT_GRAPHS */

#if ST_WANT_GRAPHS
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
int
graphLifespan(STRequest * inRequest, STRun * aRun)
{
    int retval = 0;

    if (NULL != aRun) {
        PRUint32 *YData = NULL;
        PRUint32 YDataArray[STGD_SPACE_X];
        PRUint32 traverse = 0;
        PRUint32 timeval = 0;
        PRUint32 loop = 0;
        PRBool underLock = PR_FALSE;

        /*
         ** Decide if this is custom or we should use the global cache.
         */
        if (aRun == inRequest->mContext->mSortedRun) {
            YData = inRequest->mContext->mLifespanYData;
            underLock = PR_TRUE;
        }
        else {
            YData = YDataArray;
        }

        /*
         **  Protect the shared data so that only one client has access to it
         **      at any given time.
         */
        if (PR_FALSE != underLock) {
            PR_Lock(inRequest->mContext->mImageLock);
        }

        /*
         ** Only do the computations if we aren't cached already.
         */
        if (YData != inRequest->mContext->mLifespanYData
            || PR_FALSE == inRequest->mContext->mLifespanCached) {
            PRUint32 prevTimeval = 0;
            PRUint32 lifespan = 0;

            memset(YData, 0, sizeof(PRUint32) * STGD_SPACE_X);

            /*
             ** Initialize our Y data.
             ** Pretty brutal loop here....
             */
            for (traverse = 0; 0 == retval && traverse < STGD_SPACE_X;
                 traverse++) {
                /*
                 ** Compute what timeval this Y data lands in.
                 */
                prevTimeval = timeval;
                timeval =
                    (traverse * (globals.mMaxTimeval - globals.mMinTimeval)) /
                    STGD_SPACE_X;

                /*
                 ** Loop over the run.
                 ** Should an allocation have lived between
                 **  prevTimeval and timeval....
                 */
                for (loop = 0; loop < aRun->mAllocationCount; loop++) {
                    lifespan =
                        aRun->mAllocations[loop]->mMaxTimeval -
                        aRun->mAllocations[loop]->mMinTimeval;

                    if (prevTimeval < lifespan && timeval >= lifespan) {
                        YData[traverse] +=
                            byteSize(&inRequest->mOptions,
                                     aRun->mAllocations[loop]);
                    }
                }
            }

            /*
             ** Did we cache this?
             */
            if (YData == inRequest->mContext->mLifespanYData) {
                inRequest->mContext->mLifespanCached = PR_TRUE;
            }
        }

        /*
         **  Done with the lock.
         */
        if (PR_FALSE != underLock) {
            PR_Unlock(inRequest->mContext->mImageLock);
        }

        if (0 == retval) {
            PRUint32 minMemory = (PRUint32) - 1;
            PRUint32 maxMemory = 0;
            int transparent = 0;
            gdImagePtr graph = NULL;

            /*
             ** Go through and find the minimum and maximum sizes.
             */
            for (traverse = 0; traverse < STGD_SPACE_X; traverse++) {
                if (YData[traverse] < minMemory) {
                    minMemory = YData[traverse];
                }
                if (YData[traverse] > maxMemory) {
                    maxMemory = YData[traverse];
                }
            }

            /*
             ** We can now draw the graph.
             */
            graph = createGraph(&transparent);
            if (NULL != graph) {
                gdSink theSink;
                int red = 0;
                int x1 = 0;
                int y1 = 0;
                int x2 = 0;
                int y2 = 0;
                PRUint32 percents[11] =
                    { 0, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100 };
                char *timevals[11];
                char *bytes[11];
                char timevalSpace[11][32];
                char byteSpace[11][32];
                int legendColors[1];
                const char *legends[1] = { "Live Memory" };
                PRUint32 cached = 0;

                /*
                 ** Figure out what the labels will say.
                 */
                for (traverse = 0; traverse < 11; traverse++) {
                    timevals[traverse] = timevalSpace[traverse];
                    bytes[traverse] = byteSpace[traverse];

                    cached =
                        ((globals.mMaxTimeval -
                          globals.mMinTimeval) * percents[traverse]) / 100;
                    PR_snprintf(timevals[traverse], 32, ST_TIMEVAL_FORMAT,
                                ST_TIMEVAL_PRINTABLE(cached));
                    PR_snprintf(bytes[traverse], 32, "%u",
                                ((maxMemory -
                                  minMemory) * percents[traverse]) / 100);
                }

                red = gdImageColorAllocate(graph, 255, 0, 0);
                legendColors[0] = red;

                drawGraph(graph, -1, "Allocation Lifespans", "Lifespan",
                          "Bytes", 11, percents, (const char **) timevals, 11,
                          percents, (const char **) bytes, 1, legendColors,
                          legends);

                if (maxMemory != minMemory) {
                    PRInt64 in64 = LL_INIT(0, 0);
                    PRInt64 ydata64 = LL_INIT(0, 0);
                    PRInt64 spacey64 = LL_INIT(0, 0);
                    PRInt64 mem64 = LL_INIT(0, 0);
                    PRInt32 in32 = 0;

                    /*
                     ** Go through our Y data and mark it up.
                     */
                    for (traverse = 0; traverse < STGD_SPACE_X; traverse++) {
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


                theSink.context = inRequest->mFD;
                theSink.sink = pngSink;
                gdImagePngToSink(graph, &theSink);

                gdImageDestroy(graph);
            }
            else {
                retval = __LINE__;
                REPORT_ERROR(__LINE__, createGraph);
            }
        }
    }
    else {
        retval = __LINE__;
        REPORT_ERROR(__LINE__, graphLifespan);
    }

    return retval;
}
#endif /* ST_WANT_GRAPHS */

#if ST_WANT_GRAPHS
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
int
graphWeight(STRequest * inRequest, STRun * aRun)
{
    int retval = 0;

    if (NULL != aRun) {
        PRUint64 *YData64 = NULL;
        PRUint64 YDataArray64[STGD_SPACE_X];
        PRUint32 traverse = 0;
        PRUint32 timeval = globals.mMinTimeval;
        PRUint32 loop = 0;
        PRBool underLock = PR_FALSE;

        /*
         ** Decide if this is custom or we should use the global cache.
         */
        if (aRun == inRequest->mContext->mSortedRun) {
            YData64 = inRequest->mContext->mWeightYData64;
            underLock = PR_TRUE;
        }
        else {
            YData64 = YDataArray64;
        }

        /*
         **  Protect the shared data so that only one client has access to it
         **      at any given time.
         */
        if (PR_FALSE != underLock) {
            PR_Lock(inRequest->mContext->mImageLock);
        }

        /*
         ** Only do the computations if we aren't cached already.
         */
        if (YData64 != inRequest->mContext->mWeightYData64
            || PR_FALSE == inRequest->mContext->mWeightCached) {
            PRUint32 prevTimeval = 0;

            memset(YData64, 0, sizeof(PRUint64) * STGD_SPACE_X);

            /*
             ** Initialize our Y data.
             ** Pretty brutal loop here....
             */
            for (traverse = 0; 0 == retval && traverse < STGD_SPACE_X;
                 traverse++) {
                /*
                 ** Compute what timeval this Y data lands in.
                 */
                prevTimeval = timeval;
                timeval =
                    ((traverse *
                      (globals.mMaxTimeval -
                       globals.mMinTimeval)) / STGD_SPACE_X) +
                    globals.mMinTimeval;

                /*
                 ** Loop over the run.
                 ** Should an allocation have been allocated between
                 **  prevTimeval and timeval....
                 */
                for (loop = 0; loop < aRun->mAllocationCount; loop++) {
                    if (prevTimeval < aRun->mAllocations[loop]->mMinTimeval
                        && timeval >= aRun->mAllocations[loop]->mMinTimeval) {
                        PRUint64 size64 = LL_INIT(0, 0);
                        PRUint64 lifespan64 = LL_INIT(0, 0);
                        PRUint64 weight64 = LL_INIT(0, 0);

                        LL_UI2L(size64,
                                byteSize(&inRequest->mOptions,
                                         aRun->mAllocations[loop]));
                        LL_UI2L(lifespan64,
                                (aRun->mAllocations[loop]->mMaxTimeval -
                                 aRun->mAllocations[loop]->mMinTimeval));
                        LL_MUL(weight64, size64, lifespan64);

                        LL_ADD(YData64[traverse], YData64[traverse],
                               weight64);
                    }
                }
            }

            /*
             ** Did we cache this?
             */
            if (YData64 == inRequest->mContext->mWeightYData64) {
                inRequest->mContext->mWeightCached = PR_TRUE;
            }
        }

        /*
         **  Done with the lock.
         */
        if (PR_FALSE != underLock) {
            PR_Unlock(inRequest->mContext->mImageLock);
        }

        if (0 == retval) {
            PRUint64 minWeight64 = LL_INIT(0xFFFFFFFF, 0xFFFFFFFF);
            PRUint64 maxWeight64 = LL_INIT(0, 0);
            int transparent = 0;
            gdImagePtr graph = NULL;

            /*
             ** Go through and find the minimum and maximum weights.
             */
            for (traverse = 0; traverse < STGD_SPACE_X; traverse++) {
                if (LL_UCMP(YData64[traverse], <, minWeight64)) {
                    minWeight64 = YData64[traverse];
                }
                if (LL_UCMP(YData64[traverse], >, maxWeight64)) {
                    maxWeight64 = YData64[traverse];
                }
            }

            /*
             ** We can now draw the graph.
             */
            graph = createGraph(&transparent);
            if (NULL != graph) {
                gdSink theSink;
                int red = 0;
                int x1 = 0;
                int y1 = 0;
                int x2 = 0;
                int y2 = 0;
                PRUint32 percents[11] =
                    { 0, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100 };
                char *timevals[11];
                char *bytes[11];
                char timevalSpace[11][32];
                char byteSpace[11][32];
                int legendColors[1];
                const char *legends[1] = { "Memory Weight" };
                PRUint64 percent64 = LL_INIT(0, 0);
                PRUint64 result64 = LL_INIT(0, 0);
                PRUint64 hundred64 = LL_INIT(0, 0);
                PRUint32 cached = 0;

                LL_UI2L(hundred64, 100);

                /*
                 ** Figure out what the labels will say.
                 */
                for (traverse = 0; traverse < 11; traverse++) {
                    timevals[traverse] = timevalSpace[traverse];
                    bytes[traverse] = byteSpace[traverse];

                    cached =
                        ((globals.mMaxTimeval -
                          globals.mMinTimeval) * percents[traverse]) / 100;
                    PR_snprintf(timevals[traverse], 32, ST_TIMEVAL_FORMAT,
                                ST_TIMEVAL_PRINTABLE(cached));

                    LL_UI2L(percent64, percents[traverse]);
                    LL_SUB(result64, maxWeight64, minWeight64);
                    LL_MUL(result64, result64, percent64);
                    LL_DIV(result64, result64, hundred64);
                    PR_snprintf(bytes[traverse], 32, "%llu", result64);
                }

                red = gdImageColorAllocate(graph, 255, 0, 0);
                legendColors[0] = red;

                drawGraph(graph, -1, "Allocation Weights", "Seconds",
                          "Weight", 11, percents, (const char **) timevals,
                          11, percents, (const char **) bytes, 1,
                          legendColors, legends);

                if (LL_NE(maxWeight64, minWeight64)) {
                    PRInt64 in64 = LL_INIT(0, 0);
                    PRInt64 spacey64 = LL_INIT(0, 0);
                    PRInt64 weight64 = LL_INIT(0, 0);
                    PRInt32 in32 = 0;

                    /*
                     ** Go through our Y data and mark it up.
                     */
                    for (traverse = 0; traverse < STGD_SPACE_X; traverse++) {
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


                theSink.context = inRequest->mFD;
                theSink.sink = pngSink;
                gdImagePngToSink(graph, &theSink);

                gdImageDestroy(graph);
            }
            else {
                retval = __LINE__;
                REPORT_ERROR(__LINE__, createGraph);
            }
        }
    }
    else {
        retval = __LINE__;
        REPORT_ERROR(__LINE__, graphWeight);
    }

    return retval;
}
#endif /* ST_WANT_GRAPHS */

#define ST_WEB_OPTION_BOOL(option_name, option_genre, option_help) \
    { \
        PRUint32 convert = (PRUint32)outOptions->m##option_name; \
        \
        getDataPRUint32(inFormData, #option_name, 1, &convert, 1); \
        outOptions->m##option_name = (PRBool)convert; \
    }
#define ST_WEB_OPTION_STRING(option_name, option_genre, default_value, option_help) \
    getDataString(inFormData, #option_name, 1, outOptions->m##option_name, sizeof(outOptions->m##option_name));
#define ST_WEB_OPTION_STRING_ARRAY(option_name, option_genre, array_size, option_help) \
    { \
        PRUint32 loop = 0; \
        PRUint32 found = 0; \
        char buffer[ST_OPTION_STRING_MAX]; \
        \
        for(loop = 0; loop < array_size; loop++) \
        { \
            buffer[0] = '\0'; \
            getDataString(inFormData, #option_name, (loop + 1), buffer, sizeof(buffer)); \
            \
            if('\0' != buffer[0]) \
            { \
                PR_snprintf(outOptions->m##option_name[found], sizeof(outOptions->m##option_name[found]), "%s", buffer); \
                found++; \
            } \
        } \
        \
        for(; found < array_size; found++) \
        { \
            outOptions->m##option_name[found][0] = '\0'; \
        } \
    }
#define ST_WEB_OPTION_STRING_PTR_ARRAY(option_name, option_genre, option_help)  /* no implementation */
#define ST_WEB_OPTION_UINT32(option_name, option_genre, default_value, multiplier, option_help) \
    getDataPRUint32(inFormData, #option_name, 1, &outOptions->m##option_name, multiplier);
#define ST_WEB_OPTION_UINT64(option_name, option_genre, default_value, multiplier, option_help) \
    { \
        PRUint64 mul64 = multiplier; \
        \
        getDataPRUint64(inFormData, #option_name, 1, &outOptions->m##option_name##64, mul64); \
    }
/*
**  fillOptions
**
**  Given an appropriate hexcaped string, distill the option values
**      and fill the given STOption struct.
**
**  Note that the options passed in are not touched UNLESS there is
**      a replacement found in the form data.
*/
void
fillOptions(STOptions * outOptions, const FormData * inFormData)
{
    if (NULL != outOptions && NULL != inFormData) {

#include "stoptions.h"

        /*
         **  Special sanity check here for some options that need data validation.
         */
        if (!outOptions->mCategoryName[0]
            || !findCategoryNode(outOptions->mCategoryName, &globals)) {
            PR_snprintf(outOptions->mCategoryName,
                        sizeof(outOptions->mCategoryName), "%s",
                        ST_ROOT_CATEGORY_NAME);
        }
    }
}


void
displayOptionString(STRequest * inRequest,
                    const char *option_name,
                    const char *option_genre,
                    const char *default_value,
                    const char *option_help, const char *value)
{
#if 0
    PR_fprintf(inRequest->mFD, "<input type=submit value=%s>\n", option_name);
#endif
    PR_fprintf(inRequest->mFD, "<div class=option-box>\n");
    PR_fprintf(inRequest->mFD, "<p class=option-name>%s</p>\n", option_name);
    PR_fprintf(inRequest->mFD,
               "<input type=text name=\"%s\" value=\"%s\">\n",
               option_name, value);
    PR_fprintf(inRequest->mFD,
               "<p class=option-default>Default value is \"%s\".</p>\n<p class=option-help>%s</p>\n",
               default_value, option_help);
    PR_fprintf(inRequest->mFD, "</div>\n");
}

static void
displayOptionStringArray(STRequest * inRequest,
                         const char *option_name,
                         const char *option_genre,
                         PRUint32 array_size,
                         const char *option_help, const char values[5]
                         [ST_OPTION_STRING_MAX])
{
    /* values should not be a fixed length! */
    PR_ASSERT(array_size == 5);
#if 0
    PR_fprintf(inRequest->mFD, "<input type=submit value=%s>\n", option_name);
#endif
    PR_fprintf(inRequest->mFD, "<div class=\"option-box\">\n");
    PR_fprintf(inRequest->mFD, "<p class=option-name>%s</p>\n", option_name); {
        PRUint32 loop = 0;

        for (loop = 0; loop < array_size; loop++) {
            PR_fprintf(inRequest->mFD,
                       "<input type=text name=\"%s\" value=\"%s\"><br>\n",
                       option_name, values[loop]);
        }
    }
    PR_fprintf(inRequest->mFD,
               "<p class=option-default>Up to %u occurrences allowed.</p>\n<p class=option-help>%s</p>\n",
               array_size, option_help);
    PR_fprintf(inRequest->mFD, "</div>\n");
}

static void
displayOptionInt(STRequest * inRequest,
                 const char *option_name,
                 const char *option_genre,
                 PRUint32 default_value,
                 PRUint32 multiplier, const char *option_help, PRUint32 value)
{
#if 0
    PR_fprintf(inRequest->mFD, "<input type=submit value=%s>\n", option_name);
#endif
    PR_fprintf(inRequest->mFD, "<div class=\"option-box\">\n");
    PR_fprintf(inRequest->mFD, "<p class=option-name>%s</p>\n", option_name);
    PR_fprintf(inRequest->mFD,
               "<input type=text name=%s value=%u>\n", option_name,
               value / multiplier);
    PR_fprintf(inRequest->mFD,
               "<p class=option-default>Default value is %u.</p>\n<p class=option-help>%s</p>\n",
               default_value, option_help);
    PR_fprintf(inRequest->mFD, "</div>\n");
}

static void
displayOptionInt64(STRequest * inRequest,
                   const char *option_name,
                   const char *option_genre,
                   PRUint64 default_value,
                   PRUint64 multiplier,
                   const char *option_help, PRUint64 value)
{
#if 0
    PR_fprintf(inRequest->mFD, "<input type=submit value=%s>\n", option_name);
#endif
    PR_fprintf(inRequest->mFD, "<div class=\"option-box\">\n");
    PR_fprintf(inRequest->mFD, "<p class=option-name>%s</p>\n", option_name); {
        PRUint64 def64 = default_value;
        PRUint64 mul64 = multiplier;
        PRUint64 div64;

        LL_DIV(div64, value, mul64);
        PR_fprintf(inRequest->mFD,
                   "<input type=text name=%s value=%llu>\n",
                   option_name, div64);
        PR_fprintf(inRequest->mFD,
                   "<p class=option-default>Default value is %llu.</p>\n<p class=option-help>%s</p>\n",
                   def64, option_help);
    }
    PR_fprintf(inRequest->mFD, "</div>\n");
}

/*
**  displaySettings
**
**  Present the settings for change during execution.
*/
void
displaySettings(STRequest * inRequest)
{
    int applyRes = 0;

    /*
     ** We've got a form to create.
     */
    PR_fprintf(inRequest->mFD, "<form method=get action=\"./index.html\">\n");
    /*
     **  Respect newlines in help text.
     */
#if 0
    PR_fprintf(inRequest->mFD, "<pre>\n");
#endif
#define ST_WEB_OPTION_BOOL(option_name, option_genre, option_help) \
    displayOptionBool(option_name, option_genre, option_help)
#define ST_WEB_OPTION_STRING(option_name, option_genre, default_value, option_help) \
    displayOptionString(inRequest, #option_name, #option_genre, default_value, option_help, inRequest->mOptions.m##option_name);
#define ST_WEB_OPTION_STRING_ARRAY(option_name, option_genre, array_size, option_help) \
    displayOptionStringArray(inRequest, #option_name, #option_genre, array_size, option_help, inRequest->mOptions.m##option_name);
#define ST_WEB_OPTION_STRING_PTR_ARRAY(option_name, option_genre, option_help)  /* no implementation */
#define ST_WEB_OPTION_UINT32(option_name, option_genre, default_value, multiplier, option_help) \
    displayOptionInt(inRequest, #option_name, #option_genre, default_value, multiplier, option_help, inRequest->mOptions.m##option_name);
#define ST_WEB_OPTION_UINT64(option_name, option_genre, default_value, multiplier, option_help) \
    displayOptionInt64(inRequest, #option_name, #option_genre, default_value, multiplier, option_help, inRequest->mOptions.m##option_name##64);
#include "stoptions.h"
    /*
     **  Give a submit/reset button, obligatory.
     **  Done respecting newlines in help text.
     */
    PR_fprintf(inRequest->mFD,
               "<input type=submit value=\"Save Options\"> <input type=reset>\n");
#if 0
    PR_fprintf(inRequest->mFD, "</pre>\n");
#endif
    /*
     ** Done with form.
     */
    PR_fprintf(inRequest->mFD, "</form>\n");
}

int
handleLocalFile(STRequest * inRequest, const char *aFilename)
{
    static const char *const local_files[] = {
        "spacetrace.css",
    };
    static const size_t local_file_count =
        sizeof(local_files) / sizeof(local_files[0]);
    size_t i;

    for (i = 0; i < local_file_count; i++) {
        if (0 == strcmp(local_files[i], aFilename))
            return 1;
    }
    return 0;
}

/*
** displayFile
**
** reads a file from disk, and streams it to the request
*/
int
displayFile(STRequest * inRequest, const char *aFilename)
{
    PRFileDesc *inFd;
    const char *filepath =
        PR_smprintf("res%c%s", PR_GetDirectorySeparator(), aFilename);
    char buffer[2048];
    PRInt32 readRes;

    inFd = PR_Open(filepath, PR_RDONLY, PR_IRUSR);
    if (!inFd)
        return -1;
    while ((readRes = PR_Read(inFd, buffer, sizeof(buffer))) > 0) {
        PR_Write(inRequest->mFD, buffer, readRes);
    }
    if (readRes != 0)
        return -1;
    PR_Close(inFd);
    return 0;
}

/*
** displayIndex
**
** Present a list of the reports you can drill down into.
** Returns !0 on failure.
*/
int
displayIndex(STRequest * inRequest)
{
    int retval = 0;
    STOptions *options = &inRequest->mOptions;

    /*
     ** Present reports in a list format.
     */
    PR_fprintf(inRequest->mFD, "<ul>");
    PR_fprintf(inRequest->mFD, "\n<li>");
    htmlAnchor(inRequest, "root_callsites.html", "Root Callsites",
               NULL, "mainmenu", options);
    PR_fprintf(inRequest->mFD, "\n<li>");
    htmlAnchor(inRequest, "categories_summary.html",
               "Categories Report", NULL, "mainmenu", options);
    PR_fprintf(inRequest->mFD, "\n<li>");
    htmlAnchor(inRequest, "top_callsites.html",
               "Top Callsites Report", NULL, "mainmenu", options);
    PR_fprintf(inRequest->mFD, "\n<li>");
    htmlAnchor(inRequest, "top_allocations.html",
               "Top Allocations Report", NULL, "mainmenu", options);
    PR_fprintf(inRequest->mFD, "\n<li>");
    htmlAnchor(inRequest, "memory_leaks.html",
               "Memory Leak Report", NULL, "mainmenu", options);
#if ST_WANT_GRAPHS
    PR_fprintf(inRequest->mFD, "\n<li>Graphs");
    PR_fprintf(inRequest->mFD, "<ul>");
    PR_fprintf(inRequest->mFD, "\n<li>");
    htmlAnchor(inRequest, "footprint_graph.html", "Footprint",
               NULL, "mainmenu graph", options);
    PR_fprintf(inRequest->mFD, "\n<li>");
    htmlAnchor(inRequest, "lifespan_graph.html",
               "Allocation Lifespans", NULL, "mainmenu graph", options);
    PR_fprintf(inRequest->mFD, "\n<li>");
    htmlAnchor(inRequest, "times_graph.html", "Allocation Times",
               NULL, "mainmenu graph", options);
    PR_fprintf(inRequest->mFD, "\n<li>");
    htmlAnchor(inRequest, "weight_graph.html",
               "Allocation Weights", NULL, "mainmenu graph", options);
    PR_fprintf(inRequest->mFD, "\n</ul>\n");
#endif /* ST_WANT_GRAPHS */
    PR_fprintf(inRequest->mFD, "\n</ul>\n");
    return retval;
}

/*
**  initRequestOptions
**
**  Given the request, set the options that are specific to the request.
**  These can generally be determined in the following manner:
**      Copy over global options.
**      If getData present, attempt to use options therein.
*/
void
initRequestOptions(STRequest * inRequest)
{
    if (NULL != inRequest) {
        /*
         **  Copy of global options.
         */
        memcpy(&inRequest->mOptions, &globals.mCommandLineOptions,
               sizeof(globals.mCommandLineOptions));
        /*
         **  Decide what will override global options if anything.
         */
        if (NULL != inRequest->mGetData) {
            fillOptions(&inRequest->mOptions, inRequest->mGetData);
        }
    }
}

STContext *
contextLookup(STOptions * inOptions)
/*
**  Lookup a context that matches the options.
**  The lookup may block, especially if the context needs to be created.
**  Callers of this API must eventually call contextRelease with the
**      return value;  failure to do so will cause this applications
**      to eventually not work as advertised.
**
**  inOptions   The options determine which context is relevant.
**  returns     The fully completed context on success.
**              The context is read only in practice, so please do not
**                  write to it or anything it points to.
**              NULL on failure.
*/
{
    STContext *retval = NULL;
    STContextCache *inCache = &globals.mContextCache;

    if (NULL != inOptions && NULL != inCache) {
        PRUint32 loop = 0;
        STContext *categoryException = NULL;
        PRBool newContext = PR_FALSE;
        PRBool evictContext = PR_FALSE;
        PRBool changeCategoryContext = PR_FALSE;

        /*
         **  Own the context cache while we are in here.
         */
        PR_Lock(inCache->mLock);
        /*
         **  Loop until successful.
         **  Waiting on the condition variable makes sure we don't hog the
         **      lock below.
         */
        while (1) {
            /*
             **  Go over the cache items.
             **  At this point we are looking for a cache hit, with multiple
             **      readers.
             */
            for (loop = 0; loop < inCache->mItemCount; loop++) {
                /*
                 **  Must be in use.
                 */
                if (PR_FALSE != inCache->mItems[loop].mInUse) {
                    int delta[(STOptionGenre) MaxGenres];

                    /*
                     **  Compare the relevant options, figure out if different
                     **      in any genre that we care about.
                     */
                    memset(&delta, 0, sizeof(delta));
#define ST_WEB_OPTION_BOOL(option_name, option_genre, option_help) \
    if(inOptions->m##option_name != inCache->mItems[loop].mOptions.m##option_name) \
    { \
        delta[(STOptionGenre)option_genre]++; \
    }
#define ST_WEB_OPTION_STRING(option_name, option_genre, default_value, option_help) \
    if(0 != strcmp(inOptions->m##option_name, inCache->mItems[loop].mOptions.m##option_name)) \
    { \
        delta[(STOptionGenre)option_genre]++; \
    }
#define ST_WEB_OPTION_STRING_ARRAY(option_name, option_genre, array_size, option_help) \
    { \
        PRUint32 macro_loop = 0; \
        \
        for(macro_loop = 0; macro_loop < array_size; macro_loop++) \
        { \
            if(0 != strcmp(inOptions->m##option_name[macro_loop], inCache->mItems[loop].mOptions.m##option_name[macro_loop])) \
            { \
                break; \
            } \
        } \
        \
        if(macro_loop != array_size) \
        { \
            delta[(STOptionGenre)option_genre]++; \
        } \
    }
#define ST_WEB_OPTION_STRING_PTR_ARRAY(option_name, option_genre, option_help)  /* no implementation */
#define ST_WEB_OPTION_UINT32(option_name, option_genre, default_value, multiplier, option_help) \
    if(inOptions->m##option_name != inCache->mItems[loop].mOptions.m##option_name) \
    { \
        delta[(STOptionGenre)option_genre]++; \
    }
#define ST_WEB_OPTION_UINT64(option_name, option_genre, default_value, multiplier, option_help) \
    if(LL_NE(inOptions->m##option_name##64, inCache->mItems[loop].mOptions.m##option_name##64)) \
    { \
        delta[(STOptionGenre)option_genre]++; \
    }
#include "stoptions.h"
                    /*
                     **  If there is no genre out of alignment, we accept this as the context.
                     */
                    if (0 == delta[CategoryGenre] &&
                        0 == delta[DataSortGenre] &&
                        0 == delta[DataSetGenre] && 0 == delta[DataSizeGenre]
                        ) {
                        retval = &inCache->mItems[loop].mContext;
                        break;
                    }

                    /*
                     **  A special exception to the rule here.
                     **  If all that is different is the category genre, and there
                     **      is no one looking at the context (zero ref count),
                     **      then there is some magic we can perform.
                     */
                    if (NULL == retval &&
                        0 == inCache->mItems[loop].mReferenceCount &&
                        0 != delta[CategoryGenre] &&
                        0 == delta[DataSortGenre] &&
                        0 == delta[DataSetGenre] && 0 == delta[DataSizeGenre]
                        ) {
                        categoryException = &inCache->mItems[loop].mContext;
                    }
                }
            }

            /*
             **  Pick up our category exception if relevant.
             */
            if (NULL == retval && NULL != categoryException) {
                retval = categoryException;
                categoryException = NULL;
                changeCategoryContext = PR_TRUE;
            }

            /*
             **  If we don't have a cache hit, then we need to check for an empty
             **      spot to take over.
             */
            if (NULL == retval) {
                for (loop = 0; loop < inCache->mItemCount; loop++) {
                    /*
                     **  Must NOT be in use, then it will be the context.
                     */
                    if (PR_FALSE == inCache->mItems[loop].mInUse) {
                        retval = &inCache->mItems[loop].mContext;
                        newContext = PR_TRUE;
                        break;
                    }
                }
            }

            /*
             **  If we still don't have a return value, then we need to see if
             **      there are any old items with zero ref counts that we
             **      can take over.
             */
            if (NULL == retval) {
                for (loop = 0; loop < inCache->mItemCount; loop++) {
                    /*
                     **  Must be in use.
                     */
                    if (PR_FALSE != inCache->mItems[loop].mInUse) {
                        /*
                         **  Must have a ref count of zero.
                         */
                        if (0 == inCache->mItems[loop].mReferenceCount) {
                            /*
                             **  Must be older than any other we know of.
                             */
                            if (NULL != retval) {
                                if (inCache->mItems[loop].mLastAccessed <
                                    inCache->mItems[retval->mIndex].
                                    mLastAccessed) {
                                    retval = &inCache->mItems[loop].mContext;
                                }
                            }
                            else {
                                retval = &inCache->mItems[loop].mContext;
                            }
                        }
                    }
                }

                if (NULL != retval) {
                    evictContext = PR_TRUE;
                }
            }

            /*
             **  If we still don't have a return value, then we can not avoid
             **      waiting around until someone gives us the chance.
             **  The chance, in specific, comes when a cache item reference
             **      count returns to zero, upon which we can try to take
             **      it over again.
             */
            if (NULL == retval) {
                /*
                 **  This has the side effect of release the context lock.
                 **  This is a good thing so that other clients can continue
                 **      to connect and hopefully have cache hits.
                 **  If they do not have cache hits, then we will end up
                 **      with a bunch of waiters here....
                 */
                PR_WaitCondVar(inCache->mCacheMiss, PR_INTERVAL_NO_TIMEOUT);
            }

            /*
             **  If we have a return value, improve the reference count here.
             */
            if (NULL != retval) {
                /*
                 **  Decide if there are any changes to be made.
                 **  Do as little as possible, then fall through the context
                 **      cache lock to finish up.
                 **  This way, lengthy init operations will not block
                 **      other clients, only matches to this context.
                 */
                if (PR_FALSE != newContext ||
                    PR_FALSE != evictContext ||
                    PR_FALSE != changeCategoryContext) {
                    /*
                     **  Overwrite the prefs for this context.
                     **  They are changing.
                     */
                    memcpy(&inCache->mItems[retval->mIndex].mOptions,
                           inOptions,
                           sizeof(inCache->mItems[retval->mIndex].mOptions));
                    /*
                     **  As we are going to be changing the context, we need to write lock it.
                     **  This makes sure no readers are allowed while we are making our changes.
                     */
                    PR_RWLock_Wlock(retval->mRWLock);
                }

                /*
                 **  NOTE, ref count gets incremented here, inside content
                 **      cache lock so it can not be flushed once lock
                 **      released.
                 */
                inCache->mItems[retval->mIndex].mInUse = PR_TRUE;
                inCache->mItems[retval->mIndex].mReferenceCount++;
                /*
                 **  That's all folks.
                 */
                break;
            }

        }                       /* while(1), try again */

        /*
         **  Done with context cache.
         */
        PR_Unlock(inCache->mLock);
        /*
         **  Now that the context cached is free to continue accepting other
         **      requests, we have a little more work to do.
         */
        if (NULL != retval) {
            PRBool unlock = PR_FALSE;

            /*
             **  If evicting, we need to free off the old stuff.
             */
            if (PR_FALSE != evictContext) {
                unlock = PR_TRUE;
                /*
                 **  We do not free the sorted run.
                 **  The category code takes care of this.
                 */
                retval->mSortedRun = NULL;
#if ST_WANT_GRAPHS
                /*
                 **  There is no need to
                 **      PR_Lock(retval->mImageLock)
                 **  We are already under write lock for the entire structure.
                 */
                retval->mFootprintCached = PR_FALSE;
                retval->mTimevalCached = PR_FALSE;
                retval->mLifespanCached = PR_FALSE;
                retval->mWeightCached = PR_FALSE;
#endif
            }

            /*
             **  If new or recently evicted, we need to fully init.
             */
            if (PR_FALSE != newContext || PR_FALSE != evictContext) {
                unlock = PR_TRUE;
                retval->mSortedRun =
                    createRunFromGlobal(&inCache->mItems[retval->mIndex].
                                        mOptions,
                                        &inCache->mItems[retval->mIndex].
                                        mContext);
            }

            /*
             **  If changing category, we need to do some sneaky stuff.
             */
            if (PR_FALSE != changeCategoryContext) {
                STCategoryNode *node = NULL;

                unlock = PR_TRUE;
                /*
                 ** Just a category change. We don't need to harvest. Just find the
                 ** right node and set the cache.mSortedRun. We need to recompute
                 ** cost though. But that is cheap.
                 */
                node =
                    findCategoryNode(inCache->mItems[retval->mIndex].mOptions.
                                     mCategoryName, &globals);
                if (node) {
                    /* Recalculate cost of run */
                    recalculateRunCost(&inCache->mItems[retval->mIndex].
                                       mOptions, retval,
                                       node->runs[retval->mIndex]);
                    retval->mSortedRun = node->runs[retval->mIndex];
                }

#if ST_WANT_GRAPHS
                /*
                 **  There is no need to
                 **      PR_Lock(retval->mImageLock)
                 **  We are already under write lock for the entire structure.
                 */
                retval->mFootprintCached = PR_FALSE;
                retval->mTimevalCached = PR_FALSE;
                retval->mLifespanCached = PR_FALSE;
                retval->mWeightCached = PR_FALSE;
#endif
            }

            /*
             **  Release the write lock if we took one to make changes.
             */
            if (PR_FALSE != unlock) {
                PR_RWLock_Unlock(retval->mRWLock);
            }

            /*
             **  Last thing possible, take a read lock on our return value.
             **  This will cause us to block if the context is not fully
             **      initialized in another thread holding the write lock.
             */
            PR_RWLock_Rlock(retval->mRWLock);
        }
    }

    return retval;
}

void
contextRelease(STContext * inContext)
/*
**  After a successful call to contextLookup, one should call this API when
**      done with the context.
**  This effectively removes the usage of the client on a cached item.
*/
{
    STContextCache *inCache = &globals.mContextCache;

    if (NULL != inContext && NULL != inCache) {
        /*
         **  Own the context cache while in here.
         */
        PR_Lock(inCache->mLock);
        /*
         **  Give up the read lock on the context.
         */
        PR_RWLock_Unlock(inContext->mRWLock);
        /*
         **  Decrement the reference count on the context.
         **  If it was the last reference, notify that a new item is
         **      available for eviction.
         **  A waiting thread will wake up and eat it.
         **  Also set when it was last accessed so the oldest unused item
         **      can be targeted for eviction.
         */
        inCache->mItems[inContext->mIndex].mReferenceCount--;
        if (0 == inCache->mItems[inContext->mIndex].mReferenceCount) {
            PR_NotifyCondVar(inCache->mCacheMiss);
            inCache->mItems[inContext->mIndex].mLastAccessed =
                PR_IntervalNow();
        }

        /*
         **  Done with context cache.
         */
        PR_Unlock(inCache->mLock);
    }
}


/*
** handleRequest
**
** Based on what file they are asking for, perform some processing.
** Output the results to aFD.
**
** Returns !0 on error.
*/
int
handleRequest(tmreader * aTMR, PRFileDesc * aFD,
              const char *aFileName, const FormData * aGetData)
{
    int retval = 0;

    if (NULL != aTMR && NULL != aFD && NULL != aFileName
        && '\0' != *aFileName) {
        STRequest request;

        /*
         ** Init the request.
         */
        memset(&request, 0, sizeof(request));
        request.mFD = aFD;
        request.mGetFileName = aFileName;
        request.mGetData = aGetData;
        /*
         **  Set local options for this request.
         */
        initRequestOptions(&request);
        /*
         **  Get our cached context for this client.
         **  Simply based on the options.
         */
        request.mContext = contextLookup(&request.mOptions);
        if (NULL != request.mContext) {
            /*
             ** Attempt to find the file of interest.
             */
            if (handleLocalFile(&request, aFileName)) {
                displayFile(&request, aFileName);
            }
            else if (0 == strcmp("index.html", aFileName)) {
                int displayRes = 0;

                htmlHeader(&request, "SpaceTrace Index");
                displayRes = displayIndex(&request);
                if (0 != displayRes) {
                    retval = __LINE__;
                    REPORT_ERROR(__LINE__, displayIndex);
                }

                htmlFooter(&request);
            }
            else if (0 == strcmp("settings.html", aFileName) ||
                     0 == strcmp("options.html", aFileName)) {
                htmlHeader(&request, "SpaceTrace Options");
                displaySettings(&request);
                htmlFooter(&request);
            }
            else if (0 == strcmp("top_allocations.html", aFileName)) {
                int displayRes = 0;

                htmlHeader(&request, "SpaceTrace Top Allocations Report");
                displayRes =
                    displayTopAllocations(&request,
                                          request.mContext->mSortedRun,
                                          "top-allocations",
                                          "SpaceTrace Top Allocations Report",
                                          1);
                if (0 != displayRes) {
                    retval = __LINE__;
                    REPORT_ERROR(__LINE__, displayTopAllocations);
                }

                htmlFooter(&request);
            }
            else if (0 == strcmp("top_callsites.html", aFileName)) {
                int displayRes = 0;
                tmcallsite **array = NULL;
                PRUint32 arrayCount = 0;

                /*
                 ** Display header after we figure out if we are going to focus
                 ** on a category.
                 */
                htmlHeader(&request, "SpaceTrace Top Callsites Report");
                if (NULL != request.mContext->mSortedRun
                    && 0 < request.mContext->mSortedRun->mAllocationCount) {
                    arrayCount =
                        callsiteArrayFromRun(&array, 0,
                                             request.mContext->mSortedRun);
                    if (0 != arrayCount && NULL != array) {
                        displayRes =
                            displayTopCallsites(&request, array, arrayCount,
                                                0,
                                                "top-callsites",
                                                "Top Callsites Report",
                                                0);
                        if (0 != displayRes) {
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
                else {
                    retval = __LINE__;
                    REPORT_ERROR(__LINE__, handleRequest);
                }

                htmlFooter(&request);
            }
            else if (0 == strcmp("memory_leaks.html", aFileName)) {
                int displayRes = 0;

                htmlHeader(&request, "SpaceTrace Memory Leaks Report");
                displayRes =
                    displayMemoryLeaks(&request,
                                       request.mContext->mSortedRun);
                if (0 != displayRes) {
                    retval = __LINE__;
                    REPORT_ERROR(__LINE__, displayMemoryLeaks);
                }

                htmlFooter(&request);
            }
            else if (0 == strncmp("allocation_", aFileName, 11)) {
                int scanRes = 0;
                PRUint32 allocationIndex = 0;

                /*
                 ** Oh, what a hack....
                 ** The index to the allocation structure in the global run
                 **   is in the filename.  Better than the pointer value....
                 */
                scanRes = PR_sscanf(aFileName + 11, "%u", &allocationIndex);
                if (1 == scanRes
                    && globals.mRun.mAllocationCount > allocationIndex
                    && NULL != globals.mRun.mAllocations[allocationIndex]) {
                    STAllocation *allocation =
                        globals.mRun.mAllocations[allocationIndex];
                    char buffer[128];
                    int displayRes = 0;

                    PR_snprintf(buffer, sizeof(buffer),
                                "SpaceTrace Allocation %u Details Report",
                                allocationIndex);
                    htmlHeader(&request, buffer);
                    displayRes =
                        displayAllocationDetails(&request, allocation);
                    if (0 != displayRes) {
                        retval = __LINE__;
                        REPORT_ERROR(__LINE__, displayAllocationDetails);
                    }

                    htmlFooter(&request);
                }
                else {
                    htmlNotFound(&request);
                }
            }
            else if (0 == strncmp("callsite_", aFileName, 9)) {
                int scanRes = 0;
                PRUint32 callsiteSerial = 0;
                tmcallsite *resolved = NULL;

                /*
                 ** Oh, what a hack....
                 ** The serial(key) to the callsite structure in the hash table
                 **   is in the filename.  Better than the pointer value....
                 */
                scanRes = PR_sscanf(aFileName + 9, "%u", &callsiteSerial);
                if (1 == scanRes && 0 != callsiteSerial
                    && NULL != (resolved =
                                tmreader_callsite(aTMR, callsiteSerial))) {
                    char buffer[128];
                    int displayRes = 0;

                    PR_snprintf(buffer, sizeof(buffer),
                                "SpaceTrace Callsite %u Details Report",
                                callsiteSerial);
                    htmlHeader(&request, buffer);
                    displayRes = displayCallsiteDetails(&request, resolved);
                    if (0 != displayRes) {
                        retval = __LINE__;
                        REPORT_ERROR(__LINE__, displayAllocationDetails);
                    }

                    htmlFooter(&request);
                }
                else {
                    htmlNotFound(&request);
                }
            }
            else if (0 == strcmp("root_callsites.html", aFileName)) {
                int displayRes = 0;

                htmlHeader(&request, "SpaceTrace Root Callsites");
                displayRes =
                    displayCallsites(&request, aTMR->calltree_root.kids,
                                     ST_FOLLOW_SIBLINGS, 0,
                                     "callsites-root",
                                     "SpaceTrace Root Callsites",
                                     __LINE__);
                if (0 != displayRes) {
                    retval = __LINE__;
                    REPORT_ERROR(__LINE__, displayCallsites);
                }

                htmlFooter(&request);
            }
#if ST_WANT_GRAPHS
            else if (0 == strcmp("footprint_graph.html", aFileName)) {
                int displayRes = 0;

                htmlHeader(&request, "SpaceTrace Memory Footprint Report");
                PR_fprintf(request.mFD, "<div align=center>\n");
                PR_fprintf(request.mFD, "<img src=\"./footprint.png");
                optionGetDataOut(request.mFD, &request.mOptions);
                PR_fprintf(request.mFD, "\">\n");
                PR_fprintf(request.mFD, "</div>\n");
                htmlFooter(&request);
            }
#endif /* ST_WANT_GRAPHS */
#if ST_WANT_GRAPHS
            else if (0 == strcmp("times_graph.html", aFileName)) {
                int displayRes = 0;

                htmlHeader(&request, "SpaceTrace Allocation Times Report");
                PR_fprintf(request.mFD, "<div align=center>\n");
                PR_fprintf(request.mFD, "<img src=\"./times.png");
                optionGetDataOut(request.mFD, &request.mOptions);
                PR_fprintf(request.mFD, "\">\n");
                PR_fprintf(request.mFD, "</div>\n");
                htmlFooter(&request);
            }
#endif /* ST_WANT_GRAPHS */
#if ST_WANT_GRAPHS
            else if (0 == strcmp("lifespan_graph.html", aFileName)) {
                int displayRes = 0;

                htmlHeader(&request,
                           "SpaceTrace Allocation Lifespans Report");
                PR_fprintf(request.mFD, "<div align=center>\n");
                PR_fprintf(request.mFD, "<img src=\"./lifespan.png");
                optionGetDataOut(request.mFD, &request.mOptions);
                PR_fprintf(request.mFD, "\">\n");
                PR_fprintf(request.mFD, "</div>\n");
                htmlFooter(&request);
            }
#endif /* ST_WANT_GRAPHS */
#if ST_WANT_GRAPHS
            else if (0 == strcmp("weight_graph.html", aFileName)) {
                int displayRes = 0;

                htmlHeader(&request, "SpaceTrace Allocation Weights Report");
                PR_fprintf(request.mFD, "<div align=center>\n");
                PR_fprintf(request.mFD, "<img src=\"./weight.png");
                optionGetDataOut(request.mFD, &request.mOptions);
                PR_fprintf(request.mFD, "\">\n");
                PR_fprintf(request.mFD, "</div>\n");
                htmlFooter(&request);
            }
#endif /* ST_WANT_GRAPHS */
#if ST_WANT_GRAPHS
            else if (0 == strcmp("footprint.png", aFileName)) {
                int graphRes = 0;

                graphRes =
                    graphFootprint(&request, request.mContext->mSortedRun);
                if (0 != graphRes) {
                    retval = __LINE__;
                    REPORT_ERROR(__LINE__, graphFootprint);
                }
            }
#endif /* ST_WANT_GRAPHS */
#if ST_WANT_GRAPHS
            else if (0 == strcmp("times.png", aFileName)) {
                int graphRes = 0;

                graphRes =
                    graphTimeval(&request, request.mContext->mSortedRun);
                if (0 != graphRes) {
                    retval = __LINE__;
                    REPORT_ERROR(__LINE__, graphTimeval);
                }
            }
#endif /* ST_WANT_GRAPHS */
#if ST_WANT_GRAPHS
            else if (0 == strcmp("lifespan.png", aFileName)) {
                int graphRes = 0;

                graphRes =
                    graphLifespan(&request, request.mContext->mSortedRun);
                if (0 != graphRes) {
                    retval = __LINE__;
                    REPORT_ERROR(__LINE__, graphLifespan);
                }
            }
#endif /* ST_WANT_GRAPHS */
#if ST_WANT_GRAPHS
            else if (0 == strcmp("weight.png", aFileName)) {
                int graphRes = 0;

                graphRes =
                    graphWeight(&request, request.mContext->mSortedRun);
                if (0 != graphRes) {
                    retval = __LINE__;
                    REPORT_ERROR(__LINE__, graphWeight);
                }
            }
#endif /* ST_WANT_GRAPHS */
            else if (0 == strcmp("categories_summary.html", aFileName)) {
                int displayRes = 0;

                htmlHeader(&request, "Category Report");
                displayRes =
                    displayCategoryReport(&request, &globals.mCategoryRoot,
                                          1);
                if (0 != displayRes) {
                    retval = __LINE__;
                    REPORT_ERROR(__LINE__, displayMemoryLeaks);
                }

                htmlFooter(&request);
            }
            else {
                htmlNotFound(&request);
            }

            /*
             **  Release the context we obtained earlier.
             */
            contextRelease(request.mContext);
            request.mContext = NULL;
        }
        else {
            retval = __LINE__;
            REPORT_ERROR(__LINE__, contextObtain);
        }
    }
    else {
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
**  main() of the new client thread.
** Read the fd for the request.
** Output the results.
*/
void
handleClient(void *inArg)
{
    PRFileDesc *aFD = NULL;

    aFD = (PRFileDesc *) inArg;
    if (NULL != aFD) {
        PRStatus closeRes = PR_SUCCESS;
        char aBuffer[2048];
        PRInt32 readRes = 0;

        readRes = PR_Read(aFD, aBuffer, sizeof(aBuffer));
        if (0 <= readRes) {
            const char *sanityCheck = "GET /";

            if (0 == strncmp(sanityCheck, aBuffer, 5)) {
                char *eourl = NULL;
                char *start = &aBuffer[5];
                char *getData = NULL;
                int realFun = 0;
                const char *crlf = "\015\012";
                char *eoline = NULL;
                FormData *fdGet = NULL;

                /*
                 ** Truncate the line if possible.
                 ** Only want first one.
                 */
                eoline = strstr(aBuffer, crlf);
                if (NULL != eoline) {
                    *eoline = '\0';
                }

                /*
                 ** Find the whitespace.
                 ** That is either end of line or the " HTTP/1.x" suffix.
                 ** We do not care.
                 */
                for (eourl = start; 0 == isspace(*eourl) && '\0' != *eourl;
                     eourl++) {
                    /*
                     ** No body.
                     */
                }

                /*
                 ** Cap it off.
                 ** Convert empty '/' to index.html.
                 */
                *eourl = '\0';
                if ('\0' == *start) {
                    strcpy(start, "index.html");
                }

                /*
                 ** Have we got any GET form data?
                 */
                getData = strchr(start, '?');
                if (NULL != getData) {
                    /*
                     ** Whack it off.
                     */
                    *getData = '\0';
                    getData++;
                }

                /*
                 **  Convert get data into a more useful format.
                 */
                fdGet = FormData_Create(getData);
                /*
                 ** This is totally a hack, but oh well....
                 **
                 ** Send that the request was OK, regardless.
                 **
                 ** If we have any get data, then it is a set of options
                 **      we attempt to apply.
                 **
                 ** Other code will tell the user they were wrong or if
                 **      there was an error.
                 ** If the filename contains a ".png", then send the image
                 **      mime type, otherwise, say it is text/html. 
                 */
                PR_fprintf(aFD, "HTTP/1.1 200 OK%s", crlf);
                PR_fprintf(aFD, "Server: %s%s",
                           "$Id: spacetrace.c,v 1.54 2006/11/01 23:02:17 timeless%mozdev.org Exp $",
                           crlf);
                PR_fprintf(aFD, "Content-type: ");
                if (NULL != strstr(start, ".png")) {
                    PR_fprintf(aFD, "image/png");
                }
                else if (NULL != strstr(start, ".jpg")) {
                    PR_fprintf(aFD, "image/jpeg");
                }
                else if (NULL != strstr(start, ".txt")) {
                    PR_fprintf(aFD, "text/plain");
                }
                else if (NULL != strstr(start, ".css")) {
                    PR_fprintf(aFD, "text/css");
                }
                else {
                    PR_fprintf(aFD, "text/html");
                }
                PR_fprintf(aFD, crlf);
                /*
                 ** One more to separate headers from content.
                 */
                PR_fprintf(aFD, crlf);
                /*
                 ** Ready for the real fun.
                 */
                realFun = handleRequest(globals.mTMR, aFD, start, fdGet);
                if (0 != realFun) {
                    REPORT_ERROR(__LINE__, handleRequest);
                }

                /*
                 **  Free off get data if around.
                 */
                FormData_Destroy(fdGet);
                fdGet = NULL;
            }
            else {
                REPORT_ERROR(__LINE__, handleClient);
            }
        }
        else {
            REPORT_ERROR(__LINE__, lineReader);
        }

        /*
         ** Done with the connection.
         */
        closeRes = PR_Close(aFD);
        if (PR_SUCCESS != closeRes) {
            REPORT_ERROR(__LINE__, PR_Close);
        }
    }
    else {
        REPORT_ERROR(__LINE__, handleClient);
    }
}

/*
** serverMode
**
** List on a port as a httpd.
** Output results interactively on demand.
**
** Returns !0 on error.
*/
int
serverMode(void)
{
    int retval = 0;
    PRFileDesc *socket = NULL;

    /*
     ** Create a socket.
     */
    socket = PR_NewTCPSocket();
    if (NULL != socket) {
        PRStatus closeRes = PR_SUCCESS;
        PRNetAddr bindAddr;
        PRStatus bindRes = PR_SUCCESS;

        /*
         ** Bind it to an interface/port.
         ** Any interface.
         */
        bindAddr.inet.family = PR_AF_INET;
        bindAddr.inet.port =
            PR_htons((PRUint16) globals.mCommandLineOptions.mHttpdPort);
        bindAddr.inet.ip = PR_htonl(PR_INADDR_ANY);
        bindRes = PR_Bind(socket, &bindAddr);
        if (PR_SUCCESS == bindRes) {
            PRStatus listenRes = PR_SUCCESS;
            const int backlog = 0x20;

            /*
             ** Start listening for clients.
             ** Give a decent backlog, some of our processing will take
             **  a bit.
             */
            listenRes = PR_Listen(socket, backlog);
            if (PR_SUCCESS == listenRes) {
                PRFileDesc *connection = NULL;
                int failureSum = 0;
                char message[80];

                /*
                 ** Output a little message saying we are receiving.
                 */
                PR_snprintf(message, sizeof(message),
                            "server accepting connections at http://localhost:%u/",
                            globals.mCommandLineOptions.mHttpdPort);
                REPORT_INFO(message);
                PR_fprintf(PR_STDOUT, "Peak memory used: %s bytes\n",
                           FormatNumber(globals.mPeakMemoryUsed));
                PR_fprintf(PR_STDOUT, "Allocations     : %s total\n",
                           FormatNumber(globals.mMallocCount +
                                        globals.mCallocCount +
                                        globals.mReallocCount),
                           FormatNumber(globals.mFreeCount));
                PR_fprintf(PR_STDOUT, "Breakdown       : %s malloc\n",
                           FormatNumber(globals.mMallocCount));
                PR_fprintf(PR_STDOUT, "                  %s calloc\n",
                           FormatNumber(globals.mCallocCount));
                PR_fprintf(PR_STDOUT, "                  %s realloc\n",
                           FormatNumber(globals.mReallocCount));
                PR_fprintf(PR_STDOUT, "                  %s free\n",
                           FormatNumber(globals.mFreeCount));
                PR_fprintf(PR_STDOUT, "Leaks           : %s\n",
                           FormatNumber((globals.mMallocCount +
                                         globals.mCallocCount +
                                         globals.mReallocCount) -
                                        globals.mFreeCount));
                /*
                 **  Keep accepting until we know otherwise.
                 **
                 **  We do a thread per connection.
                 **  Up to the thread to close the connection when done.
                 **
                 **  This is known by me to be suboptimal, and I would rather
                 **      do a thread pool if it ever becomes a resource issue.
                 **  Any issues would simply point to a need to get
                 **      more machines or a beefier machine to handle the
                 **      requests, as well as a need to do thread pooling and
                 **      avoid thread creation overhead.
                 **  The threads are not tracked, except possibly by NSPR
                 **      itself and PR_Cleanup will wait on them all to exit as
                 **      user threads so our shared data is valid.
                 */
                while (0 == retval) {
                    connection =
                        PR_Accept(socket, NULL, PR_INTERVAL_NO_TIMEOUT);
                    if (NULL != connection) {
                        PRThread *clientThread = NULL;

                        /*
                         **  Thread per connection.
                         */
                        clientThread = PR_CreateThread(PR_USER_THREAD,  /* PR_Cleanup sync */
                                                       handleClient, (void *) connection, PR_PRIORITY_NORMAL, PR_GLOBAL_THREAD, /* IO enabled */
                                                       PR_UNJOINABLE_THREAD,
                                                       0);
                        if (NULL == clientThread) {
                            PRStatus closeRes = PR_SUCCESS;

                            failureSum += __LINE__;
                            REPORT_ERROR(__LINE__, PR_Accept);
                            /*
                             **  Close the connection as well, no service
                             */
                            closeRes = PR_Close(connection);
                            if (PR_FAILURE == closeRes) {
                                REPORT_ERROR(__LINE__, PR_Close);
                            }
                        }
                    }
                    else {
                        failureSum += __LINE__;
                        REPORT_ERROR(__LINE__, PR_Accept);
                    }
                }

                if (0 != failureSum) {
                    retval = __LINE__;
                }

                /*
                 ** Output a little message saying it is all over.
                 */
                REPORT_INFO("server no longer accepting connections....");
            }
            else {
                retval = __LINE__;
                REPORT_ERROR(__LINE__, PR_Listen);
            }
        }
        else {
            retval = __LINE__;
            REPORT_ERROR(__LINE__, PR_Bind);
        }

        /*
         ** Done with socket.
         */
        closeRes = PR_Close(socket);
        if (PR_SUCCESS != closeRes) {
            retval = __LINE__;
            REPORT_ERROR(__LINE__, PR_Close);
        }
        socket = NULL;
    }
    else {
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
int
batchMode(void)
{
    int retval = 0;

    if (0 != globals.mCommandLineOptions.mBatchRequestCount) {
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
        for (loop = 0;
             loop < globals.mCommandLineOptions.mBatchRequestCount; loop++) {
            sprintfRes =
                PR_snprintf(aFileName, sizeof(aFileName), "%s%c%s",
                            globals.mCommandLineOptions.mOutputDir,
                            PR_GetDirectorySeparator(),
                            globals.mCommandLineOptions.mBatchRequest[loop]);
            if ((PRUint32) - 1 != sprintfRes) {
                PRFileDesc *outFile = NULL;

                outFile = PR_Open(aFileName, ST_FLAGS, ST_PERMS);
                if (NULL != outFile) {
                    PRStatus closeRes = PR_SUCCESS;

                    handleRes =
                        handleRequest(globals.mTMR, outFile,
                                      globals.mCommandLineOptions.
                                      mBatchRequest[loop], NULL);
                    if (0 != handleRes) {
                        failureSum += __LINE__;
                        REPORT_ERROR(__LINE__, handleRequest);
                    }

                    closeRes = PR_Close(outFile);
                    if (PR_SUCCESS != closeRes) {
                        failureSum += __LINE__;
                        REPORT_ERROR(__LINE__, PR_Close);
                    }
                }
                else {
                    failureSum += __LINE__;
                    REPORT_ERROR(__LINE__, PR_Open);
                }
            }
            else {
                failureSum += __LINE__;
                REPORT_ERROR(__LINE__, PR_snprintf);
            }
        }

        if (0 != failureSum) {
            retval = __LINE__;
        }
    }
    else {
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
int
doRun(void)
{
    int retval = 0;

    /*
     ** Create the new trace-malloc reader.
     */
    globals.mTMR = tmreader_new(globals.mProgramName, NULL);
    if (NULL != globals.mTMR) {
        int tmResult = 0;
        int outputResult = 0;

#if defined(DEBUG_dp)
        PRIntervalTime start = PR_IntervalNow();

        fprintf(stderr, "DEBUG: reading tracemalloc data...\n");
#endif
        tmResult =
            tmreader_eventloop(globals.mTMR,
                               globals.mCommandLineOptions.mFileName,
                               tmEventHandler);
        printf("\rReading... Done.\n");
#if defined(DEBUG_dp)
        fprintf(stderr,
                "DEBUG: reading tracemalloc data ends: %dms [%d allocations]\n",
                PR_IntervalToMilliseconds(PR_IntervalNow() - start),
                globals.mRun.mAllocationCount);
#endif
        if (0 == tmResult) {
            REPORT_ERROR(__LINE__, tmreader_eventloop);
            retval = __LINE__;
        }

        if (0 == retval) {
            /*
             ** Decide if we're going into batch mode or server mode.
             */
            if (0 != globals.mCommandLineOptions.mBatchRequestCount) {
                /*
                 ** Output in one big step while everything still exists.
                 */
                outputResult = batchMode();
                if (0 != outputResult) {
                    REPORT_ERROR(__LINE__, batchMode);
                    retval = __LINE__;
                }
            }
            else {
                int serverRes = 0;

                /*
                 ** httpd time.
                 */
                serverRes = serverMode();
                if (0 != serverRes) {
                    REPORT_ERROR(__LINE__, serverMode);
                    retval = __LINE__;
                }
            }

            /*
             ** Clear our categorization tree
             */
            freeCategories(&globals);
        }
    }
    else {
        REPORT_ERROR(__LINE__, tmreader_new);
        retval = __LINE__;
    }

    return retval;
}

int
initCaches(void)
/*
**  Initialize the global caches.
**  More involved since we have to allocated/create some objects.
**
**  returns     Zero if all is well.
**              Non-zero on error.
*/
{
    int retval = 0;
    STContextCache *inCache = &globals.mContextCache;

    if (NULL != inCache && 0 != globals.mCommandLineOptions.mContexts) {
        inCache->mLock = PR_NewLock();
        if (NULL != inCache->mLock) {
            inCache->mCacheMiss = PR_NewCondVar(inCache->mLock);
            if (NULL != inCache->mCacheMiss) {
                inCache->mItems =
                    (STContextCacheItem *) calloc(globals.mCommandLineOptions.
                                                  mContexts,
                                                  sizeof(STContextCacheItem));
                if (NULL != inCache->mItems) {
                    PRUint32 loop = 0;
                    char buffer[64];

                    inCache->mItemCount =
                        globals.mCommandLineOptions.mContexts;
                    /*
                     **  Init each item as needed.
                     */
                    for (loop = 0; loop < inCache->mItemCount; loop++) {
                        inCache->mItems[loop].mContext.mIndex = loop;
                        PR_snprintf(buffer, sizeof(buffer),
                                    "Context Item %d RW Lock", loop);
                        inCache->mItems[loop].mContext.mRWLock =
                            PR_NewRWLock(PR_RWLOCK_RANK_NONE, buffer);
                        if (NULL == inCache->mItems[loop].mContext.mRWLock) {
                            break;
                        }
#if ST_WANT_GRAPHS
                        inCache->mItems[loop].mContext.mImageLock =
                            PR_NewLock();
                        if (NULL == inCache->mItems[loop].mContext.mImageLock) {
                            break;
                        }
#endif
                    }

                    if (loop != inCache->mItemCount) {
                        retval = __LINE__;
                        REPORT_ERROR(__LINE__, initCaches);
                    }
                }
                else {
                    retval = __LINE__;
                    REPORT_ERROR(__LINE__, calloc);
                }
            }
            else {
                retval = __LINE__;
                REPORT_ERROR(__LINE__, PR_NewCondVar);
            }
        }
        else {
            retval = __LINE__;
            REPORT_ERROR(__LINE__, PR_NewLock);
        }
    }
    else {
        retval = __LINE__;
        REPORT_ERROR(__LINE__, initCaches);
    }

    return retval;
}

int
destroyCaches(void)
/*
**  Clean up any global caches we have laying around.
**
**  returns     Zero if all is well.
**              Non-zero on error.
*/
{
    int retval = 0;
    STContextCache *inCache = &globals.mContextCache;

    if (NULL != inCache) {
        PRUint32 loop = 0;

        /*
         **  Uninit item data one by one.
         */
        for (loop = 0; loop < inCache->mItemCount; loop++) {
            if (NULL != inCache->mItems[loop].mContext.mRWLock) {
                PR_DestroyRWLock(inCache->mItems[loop].mContext.mRWLock);
                inCache->mItems[loop].mContext.mRWLock = NULL;
            }
#if ST_WANT_GRAPHS
            if (NULL != inCache->mItems[loop].mContext.mImageLock) {
                PR_DestroyLock(inCache->mItems[loop].mContext.mImageLock);
                inCache->mItems[loop].mContext.mImageLock = NULL;
            }
#endif
        }

        inCache->mItemCount = 0;
        if (NULL != inCache->mItems) {
            free(inCache->mItems);
            inCache->mItems = NULL;
        }

        if (NULL != inCache->mCacheMiss) {
            PR_DestroyCondVar(inCache->mCacheMiss);
            inCache->mCacheMiss = NULL;
        }

        if (NULL != inCache->mLock) {
            PR_DestroyLock(inCache->mLock);
            inCache->mLock = NULL;
        }
    }
    else {
        retval = __LINE__;
        REPORT_ERROR(__LINE__, destroyCaches);
    }

    return retval;
}

/*
** main
**
** Process entry and exit.
*/
int
main(int aArgCount, char **aArgArray)
{
    int retval = 0;
    int optionsResult = 0;
    PRStatus prResult = PR_SUCCESS;
    int showedHelp = 0;
    int looper = 0;
    int cacheResult = 0;

    /*
     ** NSPR init.
     */
    PR_Init(PR_USER_THREAD, PR_PRIORITY_NORMAL, 0);
    /*
     ** Initialize globals
     */
    memset(&globals, 0, sizeof(globals));
    /*
     ** Set the program name.
     */
    globals.mProgramName = aArgArray[0];
    /*
     ** Set the minimum timeval really high so other code
     **  that checks the timeval will get it right.
     */
    globals.mMinTimeval = ST_TIMEVAL_MAX;
    /*
     ** Handle initializing options.
     */
    optionsResult = initOptions(aArgCount, aArgArray);
    if (0 != optionsResult) {
        REPORT_ERROR(optionsResult, initOptions);
        retval = __LINE__;
    }

    /*
     **  Initialize our caches.
     */
    cacheResult = initCaches();
    if (0 != cacheResult) {
        retval = __LINE__;
        REPORT_ERROR(__LINE__, initCaches);
    }

    /*
     **  Small alloc code init.
     */
    globals.mCategoryRoot.runs =
        (STRun **) calloc(globals.mCommandLineOptions.mContexts,
                          sizeof(STRun *));
    if (NULL == globals.mCategoryRoot.runs) {
        retval = __LINE__;
        REPORT_ERROR(__LINE__, calloc);
    }

    /*
     ** Show help on usage if need be.
     */
    showedHelp = showHelp();
    /*
     ** Only perform the run if everything is checking out.
     */
    if (0 == showedHelp && 0 == retval) {
        int runResult = 0;

        runResult = doRun();
        if (0 != runResult) {
            REPORT_ERROR(runResult, doRun);
            retval = __LINE__;
        }
    }

    if (0 != retval) {
        REPORT_ERROR(retval, main);
    }

    /*
     **  Have NSPR join all client threads we started.
     */
    prResult = PR_Cleanup();
    if (PR_SUCCESS != prResult) {
        REPORT_ERROR(retval, PR_Cleanup);
        retval = __LINE__;
    }
    /*
     **  All threads are joined/done by this line.
     */

    /*
     **  Options allocated a little.
     */
#define ST_CMD_OPTION_STRING_PTR_ARRAY(option_name, option_genre, option_help) \
    if(NULL != globals.mCommandLineOptions.m##option_name) \
    { \
        free((void*)globals.mCommandLineOptions.m##option_name); \
        globals.mCommandLineOptions.m##option_name = NULL; \
        globals.mCommandLineOptions.m##option_name##Count = 0; \
    }
#include "stoptions.h"

    /*
     **  globals has a small modification to clear up.
     */
    if (NULL != globals.mCategoryRoot.runs) {
        free(globals.mCategoryRoot.runs);
        globals.mCategoryRoot.runs = NULL;
    }

    /*
     **  Blow away our caches.
     */
    cacheResult = destroyCaches();
    if (0 != cacheResult) {
        retval = __LINE__;
        REPORT_ERROR(__LINE__, initCaches);
    }

    /*
     **  We are safe to kill our tmreader data.
     */
    if (NULL != globals.mTMR) {
        tmreader_destroy(globals.mTMR);
        globals.mTMR = NULL;
    }

    return retval;
}
