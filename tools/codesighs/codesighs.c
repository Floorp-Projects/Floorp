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
 * The Original Code is codesighs.c code, released
 * Oct 3, 2002.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *    Garrett Arch Blythe, 03-October-2002
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
**  mModules        Output module by module information.
**  mTotalOnly      Only output one number, the total.
*/
{
    const char* mProgramName;
    FILE* mInput;
    char* mInputName;
    FILE* mOutput;
    char* mOutputName;
    int mHelp;
    int mModules;
    int mTotalOnly;
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
static Switch gModuleSwitch = {"--modules", "-m", 0, NULL, "Output individual module numbers as well."};
static Switch gTotalSwitch = {"--totalonly", "-t", 0, NULL, "Output only one number." DESC_NEWLINE "The total overall size." DESC_NEWLINE "Overrides other output options."};

static Switch* gSwitches[] = {
        &gInputSwitch,
        &gOutputSwitch,
        &gModuleSwitch,
        &gTotalSwitch,
        &gHelpSwitch
};


typedef struct __struct_SizeStats
/*
**  Track totals.
**
**  mData       Size of data.
**  mCode       Size of code.
*/
{
    unsigned long mData;
    unsigned long mCode;
}
SizeStats;


typedef struct __struct_ModuleStats
/*
**  Track module level information.
**
**  mModule     Module name.
**  mSize       Size of module.
*/
{
    char* mModule;
    SizeStats mSize;
}
ModuleStats;

typedef enum __enum_SegmentClass
{
        CODE,
        DATA
}
SegmentClass;


static int moduleCompare(const void* in1, const void* in2)
/*
**  qsort helper function.
*/
{
    int retval = 0;

    const ModuleStats* one = (const ModuleStats*)in1;
    const ModuleStats* two = (const ModuleStats*)in2;
    unsigned long oneSize = one->mSize.mCode + one->mSize.mData;
    unsigned long twoSize = two->mSize.mCode + two->mSize.mData;

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


int codesighs(Options* inOptions)
/*
**  Output a simplistic report based on our options.
*/
{
    int retval = 0;
    char lineBuffer[0x500];
    int scanRes = 0;
    unsigned long size;
    char segClass[0x10];
    char scope[0x10];
    char module[0x100];
    char segment[0x40];
    char object[0x100];
    char symbol[0x200];
    SizeStats overall;
    ModuleStats* modules = NULL;
    unsigned moduleCount = 0;

    memset(&overall, 0, sizeof(overall));

    /*
    **  Read the file line by line, regardless of number of fields.
    **  We assume tab seperated value formatting, at least 7 lead values:
    **      size class scope module segment object symbol ....
    */
    while(0 == retval && NULL != fgets(lineBuffer, sizeof(lineBuffer), inOptions->mInput))
    {
        scanRes = sscanf(lineBuffer,
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
            SegmentClass segmentClass = CODE;

            if(0 == strcmp(segClass, "DATA"))
            {
                segmentClass = DATA;
            }
            else if(0 == strcmp(segClass, "CODE"))
            {
                segmentClass = CODE;
            }
            else
            {
                retval = __LINE__;
                ERROR_REPORT(retval, segClass, "Unable to determine segment class.");
            }

            if(0 == retval)
            {
                /*
                **  Update overall totals.
                */
                if(CODE == segmentClass)
                {
                    overall.mCode += size;
                }
                else if(DATA == segmentClass)
                {
                    overall.mData += size;
                }

                /*
                **  See what else we should be tracking.
                */
                if(0 == inOptions->mTotalOnly)
                {
                    if(inOptions->mModules)
                    {
                        unsigned index = 0;
                        
                        /*
                        **  Find the module to modify.
                        */
                        for(index = 0; index < moduleCount; index++)
                        {
                            if(0 == strcmp(modules[index].mModule, module))
                            {
                                break;
                            }
                        }
                        
                        /*
                        **  If the index is the same as the count, we need to
                        **      add a new module.
                        */
                        if(index == moduleCount)
                        {
                            void* moved = NULL;
                            
                            moved = realloc(modules, sizeof(ModuleStats) * (moduleCount + 1));
                            if(NULL != moved)
                            {
                                modules = (ModuleStats*)moved;
                                moduleCount++;
                                
                                memset(modules + index, 0, sizeof(ModuleStats));
                                modules[index].mModule = strdup(module);
                                if(NULL == modules[index].mModule)
                                {
                                    retval = __LINE__;
                                    ERROR_REPORT(retval, module, "Unable to duplicate string.");
                                }
                            }
                            else
                            {
                                retval = __LINE__;
                                ERROR_REPORT(retval, inOptions->mProgramName, "Unable to allocate module memory.");
                            }
                        }
                        
                        if(0 == retval)
                        {
                            if(CODE == segmentClass)
                            {
                                modules[index].mSize.mCode += size;
                            }
                            else if(DATA == segmentClass)
                            {
                                modules[index].mSize.mData += size;
                            }
                        }
                    }
                }
            }
        }
        else
        {
            retval = __LINE__;
            ERROR_REPORT(retval, inOptions->mInputName, "Problem extracting values from file.");
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
            fprintf(inOptions->mOutput, "%u\n", overall.mCode + overall.mData);
        }
        else
        {
            fprintf(inOptions->mOutput, "Overall Size\n");
            fprintf(inOptions->mOutput, "\tTotal:\t%10u\n", overall.mCode + overall.mData);
            fprintf(inOptions->mOutput, "\tCode:\t%10u\n", overall.mCode);
            fprintf(inOptions->mOutput, "\tData:\t%10u\n", overall.mData);
        }

        /*
        **  Check options to see what else we should output.
        */
        if(inOptions->mModules && moduleCount)
        {
            unsigned loop = 0;

            /*
            **  Sort the modules by their size.
            */
            qsort(modules, (size_t)moduleCount, sizeof(ModuleStats), moduleCompare);

            /*
            **  Output each one.
            **  Might as well clean up while we go too.
            */
            for(loop = 0; loop < moduleCount; loop++)
            {
                fprintf(inOptions->mOutput, "\n");
                fprintf(inOptions->mOutput, "%s\n", modules[loop].mModule);
                fprintf(inOptions->mOutput, "\tTotal:\t%10u\n", modules[loop].mSize.mCode + modules[loop].mSize.mData);
                fprintf(inOptions->mOutput, "\tCode:\t%10u\n", modules[loop].mSize.mCode);
                fprintf(inOptions->mOutput, "\tData:\t%10u\n", modules[loop].mSize.mData);

                CLEANUP(modules[loop].mModule);
            }

            /*
            **  Done with modules.
            */
            CLEANUP(modules);
            moduleCount = 0;
        }
    }

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
            else if(current == &gModuleSwitch)
            {
                outOptions->mModules = __LINE__;
            }
            else if(current == &gTotalSwitch)
            {
                outOptions->mTotalOnly = __LINE__;
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
        retval = codesighs(&options);
    }

    cleanOptions(&options);
    return retval;
}

