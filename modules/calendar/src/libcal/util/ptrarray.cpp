/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- 
 * 
 * The contents of this file are subject to the Netscape Public License 
 * Version 1.0 (the "NPL"); you may not use this file except in 
 * compliance with the NPL.  You may obtain a copy of the NPL at 
 * http://www.mozilla.org/NPL/ 
 * 
 * Software distributed under the NPL is distributed on an "AS IS" basis, 
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL 
 * for the specific language governing rights and limitations under the 
 * NPL. 
 * 
 * The Initial Developer of this code under the NPL is Netscape 
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights 
 * Reserved. 
 */

/**
 *  a pointer array with no external references into the source tree.
 *
 *  With apologies to the original authors
 *  sman
 */
#include "stdafx.h"
#include "jdefines.h"

#include <string.h>
#include <stdlib.h>

#include "ptrarray.h"

#ifndef MAX 
#define MAX(a,b) (((a)>(b))?(a):(b))
#endif

#ifndef MIN 
#define MIN(a,b) (((a)<(b))?(a):(b))
#endif


#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#ifdef XP_WIN16
#define SIZE_T_MAX 0xFF80		// a little less than 64K, the max alloc size on win16.
#define MAX_ARR_ELEMS SIZE_T_MAX/sizeof(void*)
#endif

/////////////////////////////////////////////////////////////////////////////

JulianPtrArray::JulianPtrArray()
{
    m_pData = 0;
    m_nSize = m_nMaxSize = m_nGrowBy = 0;
}

JulianPtrArray::~JulianPtrArray()
{
//  XP_ASSERT_VALID(this);

    delete[] (char*)m_pData;
}

XP_Bool JulianPtrArray::SetSize(int nNewSize, int nGrowBy)
{
//  XP_ASSERT_VALID(this);
//  XP_ASSERT(nNewSize >= 0);

    if (nGrowBy != -1)
	m_nGrowBy = nGrowBy;  // set new size

    if (nNewSize == 0)
    {
	// shrink to nothing
	delete[] (char*)m_pData;
	m_pData = 0;
	m_nSize = m_nMaxSize = 0;
    }
    else if (m_pData == 0)
    {
	// create one with exact size
#ifdef SIZE_T_MAX
//		XP_ASSERT(nNewSize <= SIZE_T_MAX/sizeof(void*));    // no overflow
#endif
	m_pData = (void**) new char[nNewSize * sizeof(void*)];
	if (m_pData == 0)
	{
	    m_nSize = 0;
	    return 0;
	}

	memset(m_pData, 0, nNewSize * sizeof(void*));  // zero fill

	m_nSize = m_nMaxSize = nNewSize;
    }
    else if (nNewSize <= m_nMaxSize)
    {
	// it fits
	if (nNewSize > m_nSize)
	{
	    // initialize the new elements
	    memset(&m_pData[m_nSize], 0, (nNewSize-m_nSize) * sizeof(void*));
	}

	m_nSize = nNewSize;
    }
    else
    {
	    // otherwise, grow array
	    int nGrowBy = m_nGrowBy;
	    if (nGrowBy == 0)
	    {
		// heuristically determine growth when nGrowBy == 0
		//  (this avoids heap fragmentation in many situations)
		nGrowBy = MIN(1024, MAX(4, m_nSize / 8));
	    }
#ifdef MAX_ARR_ELEMS
	    if (m_nSize + nGrowBy > MAX_ARR_ELEMS)
		    nGrowBy = MAX_ARR_ELEMS - m_nSize;
#endif
	    int nNewMax;
	    if (nNewSize < m_nMaxSize + nGrowBy)
		nNewMax = m_nMaxSize + nGrowBy;  // granularity
	    else
		nNewMax = nNewSize;  // no slush

#ifdef SIZE_T_MAX
	    if (nNewMax >= SIZE_T_MAX/sizeof(void*))
		return 0;
//	    XP_ASSERT(nNewMax <= SIZE_T_MAX/sizeof(void*)); // no overflow
#endif
//	    XP_ASSERT(nNewMax >= m_nMaxSize);  // no wrap around
	    void** pNewData = (void**) new char[nNewMax * sizeof(void*)];

	    if (pNewData != 0)
	    {
		// copy new data from old
		memcpy(pNewData, m_pData, m_nSize * sizeof(void*));

		// construct remaining elements
//		XP_ASSERT(nNewSize > m_nSize);

		memset(&pNewData[m_nSize], 0, (nNewSize-m_nSize) * sizeof(void*));


		// get rid of old stuff (note: no destructors called)
		delete[] (char*)m_pData;
		m_pData = pNewData;
		m_nSize = nNewSize;
		m_nMaxSize = nNewMax;
	    }
	    else
	    {
		return 0;
	    }
    }
    return 1;
}

