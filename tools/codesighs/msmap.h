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
}
MSMap_Symbol;


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
    char* mSegment;
    MSMap_SegmentClass mClass;
}
MSMap_Segment;


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
