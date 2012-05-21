/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <errno.h>
#include <math.h>

#include "nspr.h"
#include "tmreader.h"


#define ERROR_REPORT(num, val, msg)   fprintf(stderr, "error(%d):\t\"%s\"\t%s\n", (num), (val), (msg));
#define CLEANUP(ptr)    do { if(NULL != ptr) { free(ptr); ptr = NULL; } } while(0)


#define ticks2msec(reader, ticks) ticks2xsec((reader), (ticks), 1000)
#define ticks2usec(reader, ticks) ticks2xsec((reader), (ticks), 1000000)
#define TICK_RESOLUTION 1000
#define TICK_PRINTABLE(timeval) ((PRFloat64)(timeval) / (PRFloat64)ST_TIMEVAL_RESOLUTION)


typedef struct __struct_Options
/*
**  Options to control how we perform.
**
**  mProgramName    Used in help text.
**  mInputName      Name of the file.
**  mOutput         Output file, append.
**                  Default is stdout.
**  mOutputName     Name of the file.
**  mHelp           Whether or not help should be shown.
**  mOverhead       How much overhead an allocation will have.
**  mAlignment      What boundry will the end of an allocation line up on.
**  mPageSize       Controls the page size.  A page containing only fragments
**                      is not fragmented.  A page containing any life memory
**                      costs mPageSize in bytes.
*/
{
    const char* mProgramName;
    char* mInputName;
    FILE* mOutput;
    char* mOutputName;
    int mHelp;
    unsigned mOverhead;
    unsigned mAlignment;
    unsigned mPageSize;
}
Options;


typedef struct __struct_Switch
/*
**  Command line options.
*/
{
    const char* mLongName;
    const char* mShortName;
    int mHasValue;
    const char* mValue;
    const char* mDescription;
}
Switch;

#define DESC_NEWLINE "\n\t\t"

static Switch gInputSwitch = {"--input", "-i", 1, NULL, "Specify input file." DESC_NEWLINE "stdin is default."};
static Switch gOutputSwitch = {"--output", "-o", 1, NULL, "Specify output file." DESC_NEWLINE "Appends if file exists." DESC_NEWLINE "stdout is default."};
static Switch gHelpSwitch = {"--help", "-h", 0, NULL, "Information on usage."};
static Switch gAlignmentSwitch = {"--alignment", "-al", 1, NULL, "All allocation sizes are made to be a multiple of this number." DESC_NEWLINE "Closer to actual heap conditions; set to 1 for true sizes." DESC_NEWLINE "Default value is 16."};
static Switch gOverheadSwitch = {"--overhead", "-ov", 1, NULL, "After alignment, all allocations are made to increase by this number." DESC_NEWLINE "Closer to actual heap conditions; set to 0 for true sizes." DESC_NEWLINE "Default value is 8."};
static Switch gPageSizeSwitch = {"--page-size", "-ps", 1, NULL, "Sets the page size which aids the identification of fragmentation." DESC_NEWLINE "Closer to actual heap conditions; set to 4294967295 for true sizes."  DESC_NEWLINE "Default value is 4096."};

static Switch* gSwitches[] = {
        &gInputSwitch,
        &gOutputSwitch,
        &gAlignmentSwitch,
        &gOverheadSwitch,
        &gPageSizeSwitch,
        &gHelpSwitch
};


typedef struct __struct_AnyArray
/*
**  Variable sized item array.
**
**  mItems      The void pointer items.
**  mItemSize   Size of each different item.
**  mCount      The number of items in the array.
**  mCapacity   How many more items we can hold before reallocing.
**  mGrowBy     How many items we allocate when we grow.
*/
{
    void* mItems;
    unsigned mItemSize;
    unsigned mCount;
    unsigned mCapacity;
    unsigned mGrowBy;
}
AnyArray;


typedef int (*arrayMatchFunc)(void* inContext, AnyArray* inArray, void* inItem, unsigned inItemIndex)
/*
**  Callback function for the arrayIndexFn function.
**  Used to determine an item match by customizable criteria.
**
**  inContext       The criteria and state of the search.
**                  User specified/created.
**  inArray         The array the item is in.
**  inItem          The item to evaluate for match.
**  inItemIndex     The index of this particular item in the array.
**
**  return int      0 to specify a match.
**                  !0 to continue the search performed by arrayIndexFn.
*/
;