void JulianPtrArray::FreeExtra()
{
//	XP_ASSERT_VALID(this);

	if (m_nSize != m_nMaxSize)
	{
		// shrink to desired size
#ifdef SIZE_T_MAX
//		XP_ASSERT(m_nSize <= SIZE_T_MAX/sizeof(void*)); // no overflow
#endif
		void** pNewData = 0;
		if (m_nSize != 0)
		{
			pNewData = (void**) new char[m_nSize * sizeof(void*)];
			// copy new data from old
			memcpy(pNewData, m_pData, m_nSize * sizeof(void*));
		}

		// get rid of old stuff (note: no destructors called)
		delete[] (char*)m_pData;
		m_pData = pNewData;
		m_nMaxSize = m_nSize;
	}
}

/////////////////////////////////////////////////////////////////////////////

void JulianPtrArray::SetAtGrow(int nIndex, void* newElement)
{
//	XP_ASSERT_VALID(this);
//	XP_ASSERT(nIndex >= 0);

	if (nIndex >= m_nSize)
		SetSize(nIndex+1);
	m_pData[nIndex] = newElement;
}

void JulianPtrArray::InsertAt(int nIndex, void* newElement, int nCount)
{
//	XP_ASSERT_VALID(this);
//	XP_ASSERT(nIndex >= 0);    // will expand to meet need
//	XP_ASSERT(nCount > 0);     // zero or negative size not allowed

	if (nIndex >= m_nSize)
	{
		// adding after the end of the array
		SetSize(nIndex + nCount);  // grow so nIndex is valid
	}
	else
	{
		// inserting in the middle of the array
		int nOldSize = m_nSize;
		SetSize(m_nSize + nCount);  // grow it to new size
		// shift old data up to fill gap
		memmove(&m_pData[nIndex+nCount], &m_pData[nIndex],
			(nOldSize-nIndex) * sizeof(void*));

		// re-init slots we copied from

		memset(&m_pData[nIndex], 0, nCount * sizeof(void*));

	}

	// insert new value in the gap
//	XP_ASSERT(nIndex + nCount <= m_nSize);
	while (nCount--)
		m_pData[nIndex++] = newElement;
}

// returns 1 if ptr was found, and therefore removed.
XP_Bool JulianPtrArray::Remove(void *pToRemove)
{
	int index = FindIndex(0, pToRemove);
	if (index != -1)
	{
		RemoveAt(index);
		return 1;
	}
	else
		return 0;

}

void JulianPtrArray::RemoveAt(int nIndex, int nCount)
{
//	XP_ASSERT_VALID(this);
//	XP_ASSERT(nIndex >= 0);
//	XP_ASSERT(nCount >= 0);
//	XP_ASSERT(nIndex + nCount <= m_nSize);

	// just remove a range
	int nMoveCount = m_nSize - (nIndex + nCount);

	if (nMoveCount)
		/*XP_MEMMOVE*/ memmove(&m_pData[nIndex], &m_pData[nIndex + nCount],
			nMoveCount * sizeof(void*));
	m_nSize -= nCount;
}

