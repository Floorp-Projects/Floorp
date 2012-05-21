/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined __msmap_H
#define __msmap_H


#if defined(__cplusplus)
extern "C" {
#endif
#if 0
}
#endif


/*
**  Used to numerically represent addresses.
*/
typedef unsigned long address;


typedef enum __enum_MSMap_SymbolScope
/*
**  Symbol scope.
*/
{
    PUBLIC,
    STATIC,
    UNDEFINED
}
MSMap_SymbolScope;


typedef enum __enum_MSMap_SegmentClass
/*
**  Segment class.
*/
{
    CODE,
    DATA
}
MSMap_SegmentClass;


typedef struct __struct_MSMap_Segment
/*
**  Information about a segment.
*/
{
    address mPrefix;
    address mOffset;
    address mLength;
    address mUsed;
    char* mSegment;
    MSMap_SegmentClass mClass;
}
MSMap_Segment;


typedef struct __struct_MSMap_Symbol
/*
**  Information about a symbol.
*/
{
    address mPrefix;
    address mOffset;
    char* mSymbol;
    address mRVABase;
    char* mObject;
    MSMap_SymbolScope mScope;
    unsigned mSymDBSize;
    MSMap_Segment* mSection;
}
MSMap_Symbol;


typedef struct __struct_MSMap_Module
/*
**  Top level container of the map data.
*/
{
    char* mModule;
    time_t mTimestamp;
    address mPreferredLoadAddress;
    MSMap_Segment* mSegments;
    unsigned mSegmentCount;
    unsigned mSegmentCapacity;
    address mEntryPrefix;
    address mEntryOffset;
    MSMap_Symbol* mSymbols;
    unsigned mSymbolCount;
    unsigned mSymbolCapacity;
}
MSMap_Module;


/*
**  How much to grow our arrays by.
*/
#define MSMAP_SEGMENT_GROWBY 0x10
#define MSMAP_SYMBOL_GROWBY  0x100


#if 0
{
#endif
#if defined(__cplusplus)
} /* extern "C" */
#endif


#endif /* __msmap_H */