typedef enum __enum_HeapEventType
/*
**  Simple heap events are really one of two things.
*/
{
    FREE,
    ALLOC
}
HeapEventType;


typedef enum __enum_HeapObjectType
/*
**  The various types of heap objects we track.
*/
{
    ALLOCATION,
    FRAGMENT
}
HeapObjectType;


typedef struct __struct_HeapObject HeapObject;
typedef struct __struct_HeapHistory
/*
**  A marker as to what has happened.
**
**  mTimestamp      When history occurred.
**  mTMRSerial      The historical state as known to the tmreader.
**  mObjectIndex    Index to the object that was before or after this event.
**                  The index as in the index according to all heap objects
**                      kept in the TMState structure.
**                  We use an index instead of a pointer as the array of
**                      objects can change location in the heap.
*/
{
    unsigned mTimestamp;
    unsigned mTMRSerial;
    unsigned mObjectIndex;
}
HeapHistory;


struct __struct_HeapObject
/*
**  An object in the heap.
**
**  A special case should be noted here.  If either the birth or death
**      history leads to an object of the same type, then this object
**      is the same as that object, but was modified somehow.
**  Also note that multiple objects may have the same birth object,
**      as well as the same death object.
**
**  mUniqueID           Each object is unique.
**  mType               Either allocation or fragment.
**  mHeapOffset         Where in the heap the object is.
**  mSize               How much of the heap the object takes.
**  mBirth              History about the birth event.
**  mDeath              History about the death event.
*/
{
    unsigned mUniqueID;

    HeapObjectType mType;
    unsigned mHeapOffset;
    unsigned mSize;

    HeapHistory mBirth;
    HeapHistory mDeath;
};


typedef struct __struct_TMState
/*
**  State of our current operation.
**  Stats we are trying to calculate.
**
**  mOptions        Obilgatory options pointer.
**  mTMR            The tmreader, used in tmreader API calls.
**  mLoopExitTMR    Set to non zero in order to quickly exit from tmreader
**                      input loop.  This will also result in an error.
**  uMinTicks       Start of run, milliseconds.
**  uMaxTicks       End of run, milliseconds.
*/
{
    Options* mOptions;
    tmreader* mTMR;

    int mLoopExitTMR;

    unsigned uMinTicks;
    unsigned uMaxTicks;
}
TMState;


