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
 * The Original Code is msmap.h code, released
 * Oct 3, 2002.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Garrett Arch Blythe, 03-October-2002
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
