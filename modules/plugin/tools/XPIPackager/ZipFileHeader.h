// ZipFileHeader.h: interface for the CZipFileHeader class.
//
////////////////////////////////////////////////////////////////////////////////
//  Copyright (C) 2000 Tadeusz Dracz.
//  For conditions of distribution and use, see copyright notice in ZipArchive.h
////////////////////////////////////////////////////////////////////////////////

#if !defined(AFX_ZipFileHeader_H__0081FC65_C9C9_4D48_AF72_DBF37DF5E0CF__INCLUDED_)
#define AFX_ZipFileHeader_H__0081FC65_C9C9_4D48_AF72_DBF37DF5E0CF__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "ZipException.h"
#include "ZipStorage.h"
#include "ZipAutoBuffer.h"

class CZipFileHeader  
{
public:	
// convert characters in the filename from oem to ansi or vice-versa	
	void AnsiOem(bool bAnsiToOem);
// change slash to backslash or vice-versa	
	void SlashChange(bool bWindowsStyle);
// return the filename size in characters (without NULL);	
	WORD GetFileNameSize(){return (WORD)m_pszFileName.GetSize();}
// return the comment size in characters (without NULL);		
	WORD GetCommentSize(){return (WORD)m_pszComment.GetSize();}
// return the extra field size in characters (without NULL);		
	WORD GetExtraFieldSize(){return (WORD)m_pExtraField.GetSize();}

	CString GetFileName();
// return true if confersion from unicode to single byte was successful	
	bool SetFileName(LPCTSTR lpszFileName);
	CString GetComment();
// return true if confersion from unicode to single byte was successful	
	bool SetComment(LPCTSTR lpszComment);
// return true if the data descriptor is present	
	bool IsDataDescr();
//	return true if the file is encrypted
	bool IsEncrypted();
//         central file header signature   4 bytes  (0x02014b50)
	char m_szSignature[4];
//         version made by                 2 bytes
	WORD m_uVersionMadeBy;
//         version needed to extract       2 bytes
	WORD m_uVersionNeeded;
//         general purpose bit flag        2 bytes
	WORD m_uFlag;
//         compression method              2 bytes
	WORD m_uMethod;
//         last mod file time              2 bytes
	WORD m_uModTime;
//         last mod file date              2 bytes
	WORD m_uModDate;
//         crc-32                          4 bytes
	DWORD m_uCrc32;
//         compressed size                 4 bytes
	DWORD m_uComprSize;
//         uncompressed size               4 bytes
	DWORD m_uUncomprSize;
//         filename length                 2 bytes
// 	WORD m_uFileNameSize;
//         extra field length              2 bytes
// 	WORD m_uExtraFieldSize;
//         file comment length             2 bytes
// 	WORD m_uCommentSize;
//         disk number start               2 bytes
	WORD m_uDiskStart;
//         internal file attributes        2 bytes
	WORD m_uInternalAttr;
//         external file attributes        4 bytes
	DWORD m_uExternalAttr;
//         relative offset of local header 4 bytes
	DWORD m_uOffset;	
//         extra field (variable size)
	CZipAutoBuffer m_pExtraField;

	CZipFileHeader();
	virtual ~CZipFileHeader();
	CTime GetTime();
	void SetTime(const CTime& time);
	static char m_gszSignature[];
	static char m_gszLocalSignature[];
// return the total size of the structure as stored in the central directory	
	DWORD GetSize();
protected:
//         filename (variable size)
	CZipAutoBuffer m_pszFileName;	
//         file comment (variable size)
	CZipAutoBuffer m_pszComment;

	void GetCrcAndSizes(char * pBuffer);
	bool PrepareData(int iLevel, bool bExtraHeader, bool bEncrypted);
	void WriteLocal(CZipStorage& storage);
	bool CheckCrcAndSizes(char* pBuf);
	friend class CZipCentralDir;
	friend class CZipArchive;
	bool Read(CZipStorage *pStorage);
	bool ReadLocal(CZipStorage *pStorage, WORD& iLocExtrFieldSize);
	DWORD Write(CZipStorage *pStorage);

};

#endif // !defined(AFX_ZipFileHeader_H__0081FC65_C9C9_4D48_AF72_DBF37DF5E0CF__INCLUDED_)