int initOptions(Options* outOptions, int inArgc, char** inArgv)
/*
**  returns int     0 if successful.
*/
{
    int retval = 0;
    int loop = 0;
    int switchLoop = 0;
    int match = 0;
    const int switchCount = sizeof(gSwitches) / sizeof(gSwitches[0]);
    Switch* current = NULL;

    /*
    **  Set any defaults.
    */
    memset(outOptions, 0, sizeof(Options));
    outOptions->mProgramName = inArgv[0];
    outOptions->mInputName = strdup("-");
    outOptions->mOutput = stdout;
    outOptions->mOutputName = strdup("stdout");
    outOptions->mAlignment = 16;
    outOptions->mOverhead = 8;

    if(NULL == outOptions->mOutputName || NULL == outOptions->mInputName)
    {
        retval = __LINE__;
        ERROR_REPORT(retval, "stdin/stdout", "Unable to strdup.");
    }

    /*
    **  Go through and attempt to do the right thing.
    */
    for(loop = 1; loop < inArgc && 0 == retval; loop++)
    {
        match = 0;
        current = NULL;

        for(switchLoop = 0; switchLoop < switchCount && 0 == retval; switchLoop++)
        {
            if(0 == strcmp(gSwitches[switchLoop]->mLongName, inArgv[loop]))
            {
                match = __LINE__;
            }
            else if(0 == strcmp(gSwitches[switchLoop]->mShortName, inArgv[loop]))
            {
                match = __LINE__;
            }

            if(match)
            {
                if(gSwitches[switchLoop]->mHasValue)
                {
                    /*
                    **  Attempt to absorb next option to fullfill value.
                    */
                    if(loop + 1 < inArgc)
                    {
                        loop++;

                        current = gSwitches[switchLoop];
                        current->mValue = inArgv[loop];
                    }
                }
                else
                {
                    current = gSwitches[switchLoop];
                }

                break;
            }
        }

        if(0 == match)
        {
            outOptions->mHelp = __LINE__;
            retval = __LINE__;
            ERROR_REPORT(retval, inArgv[loop], "Unknown command line switch.");
        }
        else if(NULL == current)
        {
            outOptions->mHelp = __LINE__;
            retval = __LINE__;
            ERROR_REPORT(retval, inArgv[loop], "Command line switch requires a value.");
        }
        else
        {
            /*
            ** Do something based on address/swtich.
            */
            if(current == &gInputSwitch)
            {
                CLEANUP(outOptions->mInputName);
                outOptions->mInputName = strdup(current->mValue);
                if(NULL == outOptions->mInputName)
                {
                    retval = __LINE__;
                    ERROR_REPORT(retval, current->mValue, "Unable to strdup.");
                }
            }
            else if(current == &gOutputSwitch)
            {
                CLEANUP(outOptions->mOutputName);
                if(NULL != outOptions->mOutput && stdout != outOptions->mOutput)
                {
                    fclose(outOptions->mOutput);
                    outOptions->mOutput = NULL;
                }

                outOptions->mOutput = fopen(current->mValue, "a");
                if(NULL == outOptions->mOutput)
                {
                    retval = __LINE__;
                    ERROR_REPORT(retval, current->mValue, "Unable to open output file.");
                }
                else
                {
                    outOptions->mOutputName = strdup(current->mValue);
                    if(NULL == outOptions->mOutputName)
                    {
                        retval = __LINE__;
                        ERROR_REPORT(retval, current->mValue, "Unable to strdup.");
                    }
                }
            }
            else if(current == &gHelpSwitch)
            {
                outOptions->mHelp = __LINE__;
            }
            else if(current == &gAlignmentSwitch)
            {
                unsigned arg = 0;
                char* endScan = NULL;

                errno = 0;
                arg = strtoul(current->mValue, &endScan, 0);
                if(0 == errno && endScan != current->mValue)
                {
                    outOptions->mAlignment = arg;
                }
                else
                {
                    retval = __LINE__;
                    ERROR_REPORT(retval, current->mValue, "Unable to convert to a number.");
                }
            }
            else if(current == &gOverheadSwitch)
            {
                unsigned arg = 0;
                char* endScan = NULL;

                errno = 0;
                arg = strtoul(current->mValue, &endScan, 0);
                if(0 == errno && endScan != current->mValue)
                {
                    outOptions->mOverhead = arg;
                }
                else
                {
                    retval = __LINE__;
                    ERROR_REPORT(retval, current->mValue, "Unable to convert to a number.");
                }
            }
            else if(current == &gPageSizeSwitch)
            {
                unsigned arg = 0;
                char* endScan = NULL;

                errno = 0;
                arg = strtoul(current->mValue, &endScan, 0);
                if(0 == errno && endScan != current->mValue)
                {
                    outOptions->mPageSize = arg;
                }
                else
                {
                    retval = __LINE__;
                    ERROR_REPORT(retval, current->mValue, "Unable to convert to a number.");
                }
            }
            else
            {
                retval = __LINE__;
                ERROR_REPORT(retval, current->mLongName, "No handler for command line switch.");
            }
        }
    }

    return retval;
}


PRUint32 ticks2xsec(tmreader* aReader, PRUint32 aTicks, PRUint32 aResolution)
/*
** Convert platform specific ticks to second units
*/
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


void cleanOptions(Options* inOptions)
/*
**  Clean up any open handles.
*/
{
    unsigned loop = 0;

    CLEANUP(inOptions->mInputName);
    CLEANUP(inOptions->mOutputName);
    if(NULL != inOptions->mOutput && stdout != inOptions->mOutput)
    {
        fclose(inOptions->mOutput);
    }

    memset(inOptions, 0, sizeof(Options));
}


void showHelp(Options* inOptions)
/*
**  Show some simple help text on usage.
*/
{
    int loop = 0;
    const int switchCount = sizeof(gSwitches) / sizeof(gSwitches[0]);
    const char* valueText = NULL;

    printf("usage:\t%s [arguments]\n", inOptions->mProgramName);
    printf("\n");
    printf("arguments:\n");

    for(loop = 0; loop < switchCount; loop++)
    {
        if(gSwitches[loop]->mHasValue)
        {
            valueText = " <value>";
        }
        else
        {
            valueText = "";
        }

        printf("\t%s%s\n", gSwitches[loop]->mLongName, valueText);
        printf("\t %s%s", gSwitches[loop]->mShortName, valueText);
        printf(DESC_NEWLINE "%s\n\n", gSwitches[loop]->mDescription);
    }

    printf("This tool reports heap fragmentation stats from a trace-malloc log.\n");
}


