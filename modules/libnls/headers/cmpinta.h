//==========================================================================
/*
 * Copyright (c) 1995 Taligent, Inc. All Rights Reserved.
 * @version	1.0 23/10/96
 * @author	Helena Shih
 * Based on Taligent international support for java
 */
//===============================================================================
//
// File cmpinta.h
//
// Modification History:
//
//	Date		Name		Description
//	2/5/97		aliu		Added CompactIntArray streamIn and streamOut methods.
// 05/07/97     helena      Added isBogus()
//===============================================================================

#ifndef _CMPINTA
#define _CMPINTA

#include <stdio.h>

#ifndef _PTYPES
#include "ptypes.h"
#endif

//====================================
// class CompactIntArray
// Provides a compact way to store information that is indexed by Unicode values,
// such as character properties, types, keyboard values, etc.
// The ATypes are used by value, so should be small, integers or pointers.
//====================================

#ifdef NLS_MAC
#pragma export on
#endif

class T_UTILITY_API CompactIntArray {
 public:

    static  const t_int32 kUnicodeCount;
    static  const t_int32 kBlockShift;
    static  const t_int32 kBlockCount;
    static  const t_int32 kIndexShift;
    static  const t_int32 kIndexCount;
    static  const t_uint32 kBlockMask;

	CompactIntArray(t_int32 defaultValue);
	CompactIntArray(t_uint16 *indexArray, t_int32 *newValues, t_int32 count);
	CompactIntArray(const CompactIntArray& that);
	~CompactIntArray();

    CompactIntArray& operator=(const CompactIntArray& that);

	// Returns TRUE if the creation of the compact array fails.
	t_bool isBogus() const;

	// Get is inline for speed
	t_int32 get(UniChar index);

	// Set automatically expands the array if it is compacted.

	void set(UniChar index, t_int32 value);
	void set(UniChar start, UniChar end, t_int32 value);

	// Compact the array.
	// The value of cycle determines how large the overlap can be.
	// A cycle of 1 is the most compacted, but takes the most time to do.
	// If values stored in the array tend to repeat in cycles of, say, 16,
	// then using that will be faster than cycle = 1, and get almost the
	// same compression.

	void compact (t_uint32 cycle = 1/* default = 1 */);

	// Expanded takes the array back to a 65536 element array

	void expand();

	// Print Unichar Array  : Debug only

	void printIndex(t_int32 start, t_int32 count);

	void printPlainArray(t_int32 start,t_int32 count, const UniChar* tempIndex);
		
	// # of elements in the indexed array
	t_int32 getCount();
	void setCount(t_int32 n);
	const t_int32* getArray();
	const t_uint16* getIndex();

    /**
     * The streamIn and streamOut methods read and write objects of this
     * class as binary, platform-dependent data in the iostream.  The stream
     * must be in ios::binary mode for this to work.  These methods are not
     * intended for general public use; they are used by the framework to improve
     * performance by storing certain objects in binary files.
     */
    void                streamOut(FILE* os) const;
    void                streamIn(FILE* is);

protected:

	t_int32 findOverlappingPosition(t_uint32 start, 
	                            const UniChar *tempIndex, 
	                            t_int32 tempIndexCount, 
	                            t_uint32 cycle);
private:		

    static  const t_bool debugShowOverlap;
    static  const t_uint32 debugShowOverlapLimit;
    static  const t_bool debugTrace;
    static  const t_bool debugSmall;
    static  const t_uint32 debugSmallLimit;

	t_int32 NLS_HUGE * fArray;
	t_uint16* fIndex;
	t_int32 fCount;
	t_bool fCompact;	
	t_bool fBogus;
};

#ifdef NLS_MAC
#pragma export off
#endif

inline t_int32 
CompactIntArray::get(UniChar c)
{
    return (fArray[(fIndex[c >> kBlockShift] & 0xFFFF)
                   + (c & kBlockMask)]);
}

inline t_bool
CompactIntArray::isBogus() const { return fBogus; }

#endif
