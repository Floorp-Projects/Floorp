// ZipFileHeader.cpp: implementation of the CZipFileHeader class.
//
////////////////////////////////////////////////////////////////////////////////
//  Copyright (C) 2000 Tadeusz Dracz.
//  For conditions of distribution and use, see copyright notice in ZipArchive.h
////////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "ZipFileHeader.h"
#include "zlib.h"
#include "ZipAutoBuffer.h"
#include "ZipArchive.h"

#define ZipFileHeaderSIZE	46
#define LOCALZipFileHeaderSIZE	30
#define VERSIONMADEBY 20
#define ENCR_HEADER_LEN 12
//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////
char CZipFileHeader::m_gszSignature[] = {0x50, 0x4b, 0x01, 0x02};
char CZipFileHeader::m_gszLocalSignature[] = {0x50, 0x4b, 0x03, 0x04};
CZipFileHeader::CZipFileHeader()
{
	m_uExternalAttr = FILE_ATTRIBUTE_ARCHIVE;
	m_uModDate = m_uModTime = m_uInternalAttr = 0;
	m_uMethod = Z_DEFLATED;
}

CZipFileHeader::~CZipFileHeader()
{

}

// read the header from the central dir
bool CZipFileHeader::Read(CZipStorage *pStorage)
{
// 	// just in case
// 	m_pszComment.Release();
// 	m_pszFileName.Release();
	WORD uFileNameSize, uCommentSize, uExtraFieldSize;
	CZipAutoBuffer buf(ZipFileHeaderSIZE);
	pStorage->Read(buf, ZipFileHeaderSIZE, true);		
	memcpy(&m_szSignature,		buf, 4);
	memcpy(&m_uVersionMadeBy,	buf + 4, 2);
	memcpy(&m_uVersionNeeded,	buf + 6, 2);
	memcpy(&m_uFlag,			buf + 8, 2);
	memcpy(&m_uMethod,			buf + 10, 2);
	memcpy(&m_uModTime,			buf + 12, 2);
	memcpy(&m_uModDate,			buf + 14, 2);
	memcpy(&m_uCrc32,			buf + 16, 4);
	memcpy(&m_uComprSize,		buf + 20, 4);
	memcpy(&m_uUncomprSize,		buf + 24, 4);
	memcpy(&uFileNameSize,		buf + 28, 2);
	memcpy(&uExtraFieldSize,	buf + 30, 2);
	memcpy(&uCommentSize,		buf + 32, 2);
	memcpy(&m_uDiskStart,		buf + 34, 2);
	memcpy(&m_uInternalAttr,	buf + 36, 2);
	memcpy(&m_uExternalAttr,	buf + 38, 4);
	memcpy(&m_uOffset,			buf + 42, 4);
	buf.Release();

	if (memcmp(m_szSignature, m_gszSignature, 4) != 0)
		return false;

	int iCurDsk = pStorage->GetCurrentDisk();
	m_pszFileName.Allocate(uFileNameSize); // don't add NULL at the end
	pStorage->m_pFile->Read(m_pszFileName, uFileNameSize);
	if (uExtraFieldSize)
	{
		ASSERT(!m_pExtraField.IsAllocated());
		m_pExtraField.Allocate(uExtraFieldSize);
		pStorage->m_pFile->Read(m_pExtraField, uExtraFieldSize);
	}
	if (uCommentSize)
	{
		m_pszComment.Allocate(uCommentSize);
		pStorage->m_pFile->Read(m_pszComment, uCommentSize);
	}

	return pStorage->GetCurrentDisk() == iCurDsk; // check that the while header is on the one disk
}

// return CTime representation of m_uModDate, m_uModTime
CTime CZipFileHeader::GetTime()
{
	return CTime(m_uModDate, m_uModTime);
}