AnyArray* arrayCreate(unsigned inItemSize, unsigned inGrowBy)
/*
**  Create an array container object.
*/
{
    AnyArray* retval = NULL;

    if(0 != inGrowBy && 0 != inItemSize)
    {
        retval = (AnyArray*)calloc(1, sizeof(AnyArray));
        retval->mItemSize = inItemSize;
        retval->mGrowBy = inGrowBy;
    }

    return retval;
}


void arrayDestroy(AnyArray* inArray)
/*
**  Release the memory the array contains.
**  This will release the items as well.
*/
{
    if(NULL != inArray)
    {
        if(NULL != inArray->mItems)
        {
            free(inArray->mItems);
        }
        free(inArray);
    }
}


unsigned arrayAlloc(AnyArray* inArray, unsigned inItems)
/*
**  Resize the item array capcity to a specific number of items.
**  This could possibly truncate the array, so handle that as well.
**
**  returns unsigned        <= inArray->mCapacity on success.
*/
{
    unsigned retval = (unsigned)-1;

    if(NULL != inArray)
    {
        void* moved = NULL;

        moved = realloc(inArray->mItems, inItems * inArray->mItemSize);
        if(NULL != moved)
        {
            inArray->mItems = moved;
            inArray->mCapacity = inItems;
            if(inArray->mCount > inItems)
            {
                inArray->mCount = inItems;
            }

            retval = inItems;
        }
    }

    return retval;
}


void* arrayItem(AnyArray* inArray, unsigned inIndex)
/*
**  Return the array item at said index.
**  Zero based index.
**
**  returns void*       NULL on failure.
*/
{
    void* retval = NULL;

    if(NULL != inArray && inIndex < inArray->mCount)
    {
        retval = (void*)((char*)inArray->mItems + (inArray->mItemSize * inIndex));
    }

    return retval;
}


unsigned arrayIndex(AnyArray* inArray, void* inItem, unsigned inStartIndex)
/*
**  Go through the array from the index specified looking for an item
**      match based on byte for byte comparison.
**  We allow specifying the start index in order to handle arrays with
**      duplicate items.
**
**  returns unsigned        >= inArray->mCount on failure.
*/
{
    unsigned retval = (unsigned)-1;

    if(NULL != inArray && NULL != inItem && inStartIndex < inArray->mCount)
    {
        void* curItem = NULL;

        for(retval = inStartIndex; retval < inArray->mCount; retval++)
        {
            curItem = arrayItem(inArray, retval);
            if(0 == memcmp(inItem, curItem, inArray->mItemSize))
            {
                break;
            }
        }
    }


    return retval;
}


unsigned arrayIndexFn(AnyArray* inArray, arrayMatchFunc inFunc, void* inFuncContext, unsigned inStartIndex)
/*
**  Go through the array from the index specified looking for an item
**      match based upon the return value of inFunc (0, Zero, is a match).
**  We allow specifying the start index in order to facilitate looping over
**      the array which could have multiple matches.
**
**  returns unsigned        >= inArray->mCount on failure.
*/
{
    unsigned retval = (unsigned)-1;

    if(NULL != inArray && NULL != inFunc && inStartIndex < inArray->mCount)
    {
        void* curItem = NULL;

        for(retval = inStartIndex; retval < inArray->mCount; retval++)
        {
            curItem = arrayItem(inArray, retval);
            if(0 == inFunc(inFuncContext, inArray, curItem, retval))
            {
                break;
            }
        }
    }

    return retval;
}


