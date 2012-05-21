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

#define ERROR_REPORT(num, val, msg)   fprintf(stderr, "error(%d):\t\"%s\"\t%s\n", (num), (val), (msg));
#define CLEANUP(ptr)    do { if(NULL != ptr) { free(ptr); ptr = NULL; } } while(0)


typedef struct __struct_Options
/*
**  Options to control how we perform.
**
**  mProgramName    Used in help text.
**  mInput          File to read for input.
**                  Default is stdin.
**  mInputName      Name of the file.
**  mOutput         Output file, append.
**                  Default is stdout.
**  mOutputName     Name of the file.
**  mHelp           Whether or not help should be shown.
**  mSummaryOnly    Only output a signle line.
**  mZeroDrift      Output zero drift data.
**  mNegation       Perform negation heuristics on the symbol drifts.
*/
{
    const char* mProgramName;
    FILE* mInput;
    char* mInputName;
    FILE* mOutput;
    char* mOutputName;
    int mHelp;
    int mSummaryOnly;
    int mZeroDrift;
    int mNegation;
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
static Switch gSummarySwitch = {"--summary", "-s", 0, NULL, "Only output a single line." DESC_NEWLINE "The cumulative size changes." DESC_NEWLINE "Overrides all other output options."};
static Switch gZeroDriftSwitch = {"--zerodrift", "-z", 0, NULL, "Output zero drift data." DESC_NEWLINE "Reports symbol changes even when there is no net drift."};
static Switch gNegationSwitch = {"--negation", "-n", 0, NULL, "Use negation heuristics." DESC_NEWLINE "When symbol sizes are inferred by offset, order changes cause noise." DESC_NEWLINE "This helps see through the noise by eliminating equal and opposite drifts."};
static Switch gHelpSwitch = {"--help", "-h", 0, NULL, "Information on usage."};

static Switch* gSwitches[] = {
        &gInputSwitch,
        &gOutputSwitch,
        &gSummarySwitch,
        &gZeroDriftSwitch,
        &gNegationSwitch,
        &gHelpSwitch
};


typedef struct __struct_SizeComposition
/*
**  Used to keep which parts positive and negative resulted in the total.
*/
{
    int mPositive;
    int mNegative;
}
SizeComposition;


typedef struct __struct_SizeStats
/*
**  Keep track of sizes.
**  Use signed integers so that negatives are valid, in which case we shrunk.
*/
{
    int mCode;
    SizeComposition mCodeComposition;

    int mData;
    SizeComposition mDataComposition;
}
SizeStats;


typedef enum __enum_SegmentClass
/*
**  What type of data a segment holds.
*/
{
        CODE,
        DATA
}
SegmentClass;


typedef struct __struct_SymbolStats
/*
**  Symbol level stats.
*/
{
    char* mSymbol;
    int mSize;
}
SymbolStats;


typedef struct __struct_ObjectStats
/*
**  Object level stats.
*/
{
    char* mObject;
    int mSize;
    SizeComposition mComposition;
    SymbolStats* mSymbols;
    unsigned mSymbolCount;
}
ObjectStats;


typedef struct __struct_SegmentStats
/*
**  Segment level stats.
*/
{
    char* mSegment;
    SegmentClass mClass;
    int mSize;
    SizeComposition mComposition;
    ObjectStats* mObjects;
    unsigned mObjectCount;
}
SegmentStats;


typedef struct __struct_ModuleStats
/*
**  Module level stats.
*/
{
    char* mModule;
    SizeStats mSize;
    SegmentStats* mSegments;
    unsigned mSegmentCount;
}
ModuleStats;


static int moduleCompare(const void* in1, const void* in2)
/*
**  qsort helper.
*/
{
    int retval = 0;

    ModuleStats* one = (ModuleStats*)in1;
    ModuleStats* two = (ModuleStats*)in2;

    int oneSize = (one->mSize.mCode + one->mSize.mData);
    int twoSize = (two->mSize.mCode + two->mSize.mData);

    if(oneSize < twoSize)
    {
        retval = 1;
    }
    else if(oneSize > twoSize)
    {
        retval = -1;
    }
    else
    {
        retval = strcmp(one->mModule, two->mModule);
        if(0 > oneSize && 0 > twoSize)
        {
            retval *= -1;
        }
    }

    return retval;
}


static int segmentCompare(const void* in1, const void* in2)
/*
**  qsort helper.
*/
{
    int retval = 0;

    SegmentStats* one = (SegmentStats*)in1;
    SegmentStats* two = (SegmentStats*)in2;

    if(one->mSize < two->mSize)
    {
        retval = 1;
    }
    else if(one->mSize > two->mSize)
    {
        retval = -1;
    }
    else
    {
        retval = strcmp(one->mSegment, two->mSegment);
        if(0 > one->mSize && 0 > two->mSize)
        {
            retval *= -1;
        }
    }

    return retval;
}


static int objectCompare(const void* in1, const void* in2)
/*
**  qsort helper.
*/
{
    int retval = 0;

    ObjectStats* one = (ObjectStats*)in1;
    ObjectStats* two = (ObjectStats*)in2;

    if(one->mSize < two->mSize)
    {
        retval = 1;
    }
    else if(one->mSize > two->mSize)
    {
        retval = -1;
    }
    else
    {
        retval = strcmp(one->mObject, two->mObject);
        if(0 > one->mSize && 0 > two->mSize)
        {
            retval *= -1;
        }
    }

    return retval;
}


static int symbolCompare(const void* in1, const void* in2)
/*
**  qsort helper.
*/
{
    int retval = 0;

    SymbolStats* one = (SymbolStats*)in1;
    SymbolStats* two = (SymbolStats*)in2;

    if(one->mSize < two->mSize)
    {
        retval = 1;
    }
    else if(one->mSize > two->mSize)
    {
        retval = -1;
    }
    else
    {
        retval = strcmp(one->mSymbol, two->mSymbol);
        if(0 > one->mSize && 0 > two->mSize)
        {
            retval *= -1;
        }
    }

    return retval;
}


void trimWhite(char* inString)
/*
**  Remove any whitespace from the end of the string.
*/
{
    int len = strlen(inString);

    while(len)
    {
        len--;

        if(isspace(*(inString + len)))
        {
            *(inString + len) = '\0';
        }
        else
        {
            break;
        }
    }
}


int difftool(Options* inOptions)
/*
**  Read a diff file and spit out relevant information.
*/
{
    int retval = 0;
    char lineBuffer[0x500];
    SizeStats overall;
    ModuleStats* modules = NULL;
    unsigned moduleCount = 0;
    unsigned moduleLoop = 0;
    ModuleStats* theModule = NULL;
    unsigned segmentLoop = 0;
    SegmentStats* theSegment = NULL;
    unsigned objectLoop = 0;
    ObjectStats* theObject = NULL;
    unsigned symbolLoop = 0;
    SymbolStats* theSymbol = NULL;
    unsigned allSymbolCount = 0;

    memset(&overall, 0, sizeof(overall));

    /*
    **  Read the entire diff file.
    **  We're only interested in lines beginning with < or >
    */
    while(0 == retval && NULL != fgets(lineBuffer, sizeof(lineBuffer), inOptions->mInput))
    {
        trimWhite(lineBuffer);

        if(('<' == lineBuffer[0] || '>' == lineBuffer[0]) && ' ' == lineBuffer[1])
        {
            int additive = 0;
            char* theLine = &lineBuffer[2];
            int scanRes = 0;
            int size;
            #define SEGCLASS_CHARS 15
            char segClass[SEGCLASS_CHARS + 1];
            #define SCOPE_CHARS 15
            char scope[SCOPE_CHARS + 1];
            #define MODULE_CHARS 255
            char module[MODULE_CHARS + 1];
            #define SEGMENT_CHARS 63
            char segment[SEGMENT_CHARS + 1];
            #define OBJECT_CHARS 255
            char object[OBJECT_CHARS + 1];
            char* symbol = NULL;

            /*
            **  Figure out if the line adds or subtracts from something.
            */
            if('>' == lineBuffer[0])
            {
                additive = __LINE__;
            }


            /*
            **  Scan the line for information.
            */

#define STRINGIFY(s_) STRINGIFY2(s_)
#define STRINGIFY2(s_) #s_

            scanRes = sscanf(theLine,
                "%x\t%" STRINGIFY(SEGCLASS_CHARS) "s\t%"
                STRINGIFY(SCOPE_CHARS) "s\t%" STRINGIFY(MODULE_CHARS)
                "s\t%" STRINGIFY(SEGMENT_CHARS) "s\t%"
                STRINGIFY(OBJECT_CHARS) "s\t",
                (unsigned*)&size,
                segClass,
                scope,
                module,
                segment,
                object);

            if(6 == scanRes)
            {
                SegmentClass segmentClass = DATA;

                symbol = strrchr(theLine, '\t') + 1;

                if(0 == strcmp(segClass, "CODE"))
                {
                    segmentClass = CODE;
                }
                else if(0 == strcmp(segClass, "DATA"))
                {
                    segmentClass = DATA;
                }
                else
                {
                    retval = __LINE__;
                    ERROR_REPORT(retval, segClass, "Unable to determine segment class.");
                }

                if(0 == retval)
                {
                    unsigned moduleIndex = 0;

                    /*
                    **  Find, in succession, the following things:
                    **      the module
                    **      the segment
                    **      the object
                    **      the symbol
                    **  Failure to find any one of these means to create it.
                    */
                    
                    for(moduleIndex = 0; moduleIndex < moduleCount; moduleIndex++)
                    {
                        if(0 == strcmp(modules[moduleIndex].mModule, module))
                        {
                            break;
                        }
                    }
                    
                    if(moduleIndex == moduleCount)
                    {
                        void* moved = NULL;
                        
                        moved = realloc(modules, sizeof(ModuleStats) * (1 + moduleCount));
                        if(NULL != moved)
                        {
                            modules = (ModuleStats*)moved;
                            moduleCount++;
                            memset(modules + moduleIndex, 0, sizeof(ModuleStats));
                            
                            modules[moduleIndex].mModule = strdup(module);
                            if(NULL == modules[moduleIndex].mModule)
                            {
                                retval = __LINE__;
                                ERROR_REPORT(retval, module, "Unable to duplicate string.");
                            }
                        }
                        else
                        {
                            retval = __LINE__;
                            ERROR_REPORT(retval, inOptions->mProgramName, "Unable to increase module array.");
                        }
                    }
                    
                    if(0 == retval)
                    {
                        unsigned segmentIndex = 0;
                        theModule = (modules + moduleIndex);
                        
                        for(segmentIndex = 0; segmentIndex < theModule->mSegmentCount; segmentIndex++)
                        {
                            if(0 == strcmp(segment, theModule->mSegments[segmentIndex].mSegment))
                            {
                                break;
                            }
                        }
                        
                        if(segmentIndex == theModule->mSegmentCount)
                        {
                            void* moved = NULL;
                            
                            moved = realloc(theModule->mSegments, sizeof(SegmentStats) * (theModule->mSegmentCount + 1));
                            if(NULL != moved)
                            {
                                theModule->mSegments = (SegmentStats*)moved;
                                theModule->mSegmentCount++;
                                memset(theModule->mSegments + segmentIndex, 0, sizeof(SegmentStats));
                                
                                theModule->mSegments[segmentIndex].mClass = segmentClass;
                                theModule->mSegments[segmentIndex].mSegment = strdup(segment);
                                if(NULL == theModule->mSegments[segmentIndex].mSegment)
                                {
                                    retval = __LINE__;
                                    ERROR_REPORT(retval, segment, "Unable to duplicate string.");
                                }
                            }
                            else
                            {
                                retval = __LINE__;
                                ERROR_REPORT(retval, inOptions->mProgramName, "Unable to increase segment array.");
                            }
                        }
                        
                        if(0 == retval)
                        {
                            unsigned objectIndex = 0;
                            theSegment = (theModule->mSegments + segmentIndex);
                            
                            for(objectIndex = 0; objectIndex < theSegment->mObjectCount; objectIndex++)
                            {
                                if(0 == strcmp(object, theSegment->mObjects[objectIndex].mObject))
                                {
                                    break;
                                }
                            }
                            
                            if(objectIndex == theSegment->mObjectCount)
                            {
                                void* moved = NULL;
                                
                                moved = realloc(theSegment->mObjects, sizeof(ObjectStats) * (1 + theSegment->mObjectCount));
                                if(NULL != moved)
                                {
                                    theSegment->mObjects = (ObjectStats*)moved;
                                    theSegment->mObjectCount++;
                                    memset(theSegment->mObjects + objectIndex, 0, sizeof(ObjectStats));
                                    
                                    theSegment->mObjects[objectIndex].mObject = strdup(object);
                                    if(NULL == theSegment->mObjects[objectIndex].mObject)
                                    {
                                        retval = __LINE__;
                                        ERROR_REPORT(retval, object, "Unable to duplicate string.");
                                    }
                                }
                                else
                                {
                                    retval = __LINE__;
                                    ERROR_REPORT(retval, inOptions->mProgramName, "Unable to increase object array.");
                                }
                            }
                            
                            if(0 == retval)
                            {
                                unsigned symbolIndex = 0;
                                theObject = (theSegment->mObjects + objectIndex);
                                
                                for(symbolIndex = 0; symbolIndex < theObject->mSymbolCount; symbolIndex++)
                                {
                                    if(0 == strcmp(symbol, theObject->mSymbols[symbolIndex].mSymbol))
                                    {
                                        break;
                                    }
                                }
                                
                                if(symbolIndex == theObject->mSymbolCount)
                                {
                                    void* moved = NULL;
                                    
                                    moved = realloc(theObject->mSymbols, sizeof(SymbolStats) * (1 + theObject->mSymbolCount));
                                    if(NULL != moved)
                                    {
                                        theObject->mSymbols = (SymbolStats*)moved;
                                        theObject->mSymbolCount++;
                                        allSymbolCount++;
                                        memset(theObject->mSymbols + symbolIndex, 0, sizeof(SymbolStats));
                                        
                                        theObject->mSymbols[symbolIndex].mSymbol = strdup(symbol);
                                        if(NULL == theObject->mSymbols[symbolIndex].mSymbol)
                                        {
                                            retval = __LINE__;
                                            ERROR_REPORT(retval, symbol, "Unable to duplicate string.");
                                        }
                                    }
                                    else
                                    {
                                        retval = __LINE__;
                                        ERROR_REPORT(retval, inOptions->mProgramName, "Unable to increase symbol array.");
                                    }
                                }
                                
                                if(0 == retval)
                                {
                                    theSymbol = (theObject->mSymbols + symbolIndex);

                                    /*
                                    **  Update our various totals.
                                    */
                                    if(additive)
                                    {
                                        if(CODE == segmentClass)
                                        {
                                            overall.mCode += size;
                                            theModule->mSize.mCode += size;
                                        }
                                        else if(DATA == segmentClass)
                                        {
                                            overall.mData += size;
                                            theModule->mSize.mData += size;
                                        }

                                        theSegment->mSize += size;
                                        theObject->mSize += size;
                                        theSymbol->mSize += size;
                                    }
                                    else
                                    {
                                        if(CODE == segmentClass)
                                        {
                                            overall.mCode -= size;
                                            theModule->mSize.mCode -= size;
                                        }
                                        else if(DATA == segmentClass)
                                        {
                                            overall.mData -= size;
                                            theModule->mSize.mData -= size;
                                        }

                                        theSegment->mSize -= size;
                                        theObject->mSize -= size;
                                        theSymbol->mSize -= size;
                                    }
                                }
                            }
                        }
                    }
                }
            }
            else
            {
                retval = __LINE__;
                ERROR_REPORT(retval, inOptions->mInputName, "Unable to scan line data.");
            }
        }
    }

    if(0 == retval && 0 != ferror(inOptions->mInput))
    {
        retval = __LINE__;
        ERROR_REPORT(retval, inOptions->mInputName, "Unable to read file.");
    }

    /*
    **  Next, it is time to perform revisionist history of sorts.
    **  If the negation switch is in play, we perfrom the following
    **      aggressive steps:
    **
    **  For each section, find size changes which have an equal and
    **      opposite change, and set them both to zero.
    **  However, you can only do this if the number of negating changes
    **      is even, as if it is odd, then any one of the many could be
    **      at fault for the actual change.
    **
    **  This orginally exists to make the win32 codesighs reports more
    **      readable/meaningful.
    */
    if(0 == retval && 0 != inOptions->mNegation)
    {
        ObjectStats** objArray = NULL;
        SymbolStats** symArray = NULL;

        /*
        **  Create arrays big enough to hold all symbols.
        **  As well as an array to keep the owning object at the same index.
        **  We will keep the object around as we may need to modify the size.
        */
        objArray = (ObjectStats**)malloc(allSymbolCount * sizeof(ObjectStats*));
        symArray = (SymbolStats**)malloc(allSymbolCount * sizeof(SymbolStats*));
        if(NULL == objArray || NULL == symArray)
        {
            retval = __LINE__;
            ERROR_REPORT(retval, inOptions->mProgramName, "Unable to allocate negation array memory.");
        }
        else
        {
            unsigned arrayCount = 0;
            unsigned arrayLoop = 0;

            /*
            **  Go through and perform the steps on each section/segment.
            */
            for(moduleLoop = 0; moduleLoop < moduleCount; moduleLoop++)
            {
                theModule = modules + moduleLoop;

                for(segmentLoop = 0; segmentLoop < theModule->mSegmentCount; segmentLoop++)
                {
                    theSegment = theModule->mSegments + segmentLoop;

                    /*
                    **  Collect all symbols under this section.
                    **  The symbols are spread out between all the objects,
                    **      so keep track of both independently at the
                    **      same index.
                    */
                    arrayCount = 0;

                    for(objectLoop = 0; objectLoop < theSegment->mObjectCount; objectLoop++)
                    {
                        theObject = theSegment->mObjects + objectLoop;

                        for(symbolLoop = 0; symbolLoop < theObject->mSymbolCount; symbolLoop++)
                        {
                            theSymbol = theObject->mSymbols + symbolLoop;

                            objArray[arrayCount] = theObject;
                            symArray[arrayCount] = theSymbol;
                            arrayCount++;
                        }
                    }

                    /*
                    **  Now that we have a list of symbols, go through each
                    **      and see if there is a chance of negation.
                    */
                    for(arrayLoop = 0; arrayLoop < arrayCount; arrayLoop++)
                    {
                        /*
                        **  If the item is NULL, it was already negated.
                        **  Don't do this for items with a zero size.
                        */
                        if(NULL != symArray[arrayLoop] && 0 != symArray[arrayLoop]->mSize)
                        {
                            unsigned identicalValues = 0;
                            unsigned oppositeValues = 0;
                            unsigned lookLoop = 0;
                            const int lookingFor = symArray[arrayLoop]->mSize;

                            /*
                            **  Count the number of items with this value.
                            **  Count the number of items with the opposite equal value.
                            **  If they are equal, go through and negate all sizes.
                            */
                            for(lookLoop = arrayLoop; lookLoop < arrayCount; lookLoop++)
                            {
                                /*
                                **  Skip negated items.
                                **  Skip zero length items.
                                */
                                if(NULL == symArray[lookLoop] || 0 == symArray[lookLoop]->mSize)
                                {
                                    continue;
                                }

                                if(lookingFor == symArray[lookLoop]->mSize)
                                {
                                    identicalValues++;
                                }
                                else if((-1 * lookingFor) == symArray[lookLoop]->mSize)
                                {
                                    oppositeValues++;
                                }
                            }
                            
                            if(0 != identicalValues && identicalValues == oppositeValues)
                            {
                                unsigned negationLoop = 0;

                                for(negationLoop = arrayLoop; 0 != identicalValues || 0 != oppositeValues; negationLoop++)
                                {
                                    /*
                                    **  Skip negated items.
                                    **  Skip zero length items.
                                    */
                                    if(NULL == symArray[negationLoop] || 0 == symArray[negationLoop]->mSize)
                                    {
                                        continue;
                                    }

                                    /*
                                    **  Negate any size matches.
                                    **  Reflect the change in the object as well.
                                    **  Clear the symbol.
                                    */
                                    if(lookingFor == symArray[negationLoop]->mSize)
                                    {
                                        objArray[negationLoop]->mSize -= lookingFor;
                                        symArray[negationLoop]->mSize = 0;
                                        symArray[negationLoop] = NULL;

                                        identicalValues--;
                                    }
                                    else if((-1 * lookingFor) == symArray[negationLoop]->mSize)
                                    {
                                        objArray[negationLoop]->mSize += lookingFor;
                                        symArray[negationLoop]->mSize = 0;
                                        symArray[negationLoop] = NULL;

                                        oppositeValues--;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        CLEANUP(objArray);
        CLEANUP(symArray);
    }


    /*
    **  If all went well, time to report.
    */
    if(0 == retval)
    {
        /*
        **  Loop through our data once more, so that the symbols can
        **      propigate their changes upwards in a positive/negative
        **      fashion.
        **  This will help give the composite change more meaning.
        */
        for(moduleLoop = 0; moduleLoop < moduleCount; moduleLoop++)
        {
            theModule = modules + moduleLoop;
            
            /*
            **  Skip if there is zero drift, or no net change.
            */
            if(0 == inOptions->mZeroDrift && 0 == (theModule->mSize.mCode + theModule->mSize.mData))
            {
                continue;
            }

            for(segmentLoop = 0; segmentLoop < theModule->mSegmentCount; segmentLoop++)
            {
                theSegment = theModule->mSegments + segmentLoop;
                
                /*
                **  Skip if there is zero drift, or no net change.
                */
                if(0 == inOptions->mZeroDrift && 0 == theSegment->mSize)
                {
                    continue;
                }
                
                for(objectLoop = 0; objectLoop < theSegment->mObjectCount; objectLoop++)
                {
                    theObject = theSegment->mObjects + objectLoop;
                    
                    /*
                    **  Skip if there is zero drift, or no net change.
                    */
                    if(0 == inOptions->mZeroDrift && 0 == theObject->mSize)
                    {
                        continue;
                    }

                    for(symbolLoop = 0; symbolLoop < theObject->mSymbolCount; symbolLoop++)
                    {
                        theSymbol = theObject->mSymbols + symbolLoop;
                        
                        /*
                        **  Propagate the composition all the way to the top.
                        **  Sizes of zero change are skipped.
                        */
                        if(0 < theSymbol->mSize)
                        {
                            theObject->mComposition.mPositive += theSymbol->mSize;
                            theSegment->mComposition.mPositive += theSymbol->mSize;
                            if(CODE == theSegment->mClass)
                            {
                                overall.mCodeComposition.mPositive += theSymbol->mSize;
                                theModule->mSize.mCodeComposition.mPositive += theSymbol->mSize;
                            }
                            else if(DATA == theSegment->mClass)
                            {
                                overall.mDataComposition.mPositive += theSymbol->mSize;
                                theModule->mSize.mDataComposition.mPositive += theSymbol->mSize;
                            }
                        }
                        else if(0 > theSymbol->mSize)
                        {
                            theObject->mComposition.mNegative += theSymbol->mSize;
                            theSegment->mComposition.mNegative += theSymbol->mSize;
                            if(CODE == theSegment->mClass)
                            {
                                overall.mCodeComposition.mNegative += theSymbol->mSize;
                                theModule->mSize.mCodeComposition.mNegative += theSymbol->mSize;
                            }
                            else if(DATA == theSegment->mClass)
                            {
                                overall.mDataComposition.mNegative += theSymbol->mSize;
                                theModule->mSize.mDataComposition.mNegative += theSymbol->mSize;
                            }
                        }
                    }
                }
            }
        }


        if(inOptions->mSummaryOnly)
        {
            fprintf(inOptions->mOutput, "%+d (%+d/%+d)\n", overall.mCode + overall.mData, overall.mCodeComposition.mPositive + overall.mDataComposition.mPositive, overall.mCodeComposition.mNegative + overall.mDataComposition.mNegative);
        }
        else
        {
            fprintf(inOptions->mOutput, "Overall Change in Size\n");
            fprintf(inOptions->mOutput, "\tTotal:\t%+11d (%+d/%+d)\n", overall.mCode + overall.mData, overall.mCodeComposition.mPositive + overall.mDataComposition.mPositive, overall.mCodeComposition.mNegative + overall.mDataComposition.mNegative);
            fprintf(inOptions->mOutput, "\tCode:\t%+11d (%+d/%+d)\n", overall.mCode, overall.mCodeComposition.mPositive, overall.mCodeComposition.mNegative);
            fprintf(inOptions->mOutput, "\tData:\t%+11d (%+d/%+d)\n", overall.mData, overall.mDataComposition.mPositive, overall.mDataComposition.mNegative);
        }

        /*
        **  Check what else we should output.
        */
        if(0 == inOptions->mSummaryOnly && NULL != modules && moduleCount)
        {
            const char* segmentType = NULL;

            /*
            **  We're going to sort everything.
            */
            qsort(modules, moduleCount, sizeof(ModuleStats), moduleCompare);
            for(moduleLoop = 0; moduleLoop < moduleCount; moduleLoop++)
            {
                theModule = modules + moduleLoop;

                qsort(theModule->mSegments, theModule->mSegmentCount, sizeof(SegmentStats), segmentCompare);

                for(segmentLoop = 0; segmentLoop < theModule->mSegmentCount; segmentLoop++)
                {
                    theSegment = theModule->mSegments + segmentLoop;

                    qsort(theSegment->mObjects, theSegment->mObjectCount, sizeof(ObjectStats), objectCompare);

                    for(objectLoop = 0; objectLoop < theSegment->mObjectCount; objectLoop++)
                    {
                        theObject = theSegment->mObjects + objectLoop;

                        qsort(theObject->mSymbols, theObject->mSymbolCount, sizeof(SymbolStats), symbolCompare);
                    }
                }
            }

            /*
            **  Loop through for output.
            */
            for(moduleLoop = 0; moduleLoop < moduleCount; moduleLoop++)
            {
                theModule = modules + moduleLoop;

                /*
                **  Skip if there is zero drift, or no net change.
                */
                if(0 == inOptions->mZeroDrift && 0 == (theModule->mSize.mCode + theModule->mSize.mData))
                {
                    continue;
                }

                fprintf(inOptions->mOutput, "\n");
                fprintf(inOptions->mOutput, "%s\n", theModule->mModule);
                fprintf(inOptions->mOutput, "\tTotal:\t%+11d (%+d/%+d)\n", theModule->mSize.mCode + theModule->mSize.mData, theModule->mSize.mCodeComposition.mPositive + theModule->mSize.mDataComposition.mPositive, theModule->mSize.mCodeComposition.mNegative + theModule->mSize.mDataComposition.mNegative);
                fprintf(inOptions->mOutput, "\tCode:\t%+11d (%+d/%+d)\n", theModule->mSize.mCode, theModule->mSize.mCodeComposition.mPositive, theModule->mSize.mCodeComposition.mNegative);
                fprintf(inOptions->mOutput, "\tData:\t%+11d (%+d/%+d)\n", theModule->mSize.mData, theModule->mSize.mDataComposition.mPositive, theModule->mSize.mDataComposition.mNegative);

                for(segmentLoop = 0; segmentLoop < theModule->mSegmentCount; segmentLoop++)
                {
                    theSegment = theModule->mSegments + segmentLoop;

                    /*
                    **  Skip if there is zero drift, or no net change.
                    */
                    if(0 == inOptions->mZeroDrift && 0 == theSegment->mSize)
                    {
                        continue;
                    }

                    if(CODE == theSegment->mClass)
                    {
                        segmentType = "CODE";
                    }
                    else if(DATA == theSegment->mClass)
                    {
                        segmentType = "DATA";
                    }

                    fprintf(inOptions->mOutput, "\t%+11d (%+d/%+d)\t%s (%s)\n", theSegment->mSize, theSegment->mComposition.mPositive, theSegment->mComposition.mNegative, theSegment->mSegment, segmentType);

                    for(objectLoop = 0; objectLoop < theSegment->mObjectCount; objectLoop++)
                    {
                        theObject = theSegment->mObjects + objectLoop;

                        /*
                        **  Skip if there is zero drift, or no net change.
                        */
                        if(0 == inOptions->mZeroDrift && 0 == theObject->mSize)
                        {
                            continue;
                        }

                        fprintf(inOptions->mOutput, "\t\t%+11d (%+d/%+d)\t%s\n", theObject->mSize, theObject->mComposition.mPositive, theObject->mComposition.mNegative, theObject->mObject);
                        
                        for(symbolLoop = 0; symbolLoop < theObject->mSymbolCount; symbolLoop++)
                        {
                            theSymbol = theObject->mSymbols + symbolLoop;

                            /*
                            **  Skip if there is zero drift, or no net change.
                            */
                            if(0 == inOptions->mZeroDrift && 0 == theSymbol->mSize)
                            {
                                continue;
                            }

                            fprintf(inOptions->mOutput, "\t\t\t%+11d\t%s\n", theSymbol->mSize, theSymbol->mSymbol);
                        }
                    }
                }
            }
        }
    }

    /*
    **  Cleanup time.
    */
    for(moduleLoop = 0; moduleLoop < moduleCount; moduleLoop++)
    {
        theModule = modules + moduleLoop;
        
        for(segmentLoop = 0; segmentLoop < theModule->mSegmentCount; segmentLoop++)
        {
            theSegment = theModule->mSegments + segmentLoop;
            
            for(objectLoop = 0; objectLoop < theSegment->mObjectCount; objectLoop++)
            {
                theObject = theSegment->mObjects + objectLoop;
                
                for(symbolLoop = 0; symbolLoop < theObject->mSymbolCount; symbolLoop++)
                {
                    theSymbol = theObject->mSymbols + symbolLoop;
                    
                    CLEANUP(theSymbol->mSymbol);
                }
                
                CLEANUP(theObject->mSymbols);
                CLEANUP(theObject->mObject);
            }
            
            CLEANUP(theSegment->mObjects);
            CLEANUP(theSegment->mSegment);
        }
        
        CLEANUP(theModule->mSegments);
        CLEANUP(theModule->mModule);
    }
    CLEANUP(modules);
    
    return retval;
}


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
    outOptions->mInput = stdin;
    outOptions->mInputName = strdup("stdin");
    outOptions->mOutput = stdout;
    outOptions->mOutputName = strdup("stdout");

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
                if(NULL != outOptions->mInput && stdin != outOptions->mInput)
                {
                    fclose(outOptions->mInput);
                    outOptions->mInput = NULL;
                }

                outOptions->mInput = fopen(current->mValue, "r");
                if(NULL == outOptions->mInput)
                {
                    retval = __LINE__;
                    ERROR_REPORT(retval, current->mValue, "Unable to open input file.");
                }
                else
                {
                    outOptions->mInputName = strdup(current->mValue);
                    if(NULL == outOptions->mInputName)
                    {
                        retval = __LINE__;
                        ERROR_REPORT(retval, current->mValue, "Unable to strdup.");
                    }
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
            else if(current == &gSummarySwitch)
            {
                outOptions->mSummaryOnly = __LINE__;
            }
            else if(current == &gZeroDriftSwitch)
            {
                outOptions->mZeroDrift = __LINE__;
            }
            else if(current == &gNegationSwitch)
            {
                outOptions->mNegation = __LINE__;
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


void cleanOptions(Options* inOptions)
/*
**  Clean up any open handles.
*/
{
    CLEANUP(inOptions->mInputName);
    if(NULL != inOptions->mInput && stdin != inOptions->mInput)
    {
        fclose(inOptions->mInput);
    }
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

    printf("This tool takes the diff of two sorted tsv files to form a summary report\n");
    printf("of code and data size changes which is hoped to be human readable.\n");
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
        retval = difftool(&options);
    }

    cleanOptions(&options);
    return retval;
}

