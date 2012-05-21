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
#define DEMANGLE_SAFE_CHAR(eval)  (isprint(eval) ? eval : ' ')

#else
#define F_DEMANGLE 0
#endif /* WIN32 */

#define STRINGIFY(s_) STRINGIFY2(s_)
#define STRINGIFY2(s_) #s_
#define SYMBOL_BUF_CHARS 511

#define ERROR_REPORT(num, val, msg)   fprintf(stderr, "error(%d):\t\"%s\"\t%s\n", (num), (val), (msg));
#define CLEANUP(ptr)    do { if(NULL != ptr) { free(ptr); ptr = NULL; } } while(0)


typedef struct __struct_SymDB_Size
/*
**  The size of the symbol.
**  The size is nested withing a symbols structures to produce a fast
**      lookup path.
**  The objects are listed in case the client of the symdb needs to
**      match the object name in the scenario where multiple symbol
**      sizes are present.
**
**  mSize           The size of the symbol in these objects.
**  mObjects        A list of objects containing said symbol.
**  mObjectCount    Number of objects.
*/
{
    unsigned            mSize;
    char**              mObjects;
    unsigned            mObjectCount;
}
SymDB_Size;


typedef struct __struct_SymDB_Section
/*
**  Each section for a symbol has a list of sizes.
**  Should there be exactly one size for the symbol, then that
**      is the size that should be accepted.
**  If there is more than one size, then a match on the object
**      should be attempted, held withing each size.
**
**  mName           The section name.
**  mSizes          The varoius sizes of the symbol in this section.
**  mSizeCount      The number of available sizes.
*/
{
    char*               mName;
    SymDB_Size*         mSizes;
    unsigned            mSizeCount;
}
SymDB_Section;


typedef struct __struct_SymDB_Symbol
/*
**  Each symbol has at least one section.
**  The section indicates what type of symbol a client may be looking for.
**  If there is no match on the section, then the client should not trust
**      the symbdb.
**
**  mName           The mangled name of the symbol.
**  mSections       Various sections this symbol belongs to.
**  mSectionCount   The number of sections.
*/
{
    char*               mName;
    SymDB_Section*      mSections;
    unsigned            mSectionCount;
}
SymDB_Symbol;


#define SYMDB_SYMBOL_GROWBY 0x1000 /* how many sybols to allocate at a time */