unsigned arrayAddItem(AnyArray* inArray, void* inItem)
/*
**  Add a new item to the array.
**  This is done by copying the item.
**
**  returns unsigned        < inArray->mCount on success.
*/
{
    unsigned retval = (unsigned)-1;

    if(NULL != inArray && NULL != inItem)
    {
        int noCopy = 0;

        /*
        **  See if the array should grow.
        */
        if(inArray->mCount == inArray->mCapacity)
        {
            unsigned allocRes = 0;

            allocRes = arrayAlloc(inArray, inArray->mCapacity + inArray->mGrowBy);
            if(allocRes > inArray->mCapacity)
            {
                noCopy = __LINE__;
            }
        }

        if(0 == noCopy)
        {
            retval = inArray->mCount;

            inArray->mCount++;
            memcpy(arrayItem(inArray, retval), inItem, inArray->mItemSize);
        }
    }

    return retval;
}


HeapObject* initHeapObject(HeapObject* inObject)
/*
**  Function to init the heap object just right.
**  Sets the unique ID to something unique.
*/
{
    HeapObject* retval = inObject;

    if(NULL != inObject)
    {
        static unsigned uniqueGenerator = 0;

        memset(inObject, -1, sizeof(HeapObject));

        inObject->mUniqueID = uniqueGenerator;
        uniqueGenerator++;
    }

    return retval;
}


int simpleHeapEvent(TMState* inStats, HeapEventType inType, unsigned mTimestamp, unsigned inSerial, unsigned inHeapID, unsigned inSize)
/*
**  A new heap event will cause the creation of a new heap object.
**  The new heap object will displace, or replace, a heap object of a different type.
*/
{
    int retval = 0;
    HeapObject newObject;

    /*
    **  Set the most basic object details.
    */
    initHeapObject(&newObject);
    newObject.mHeapOffset = inHeapID;
    newObject.mSize = inSize;
    if(FREE == inType)
    {
        newObject.mType = FRAGMENT;
    }
    else if(ALLOC == inType)
    {
        newObject.mType = ALLOCATION;
    }

    /*
    **  Add it to the heap object array.
    */

    /*
    **  TODO GAB
    **
    **  First thing to do is to add the new object to the heap in order to
    **      obtain a valid index.
    **
    **  Next, find all matches to this range of heap memory that this event
    **      refers to, that are alive during this timestamp (no death yet).
    **  Fill in the death event of those objects.
    **  If the objects contain some portions outside of the range, then
    **      new objects for those ranges need to be created that carry on
    **      the same object type, have the index of the old object for birth,
    **      and the serial of the old object, new timestamp of course.
    **  The old object's death points to the new object, which tells why the
    **      fragmentation took place.
    **  The new object birth points to the old object only if a fragment.
    **  An allocation only has a birth object when it is a realloc (complex)
    **      heap event.
    **
    **  I believe this give us enough information to look up particular
    **      details of the heap at any given time.
    */

    return retval;
}


int complexHeapEvent(TMState* inStats, unsigned mTimestamp, unsigned inOldSerial, unsigned inOldHeapID, unsigned inOSize, unsigned inNewSerial, unsigned inNewHeapID, unsigned inNewSize)
/*
**  Generally, this event intends to chain one old heap object to a newer heap object.
**  Otherwise, the functionality should recognizable ala simpleHeapEvent.
*/
{
    int retval = 0;

    /*
    **  TODO GAB
    */

    return retval;
}


unsigned actualByteSize(Options* inOptions, unsigned retval)
/*
**  Apply alignment and overhead to size to figure out actual byte size.
**  This by default mimics spacetrace with default options (msvc crt heap).
*/
{
    if(0 != retval)
    {
        unsigned eval = 0;
        unsigned over = 0;

        eval = retval - 1;
        if(0 != inOptions->mAlignment)
        {
            over = eval % inOptions->mAlignment;
        }
        retval = eval + inOptions->mOverhead + inOptions->mAlignment - over;
    }

    return retval;
}


