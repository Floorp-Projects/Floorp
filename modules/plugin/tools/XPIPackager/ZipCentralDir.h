// ZipCentralDir.h: interface for the CZipCentralDir class.
//
////////////////////////////////////////////////////////////////////////////////
//  Copyright (C) 2000 Tadeusz Dracz.
//  For conditions of distribution and use, see copyright notice in ZipArchive.h
////////////////////////////////////////////////////////////////////////////////

#if !defined(AFX_ZipCentralDir_H__859029E8_8927_4717_9D4B_E26E5DA12BAE__INCLUDED_)
#define AFX_ZipCentralDir_H__859029E8_8927_4717_9D4B_E26E5DA12BAE__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
#include "ZipException.h"
#include <afxtempl.h>
#include "ZipFileHeader.h"
#include "ZipAutoBuffer.h"

/**
	Used in fast finding files by the filename.
	\see CZipCentralDir::m_findarray
	\see CZipArchive::FindFile
*/
struct CZipFindFast
{
	CZipFindFast()
	{
		m_uIndex = 0;
		m_pHeader= NULL;
	}
	CZipFindFast(CZipFileHeader* pHeader, WORD uIndex):m_pHeader(pHeader), m_uIndex(uIndex){}
	/**
		We extract a name from it.
	*/
	CZipFileHeader* m_pHeader;

	/**
		The index in the central directory of the \e m_pHeader.
	*/
	WORD m_uIndex;
};


class CZipCentralDir  
{

public:
	
//		end of central dir signature    4 bytes  (0x06054b50)
	char m_szSignature[4];
//		number of this disk             2 bytes
	WORD m_uThisDisk;
//		number of the disk with the
//		start of the central directory  2 bytes
	WORD m_uDiskWithCD;
//		total number of entries in
//		the central dir on this disk    2 bytes
	WORD m_uDiskEntriesNo;
//		total number of entries in
//		the central dir                 2 bytes
	WORD m_uEntriesNumber;
//		size of the central directory   4 bytes
	DWORD m_uSize;
//		offset of start of central
//		directory with respect to
//		the starting disk number        4 bytes
	DWORD m_uOffset;
//		zipfile comment length          2 bytes
// 	WORD m_uCommentSize;
//		zipfile comment (variable size)
	CZipAutoBuffer m_pszComment;
	bool m_bFindFastEnabled;
	CZipFileHeader* m_pOpenedFile;
	void RemoveFile(WORD uIndex);
	void Clear(bool bEverything = true);
	CZipStorage* m_pStorage;
	DWORD m_uCentrDirPos;
	DWORD m_uBytesBeforeZip;
	CZipCentralDir();
	virtual ~CZipCentralDir();
	void CloseFile();
	void OpenFile(WORD uIndex);
	bool IsValidIndex(WORD uIndex);
	void Read();
	void Init();
	void CloseNewFile();
	void Write();
	int m_iBufferSize;
	bool m_bOnDisk;
	static char m_gszSignature[];
	CTypedPtrArray<CPtrArray, CZipFileHeader*> m_headers;
	CZipAutoBuffer m_pLocalExtraField;
	void AddNewFile(CZipFileHeader & header);
	void RemoveFromDisk();
	DWORD GetSize(bool bWhole = false);
	CArray<CZipFindFast, CZipFindFast> m_findarray;
	int FindFileNameIndex(LPCTSTR lpszFileName, bool bCaseSensitive);
	void BuildFindFastArray();
	/**
		- If \c true, the conversion of the filenames takes 
		place after opening the archive (after reading the central directory 
		from the file), and before writing the central directory back to
		the archive.
		- If \c false, the conversion takes place on each call to CZipArchive::GetFileInfo

		Set it to \c true when you plan to use CZipArchive::FindFile or get the stored files information. <BR>
		Set it to \c false when you plan mostly to only modify the archive.

		\b Default: \c true
		\note Set it before opening the archive.
		\see ConvertFileName
	*/
	bool m_bConvertAfterOpen;

/**
	Convert the filename of the CZipFileHeader.
	\param	bFromZip
		if \c true, convert from archive format
	\param	bAfterOpen
		if \c true, called after opening the archive or before closing
	\param	pHeader		
		the header to have filename converted; if \c NULL convert the currently
		opened file
	\see m_bConvertAfterOpen
*/
	void ConvertFileName(bool bFromZip, bool bAfterOpen, CZipFileHeader* pHeader = NULL)
	{
		if (bAfterOpen != m_bConvertAfterOpen)
			return;
		if (!pHeader)
		{
			pHeader = m_pOpenedFile;
			ASSERT(pHeader);
		}
		pHeader->AnsiOem(!bFromZip);
		pHeader->SlashChange(bFromZip);
	}
	void ConvertAll();
protected:
	void InsertFindFastElement(CZipFileHeader* pHeader, WORD uIndex);
	void RemoveHeaders();
	bool RemoveDataDescr(bool bFromBuffer);
	DWORD WriteCentralEnd();
	void WriteHeaders();
	void ReadHeaders();
	void ThrowError(int err);
	DWORD Locate();	
	int CompareElement(LPCTSTR lpszFileName, WORD uIndex, bool bCaseSensitive)
	{
		return bCaseSensitive ? m_findarray[uIndex].m_pHeader->GetFileName().Collate(lpszFileName)
			: m_findarray[uIndex].m_pHeader->GetFileName().CollateNoCase(lpszFileName);
	}

};

#endif // !defined(AFX_ZipCentralDir_H__859029E8_8927_4717_9D4B_E26E5DA12BAE__INCLUDED_)