typedef struct __struct_SymDB_Container
/*
**  The symbol DB container object.
**  The goal of the symbol DB is to have exactly one SymDB_Symbol for each
**      mangled name, no matter how ever many identical mangled names there
**      are in the input.
**  The input is already expected to be well sorted, futher this leads to
**      the ability to binary search for symbol name matches.
**
**  mSymbols        The symbols.
**  mSymbolCount    The number of symbols in the DB.
**  mSymbolCapacity The number of symbols we can hold (before realloc).
*/
{
    SymDB_Symbol*       mSymbols;
    unsigned            mSymbolCount;
    unsigned            mSymbolCapacity;
}
SymDB_Container;


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
**  mHelp               Whether or not help should be shown.
**  mMatchModules       Array of strings which the module name should match.
**  mMatchModuleCount   Number of items in array.
**  mSymDBName          Symbol DB filename.
**  mBatchMode          Batch mode.
**                      When in batch mode, the input file contains a list of
**                          map files to process.
**                      Normally the input file is a single map file itself.
*/
{
    const char* mProgramName;
    FILE* mInput;
    char* mInputName;
    FILE* mOutput;
    char* mOutputName;
    int mHelp;
    char** mMatchModules;
    unsigned mMatchModuleCount;
    char* mSymDBName;
    SymDB_Container* mSymDB;
    int mBatchMode;
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
static Switch gMatchModuleSwitch = {"--match-module", "-mm", 1, NULL, "Specify a valid module name." DESC_NEWLINE "Multiple specifications allowed." DESC_NEWLINE "If a module name does not match one of the names specified then no output will occur."};
static Switch gSymDBSwitch = {"--symdb", "-sdb", 1, NULL, "Specify a symbol tsv db input file." DESC_NEWLINE "Such a symdb is produced using the tool msdump2symdb." DESC_NEWLINE "This allows better symbol size approximations." DESC_NEWLINE "The symdb file must be pre-sorted."};
static Switch gBatchModeSwitch = {"--batch", "-b", 0, NULL, "Runs in batch mode." DESC_NEWLINE "The input file contains a list of map files." DESC_NEWLINE "Normally the input file is a map file itself." DESC_NEWLINE "This eliminates reprocessing the symdb for multiple map files."};

static Switch* gSwitches[] = {
        &gInputSwitch,
        &gOutputSwitch,
        &gMatchModuleSwitch,
        &gSymDBSwitch,
        &gBatchModeSwitch,
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


MSMap_Segment* getSymbolSection(MSMap_Module* inModule, MSMap_Symbol* inoutSymbol)
/*
**  Perform a lookup for the section of the symbol.
**  The function could cache the value.
*/
{
    MSMap_Segment* retval = NULL;

    if(NULL != inoutSymbol->mSection)
    {
        /*
        **  Use cached value.
        */
        retval = inoutSymbol->mSection;
    }
    else
    {
        unsigned secLoop = 0;

        /*
        **  Go through sections in module to find the match for the symbol.
        */
        for(secLoop = 0; secLoop < inModule->mSegmentCount; secLoop++)
        {
            if(inoutSymbol->mPrefix == inModule->mSegments[secLoop].mPrefix)
            {
                if(inoutSymbol->mOffset >= inModule->mSegments[secLoop].mOffset)
                {
                    if(inoutSymbol->mOffset < (inModule->mSegments[secLoop].mOffset + inModule->mSegments[secLoop].mLength))
                    {
                        /*
                        **  We have the section.
                        */
                        retval = &inModule->mSegments[secLoop];
                        break;
                    }
                }
            }
        }

        /*
        **  Cache the value for next time.
        */
        inoutSymbol->mSection = retval;
    }

    return retval;
}


int readSymDB(const char* inDBName, SymDB_Container** outDB)
/*
**  Intialize the symbol DB.
**  Only call if the symbol DB should be initialized.
*/
{
    int retval = 0;

    /*
    **  Initialize out arguments.
    */
    if(NULL != outDB)
    {
        *outDB = NULL;
    }

    if(NULL != outDB && NULL != inDBName)
    {
        FILE* symDB = NULL;

        symDB = fopen(inDBName, "r");
        if(NULL != symDB)
        {
            *outDB = (SymDB_Container*)calloc(1, sizeof(SymDB_Container));
            if(NULL != *outDB)
            {
                char lineBuf[0x400];
                char* symbol = NULL;
                char* section = NULL;
                char* object = NULL;
                char* length = NULL;
                unsigned lengthNum = 0;
                char* endLength = NULL;
                
                /*
                **  Read the file line by line.
                */
                while(0 == retval && NULL != fgets(lineBuf, sizeof(lineBuf), symDB))
                {
                    trimWhite(lineBuf);
                    
                    /*
                    **  Each line has four arguments.  tab separated values (tsv).
                    **      Symbol
                    **      Section
                    **      Length
                    **      Object
                    */
                    
                    symbol = skipWhite(lineBuf);
                    if(NULL == symbol)
                    {
                        retval = __LINE__;
                        ERROR_REPORT(retval, inDBName, "File does not appear to be a symbol DB.");
                        break;
                    }
                    
                    section = strchr(symbol, '\t');
                    if(NULL == section)
                    {
                        retval = __LINE__;
                        ERROR_REPORT(retval, inDBName, "File does not appear to be a symbol DB.");
                        break;
                    }
                    *section = '\0';
                    section++;
                    
                    length = strchr(section, '\t');
                    if(NULL == length)
                    {
                        retval = __LINE__;
                        ERROR_REPORT(retval, inDBName, "File does not appear to be a symbol DB.");
                        break;
                    }
                    *length = '\0';
                    length++;
                    
                    object = strchr(length, '\t');
                    if(NULL == object)
                    {
                        retval = __LINE__;
                        ERROR_REPORT(retval, inDBName, "File does not appear to be a symbol DB.");
                        break;
                    }
                    *object = '\0';
                    object++;
                    
                    /*
                    **  Convert the length into a number.
                    */
                    errno = 0;
                    lengthNum = strtoul(length, &endLength, 16);
                    if(0 == errno && endLength != length)
                    {
                        SymDB_Symbol* dbSymbol = NULL;
                        SymDB_Section* dbSection = NULL;
                        SymDB_Size* dbSize = NULL;
                        char* dbObject = NULL;
                        void* moved = NULL;
                        
                        /*
                        **  Are we looking at the same symbol as last line?
                        **  This assumes the symdb is pre sorted!!!
                        */
                        if(0 != (*outDB)->mSymbolCount)
                        {
                            unsigned index = (*outDB)->mSymbolCount - 1;
                            
                            if(0 == strcmp((*outDB)->mSymbols[index].mName, symbol))
                            {
                                dbSymbol = &(*outDB)->mSymbols[index];
                            }
                        }
                        
                        /*
                        **  May need to create symbol.
                        */
                        if(NULL == dbSymbol)
                        {
                            /*
                            **  Could be time to grow the symbol pool.
                            */
                            if((*outDB)->mSymbolCount >= (*outDB)->mSymbolCapacity)
                            {
                                moved = realloc((*outDB)->mSymbols, sizeof(SymDB_Symbol) * ((*outDB)->mSymbolCapacity + SYMDB_SYMBOL_GROWBY));
                                if(NULL != moved)
                                {
                                    (*outDB)->mSymbols = (SymDB_Symbol*)moved;
                                    memset(&(*outDB)->mSymbols[(*outDB)->mSymbolCapacity], 0, sizeof(SymDB_Symbol) * SYMDB_SYMBOL_GROWBY);
                                    (*outDB)->mSymbolCapacity += SYMDB_SYMBOL_GROWBY;
                                }
                                else
                                {
                                    retval = __LINE__;
                                    ERROR_REPORT(retval, inDBName, "Unable to grow symbol DB symbol array.");
                                    break;
                                }
                            }

                            if((*outDB)->mSymbolCount < (*outDB)->mSymbolCapacity)
                            {
                                dbSymbol = &(*outDB)->mSymbols[(*outDB)->mSymbolCount];
                                (*outDB)->mSymbolCount++;
                                
                                dbSymbol->mName = strdup(symbol);
                                if(NULL == dbSymbol->mName)
                                {
                                    retval = __LINE__;
                                    ERROR_REPORT(retval, symbol, "Unable to duplicate string.");
                                    break;
                                }
                            }
                            else
                            {
                                retval = __LINE__;
                                ERROR_REPORT(retval, symbol, "Unable to grow symbol DB for symbol.");
                                break;
                            }
                        }
                        
                        /*
                        **  Assume we have the symbol.
                        **
                        **  Is this the same section as the last section in the symbol?
                        **  This assumes the symdb was presorted!!!!
                        */
                        if(0 != dbSymbol->mSectionCount)
                        {
                            unsigned index = dbSymbol->mSectionCount - 1;
                            
                            if(0 == strcmp(dbSymbol->mSections[index].mName, section))
                            {
                                dbSection = &dbSymbol->mSections[index];
                            }
                        }
                        
                        /*
                        **  May need to create the section.
                        */
                        if(NULL == dbSection)
                        {
                            moved = realloc(dbSymbol->mSections, sizeof(SymDB_Section) * (dbSymbol->mSectionCount + 1));
                            if(NULL != moved)
                            {
                                dbSymbol->mSections = (SymDB_Section*)moved;
                                dbSection = &dbSymbol->mSections[dbSymbol->mSectionCount];
                                dbSymbol->mSectionCount++;
                                
                                memset(dbSection, 0, sizeof(SymDB_Section));
                                
                                dbSection->mName = strdup(section);
                                if(NULL == dbSection->mName)
                                {
                                    retval = __LINE__;
                                    ERROR_REPORT(retval, section, "Unable to duplicate string.");
                                    break;
                                }
                            }
                            else
                            {
                                retval = __LINE__;
                                ERROR_REPORT(retval, section, "Unable to grow symbol sections for symbol DB.");
                                break;
                            }
                        }
                        
                        /*
                        **  Assume we have the section.
                        **
                        **  Is this the same size as the last size?
                        **  This assumes the symdb was presorted!!!
                        */
                        if(0 != dbSection->mSizeCount)
                        {
                            unsigned index = dbSection->mSizeCount - 1;
                            
                            if(dbSection->mSizes[index].mSize == lengthNum)
                            {
                                dbSize = &dbSection->mSizes[index];
                            }
                        }
                        
                        /*
                        **  May need to create the size in question.
                        */
                        if(NULL == dbSize)
                        {
                            moved = realloc(dbSection->mSizes, sizeof(SymDB_Size) * (dbSection->mSizeCount + 1));
                            if(NULL != moved)
                            {
                                dbSection->mSizes = (SymDB_Size*)moved;
                                dbSize = &dbSection->mSizes[dbSection->mSizeCount];
                                dbSection->mSizeCount++;
                                
                                memset(dbSize, 0, sizeof(SymDB_Size));
                                
                                dbSize->mSize = lengthNum;
                            }
                            else
                            {
                                retval = __LINE__;
                                ERROR_REPORT(retval, length, "Unable to grow symbol section sizes for symbol DB.");
                                break;
                            }
                        }
                        
                        /*
                        **  Assume we have the size.
                        **
                        **  We assume a one to one correllation between size and object.
                        **  Always try to add the new object name.
                        **  As the symdb is assumed to be sorted, the object names should also be in order.
                        */
                        moved = realloc(dbSize->mObjects, sizeof(char*) * (dbSize->mObjectCount + 1));
                        if(NULL != moved)
                        {
                            dbObject = strdup(object);
                            
                            dbSize->mObjects = (char**)moved;
                            dbSize->mObjects[dbSize->mObjectCount] = dbObject;
                            dbSize->mObjectCount++;
                            
                            if(NULL == dbObject)
                            {
                                retval = __LINE__;
                                ERROR_REPORT(retval, object, "Unable to duplicate string.");
                                break;
                            }
                        }
                        else
                        {
                            retval = __LINE__;
                            ERROR_REPORT(retval, object, "Unable to grow symbol section size objects for symbol DB.");
                            break;
                        }
                    }
                    else
                    {
                        retval = __LINE__;
                        ERROR_REPORT(retval, length, "Unable to convert symbol DB length into a number.");
                        break;
                    }
                }
            
                if(0 == retval && 0 != ferror(symDB))
                {
                    retval = __LINE__;
                    ERROR_REPORT(retval, inDBName, "Unable to read file.");
                }
            }
            else
            {
                retval = __LINE__;
                ERROR_REPORT(retval, inDBName, "Unable to allocate symbol DB.");
            }

            fclose(symDB);
            symDB = NULL;
        }
        else
        {
            retval = __LINE__;
            ERROR_REPORT(retval, inDBName, "Unable to open symbol DB.");
        }
    }
    else
    {
        retval = __LINE__;
        ERROR_REPORT(retval, "(NULL)", "Invalid arguments.");
    }

    return retval;
}


void cleanSymDB(SymDB_Container** inDB)
/*
**  Free it all up.
*/
{
    if(NULL != inDB && NULL != *inDB)
    {
        unsigned symLoop = 0;
        unsigned secLoop = 0;
        unsigned sizLoop = 0;
        unsigned objLoop = 0;

        for(symLoop = 0; symLoop < (*inDB)->mSymbolCount; symLoop++)
        {
            for(secLoop = 0; secLoop < (*inDB)->mSymbols[symLoop].mSectionCount; secLoop++)
            {
                for(sizLoop = 0; sizLoop < (*inDB)->mSymbols[symLoop].mSections[secLoop].mSizeCount; sizLoop++)
                {
                    for(objLoop = 0; objLoop < (*inDB)->mSymbols[symLoop].mSections[secLoop].mSizes[sizLoop].mObjectCount; objLoop++)
                    {
                        CLEANUP((*inDB)->mSymbols[symLoop].mSections[secLoop].mSizes[sizLoop].mObjects[objLoop]);
                    }
                    CLEANUP((*inDB)->mSymbols[symLoop].mSections[secLoop].mSizes[sizLoop].mObjects);
                }
                CLEANUP((*inDB)->mSymbols[symLoop].mSections[secLoop].mName);
                CLEANUP((*inDB)->mSymbols[symLoop].mSections[secLoop].mSizes);
            }
            CLEANUP((*inDB)->mSymbols[symLoop].mName);
            CLEANUP((*inDB)->mSymbols[symLoop].mSections);
        }
        CLEANUP((*inDB)->mSymbols);
        CLEANUP(*inDB);
    }
}


int symDBLookup(const void* inKey, const void* inItem)
/*
**  bsearch utility routine to find the symbol in the symdb.
*/
{
    int retval = 0;
    const char* key = (const char*)inKey;
    const SymDB_Symbol* symbol = (const SymDB_Symbol*)inItem;

    retval = strcmp(key, symbol->mName);

    return retval;
}


int fillSymbolSizeFromDB(Options* inOptions, MSMap_Module* inModule, MSMap_Symbol* inoutSymbol, const char* inMangledName)
/*
**  If we have a symbol DB, attempt to determine the real size of the symbol
**      up front.
**  This helps us later in the game to avoid performing size guesses by
**      offset.
*/
{
    int retval = 0;

    /*
    **  May need to initialize symdb.
    */
    if(NULL == inOptions->mSymDB && NULL != inOptions->mSymDBName)
    {
        retval = readSymDB(inOptions->mSymDBName, &inOptions->mSymDB);
    }

    /*
    **  Optional
    */
    if(0 == retval && NULL != inOptions->mSymDB)
    {
        void* match = NULL;

        /*
        **  Find the symbol.
        */
        match = bsearch(inMangledName, inOptions->mSymDB->mSymbols, inOptions->mSymDB->mSymbolCount, sizeof(SymDB_Symbol), symDBLookup);
        if(NULL != match)
        {
            SymDB_Symbol* symbol = (SymDB_Symbol*)match;
            unsigned symDBSize = 0;
            MSMap_Segment* mapSection = NULL;

            /*
            **  We found the symbol.
            **
            **  See if it has the section in question.
            */
            mapSection = getSymbolSection(inModule, inoutSymbol);
            if(NULL != mapSection)
            {
                unsigned secLoop = 0;

                for(secLoop = 0; secLoop < symbol->mSectionCount; secLoop++)
                {
                    if(0 == strcmp(mapSection->mSegment, symbol->mSections[secLoop].mName))
                    {
                        SymDB_Section* section = &symbol->mSections[secLoop];

                        /*
                        **  We have a section match.
                        **  Should there be a single size for the symbol,
                        **      then we just default to that.
                        **  If more than one size, we have to do an
                        **      object match search.
                        **  Should there be no object match, we do nothign.
                        */
                        if(1 == section->mSizeCount)
                        {
                            symDBSize = section->mSizes[0].mSize;
                        }
                        else
                        {
                            char* mapObject = NULL;
                            
                            /*
                            **  Figure out the map object file name.
                            **  Skip any colon.
                            **  If it doesn't have a .obj in it, not worth continuing.
                            */
                            mapObject = strrchr(inoutSymbol->mObject, ':');
                            if(NULL == mapObject)
                            {
                                mapObject = inoutSymbol->mObject;
                            }
                            else
                            {
                                mapObject++; /* colon */
                            }

                            if(NULL != strstr(mapObject, ".obj"))
                            {
                                unsigned sizLoop = 0;
                                unsigned objLoop = 0;
                                SymDB_Size* size = NULL;

                                for(sizLoop = 0; sizLoop < section->mSizeCount; sizLoop++)
                                {
                                    size = &section->mSizes[sizLoop];
                                    
                                    for(objLoop = 0; objLoop < size->mObjectCount; objLoop++)
                                    {
                                        if(NULL != strstr(size->mObjects[objLoop], mapObject))
                                        {
                                            /*
                                            **  As we matched the object, in a particular section,
                                            **      we'll go with this as the number.
                                            */
                                            symDBSize = size->mSize;
                                            break;
                                        }
                                    }
                                    
                                    /*
                                    **  If the object loop broke early, we break too.
                                    */
                                    if(objLoop < size->mObjectCount)
                                    {
                                        break;
                                    }
                                }
                            }
                        }

                        break;
                    }
                }
            }

            /*
            **  Put the size in.
            */
            inoutSymbol->mSymDBSize = symDBSize;
        }
    }

    return retval;
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
                                *curresult++ = DEMANGLE_SAFE_CHAR(*curchar);
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
                                *curresult++ = DEMANGLE_SAFE_CHAR(hex_state);
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
                                            char symbolBuf[SYMBOL_BUF_CHARS + 1];
                                            
                                            index = inModule->mSymbolCount;
                                            inModule->mSymbolCount++;
                                            theSymbol = (inModule->mSymbols + index);
                                            
                                            memset(theSymbol, 0, sizeof(MSMap_Symbol));
                                            theSymbol->mScope = STATIC;
                                            
                                            scanRes = sscanf(current, "%x:%x %" STRINGIFY(SYMBOL_BUF_CHARS) "s %x", (unsigned*)&(theSymbol->mPrefix), (unsigned*)&(theSymbol->mOffset), symbolBuf, (unsigned*)&(theSymbol->mRVABase));
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
                                        char symbolBuf[SYMBOL_BUF_CHARS + 1];
                                        
                                        index = inModule->mSymbolCount;
                                        inModule->mSymbolCount++;
                                        theSymbol = (inModule->mSymbols + index);
                                        
                                        memset(theSymbol, 0, sizeof(MSMap_Symbol));
                                        theSymbol->mScope = PUBLIC;
                                        
                                        scanRes = sscanf(current, "%x:%x %" STRINGIFY(SYMBOL_BUF_CHARS) "s %x", (unsigned*)&(theSymbol->mPrefix), (unsigned*)&(theSymbol->mOffset), symbolBuf, (unsigned *)&(theSymbol->mRVABase));
                                        if(4 == scanRes)
                                        {
                                            theSymbol->mSymbol = symdup(symbolBuf);

                                            if(NULL != theSymbol->mSymbol)
                                            {
                                                char *last = lastWord(current);
                                                
                                                theSymbol->mObject = strdup(last);
                                                if(NULL != theSymbol->mObject)
                                                {
                                                    /*
                                                    **  Finally, attempt to lookup the actual size of the symbol
                                                    **      if there is a symbol DB available.
                                                    */
                                                    retval = fillSymbolSizeFromDB(inOptions, inModule, theSymbol, symbolBuf);
                                                }
                                                else
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
                                    #define CLASS_BUF_CHARS 15
                                    char classBuf[CLASS_BUF_CHARS + 1];
                                    #define NAME_BUF_CHARS 31
                                    char nameBuf[NAME_BUF_CHARS + 1];
                                    int scanRes = 0;
                                    
                                    index = inModule->mSegmentCount;
                                    inModule->mSegmentCount++;
                                    theSegment = (inModule->mSegments + index);
                                    
                                    memset(theSegment, 0, sizeof(MSMap_Segment));
                                    
                                    scanRes = sscanf(current, "%x:%x %xH %" STRINGIFY(NAME_BUF_CHARS) "s %" STRINGIFY(CLASS_BUF_CHARS) "s", (unsigned*)&(theSegment->mPrefix), (unsigned*)&(theSegment->mOffset), (unsigned*)&(theSegment->mLength), nameBuf, classBuf);
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


static int tsvout(Options* inOptions, unsigned inSize, MSMap_SegmentClass inClass, MSMap_SymbolScope inScope, const char* inModule, const char* inSegment, const char* inObject, const char* inSymbol)
/*
**  Output a line of map information separated by tabs.
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
                "%.8X\t%s\t%s\t%s\t%s\t%s\t%s\n",
                inSize,
                segClass,
                symScope,
                inModule,
                inSegment,
                inObject,
                inSymbol
                );

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
**  Output tab separated value data.
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
        unsigned symLoop = 0;
        MSMap_Symbol* symbol = NULL;
        unsigned secLoop = 0;
        MSMap_Segment* section = NULL;
        unsigned size = 0;
        unsigned dbSize = 0;
        unsigned offsetSize = 0;
        unsigned endOffset = 0;

        /*
        **  Quick sort the symbols via RVABase.
        */
        qsort(module.mSymbols, module.mSymbolCount, sizeof(MSMap_Symbol), qsortRVABase);

        /*
        **  Go through all the symbols (in order by sort).
        **  Output their sizes.
        */
        for(symLoop = 0; 0 == retval && symLoop < module.mSymbolCount; symLoop++)
        {
            symbol = &module.mSymbols[symLoop];
            section = getSymbolSection(&module, symbol);
            if (!section)
                continue;

            /*
            **  Use the symbol DB size if available.
            */
            dbSize = symbol->mSymDBSize;

            /*
            **  Guess using offsets.
            **  Is there a next symbol available?  If so, its start offset is the end of this symbol.
            **  Otherwise, our section offset + length is the end of this symbol.
            **
            **  The trick is, the DB size can not go beyond the offset size, for sanity.
            */
            
            /*
            **  Try next symbol, but only if in same section.
            **  If still not, use the end of the segment.
            **  This implies we were the last symbol in the segment.
            */
            if((symLoop + 1) < module.mSymbolCount)
            {
                MSMap_Symbol* nextSymbol = NULL;
                MSMap_Segment* nextSection = NULL;
                
                nextSymbol = &module.mSymbols[symLoop + 1];
                nextSection = getSymbolSection(&module, nextSymbol);
                
                if(section == nextSection)
                {
                    endOffset = nextSymbol->mOffset;
                }
                else
                {
                    endOffset = section->mOffset + section->mLength;
                }
            }
            else
            {
                endOffset = section->mOffset + section->mLength;
            }

            /*
            **  Can now guess at size.
            */
            offsetSize = endOffset - symbol->mOffset;

            /*
            **  Now, determine which size to use.
            **  This is really a sanity check as well.
            */
            size = offsetSize;
            if(0 != dbSize)
            {
                if(dbSize < offsetSize)
                {
                    size = dbSize;
                }
            }

            /*
            **  Output the symbol with the size.
            */
            retval = tsvout(inOptions,
                size,
                section->mClass,
                symbol->mScope,
                module.mModule,
                section->mSegment,
                symbol->mObject,
                symbol->mSymbol
                );

            /*
            **  Make sure we mark this amount of space as used in the section.
            */
            section->mUsed += size;
        }

        /*
        **  Go through the sections, and those whose length is longer than the
        **      amount of space used, output dummy filler values.
        */
        for(secLoop = 0; 0 == retval && secLoop < module.mSegmentCount; secLoop++)
        {
            section = &module.mSegments[secLoop];

            if(section && section->mUsed < section->mLength)
            {
                retval = tsvout(inOptions,
                    section->mLength - section->mUsed,
                    section->mClass,
                    UNDEFINED,
                    module.mModule,
                    section->mSegment,
                    NULL,
                    NULL
                    );
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
            else if(current == &gSymDBSwitch)
            {
                CLEANUP(outOptions->mSymDBName);
                outOptions->mSymDBName = strdup(current->mValue);
                if(NULL == outOptions->mSymDBName)
                {
                    retval = __LINE__;
                    ERROR_REPORT(retval, current->mValue, "Unable to duplicate symbol db name.");
                }
            }
            else if(current == &gBatchModeSwitch)
            {
                outOptions->mBatchMode = __LINE__;
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
**  Clean up any open handles, et. al.
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

    cleanSymDB(&inOptions->mSymDB);

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


int batchMode(Options* inOptions)
/*
**  Batch mode means that the input file is actually a list of map files.
**  We simply swap out our input file names while we do this.
*/
{
    int retval = 0;
    char lineBuf[0x400];
    FILE* realInput = NULL;
    char* realInputName = NULL;
    FILE* mapFile = NULL;
    int finalRes = 0;

    realInput = inOptions->mInput;
    realInputName = inOptions->mInputName;

    while(0 == retval && NULL != fgets(lineBuf, sizeof(lineBuf), realInput))
    {
        trimWhite(lineBuf);

        /*
        **  Skip/allow blank lines.
        */
        if('\0' == lineBuf[0])
        {
            continue;
        }

        /*
        **  Override what we believe to be the input for this line.
        */
        inOptions->mInputName = lineBuf;
        inOptions->mInput = fopen(lineBuf, "r");
        if(NULL != inOptions->mInput)
        {
            int mapRes = 0;

            /*
            **  Do it.
            */
            mapRes = map2tsv(inOptions);

            /*
            **  We report the first error that we encounter, but we continue.
            **  This is batch mode after all.
            */
            if(0 == finalRes)
            {
                finalRes = mapRes;
            }
            
            /*
            **  Close the input file.
            */
            fclose(inOptions->mInput);
        }
        else
        {
            retval = __LINE__;
            ERROR_REPORT(retval, lineBuf, "Unable to open map file.");
            break;
        }
    }

    if(0 == retval && 0 != ferror(realInput))
    {
        retval = __LINE__;
        ERROR_REPORT(retval, realInputName, "Unable to read file.");
    }

    /*
    **  Restore what we've swapped.
    */
    inOptions->mInput = realInput;
    inOptions->mInputName = realInputName;

    /*
    **  Report first map file error if there were no other operational
    **      problems.
    */
    if(0 == retval)
    {
        retval = finalRes;
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
        if(options.mBatchMode)
        {
            retval = batchMode(&options);
        }
        else
        {
            retval = map2tsv(&options);
        }
    }

    cleanOptions(&options);
    return retval;
}