void tmEventHandler(tmreader* inReader, tmevent* inEvent)
/*
**  Callback from the tmreader_eventloop.
**  Build up our fragmentation information herein.
*/
{
    char type = inEvent->type;
    TMState* stats = (TMState*)inReader->data;

    /*
    **  Only intersted in handling events of a particular type.
    */
    switch(type)
    {
    default:
        return;

    case TM_EVENT_MALLOC:
    case TM_EVENT_CALLOC:
    case TM_EVENT_REALLOC:
    case TM_EVENT_FREE:
        break;
    }

    /*
    **  Should we even try to look?
    **  Set mLoopExitTMR to non-zero to abort the read loop faster.
    */
    if(0 == stats->mLoopExitTMR)
    {
        Options* options = (Options*)stats->mOptions;
        unsigned timestamp = ticks2msec(stats->mTMR, inEvent->u.alloc.interval);
        unsigned actualSize = actualByteSize(options, inEvent->u.alloc.size);
        unsigned heapID = inEvent->u.alloc.ptr;
        unsigned serial = inEvent->serial;
        
        /*
        **  Check the timestamp range of our overall state.
        */
        if(stats->uMinTicks > timestamp)
        {
            stats->uMinTicks = timestamp;
        }
        if(stats->uMaxTicks < timestamp)
        {
            stats->uMaxTicks = timestamp;
        }
        
        /*
        **  Realloc in general deserves some special attention if dealing
        **      with an old allocation (not new memory).
        */
        if(TM_EVENT_REALLOC == type && 0 != inEvent->u.alloc.oldserial)
        {
            unsigned oldActualSize = actualByteSize(options, inEvent->u.alloc.oldsize);
            unsigned oldHeapID = inEvent->u.alloc.oldptr;
            unsigned oldSerial = inEvent->u.alloc.oldserial;

            if(0 == actualSize)
            {
                /*
                **  Reallocs of size zero are to become free events.
                */
                stats->mLoopExitTMR = simpleHeapEvent(stats, FREE, timestamp, serial, oldHeapID, oldActualSize);
            }
            else if(heapID != oldHeapID || actualSize != oldActualSize)
            {
                /*
                **  Reallocs which moved generate two events.
                **  Reallocs which changed size generate two events.
                **
                **  One event to free the old memory area.
                **  Another event to allocate the new memory area.
                **  They are to be linked to one another, so the history
                **      and true origin can be tracked.
                */
                stats->mLoopExitTMR = complexHeapEvent(stats, timestamp, oldSerial, oldHeapID, oldActualSize, serial, heapID, actualSize);
            }
            else
            {
                /*
                **  The realloc is not considered an operation and is skipped.
                **  It is not an operation, because it did not move or change
                **      size; this can happen if a realloc falls within the
                **      alignment of an allocation.
                **  Say if you realloc a 1 byte allocation to 2 bytes, it will
                **      not really change heap impact unless you have 1 set as
                **      the alignment of your allocations.
                */
            }
        }
        else if(TM_EVENT_FREE == type)
        {
            /*
            **  Generate a free event to create a fragment.
            */
            stats->mLoopExitTMR = simpleHeapEvent(stats, FREE, timestamp, serial, heapID, actualSize);
        }
        else
        {
            /*
            **  Generate an allocation event to clear fragments.
            */
            stats->mLoopExitTMR = simpleHeapEvent(stats, ALLOC, timestamp, serial, heapID, actualSize);
        }
    }
}


int tmfrags(Options* inOptions)
/*
**  Load the input file and report stats.
*/
{
    int retval = 0;
    TMState stats;

    memset(&stats, 0, sizeof(stats));
    stats.mOptions = inOptions;
    stats.uMinTicks = 0xFFFFFFFFU;

    /*
    **  Need a tmreader.
    */
    stats.mTMR = tmreader_new(inOptions->mProgramName, &stats);
    if(NULL != stats.mTMR)
    {
        int tmResult = 0;

        tmResult = tmreader_eventloop(stats.mTMR, inOptions->mInputName, tmEventHandler);
        if(0 == tmResult)
        {
            retval = __LINE__;
            ERROR_REPORT(retval, inOptions->mInputName, "Problem reading trace-malloc data.");
        }
        if(0 != stats.mLoopExitTMR)
        {
            retval = stats.mLoopExitTMR;
            ERROR_REPORT(retval, inOptions->mInputName, "Aborted trace-malloc input loop.");
        }

        tmreader_destroy(stats.mTMR);
        stats.mTMR = NULL;
    }
    else
    {
        retval = __LINE__;
        ERROR_REPORT(retval, inOptions->mProgramName, "Unable to obtain tmreader.");
    }

    return retval;
}


int main(int inArgc, char** inArgv)
{
    int retval = 0;
    Options options;

    retval = initOptions(&options, inArgc, inArgv);
    if(options.mHelp)
    {
        showHelp(&options);
    }
    else if(0 == retval)
    {
        retval = tmfrags(&options);
    }

    cleanOptions(&options);
    return retval;
}

