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
 * The Original Code is msmap2tsv.c code, released
 * Oct 3, 2002.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 2002 Netscape Communications Corporation. All
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

#include "msmap.h"

#if defined(_WIN32)
#include <windows.h>
#include <imagehlp.h>

#define F_DEMANGLE 1
#define DEMANGLE_STATE_NORMAL 0
#define DEMANGLE_STATE_QDECODE 1
#define DEMANGLE_STATE_PROLOGUE_1 2
#define DEMANGLE_STATE_HAVE_TYPE 3
#define DEMANGLE_STATE_DEC_LENGTH 4
#define DEMANGLE_STATE_HEX_LENGTH 5
#define DEMANGLE_STATE_PROLOGUE_SECONDARY 6
#define DEMANGLE_STATE_DOLLAR_1 7
#define DEMANGLE_STATE_DOLLAR_2 8
#define DEMANGLE_STATE_START 9
#define DEMANGLE_STATE_STOP 10

#else
#define F_DEMANGLE 0
#endif /* WIN32 */


#define ERROR_REPORT(num, val, msg)   fprintf(stderr, "error(%d):\t\"%s\"\t%s\n", (num), (val), (msg));
#define CLEANUP(ptr)    do { if(NULL != ptr) { free(ptr); ptr = NULL; } } while(0)