// write the header to the central dir
DWORD CZipFileHeader::Write(CZipStorage *pStorage)
{
	WORD uFileNameSize = GetFileNameSize(), uCommentSize = GetCommentSize(),
		uExtraFieldSize = GetExtraFieldSize();
	DWORD iSize = GetSize();
	CZipAutoBuffer buf(iSize);
	memcpy(buf, &m_szSignature, 4);
	memcpy(buf + 4, &m_uVersionMadeBy, 2);
	memcpy(buf + 6, &m_uVersionNeeded, 2);
	memcpy(buf + 8, &m_uFlag, 2);
	memcpy(buf + 10, &m_uMethod, 2);
	memcpy(buf + 12, &m_uModTime, 2);
	memcpy(buf + 14, &m_uModDate, 2);
	memcpy(buf + 16, &m_uCrc32, 4);
	memcpy(buf + 20, &m_uComprSize, 4);
	memcpy(buf + 24, &m_uUncomprSize, 4);
	memcpy(buf + 28, &uFileNameSize, 2);
	memcpy(buf + 30, &uExtraFieldSize, 2);
	memcpy(buf + 32, &uCommentSize, 2);
	memcpy(buf + 34, &m_uDiskStart, 2);
	memcpy(buf + 36, &m_uInternalAttr, 2);
	memcpy(buf + 38, &m_uExternalAttr, 4);
	memcpy(buf + 42, &m_uOffset, 4);

	memcpy(buf + 46, m_pszFileName, uFileNameSize);

	if (uExtraFieldSize)
		memcpy(buf + 46 + uFileNameSize, m_pExtraField, uExtraFieldSize);

	if (uCommentSize)
		memcpy(buf + 46 + uFileNameSize + uExtraFieldSize, m_pszComment, uCommentSize);

	pStorage->Write(buf, iSize, true);
	return iSize;
}

// read local header
bool CZipFileHeader::ReadLocal(CZipStorage *pStorage, WORD& iLocExtrFieldSize)
{
	char buf[LOCALZipFileHeaderSIZE];
	pStorage->Read(buf, LOCALZipFileHeaderSIZE, true);
	if (memcmp(buf, m_gszLocalSignature, 4) != 0)
		return false;

	bool bIsDataDescr = (((WORD)*(buf + 6)) & 8) != 0;

	WORD uFileNameSize = GetFileNameSize();
	if ((memcmp(buf + 6, &m_uFlag, 2) != 0)
		||(memcmp(buf + 8, &m_uMethod, 2) != 0)
		|| (m_uMethod && (m_uMethod != Z_DEFLATED))
		|| (memcmp(buf + 26, &uFileNameSize, 2) != 0))
		return false;

// jeszcze mo¿naby porównaæ nazwy plików

	if (!bIsDataDescr/* || !pStorage->IsSpanMode()*/)
		if (!CheckCrcAndSizes(buf + 14))
			return false;

	memcpy(&iLocExtrFieldSize, buf + 28, 2);
	pStorage->m_pFile->Seek(uFileNameSize, CFile::current);

	return true;
}

// set the m_uModDate, m_uModTime values using CTime object
void CZipFileHeader::SetTime(const CTime &time)
{
    WORD year = (WORD)time.GetYear();
    if (year <= 1980)
		year = 0;
	else
		year -= 1980;
    m_uModDate = (WORD) (time.GetDay() + (time.GetMonth() << 5) + (year << 9));
    m_uModTime = (WORD) ((time.GetSecond() >> 1) + (time.GetMinute() << 5) + 
		(time.GetHour() << 11));
}
//	the buffer contains crc32, compressed and uncompressed sizes to be compared 
//	with the actual values
bool CZipFileHeader::CheckCrcAndSizes(char *pBuf)
{
	return (memcmp(pBuf, &m_uCrc32, 4) == 0) && (memcmp(pBuf + 4, &m_uComprSize, 4) == 0)
		&& (memcmp(pBuf + 8, &m_uUncomprSize, 4) == 0);
}

// write the local header
void CZipFileHeader::WriteLocal(CZipStorage& storage)
{
	// extra field is local by now
	WORD uFileNameSize = GetFileNameSize(),	uExtraFieldSize = GetExtraFieldSize();
	DWORD iLocalSize = LOCALZipFileHeaderSIZE + uExtraFieldSize + uFileNameSize;
	CZipAutoBuffer buf(iLocalSize);
	memcpy(buf, m_gszLocalSignature, 4);
	memcpy(buf + 4, &m_uVersionNeeded, 2);
	memcpy(buf + 6, &m_uFlag, 2);
	memcpy(buf + 8, &m_uMethod, 2);
	memcpy(buf + 10, &m_uModTime, 2);
	memcpy(buf + 12, &m_uModDate, 2);
	memcpy(buf + 14, &m_uCrc32, 4);
	memcpy(buf + 18, &m_uComprSize, 4);
	memcpy(buf + 22, &m_uUncomprSize, 4);
	memcpy(buf + 26, &uFileNameSize, 2);
	memcpy(buf + 28, &uExtraFieldSize, 2);
	memcpy(buf + 30, m_pszFileName, uFileNameSize);
	memcpy(buf + 30 + uFileNameSize, m_pExtraField, uExtraFieldSize);

	// possible disk change before writting to the file in the disk spanning mode
	// so write the local header first 
	storage.Write(buf, iLocalSize, true);
	// it was only local information, use CZipArchive::SetExtraField to set the file extra field in the central directory
	m_pExtraField.Release();

	m_uDiskStart = (WORD)storage.GetCurrentDisk();
	m_uOffset = storage.GetPosition() - iLocalSize;
}

