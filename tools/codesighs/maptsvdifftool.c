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
**  mHelp           Wether or not help should be shown.
**  mTotalOnly      Only output a signle digit.
**  mZeroDrift      Output zero drift data.
*/
{
    const char* mProgramName;
    FILE* mInput;
    char* mInputName;
    FILE* mOutput;
    char* mOutputName;
    int mHelp;
    int mTotalOnly;
    int mZeroDrift;
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
static Switch gTotalSwitch = {"--totalonly", "-t", 0, NULL, "Only output a single number." DESC_NEWLINE "The cumulative size change." DESC_NEWLINE "Overrides all other output options."};
static Switch gZeroDriftSwitch = {"--zerodrift", "-z", 0, NULL, "Output zero drift data."  DESC_NEWLINE "Zero drift data includes all changes, even if they cancel out."};
static Switch gHelpSwitch = {"--help", "-h", 0, NULL, "Information on usage."};

static Switch* gSwitches[] = {
        &gInputSwitch,
        &gOutputSwitch,
        &gTotalSwitch,
        &gZeroDriftSwitch,
        &gHelpSwitch
};


typedef struct __struct_SizeStats
/*
**  Keep track of sizes.
**  Use signed integers so that negatives are valid, in which case we shrunk.
*/
{
    int mCode;
    int mData;
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

    return retval;
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

    memset(&overall, 0, sizeof(overall));

    /*
    **  Read the entire diff file.
    **  We're only interested in lines beginning with < or >
    */
    while(0 == retval && NULL != fgets(lineBuffer, sizeof(lineBuffer), inOptions->mInput))
    {
        if(('<' == lineBuffer[0] || '>' == lineBuffer[0]) && ' ' == lineBuffer[1])
        {
            int additive = 0;
            char* theLine = &lineBuffer[2];
            int scanRes = 0;
            int size;
            char segClass[0x10];
            char scope[0x10];
            char module[0x100];
            char segment[0x40];
            char object[0x100];
            char symbol[0x200];

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
            scanRes = sscanf(theLine,
                "%x\t%s\t%s\t%s\t%s\t%s\t%s",
                &size,
                segClass,
                scope,
                module,
                segment,
                object,
                symbol);

            if(7 == scanRes)
            {
                SegmentClass segmentClass = DATA;

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
                    /*
                    **  Update our overall totals.
                    */
                    if(CODE == segmentClass)
                    {
                        if(additive)
                        {
                            overall.mCode += size;
                        }
                        else
                        {
                            overall.mCode -= size;
                        }
                    }
                    else
                    {
                        if(additive)
                        {
                            overall.mData += size;
                        }
                        else
                        {
                            overall.mData -= size;
                        }
                    }

                    /*
                    **  Anything else to track?
                    */
                    if(0 == inOptions->mTotalOnly)
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
                            ModuleStats* theModule = (modules + moduleIndex);

                            if(CODE == segmentClass)
                            {
                                if(additive)
                                {
                                    modules[moduleIndex].mSize.mCode += size;
                                }
                                else
                                {
                                    modules[moduleIndex].mSize.mCode -= size;
                                }
                            }
                            else
                            {
                                if(additive)
                                {
                                    modules[moduleIndex].mSize.mData += size;
                                }
                                else
                                {
                                    modules[moduleIndex].mSize.mData -= size;
                                }
                            }

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
                                SegmentStats* theSegment = (theModule->mSegments + segmentIndex);

                                if(additive)
                                {
                                    theSegment->mSize += size;
                                }
                                else
                                {
                                    theSegment->mSize -= size;
                                }

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
                                    ObjectStats* theObject = (theSegment->mObjects + objectIndex);

                                    if(additive)
                                    {
                                        theObject->mSize += size;
                                    }
                                    else
                                    {
                                        theObject->mSize -= size;
                                    }

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
                                        SymbolStats* theSymbol = (theObject->mSymbols + symbolIndex);

                                        if(additive)
                                        {
                                            theSymbol->mSize += size;
                                        }
                                        else
                                        {
                                            theSymbol->mSize -= size;
                                        }
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
    **  If all went well, time to report.
    */
    if(0 == retval)
    {
        if(inOptions->mTotalOnly)
        {
            fprintf(inOptions->mOutput, "%+d\n", overall.mCode + overall.mData);
        }
        else
        {
            fprintf(inOptions->mOutput, "Overall Change in Size\n");
            fprintf(inOptions->mOutput, "\tTotal:\t%+11d\n", overall.mCode + overall.mData);
            fprintf(inOptions->mOutput, "\tCode:\t%+11d\n", overall.mCode);
            fprintf(inOptions->mOutput, "\tData:\t%+11d\n", overall.mData);
        }

        /*
        **  Check what else we should output.
        */
        if(NULL != modules && moduleCount)
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
                if(0 == inOptions->mZeroDrift && 0 == (theModule->mSize.mCode + theModule->mSize.mCode ))
                {
                    continue;
                }

                fprintf(inOptions->mOutput, "\n");
                fprintf(inOptions->mOutput, "%s\n", theModule->mModule);
                fprintf(inOptions->mOutput, "\tTotal:\t%+11d\n", theModule->mSize.mCode + theModule->mSize.mData);
                fprintf(inOptions->mOutput, "\tCode:\t%+11d\n", theModule->mSize.mCode);
                fprintf(inOptions->mOutput, "\tData:\t%+11d\n", theModule->mSize.mData);

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

                    fprintf(inOptions->mOutput, "\t%+11d\t%s (%s)\n", theSegment->mSize, theSegment->mSegment, segmentType);

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

                        fprintf(inOptions->mOutput, "\t\t%+11d\t%s\n", theObject->mSize, theObject->mObject);
                        
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
            else if(current == &gTotalSwitch)
            {
                outOptions->mTotalOnly = __LINE__;
            }
            else if(current == &gZeroDriftSwitch)
            {
                outOptions->mZeroDrift = __LINE__;
            }
            else
            {
                retval = __LINE__;
                ERROR_REPORT(retval, current->mLongName, "No hanlder for command line switch.");
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