typedef struct __struct_Options
/*
**  Options to control how we perform.
**
**  mProgramName        Used in help text.
**  mInput              File to read for input.
**                      Default is stdin.
**  mInputName          Name of the file.
**  mOutput             Output file, append.
**                      Default is stdout.
**  mOutputName         Name of the file.
**  mHelp               Wether or not help should be shown.
**  mAddress            Wether or not to output addresses.
**  mMatchModules       Array of strings which the module name should match.
**  mMatchModuleCount   Number of items in array.
*/
{
    const char* mProgramName;
    FILE* mInput;
    char* mInputName;
    FILE* mOutput;
    char* mOutputName;
    int mHelp;
    int mAddresses;
    char** mMatchModules;
    unsigned mMatchModuleCount;
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
static Switch gAddressesSwitch = {"--addresses", "-a", 0, NULL, "Output segment addresses." DESC_NEWLINE "Helps reveal symbol ordering." DESC_NEWLINE "Lack of simplifies size diffing."};
static Switch gMatchModuleSwitch = {"--match-module", "-mm", 1, NULL, "Specify a valid module name." DESC_NEWLINE "Multiple specifications allowed." DESC_NEWLINE "If a module name does not match one of the names specified then no output will occur."};

static Switch* gSwitches[] = {
        &gInputSwitch,
        &gOutputSwitch,
        &gAddressesSwitch,
        &gMatchModuleSwitch,
        &gHelpSwitch
};


typedef struct __struct_MSMap_ReadState
/*
**  Keep track of what state we are while reading input.
**  This gives the input context in which we absorb the datum.
*/
{
    int mHasModule;

    int mHasTimestamp;

    int mHasPreferredLoadAddress;

    int mHasSegmentData;
    int mSegmentDataSkippedLine;

    int mHasPublicSymbolData;
    int mHasPublicSymbolDataSkippedLines;

    int mHasEntryPoint;

    int mFoundStaticSymbols;
}
MSMap_ReadState;


char* skipWhite(char* inScan)
/*
**  Skip whitespace.
*/
{
    char* retval = inScan;

    while(isspace(*retval))
    {
        retval++;
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


char* lastWord(char* inString)
/*
**  Finds and returns the last word in a string.
**  It is assumed no whitespace is at the end of the string.
*/
{
    int mod = 0;
    int len = strlen(inString);

    while(len)
    {
        len--;
        if(isspace(*(inString + len)))
        {
            mod = 1;
            break;
        }
    }

    return inString + len + mod;
}


char* symdup(const char* inSymbol)
/*
**  Attempts to demangle the symbol if appropriate.
**  Otherwise acts like strdup.
*/
{
    char* retval = NULL;

#if F_DEMANGLE
    {
        int isImport = 0;

        if(0 == strncmp("__imp_", inSymbol, 6))
        {
            isImport = __LINE__;
            inSymbol += 6;
        }

        if('?' == inSymbol[0])
        {
            char demangleBuf[0x200];
            DWORD demangleRes = 0;
            
            demangleRes = UnDecorateSymbolName(inSymbol, demangleBuf, sizeof(demangleBuf), UNDNAME_COMPLETE);
            if(0 != demangleRes)
            {
                if (strcmp(demangleBuf, "`string'") == 0)
                {
                    
                    /* attempt manual demangling of string prefix.. */

                    /* first make sure we have enough space for the
                       updated string - the demangled string will
                       always be shorter than strlen(inSymbol) and the
                       prologue will always be longer than the
                       "string: " that we tack on the front of the string
                    */
                    char *curresult = retval = malloc(strlen(inSymbol) + 11);
                    const char *curchar = inSymbol;
                    
                    int state = DEMANGLE_STATE_START;

                    /* the hex state is for stuff like ?$EA which
                       really means hex value 0x40 */
                    char hex_state = 0;
                    char string_is_unicode = 0;

                    /* sometimes we get a null-termination before the
                       final @ sign - in that case, remember that
                       we've seen the whole string */
                    int have_null_char = 0;

                    /* stick our user-readable prefix on */
                    strcpy(curresult, "string: \"");
                    curresult += 9;
                    
                    while (*curchar) {
                        
                        // process current state
                        switch (state) {

                            /* the Prologue states are divided up so
                               that someday we can try to decode
                               the random letters in between the '@'
                               signs. Also, some strings only have 2
                               prologue '@' signs, so we have to
                               figure out how to distinguish between
                               them at some point. */
                        case DEMANGLE_STATE_START:
                            if (*curchar == '@')
                                state = DEMANGLE_STATE_PROLOGUE_1;
                            /* ignore all other states */
                            break;

                        case DEMANGLE_STATE_PROLOGUE_1:
                            switch (*curchar) {
                            case '0':
                                string_is_unicode=0;
                                state = DEMANGLE_STATE_HAVE_TYPE;
                                break;
                            case '1':
                                string_is_unicode=1;
                                state = DEMANGLE_STATE_HAVE_TYPE;
                                break;

                                /* ignore all other characters */
                            }
                            break;

                        case DEMANGLE_STATE_HAVE_TYPE:
                            if (*curchar >= '0' && *curchar <= '9') {
                                state = DEMANGLE_STATE_DEC_LENGTH;
                            } else if (*curchar >= 'A' && *curchar <= 'Z') {
                                state = DEMANGLE_STATE_HEX_LENGTH;
                            }
                        case DEMANGLE_STATE_DEC_LENGTH:
                            /* decimal lengths don't have the 2nd
                               field
                            */
                            if (*curchar == '@')
                                state = DEMANGLE_STATE_NORMAL;
                            break;
                            
                        case DEMANGLE_STATE_HEX_LENGTH:
                            /* hex lengths have a 2nd field
                               (though I have no idea what it is for)
                            */
                            if (*curchar == '@')
                                state = DEMANGLE_STATE_PROLOGUE_SECONDARY;
                            break;

                        case DEMANGLE_STATE_PROLOGUE_SECONDARY:
                            if (*curchar == '@')
                                state = DEMANGLE_STATE_NORMAL;
                            break;
                        
                        case DEMANGLE_STATE_NORMAL:
                            switch (*curchar) {
                            case '?':
                                state = DEMANGLE_STATE_QDECODE;
                                break;
                            case '@':
                                state = DEMANGLE_STATE_STOP;
                                break;
                            default:
                                *curresult++ = *curchar;
                                state = DEMANGLE_STATE_NORMAL;
                                break;
                            }
                            break;

                            /* found a '?' */
                        case DEMANGLE_STATE_QDECODE:
                            state = DEMANGLE_STATE_NORMAL;

                            /* there are certain shortcuts, like
                               "?3" means ":"
                            */
                            switch (*curchar) {
                            case '1':
                                *curresult++ = '/';
                                break;
                            case '2':
                                *curresult++ = '\\';
                                break;
                            case '3':
                                *curresult++ = ':';
                                break;
                            case '4':
                                *curresult++ = '.';
                                break;
                            case '5':
                                *curresult++ = ' ';
                                break;
                            case '6':
                                *curresult++ = '\\';
                                *curresult++ = 'n';
                                break;
                            case '8':
                                *curresult++ = '\'';
                                break;
                            case '9':
                                *curresult++ = '-';
                                break;

                                /* any other arbitrary ASCII value can
                                   be stored by prefixing it with ?$
                                */
                            case '$':
                                state = DEMANGLE_STATE_DOLLAR_1;
                            }
                            break;
                            
                        case DEMANGLE_STATE_DOLLAR_1:
                            /* first digit of ?$ notation. All digits
                               are hex, represented starting with the
                               capital leter 'A' such that 'A' means 0x0,
                               'B' means 0x1, 'K' means 0xA
                            */
                            hex_state = (*curchar - 'A') * 0x10;
                            state = DEMANGLE_STATE_DOLLAR_2;
                            break;

                        case DEMANGLE_STATE_DOLLAR_2:
                            /* same mechanism as above */
                            hex_state += (*curchar - 'A');
                            if (hex_state) {
                                *curresult++ = hex_state;
                                have_null_char = 0;
                            }
                            else {
                                have_null_char = 1;
                            }
                            
                            state = DEMANGLE_STATE_NORMAL;
                            break;

                        case DEMANGLE_STATE_STOP:
                            break;
                        }

                        curchar++;
                    }
                    
                    /* add the appropriate termination depending
                       if we completed the string or not */
                    if (!have_null_char)
                        strcpy(curresult, "...\"");
                    else
                        strcpy(curresult, "\"");
                } else {
                    retval = strdup(demangleBuf);
                }
            }
            else
            {
                /*
                ** fall back to normal.
                */
                retval = strdup(inSymbol);
            }
        }
        else if('_' == inSymbol[0])
        {
            retval = strdup(inSymbol + 1);
        }
        else
        {
            retval = strdup(inSymbol);
        }

        /*
        **  May need to rewrite the symbol if an import.
        */
        if(NULL != retval && isImport)
        {
            const char importPrefix[] = "__declspec(dllimport) ";
            char importBuf[0x200];
            int printRes = 0;

            printRes = _snprintf(importBuf, sizeof(importBuf), "%s%s", importPrefix, retval);
            free(retval);
            retval = NULL;

            if(printRes > 0)
            {
                retval = strdup(importBuf);
            }
        }
    }
#else /* F_DEMANGLE */
    retval = strdup(inSymbol);
#endif  /* F_DEMANGLE */

    return retval;
}


int readmap(Options* inOptions, MSMap_Module* inModule)
/*
**  Read the input line by line, adding it to the module.
*/
{
    int retval = 0;
    char lineBuffer[0x400];
    char* current = NULL;
    MSMap_ReadState fsm;
    int len = 0;
    int forceContinue = 0;
    
    memset(&fsm, 0, sizeof(fsm));
    
    /*
    **  Read the map file line by line.
    **  We keep a simple state machine to determine what we're looking at.
    */
    while(0 == retval && NULL != fgets(lineBuffer, sizeof(lineBuffer), inOptions->mInput))
    {
        if(forceContinue)
        {
            /*
            **  Used to skip anticipated blank lines.
            */
            forceContinue--;
            continue;
        }

        current = skipWhite(lineBuffer);
        trimWhite(current);
        
        len = strlen(current);
        
        if(fsm.mHasModule)
        {
            if(fsm.mHasTimestamp)
            {
                if(fsm.mHasPreferredLoadAddress)
                {
                    if(fsm.mHasSegmentData)
                    {
                        if(fsm.mHasPublicSymbolData)
                        {
                            if(fsm.mHasEntryPoint)
                            {
                                if(fsm.mFoundStaticSymbols)
                                {
                                    /*
                                    **  A blank line means we've reached the end of all static symbols.
                                    */
                                    if(len)
                                    {
                                       /*
                                        **  We're adding a new symbol.
                                        **  Make sure we have room for it.
                                        */
                                        if(inModule->mSymbolCapacity == inModule->mSymbolCount)
                                        {
                                            void* moved = NULL;
                                            
                                            moved = realloc(inModule->mSymbols, sizeof(MSMap_Symbol) * (inModule->mSymbolCapacity + MSMAP_SYMBOL_GROWBY));
                                            if(NULL != moved)
                                            {
                                                inModule->mSymbolCapacity += MSMAP_SYMBOL_GROWBY;
                                                inModule->mSymbols = (MSMap_Symbol*)moved;
                                            }
                                            else
                                            {
                                                retval = __LINE__;
                                                ERROR_REPORT(retval, inModule->mModule, "Unable to grow symbols.");
                                            }
                                        }
                                        
                                        if(0 == retval && inModule->mSymbolCapacity > inModule->mSymbolCount)
                                        {
                                            MSMap_Symbol* theSymbol = NULL;
                                            unsigned index = 0;
                                            int scanRes = 0;
                                            char symbolBuf[0x200];
                                            
                                            index = inModule->mSymbolCount;
                                            inModule->mSymbolCount++;
                                            theSymbol = (inModule->mSymbols + index);
                                            
                                            memset(theSymbol, 0, sizeof(MSMap_Symbol));
                                            theSymbol->mScope = STATIC;
                                            
                                            scanRes = sscanf(current, "%x:%x %s %x", (unsigned*)&(theSymbol->mPrefix), (unsigned*)&(theSymbol->mOffset), symbolBuf, (unsigned*)&(theSymbol->mRVABase));
                                            if(4 == scanRes)
                                            {
                                                theSymbol->mSymbol = symdup(symbolBuf);

                                                if(0 == retval)
                                                {
                                                    if(NULL != theSymbol->mSymbol)
                                                    {
                                                        char *last = lastWord(current);
                                                        
                                                        theSymbol->mObject = strdup(last);
                                                        if(NULL == theSymbol->mObject)
                                                        {
                                                            retval = __LINE__;
                                                            ERROR_REPORT(retval, last, "Unable to copy object name.");
                                                        }
                                                    }
                                                    else
                                                    {
                                                        retval = __LINE__;
                                                        ERROR_REPORT(retval, symbolBuf, "Unable to copy symbol name.");
                                                    }
                                                }
                                            }
                                            else
                                            {
                                                retval = __LINE__;
                                                ERROR_REPORT(retval, inModule->mModule, "Unable to scan static symbols.");
                                            }
                                        }
                                    }
                                    else
                                    {
                                        /*
                                        **  All done.
                                        */
                                        break;
                                    }
                                }
                                else
                                {
                                    /*
                                    **  Static symbols are optional.
                                    **  If no static symbols we're done.
                                    **  Otherwise, set the flag such that it will work more.
                                    */
                                    if(0 == strcmp(current, "Static symbols"))
                                    {
                                        fsm.mFoundStaticSymbols = __LINE__;
                                        forceContinue = 1;
                                    }
                                    else
                                    {
                                        /*
                                        **  All done.
                                        */
                                        break;
                                    }
                                }
                            }
                            else
                            {
                                int scanRes = 0;
                                
                                scanRes = sscanf(current, "entry point at %x:%x", (unsigned*)&(inModule->mEntryPrefix), (unsigned*)&(inModule->mEntryOffset));
                                if(2 == scanRes)
                                {
                                    fsm.mHasEntryPoint = __LINE__;
                                    forceContinue = 1;
                                }
                                else
                                {
                                    retval = __LINE__;
                                    ERROR_REPORT(retval, current, "Unable to obtain entry point.");
                                }
                            }
                        }
                        else
                        {
                            /*
                            **  Skip the N lines of public symbol data (column headers).
                            */
                            if(2 <= fsm.mHasPublicSymbolDataSkippedLines)
                            {
                                /*
                                **  A blank line indicates end of public symbols. 
                                */
                                if(len)
                                {
                                    /*
                                    **  We're adding a new symbol.
                                    **  Make sure we have room for it.
                                    */
                                    if(inModule->mSymbolCapacity == inModule->mSymbolCount)
                                    {
                                        void* moved = NULL;
                                        
                                        moved = realloc(inModule->mSymbols, sizeof(MSMap_Symbol) * (inModule->mSymbolCapacity + MSMAP_SYMBOL_GROWBY));
                                        if(NULL != moved)
                                        {
                                            inModule->mSymbolCapacity += MSMAP_SYMBOL_GROWBY;
                                            inModule->mSymbols = (MSMap_Symbol*)moved;
                                        }
                                        else
                                        {
                                            retval = __LINE__;
                                            ERROR_REPORT(retval, inModule->mModule, "Unable to grow symbols.");
                                        }
                                    }
                                    
                                    if(0 == retval && inModule->mSymbolCapacity > inModule->mSymbolCount)
                                    {
                                        MSMap_Symbol* theSymbol = NULL;
                                        unsigned index = 0;
                                        int scanRes = 0;
                                        char symbolBuf[0x200];
                                        
                                        index = inModule->mSymbolCount;
                                        inModule->mSymbolCount++;
                                        theSymbol = (inModule->mSymbols + index);
                                        
                                        memset(theSymbol, 0, sizeof(MSMap_Symbol));
                                        theSymbol->mScope = PUBLIC;
                                        
                                        scanRes = sscanf(current, "%x:%x %s %x", (unsigned*)&(theSymbol->mPrefix), (unsigned*)&(theSymbol->mOffset), symbolBuf, (unsigned *)&(theSymbol->mRVABase));
                                        if(4 == scanRes)
                                        {
                                            theSymbol->mSymbol = symdup(symbolBuf);

                                            if(NULL != theSymbol->mSymbol)
                                            {
                                                char *last = lastWord(current);
                                                
                                                theSymbol->mObject = strdup(last);
                                                if(NULL == theSymbol->mObject)
                                                {
                                                    retval = __LINE__;
                                                    ERROR_REPORT(retval, last, "Unable to copy object name.");
                                                }
                                            }
                                            else
                                            {
                                                retval = __LINE__;
                                                ERROR_REPORT(retval, symbolBuf, "Unable to copy symbol name.");
                                            }
                                        }
                                        else
                                        {
                                            retval = __LINE__;
                                            ERROR_REPORT(retval, inModule->mModule, "Unable to scan public symbols.");
                                        }
                                    }
                                }
                                else
                                {
                                    fsm.mHasPublicSymbolData = __LINE__;
                                }
                            }
                            else
                            {
                                fsm.mHasPublicSymbolDataSkippedLines++;
                            }
                        }
                    }
                    else
                    {
                        /*
                        **  Skip the first line of segment data (column headers).
                        **  Mark that we've begun grabbing segement data.
                        */
                        if(fsm.mSegmentDataSkippedLine)
                        {
                            /*
                            **  A blank line means end of the segment data.
                            */
                            if(len)
                            {
                                /*
                                **  We're adding a new segment.
                                **  Make sure we have room for it.
                                */
                                if(inModule->mSegmentCapacity == inModule->mSegmentCount)
                                {
                                    void* moved = NULL;
                                    
                                    moved = realloc(inModule->mSegments, sizeof(MSMap_Segment) * (inModule->mSegmentCapacity + MSMAP_SEGMENT_GROWBY));
                                    if(NULL != moved)
                                    {
                                        inModule->mSegmentCapacity += MSMAP_SEGMENT_GROWBY;
                                        inModule->mSegments = (MSMap_Segment*)moved;
                                    }
                                    else
                                    {
                                        retval = __LINE__;
                                        ERROR_REPORT(retval, inModule->mModule, "Unable to grow segments.");
                                    }
                                }
                                
                                if(0 == retval && inModule->mSegmentCapacity > inModule->mSegmentCount)
                                {
                                    MSMap_Segment* theSegment = NULL;
                                    unsigned index = 0;
                                    char classBuf[0x10];
                                    char nameBuf[0x20];
                                    int scanRes = 0;
                                    
                                    index = inModule->mSegmentCount;
                                    inModule->mSegmentCount++;
                                    theSegment = (inModule->mSegments + index);
                                    
                                    memset(theSegment, 0, sizeof(MSMap_Segment));
                                    
                                    scanRes = sscanf(current, "%x:%x %xH %s %s", (unsigned*)&(theSegment->mPrefix), (unsigned*)&(theSegment->mOffset), (unsigned*)&(theSegment->mLength), nameBuf, classBuf);
                                    if(5 == scanRes)
                                    {
                                        if('.' == nameBuf[0])
                                        {
                                            theSegment->mSegment = strdup(&nameBuf[1]);
                                        }
                                        else
                                        {
                                            theSegment->mSegment = strdup(nameBuf);
                                        }

                                        if(NULL != theSegment->mSegment)
                                        {
                                            if(0 == strcmp("DATA", classBuf))
                                            {
                                                theSegment->mClass = DATA;
                                            }
                                            else if(0 == strcmp("CODE", classBuf))
                                            {
                                                theSegment->mClass = CODE;
                                            }
                                            else
                                            {
                                                retval = __LINE__;
                                                ERROR_REPORT(retval, classBuf, "Unrecognized segment class.");
                                            }
                                        }
                                        else
                                        {
                                            retval = __LINE__;
                                            ERROR_REPORT(retval, nameBuf, "Unable to copy segment name.");
                                        }
                                    }
                                    else
                                    {
                                        retval = __LINE__;
                                        ERROR_REPORT(retval, inModule->mModule, "Unable to scan segments.");
                                    }
                                }
                            }
                            else
                            {
                                fsm.mHasSegmentData = __LINE__;
                            }
                        }
                        else
                        {
                            fsm.mSegmentDataSkippedLine = __LINE__;
                        }
                    }
                }
                else
                {
                    int scanRes = 0;
                    
                    /*
                    **  The PLA has a particular format.
                    */
                    scanRes = sscanf(current, "Preferred load address is %x", (unsigned*)&(inModule->mPreferredLoadAddress));
                    if(1 == scanRes)
                    {
                        fsm.mHasPreferredLoadAddress = __LINE__;
                        forceContinue = 1;
                    }
                    else
                    {
                        retval = __LINE__;
                        ERROR_REPORT(retval, current, "Unable to obtain preferred load address.");
                    }
                }
            }
            else
            {
                int scanRes = 0;
                
                /*
                **  The timestamp has a particular format.
                */
                scanRes = sscanf(current, "Timestamp is %x", (unsigned*)&(inModule->mTimestamp));
                if(1 == scanRes)
                {
                    fsm.mHasTimestamp = __LINE__;
                    forceContinue = 1;
                }
                else
                {
                    retval = __LINE__;
                    ERROR_REPORT(retval, current, "Unable to obtain timestamp.");
                }
            }
        }
        else
        {
            /*
            **  The module is on a line by itself.
            */
            inModule->mModule = strdup(current);
            if(NULL != inModule->mModule)
            {
                fsm.mHasModule = __LINE__;
                forceContinue = 1;

                if(0 != inOptions->mMatchModuleCount)
                {
                    unsigned matchLoop = 0;
                    
                    /*
                    **  If this module name doesn't match, then bail.
                    **  Compare in a case sensitive manner, exact match only.
                    */
                    for(matchLoop = 0; matchLoop < inOptions->mMatchModuleCount; matchLoop++)
                    {
                        if(0 == strcmp(inModule->mModule, inOptions->mMatchModules[matchLoop]))
                        {
                            break;
                        }
                    }
                    
                    if(matchLoop == inOptions->mMatchModuleCount)
                    {
                        /*
                        **  A match did not occur, bail out of read loop.
                        **  No error, however.
                        */
                        break;
                    }
                }
            }
            else
            {
                retval = __LINE__;
                ERROR_REPORT(retval, current, "Unable to obtain module.");
            }
        }
    }
    
    if(0 == retval && 0 != ferror(inOptions->mInput))
    {
        retval = __LINE__;
        ERROR_REPORT(retval, inOptions->mInputName, "Unable to read file.");
    }
    
    return retval;
}


static int qsortRVABase(const void* in1, const void* in2)
/*
**  qsort callback to sort the symbols by their RVABase.
*/
{
    MSMap_Symbol* sym1 = (MSMap_Symbol*)in1;
    MSMap_Symbol* sym2 = (MSMap_Symbol*)in2;
    int retval = 0;

    if(sym1->mRVABase < sym2->mRVABase)
    {
        retval = -1;
    }
    else if(sym1->mRVABase > sym2->mRVABase)
    {
        retval = 1;
    }

    return retval;
}


static int tsvout(Options* inOptions, unsigned inSize, MSMap_SegmentClass inClass, MSMap_SymbolScope inScope, const char* inModule, const char* inSegment, address inPrefix, address inOffset, const char* inObject, const char* inSymbol)
/*
**  Output a line of map information seperated by tabs.
**  Some items (const char*), if not present, will receive a default value.
*/
{
    int retval = 0;

    /*
    **  No need to output on no size.
    **  This can happen with zero sized segments,
    **      or an imported symbol which has multiple names (one will count).
    */
    if(0 != inSize)
    {
        char objectBuf[0x100];
        const char* symScope = NULL;
        const char* segClass = NULL;
        const char* undefined = "UNDEF";
        
        /*
        **  Fill in unspecified values.
        */
        if(NULL == inObject)
        {
            sprintf(objectBuf, "%s:%s:%s", undefined, inModule, inSegment);
            inObject = objectBuf;
        }
        if(NULL == inSymbol)
        {
            inSymbol = inObject;
        }
        
        /*
        **  Convert some enumerations to text.
        */
        switch(inClass)
        {
        case CODE:
            segClass = "CODE";
            break;
        case DATA:
            segClass = "DATA";
            break;
        default:
            retval = __LINE__;
            ERROR_REPORT(retval, "", "Unable to determine class for output.");
            break;
        }
        
        switch(inScope)
        {
        case PUBLIC:
            symScope = "PUBLIC";
            break;
        case STATIC:
            symScope = "STATIC";
            break;
        case UNDEFINED:
            symScope = undefined;
            break;
        default:
            retval = __LINE__;
            ERROR_REPORT(retval, "", "Unable to determine scope for symbol.");
            break;
        }
        
        if(0 == retval)
        {
            int printRes = 0;
            
            printRes = fprintf(inOptions->mOutput,
                "%.8X\t%s\t%s\t%s\t%s\t%s\t%s",
                inSize,
                segClass,
                symScope,
                inModule,
                inSegment,
                inObject,
                inSymbol
                );

            if(0 <= printRes && inOptions->mAddresses)
            {
                printRes = fprintf(inOptions->mOutput,
                    "\t%.4X:%.8X",
                    (unsigned)inPrefix,
                    (unsigned)inOffset
                    );
            }

            if(0 <= printRes)
            {
                printRes = fprintf(inOptions->mOutput, "\n");
            }
            
            if(0 > printRes)
            {
                retval = __LINE__;
                ERROR_REPORT(retval, inOptions->mOutputName, "Unable to output tsv data.");
            }
        }
    }

    return retval;
}


void cleanModule(MSMap_Module* inModule)
{
    unsigned loop = 0;

    for(loop = 0; loop < inModule->mSymbolCount; loop++)
    {
        CLEANUP(inModule->mSymbols[loop].mObject);
        CLEANUP(inModule->mSymbols[loop].mSymbol);
    }
    CLEANUP(inModule->mSymbols);

    for(loop = 0; loop < inModule->mSegmentCount; loop++)
    {
        CLEANUP(inModule->mSegments[loop].mSegment);
    }
    CLEANUP(inModule->mSegments);

    CLEANUP(inModule->mModule);

    memset(inModule, 0, sizeof(MSMap_Module));
}


int map2tsv(Options* inOptions)
/*
**  Read all input.
**  Output tab seperated value data.
*/
{
    int retval = 0;
    MSMap_Module module;

    memset(&module, 0, sizeof(module));

    /*
    **  Read in the map file.
    */
    retval = readmap(inOptions, &module);
    if(0 == retval)
    {
        unsigned segLoop = 0;
        MSMap_Segment* segment = NULL;
        unsigned symLoop = 0;
        MSMap_Symbol* symbol = NULL;
        MSMap_Symbol* firstSymbol = NULL;
        MSMap_Symbol* matchSymbol = NULL;
        MSMap_Symbol* prevSymbol = NULL;

        /*
        **  Quick sort the symbols via RVABase.
        */
        qsort(module.mSymbols, module.mSymbolCount, sizeof(MSMap_Symbol), qsortRVABase);

        /*
        **  Go through each segment in order:
        **      Output symbols and sizes, already in order.
        **      Make up symbol names for those missing symbols,
        **      or for blank spots at the beginning of a segment.
        */
        for(segLoop = 0; 0 == retval && segLoop < module.mSegmentCount; segLoop++)
        {
            segment = (module.mSegments + segLoop);
            firstSymbol = NULL;
            matchSymbol = NULL;
            prevSymbol = NULL;

            for(symLoop = 0; 0 == retval && symLoop < module.mSymbolCount; symLoop++)
            {
                symbol = (module.mSymbols + symLoop);

                /*
                **  Symbol must fall in range of segment for consideration.
                */
                if(symbol->mPrefix == segment->mPrefix)
                {
                    if(symbol->mOffset >= segment->mOffset)
                    {
                        if(symbol->mOffset < (segment->mOffset + segment->mLength))
                        {
                            /*
                            **  Matched.
                            */
                            prevSymbol = matchSymbol;
                            matchSymbol = symbol;

                            if(NULL == firstSymbol)
                            {
                                firstSymbol = matchSymbol;

                                /*
                                **  Check to see if we need to output a dummy
                                **      to start the segment.
                                */
                                if(0 == retval && firstSymbol->mOffset != segment->mOffset)
                                {
                                    retval = tsvout(inOptions,
                                        firstSymbol->mOffset - segment->mOffset,
                                        segment->mClass,
                                        UNDEFINED,
                                        module.mModule,
                                        segment->mSegment,
                                        segment->mPrefix,
                                        segment->mOffset,
                                        NULL,
                                        NULL
                                        );
                                }
                            }

                            /*
                            **  Can now output previous symbol as can calculate size.
                            */
                            if(0 == retval && NULL != prevSymbol)
                            {
                                retval = tsvout(inOptions,
                                    matchSymbol->mOffset - prevSymbol->mOffset,
                                    segment->mClass,
                                    prevSymbol->mScope,
                                    module.mModule,
                                    segment->mSegment,
                                    prevSymbol->mPrefix,
                                    prevSymbol->mOffset,
                                    prevSymbol->mObject,
                                    prevSymbol->mSymbol
                                    );
                            }
                        }
                    }
                }
            }

            /*
            **  If there was no symbol, output a fake one for the entire segment.
            **  Otherwise, there is always one final match which we must output
            **      taking up the remainder of the segment.
            */
            if(0 == retval)
            {
                if(NULL == firstSymbol)
                {
                    retval = tsvout(inOptions,
                        segment->mLength,
                        segment->mClass,
                        UNDEFINED,
                        module.mModule,
                        segment->mSegment,
                        segment->mPrefix,
                        segment->mOffset,
                        NULL,
                        NULL
                        );
                }
                else
                {
                    retval = tsvout(inOptions,
                        (segment->mOffset + segment->mLength) - matchSymbol->mOffset,
                        segment->mClass,
                        matchSymbol->mScope,
                        module.mModule,
                        segment->mSegment,
                        matchSymbol->mPrefix,
                        matchSymbol->mOffset,
                        matchSymbol->mObject,
                        matchSymbol->mSymbol
                        );
                }
            }
        }
    }

    /*
    **  Cleanup.
    */
    cleanModule(&module);

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
            else if(current == &gAddressesSwitch)
            {
                outOptions->mAddresses = __LINE__;
            }
            else if(current == &gHelpSwitch)
            {
                outOptions->mHelp = __LINE__;
            }
            else if(current == &gMatchModuleSwitch)
            {
                void* moved = NULL;

                /*
                **  Add the value to the list of allowed module names.
                */
                moved = realloc(outOptions->mMatchModules, sizeof(char*) * (outOptions->mMatchModuleCount + 1));
                if(NULL != moved)
                {
                    outOptions->mMatchModules = (char**)moved;
                    outOptions->mMatchModules[outOptions->mMatchModuleCount] = strdup(current->mValue);
                    if(NULL != outOptions->mMatchModules[outOptions->mMatchModuleCount])
                    {
                        outOptions->mMatchModuleCount++;
                    }
                    else
                    {
                        retval = __LINE__;
                        ERROR_REPORT(retval, current->mValue, "Unable to duplicate string.");
                    }
                }
                else
                {
                    retval = __LINE__;
                    ERROR_REPORT(retval, current->mValue, "Unable to allocate space for string.");
                }
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
    while(0 != inOptions->mMatchModuleCount)
    {
        inOptions->mMatchModuleCount--;
        CLEANUP(inOptions->mMatchModules[inOptions->mMatchModuleCount]);
    }
    CLEANUP(inOptions->mMatchModules);

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

    printf("This tool normalizes MS linker .map files for use by other tools.\n");
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
        retval = map2tsv(&options);
    }

    cleanOptions(&options);
    return retval;
}

