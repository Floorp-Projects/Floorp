// ZipBigFile.cpp: implementation of the CZipBigFile class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "ZipBigFile.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNAMIC(CZipBigFile, CFile)

CZipBigFile::CZipBigFile()
{

}

CZipBigFile::~CZipBigFile()
{

}

_int64 CZipBigFile::Seek(_int64 dOff, UINT nFrom)
{
	ASSERT_VALID(this);
	ASSERT(m_hFile != (UINT)hFileNull);
	ASSERT(nFrom == begin || nFrom == end || nFrom == current);
	ASSERT(begin == FILE_BEGIN && end == FILE_END && current == FILE_CURRENT);
	LARGE_INTEGER li;
	li.QuadPart = dOff;

	li.LowPart  = ::SetFilePointer((HANDLE)m_hFile, li.LowPart, &li.HighPart, (DWORD)nFrom);
	DWORD dw = GetLastError();
	if ((li.LowPart == (DWORD)-1) && (dw != NO_ERROR))
	{
		CFileException::ThrowOsError((LONG)dw);
	}

	return li.QuadPart;

}
