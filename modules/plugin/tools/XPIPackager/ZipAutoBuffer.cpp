// ZipAutoBuffer.cpp: implementation of the CZipAutoBuffer class.
// Part of the ZipArchive library
// 
// Copyright (C) 2000 - 2001 Tadeusz Dracz.
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
// 
// For the licensing details see the file License.txt
////////////////////////////////////////////////////////////////////////////////


#include "stdafx.h"
#include "ZipAutoBuffer.h"
#include <memory.h> 

// #ifdef _DEBUG
// #undef THIS_FILE
// static char THIS_FILE[]=__FILE__;
// #define new DEBUG_NEW
// #endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CZipAutoBuffer::CZipAutoBuffer()
{
	m_iSize = 0;
	m_pBuffer = NULL;	
}

CZipAutoBuffer::CZipAutoBuffer(DWORD iSize, bool bZeroMemory)
{
	m_iSize = 0;
	m_pBuffer = NULL;
	Allocate(iSize, bZeroMemory);
}

CZipAutoBuffer::~CZipAutoBuffer()
{
	Release();
}


void CZipAutoBuffer::Release()
{
	if (m_pBuffer)
	{
		delete [] m_pBuffer;
		m_iSize = 0;
		m_pBuffer = NULL;
	}
}

char* CZipAutoBuffer::Allocate(DWORD iSize, bool bZeroMemory)
{
	if (iSize != m_iSize)
		Release();
	else
	{
		if (bZeroMemory)
			memset(m_pBuffer, 0, iSize); // zerowanie bufora
		return m_pBuffer;
	}

	if (iSize > 0)
	{
			m_pBuffer = new char [iSize];
			if (bZeroMemory)
				memset(m_pBuffer, 0, iSize); // zerowanie bufora
			m_iSize = iSize;
	}
	else 
		m_pBuffer = NULL;

	return m_pBuffer;
}


CZipAutoBuffer::CZipAutoBuffer(const CZipAutoBuffer& buffer)
{
	m_pBuffer = NULL;
	m_iSize = 0;

	if (buffer.m_pBuffer)
	{
		Allocate(buffer.m_iSize);
		memcpy(m_pBuffer, buffer.m_pBuffer, buffer.m_iSize);
	}	
}
CZipAutoBuffer& CZipAutoBuffer::operator=(const CZipAutoBuffer& buffer)
{
	if (this == &buffer)
		return *this;
	Release();
	if (buffer.m_pBuffer)
	{
		Allocate(buffer.m_iSize);
		memcpy(m_pBuffer, buffer.m_pBuffer, buffer.m_iSize);
	}
	return *this;
}