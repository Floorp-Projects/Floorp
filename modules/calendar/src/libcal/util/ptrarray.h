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

/* Insert copyright and license here 1996 */
// ptrarray.h
#ifndef JULIAN_PTR_ARRAY_H_
#define JULIAN_PTR_ARRAY_H_

#ifdef XP_UNIX
#define __cdecl
#endif

/*
 *  Need to fix this...
 */
#if defined(WIN32)
#define XP_Bool int
#define int32 int
#elif defined(unix)
#define XP_Bool int
#define int32 int
#endif

typedef int XPCompareFunc(const void *, const void * );

class JULIAN_PUBLIC JulianPtrArray 
{

public:

// Construction
	JulianPtrArray();

// Attributes
	int GetSize() const;
	int GetUpperBound() const;
	XP_Bool SetSize(int nNewSize, int nGrowBy = -1);
	XP_Bool IsValidIndex(int32 nIndex);
// Operations
	// Clean up
	void FreeExtra();
	void RemoveAll();

	// Accessing elements
	void* GetAt(int nIndex) const;
	void SetAt(int nIndex, void* newElement);
	void*& ElementAt(int nIndex);
	virtual int FindIndex (int nStartIndex, void *pToFind) const;

	// Potentially growing the array
	void SetAtGrow(int nIndex, void* newElement);
	virtual int Add(void* newElement);

	// overloaded operator helpers
	void* operator[](int nIndex) const;
	void*& operator[](int nIndex);

	// Operations that move elements around
	void	InsertAt(int nIndex, void* newElement, int nCount = 1);
	void	RemoveAt(int nIndex, int nCount = 1);
	void	InsertAt(int nStartIndex, const JulianPtrArray* pNewArray);
	void	RemoveAt(int nStartIndex, const JulianPtrArray* pNewArray);
	XP_Bool Remove(void *pToRemove);
	void	QuickSort(XPCompareFunc *compare);
	int		InsertBinary(void* newElement, XPCompareFunc *compare);

// Implementation
protected:
	void** m_pData;   // the actual array of data
	int m_nSize;     // # of elements (upperBound - 1)
	int m_nMaxSize;  // max allocated
	int m_nGrowBy;   // grow amount

public:
	virtual ~JulianPtrArray();
#ifdef _DEBUG
	void AssertValid() const;
#endif

};


#endif /* JULIAN_PTR_ARRAY_H_ */