// prepare the data before adding a new file
bool CZipFileHeader::PrepareData(int iLevel, bool bExtraHeader, bool bEncrypted)
{
	memcpy(m_szSignature, m_gszSignature, 4);
	m_uInternalAttr = 0;
	m_uVersionMadeBy = VERSIONMADEBY;
	m_uVersionNeeded = 20;

	m_uCrc32 = 0;
	m_uComprSize = 0;
	m_uUncomprSize = 0;
	if (iLevel == 0)
		m_uMethod = 0;

	if ((m_uMethod != Z_DEFLATED) && (m_uMethod != 0))
		m_uMethod = Z_DEFLATED;

	m_uFlag  = 0;
	if (m_uMethod == Z_DEFLATED)
		switch (iLevel)
		{
		case 1:
			m_uFlag  |= 6;
			break;
		case 2:
			m_uFlag  |= 4;
			break;
		case 8:
		case 9:
			m_uFlag  |= 2;
			break;
		}

	if (bExtraHeader)
		m_uFlag  |= 8; // data descriptor present

	if (bEncrypted)
	{
		m_uComprSize = ENCR_HEADER_LEN;	// encrypted header
		m_uFlag  |= 9;		// encrypted file
	}

	return !(m_pszComment.GetSize() > USHRT_MAX || m_pszFileName.GetSize() > USHRT_MAX
		|| m_pExtraField.GetSize() > USHRT_MAX);
}

// fill the buffer with the current values
void CZipFileHeader::GetCrcAndSizes(char * pBuffer)
{
	memcpy(pBuffer, &m_uCrc32, 4);
	memcpy(pBuffer + 4, &m_uComprSize, 4);
	memcpy(pBuffer + 8, &m_uUncomprSize, 4);
}

DWORD CZipFileHeader::GetSize()
{
	return ZipFileHeaderSIZE + GetExtraFieldSize() + GetFileNameSize() + GetCommentSize();
}


bool CZipFileHeader::IsEncrypted()
{
	return (m_uFlag & (WORD) 1) != 0;
}

bool CZipFileHeader::IsDataDescr()
{
	return (m_uFlag & (WORD) 8) != 0;
}

bool CZipFileHeader::SetComment(LPCTSTR lpszComment)
{
	return CZipArchive::WideToSingle(lpszComment, m_pszComment)	!= -1;
}

CString CZipFileHeader::GetComment()
{
	CString temp;
	CZipArchive::SingleToWide(m_pszComment, temp);
	return temp;

}

bool CZipFileHeader::SetFileName(LPCTSTR lpszFileName)
{
	return CZipArchive::WideToSingle(lpszFileName, m_pszFileName) != -1;
}

CString CZipFileHeader::GetFileName()
{
	CString temp;
	CZipArchive::SingleToWide(m_pszFileName, temp);
	return temp;
}


void CZipFileHeader::SlashChange(bool bWindowsStyle)
{
	char t1 = '\\', t2 = '/', c1, c2;
	if (bWindowsStyle)
	{
		c1 = t1;
		c2 = t2;
	}
	else
	{
		c1 = t2;
		c2 = t1;
	}
	for (DWORD i = 0; i < m_pszFileName.GetSize(); i++)
	{
		if (m_pszFileName[i] == c2)
			m_pszFileName[i] = c1;
	}
}

void CZipFileHeader::AnsiOem(bool bAnsiToOem)
{
	if (bAnsiToOem)
		CharToOemBuffA(m_pszFileName, m_pszFileName, m_pszFileName.GetSize());
	else
		OemToCharBuffA(m_pszFileName, m_pszFileName, m_pszFileName.GetSize());
}