void JulianPtrArray::InsertAt(int nStartIndex, const JulianPtrArray* pNewArray)
{
//	XP_ASSERT_VALID(this);
//	XP_ASSERT(pNewArray != 0);
//	XP_ASSERT_VALID(pNewArray);
//	XP_ASSERT(nStartIndex >= 0);

	if (pNewArray->GetSize() > 0)
	{
		InsertAt(nStartIndex, pNewArray->GetAt(0), pNewArray->GetSize());
		for (int i = 0; i < pNewArray->GetSize(); i++)
			SetAt(nStartIndex + i, pNewArray->GetAt(i));
	}
}

// Remove all elements in pArray from 'this'
void JulianPtrArray::RemoveAt (int nStartIndex, const JulianPtrArray *pArray)
{
//	XP_ASSERT(pArray!= 0);
//	XP_ASSERT(nStartIndex >= 0);

	if (pArray->GetSize() > 0)
		for (int i = 0; i < pArray->GetSize(); i++)
		{
			int index = FindIndex(nStartIndex, pArray->GetAt(i));
			if (index >= 0)
				RemoveAt (index);
		}
}

void JulianPtrArray::QuickSort (int ( *compare )(const void *elem1, const void *elem2 ))
{
	if (m_nSize > 1)
		/*XP_QSORT*/ qsort(m_pData, m_nSize, sizeof(void*), compare);

}

int JulianPtrArray::FindIndex (int nStartIndex, void *pToFind) const
{
	for (int i = nStartIndex; i < GetSize(); i++)
		if (m_pData[i] == pToFind)
			return i;
	return -1;
}

int JulianPtrArray::GetSize() const 	    { return m_nSize; }
int JulianPtrArray::GetUpperBound() const 	{ return m_nSize-1; }
void JulianPtrArray::RemoveAll()       	    { SetSize(0); }
 
void* JulianPtrArray::GetAt(int nIndex) const
{
//    XP_ASSERT(nIndex >= 0 && nIndex < m_nSize);
    return m_pData[nIndex]; 
}

void JulianPtrArray::SetAt(int nIndex, void* newElement)
{
//    XP_ASSERT(nIndex >= 0 && nIndex < m_nSize);
    m_pData[nIndex] = newElement; 
}

void*& JulianPtrArray::ElementAt(int nIndex)
{
//    XP_ASSERT(nIndex >= 0 && nIndex < m_nSize);
	return m_pData[nIndex]; 
}

int JulianPtrArray::Add(void* newElement)
{
	int nIndex = m_nSize;
#ifdef XP_WIN16
	if (nIndex >= 0x10000L / 4L) 
	{
//			XP_ASSERT(0);
		return -1;	     
	}
#endif			
	SetAtGrow(nIndex, newElement);
	return nIndex; 
}

void* JulianPtrArray::operator[](int nIndex) const 	{ return GetAt(nIndex); }
void*& JulianPtrArray::operator[](int nIndex)       { return ElementAt(nIndex); }


XP_Bool JulianPtrArray::IsValidIndex(int32 nIndex)
{
	return (nIndex < GetSize() && nIndex >= 0);
}

int JulianPtrArray::InsertBinary(void* newElement, int ( *compare )(const void *elem1, const void *elem2 ))
{
	int		current = 0;
	int		left = 0;
	int		right = GetSize() - 1;
	int		comparison = 0;
	
	while (left <= right)
	{
		current = (left + right) / 2;
		
		void* pCurrent = GetAt(current);

		comparison = compare(&pCurrent, &newElement);
		// comparison = compare(pCurrent, newElement);
										  
		if (comparison == 0)
			break;
		else if (comparison > 0)
			right = current - 1;
		else
			left = current + 1;
	}
	
	if (comparison < 0)
		current += 1;
	
	JulianPtrArray::InsertAt(current, newElement);
	return current;
}
