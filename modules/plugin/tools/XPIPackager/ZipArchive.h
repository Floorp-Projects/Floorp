// ZipArchive.h: interface for the CZipArchive class.
//
//////////////////////////////////////////////////////////////////////
//   ZipArchive 1.5.1, March 2001
// 
// 
//	 This library allows to crate ZIP files in the compatible way with 
//		PKZIP version 2.6. Some important issues:
//		- multiple disk spanning is supported
//		- encyption is not supported
//		- allows to create disk spanning archive also on non removable devices
//			and with user-defined volume size
//		- this library uses the zlib library by Jean-loup Gailly and Mark Adler
//			to perform inflate, deflate operations
// 
// 
// 
//   Copyright (C) 2000 - 2001 Tadeusz Dracz
//		
// 
//  Permission is granted to anyone to use this software for any purpose 
//  and to alter it and redistribute it freely, subject to the 
//  following restrictions:
// 
//   1. Using this software in commercial applications requires an author permission.
//      The permission will be granted to everyone excluding the cases when 
//      someone simply tries to resell the code.
// 
//   2. The origin of this software must not be misrepresented; you must not
//      claim that you wrote the original software. If you use this software
//      in a product, an acknowledgment in the product documentation would be
//      appreciated but is not required.
// 
//   3. Altered source versions must be plainly marked as such, and must not be
//      misrepresented as being the original software.
// 
//   4. This notice may not be removed or altered from any source distribution.
// 
// 
//		You can contact me at:
//		tdracz@artpol.com.pl
//
//		For new versions check the site:
//		http://software.artpol.com.pl
// 
//		History:
// 03.2001
// 
// - when the central directory was not located, the library throws CZipException::cdirNotFound,
// which allows distinguish from other exceptions (useful when we want to keep prompting
// the user to insert the last disk in a multi-disk spanning archive). 
//  
// 
// 02.2001 
//	Features added: 
//		- ability to reuse the archive after an exception thrown during extraction 
//		- added progress control possibilities to CZipArchive::AddNewFile, CZipArchive::ExtractFile, CZipArchive::TestFile 
// Changes: 
//		- CZipArchive::FindFile operation boosted ( thanks to Darin Warling for the idea) 
//		- library name changed to ZipArchive 
// Bugs fixed: 
//		- removed bug during extracting an encrypted file with 0 size 
//		- fixed bug when extracting a file with an extra field in a local file header (CZipFileHeader::ReadLocal) 
// 
//  01.2001
// 		- Disk numbering in a disk spanning archive begins now from PKBACK# 001 not PKBACK# 000
// 		- Adding and extracting without a full path possible in CZipArchive::AddNewFile and CZipArchive::ExtractFile.
// 		- Several minor changes and enhancements
// 		- Changed names for several classes.
// 
//  11.2000
//  	- Added support for password encryption and decryption.
// 			The encryption used in PKZIP was generously supplied by Roger
// 			Schlafly.  
// 		- Testing the archive made easier
// 		- Unicode support added
// 
//  08.2000
// 		- Bugs fixed
// 
//  06.2000
// 
// 		the code has been completely rewritten since the very beginning;
// 		the main improvements are:
// 		- multi-disk archive support
// 		- creation of the disk spanning archives with the user-defined volume size
// 		- ability to modify existing archives (add, delete files)
// 		- modification self-extracting archives
// 		- write buffer used for faster disk write operations 
// 		- one class for zip and unzip functions
// 		- fast adding, deleting and extracting files with a single function call
//  
//  03.2000
// 		- international characters in filenames inside archive are now
// 			converted in a compatible way with other archiving programs (they are stored
// 		converted to OEM inside archive).
// 
//  01.2000
// 
// 		first version; it is just modified code from zip.c and unzip.c files
// 			written by Gilles Vollant and distributed with zlib library; 
// 			the modifications are as follows:
// 		- added class' wrappers
// 		- several bugs fixed
// 		- several enhancements added
// 		- MFC support added
// 		- memory leaks eliminated when write error occurred
// 		- automatically free used memory on destruction or exception
// 		- modern error notification using exceptions
// 
//     for more info about .ZIP format, see 
//       ftp://ftp.pkware.com/appnote.zip
// 	the unpacked file (appnote.txt) from this archive is a part of this project
// 
//
//	Note: 
//  if a file added to the archive is bigger after compressing 
//	(e.g. it is an other archive) it should be delted from the archive and
//	added once again with store method (it may be done only on the files 
//  not divided between volumes)

#if !defined(AFX_ZIPARCHIVE_H__A7F528A6_1872_4071_BE66_D56CC2DDE0E6__INCLUDED_)
#define AFX_ZIPARCHIVE_H__A7F528A6_1872_4071_BE66_D56CC2DDE0E6__INCLUDED_

#include "ZipStorage.h"	// Added by ClassView
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "ZipException.h"
#include "ZipCentralDir.h"	// Added by ClassView
#include "ZipInternalInfo.h"	// Added by ClassView


//////////////////////////////////////////////////////////////////////////
/////////////////////  CZipArchive   /////////////////////////////////////

class CZipArchive  
{
public:
	static int SingleToWide(CZipAutoBuffer &szSingle, CString& szWide);
	static int WideToSingle(LPCTSTR lpWide, CZipAutoBuffer &szSingle);

// Function name	: TestFile
// Description	    : Test the file with the given index
//		the function throws CException*, but performs all the necessary cleanup
//		before, so that the next file can be tested after catchig the exception
//		and examining it for the reason of error.
// Return type		: bool 
//		return false if the incorrect action has been taken by 
//		the user or the programmer (it is when OpenFile(WORD nIndex) or 
//		GetFileInfo(CZipFileHeader & fhInfo, WORD uIndex) return false or uBufSize
//		is 0
// Argument         : WORD uIndex
//		index of the file to test
// 	Argument         : ZIPCALLBACKFUN pCallback = NULL
//		See the description at AddNewFile
// 	Argument         : void* pUserData = NULL
// Argument         : DWORD uBufSize = 65535
//		the size of the buffer used during extraction
	bool TestFile(WORD uIndex, ZIPCALLBACKFUN pCallback = NULL, void* pUserData = NULL, DWORD uBufSize = 65535);

// Function name	: CloseFileAfterTestFailed
// Description	    : Perform the necessary cleanup after the exception 
//		while testing the archive so that next files in the archive can be tested
// Return type		: void 
	void CloseFileAfterTestFailed();


// Function name	: SetPassword
// Description	    : set the password for the file that is going to be opened or created
//						use this function BEFORE opening or adding a file					
// Return type		: bool 
//						return false if the password contains ASCII characters
//						with values 128 or higher or the file inside archive is
//						opened		
// Argument         : LPCTSTR lpszPassword = NULL
//						set it to NULL to clear password
	bool SetPassword(LPCTSTR lpszPassword = NULL);

// Function name	: SetAdvanced
// Description	    :  set advanced options
// Return type		: void 
// Argument         : int iWriteBuffer = 65535
//		buffer used during write operation to the disk, the bigger, the better;
//		it is pointless, however, to set it bigger than the size of the volume;
//		the optimal size is the size of the volume
// Argument         : int iExtractBuffer = 16384
//		set the size of the buffer used in extracting and compressing files
//		default 16384, must not be < 1024
//		set it before opening the archive
// Argument         : int iSearchBuffer = 32768
//		set the size of the buffer used in searching for the central dir
//		useful with big archives, default 32768, must not be < 1024
	void SetAdvanced(int iWriteBuffer = 65535, int iExtractBuffer = 16384, int iSearchBuffer = 32768);

// Function name	: SetSpanCallback
// Description	    : set callback function used during operations on a
//		pkzip compatible disk spanning archive to change disks; 
//		set it usualy before opening the archive for reading
// Return type		: void 
// Argument         : ZIPCALLBACKFUN pFunc
//		for the description of callback function see CZipStorage.h
// Argument         : void* pData
//		user data to be passed to the callback function as the last parameter
	void SetSpanCallback(ZIPCALLBACKFUN pFunc, void* pData = NULL);

//	archive open modes	
	enum {open, openReadOnly, create, createSpan};

//		Disk spanning archive modes:
//		pkzip compatible mode (pkSpan):
//			- only on removeble devices
//			- autodetect the size of the volume
//			- write a label to the disk
//			- there is a need to set the span callback function
//		TD mode (tdSpan):
//			- may be created on non removable devices
//			- uses user-defined volume size
//			- no need to set the span callback function

// Function name	: Open
// Description	    : Open a zip archive
// Return type		: void
// Argument         : LPCTSTR szPathName
//		Path to the archive
// Argument         : int iMode= CZipArchive::open;
//		open mode flags:
//		open			open an existing archive
//		openReadOnly	open an existing archive as read only file 
//			(this mode is intended to use in self extract code)
//			if you try to add or delete a file in this mode, an exception will be thrown
//		create			create a new archive
//		createSpan		create a disk spanning archive
// Argument         : int iVolumeSize = 0
//		if iMode == createSpan and iVolumeSize <= 0 then create disk spanning 
//			archive in pkzip mode
//		if iMode == createSpan and iVolumeSize > 0 then create disk spanning 
//			archive in TD mode
//		if iMode == open and the exisitng archive is a spanned archive
//			the pkSpan mode is assumed if the archive is on a removable device
//			or tdSpan otherwise;
//			if you want to open tdSpan archive on a removable device, set iVolumeSize
//			to a value different from 0	
//		if iMode == create this argument doesn't matter
	void Open(LPCTSTR szPathName, int iMode = open, int iVolumeSize = 0);

// Function name	: Open
// Description	    : Open or create an archive in CMemFile
// Return type		: void 
// Argument         : CMemFile& mf
// Argument         : int iMode = open
//		the following modes are valid:  open, openReadOnly, create
	void Open(CMemFile& mf, int iMode = open);


// Function name	: AddNewFile
// Description	    : add quickly a new file to the archive
// Return type		: bool 
// Argument         : LPCTSTR lpszFilePath
//		file to be added
// Argument         : int iLevel = Z_DEFAULT_COMPRESSION
//		the compression level (see OpenNewFile() for detailed desciption)
// Argument			: bool bFullPath
//		if true, include full path of the file inside the archive	
// 	Argument         : ZIPCALLBACKFUN pCallback = NULL
// callback function (may be NULL)
// 	To set the callback function for this operation pass its pointer as the 
// 	argument (do not use SetSpanCallback for it - its for different purposes).
// 	The callback function, if set, is called after reading and writing one portion of data.
// 	- the first argument (DWORD):
// 			total number of bytes to read (the size of the file)
// 	- the second one (int) :
// 			total number bytes already read
// 	- the third argument (void*): 
// 		pUserData argument passed to #AddNewFile
// 	Argument         : void* pUserData = NULL
// 		user - defined data passed on to the callback function
// 		(doesn't matter if there is no callback function defined)
// Argument         : DWORD nBufSize = 65535
//		the size of the buffer used during compression

/*
*/
	bool AddNewFile(LPCTSTR lpszFilePath, int iLevel = -1, bool bFullPath = true, ZIPCALLBACKFUN pCallback = NULL, void* pUserData = NULL, unsigned long nBufSize = 65535);

// Function name	: OpenNewFile
// Description	    : add a new file to the zip archive
// Return type		: bool 
//		return false in the following cases:
//		- the lpszFilePath is not NULL and the file	attributes and data was not correctly retreived
//		- a file is already opened for extraction or compression
//		- archive is an existing disk span archive
//		- maximum file count inside archive reached (65535)
// Argument         : CZipFileHeader & header
//		structure that have addtional information; the following fields are valid:
//			- m_uMethod - file compression method; can be 0 (storing) or Z_DEFLATE (deflating)
//				otherwise Z_DEFLATE is assumed
//			- m_uModDate, m_uModTime - use SetTime method of CFileHeadeer to set them
//				if lpszFilePath is not NULL this fields are updated automaticaly
//			- m_uExternalAttr - attributes of the file
//				if lpszFilePath is not NULL this field is updated automaticaly
//			- m_szFileName - file name (may be with path) to be stored inside archive
//				to represent this file
//			- m_szComment - file comment
//			- m_pExtraField - LOCAL extra field, use SetExtraField() after opening 
//				a new file, but before closing it to set the extra field 
//				in the header in the central directory
//		other fields are ignored - they are updated automaticaly
// Argument         : int iLevel = Z_DEFAULT_COMPRESSION
//		the level of compression (-1, 0 - 9); named values:
//		Z_DEFAULT_COMPRESSION	: -1
//		Z_NO_COMPRESSION		: 0
//		Z_BEST_SPEED			: 1
//		Z_BEST_COMPRESSION		: 9
// Argument         : LPCTSTR lpszFilePath = NULL
//		the path to the file to retreive date and attributes from
	bool OpenNewFile(CZipFileHeader & header, int iLevel = Z_DEFAULT_COMPRESSION, LPCTSTR lpszFilePath = NULL);

// Function name	: WriteNewFile
// Description	    : compress the contents of the buffer and write it to a new file
// Return type		: bool 
//		return false if the new file hasn't been opened
// Argument         : void *pBuf
//		buffer containing the data to be compressed and written
// Argument         : DWORD iSize
//		the size of the buffer
	bool WriteNewFile(void *pBuf, DWORD iSize);

// Function name	: SetExtraField
// Description	    : set the extra field in the file header in the central directory
//		must be used after opening a new file in the archive, but before closing it
// Return type		: void 
// Argument         : char *pBuf
//		bufer with the data to be copied
// Argument         : DWORD iSize
//		size of the buffer
	void SetExtraField(char *pBuf, WORD iSize);

// Function name	: CloseNewFile
// Description	    : close the new file in the archive
// Return type		: bool 
//		return false if no new file is opened
// Argument         : bool bAfterException  = false
//		set it to true if you want to reuse CZipArchive after is has thrown an exception
	bool CloseNewFile();

// Function name	: ExtractFile
// Description	    : fast extracting
// Return type		: bool 
// Argument         : WORD uIndex
//		the index of the file
// Argument         : LPCTSTR lpszPath
//		PATH only to extract the file to
// Argument			: bool bFullPath = true
//		extract the file with full path (if there is a path stored with the filename)
//		or just with the filename alone
//		(it means that the resulting file path is lpszPath + one of the above)	
// Argument			: LPCTSTR lpszNewName = NULL
//			if NULL the default file name is taken (from the archive)
// 	Argument         : ZIPCALLBACKFUN pCallback = NULL
//		See the description at AddNewFile
// 	Argument         : void* pUserData = NULL
// Argument         : DWORD nBufSize = 65535
//		the size of the buffer used during extraction
	bool ExtractFile(WORD uIndex, LPCTSTR lpszPath, bool bFullPath = true, LPCTSTR lpszNewName = NULL, ZIPCALLBACKFUN pCallback = NULL, void* pUserData = NULL, DWORD nBufSize = 65535);

// Function name	: OpenFile
// Description	    : open the file with the given index in the archive for extracting
// Argument         : WORD uIndex
// Return type		: bool 
	bool OpenFile(WORD uIndex);

// Function name	: ReadFile
// Description	    : decompress currently opened file to the bufor
// Return type		: DWORD 
//		number of bytes read			
// Argument         : void *pBuf
//		buffer to receive data
// Argument         : DWORD iSize
//		the size of the buffer
	DWORD ReadFile(void *pBuf, DWORD iSize);

// Function name	: GetLocalExtraField
// Description	    : get the local extra filed of the currently opened 
//					  for extraction file in the archive
// Return type		: int 
//		if pBuf == NULL return the size of the local extra field
// Argument         : char* pBuf
//		the buffer to receive the data
// Argument         : int iSize
//		the size of the buffer
	int GetLocalExtraField(char* pBuf, int iSize);

// Function name	: CloseFile
// Description	    : close current file  and update
//		date and attribute information of CFile, closes CFile
// Return type		: int
//		see below
// Argument         : CFile & file
//		OPENED CFile structure of the extracted file
	int CloseFile(CFile &file);


/**
	Close the file opened for extraction in the archive and copy its date and 
	attributes to the file pointed by \e lpszFilePath
	\param	lpszFilePath 
		Points to the path of the file to have the date and attributes information updated.
	\param bAfterException
		Set to \c true to close the file inside archive after an exception has been 
		thrown, to allow futher operations on the archive.
	\warning Close the file pointed by \e lpszFilePath before using this method, 
		because the system may not be able to retrieve information from it.
	\return		
	-  "1" = ok
	-  "-1" = some bytes left to uncompress - probably due to a bad password
	-  "-2" = setting extracted file date and attributes was not successful	
	\note Throws exceptions.
*/	
	int CloseFile(LPCTSTR lpszFilePath = NULL, bool bAfterException = false);

// Function name	: DeleteFile
// Description	    : delete the file with the given index
// Return type		: bool 
// Argument         : WORD uIndex
//		index of the file to be deleted
	bool DeleteFile(WORD uIndex);

/*	delete files from the archive opened in the Delete mode specified by aIndexes
	or aNames

	aIndexes is a array of indexes of the files inside the archive;
		the index no. 0 is the first file in the archive

	aNames is a array of file names inside the archive; they must be the 
	same as they apppear in the archive (the name and the path (if persists) 
	is required, lower and upper case are not distinguished)

*/
	void DeleteFiles(CWordArray &aIndexes);
	void DeleteFiles(CStringArray &aNames, bool bCaseSensitive = false);


// Function name	: SetGlobalComment
// Description	    : set the global comment in the archive
// Return type		: bool 
//		return false if the archive is closed or if it is an existing disk spanning archive
// Argument         : const CString& szComment
	bool SetGlobalComment(const CString& szComment);

// Function name	: GetGlobalComment
// Description	    : get the global commment
// Return type		: CString 
//		return an empty string if the archive is closed
	CString GetGlobalComment();


// Function name	: SetFileComment
// Description	    : set the comment of the file with the given index
// Return type		: bool 
//		return false if the comment change is imposible
// Argument         : WORD uIndex
//		index of the file
// Argument         : CString szComment
//		comment to add
	bool SetFileComment(WORD uIndex, CString szComment);

// Function name	: GetArchivePath
// Description	    : return the path of the currently opended archive volume
// Return type		: CString 
	CString GetArchivePath();

// Function name	: GetCurrentDisk
// Description	    : return the zero-base number of the current disk
// Return type		: int 
//		return -1 if there is no current disk (archive is closed)
	int GetCurrentDisk();

// Function name	: GetSpanMode
// Description	    : return the disk spanning mode of the cuurrent archive
// Return type		: int 
//		CZipStorage::tdSpan		 == - 2 - exisitng TD compatible disk spanning
//		CZipStorage::pkzipSpan	 == - 1 - exisitng pkzip compatible disk spanning
//		CZipStorage::noSpan		 ==	  0 - no disk spanning
//		CZipStorage::pkzipSpan	 ==   1 - pkzip compatible disk spanning in creation
//		CZipStorage::tdSpan		 ==   2 - TD compatible disk spanning in creation
	int GetSpanMode();

// Function name	: IsFileDirectory
// Description	    : check if the file with the given index is a directory
// Argument         : WORD uIndex
//		index of the file
// Return type		: bool 
//		return true if the file is a directory
//		return false if the file is not a directory or if there is no file
//		with the given index
	bool IsFileDirectory(WORD uIndex);

// Function name	: FindFile
// Description	    : find the file in the archive
// 	This function requires CZipCentralDir::m_bFindFastEnabled set to true
// 	Use EnableFindFast()
// Return type		: int
//		the index of the file found or -1 if no file was found
// Argument         : CString szFileName
//		the name of the file to be found
// Argument         : bool bCaseSensitive = false
//		if true - perform case sensitive search
	int FindFile(CString szFileName, bool bCaseSensitive = false);


/*
	Function name	: EnableFindFast
	Description	    : 
	 	Enable fast finding by the file name of the files inside the archive.
 		Set CZipCentralDir::m_bFindFastEnabled to true, which is required by FindFile.
		Do not enable it, if you don't plan to use FindFile function

	Return type		: void 
	Argument         : bool bEnable = true
*/
	void EnableFindFast(bool bEnable = true);


/*
	Function name	: SetConvertAfterOpen
	Description	    : Set CZipCentralDir::m_bConvertAfterOpen value. Use before opening the archive
	see CZipCentralDir::m_bConvertAfterOpen
	Return type		: void
	Argument         : bool bConvertAfterOpen
*/
	void SetConvertAfterOpen (bool bConvertAfterOpen)
	{
		if (!IsClosed())
		{
			TRACE(_T("Set it before opening the archive"));
			return;
		}
		m_centralDir.m_bConvertAfterOpen = bConvertAfterOpen;

	}


// Function name	: GetFileInfo
// Description	    : get the info of the file with the given index
// Return type		: bool 
//		true if successful
// Argument         : CZipFileHeader & fhInfo
//		structure to receive info
// Argument         : WORD uIndex
//		zero-based index of the file
	bool GetFileInfo(CZipFileHeader & fhInfo, WORD uIndex);


// Function name	: GetNoEntries
// Description	    : get number of files in the archive
// Return type		: int 
	int GetNoEntries();

// Function name	: Close
// Description	    : close archive
// Return type		: void 
// Argument         : bool bAfterException  = false
//		set it to true if you want to close and reuse CZipArchive after is has thrown an exception
//		( it doesn't write any data to the file but only makes some cleaning then)
	void Close(bool bAfterException = false);



// Function name	: IsClosed
// Description	    : test if the archive or the current volume file is closed
// Return type		: bool 
// Argument         : bool bArchive = true
//		if true test for the whole archive, if false - for the volume file only
	bool IsClosed(bool bArchive = true);

// specify whether to control memory allocation and freeing by zlib library
// strongly suggested to set it to true (default)
// set it before opening a file (new or current) in the archive
	bool m_bDetectZlibMemoryLeaks;


	CZipArchive();
	virtual ~CZipArchive();

////////////////////////////////////////////////////////////////////////
//////////////////////  static helper functions  ///////////////////////
////////////////////////////////////////////////////////////////////////

// Function name	: GetFileTitle
// Description	    : get the title of the file
// Return type		: CString 
// Argument         : LPCTSTR lpszFilePath
	static CString GetFileTitle(LPCTSTR lpszFilePath);

// Function name	: GetFileDirAndName
// Description	    : get the directory and the file name from the file path
// Return type		: static CString 
// Argument         : LPCTSTR lpszFilePath
	static CString GetFileDirAndName(LPCTSTR lpszFilePath);

// Function name	: GetDrive
// Description	    : return the (drive:) part from the path
// Return type		: static CString 
// Argument         : LPCTSTR lpszFilePath
	static CString GetDrive(LPCTSTR lpszFilePath);

// Function name	: IsDriveRemovable
// Description	    : return true if the file, path or (disk:) specified by the
//		argument is (on) a removable drive
// Return type		: static bool 
// Argument         : LPCTSTR lpszFilePath
	static bool IsDriveRemovable(LPCTSTR lpszFilePath);

// Function name	: DirectoryExists
// Description	    : check if the given directory exists
// Return type		: static bool 
// Argument         : LPCTSTR lpszDir
	static bool DirectoryExists(LPCTSTR lpszDir);


// Function name	: FileExists
// Description	    : check if the given file or directory exists
// Return type		: static int 
//		return -1 if the given file is a directory, 1 if is a file
//		or 0 if there is no such a file
// Argument         : LPCTSTR lpszName
	static int FileExists(LPCTSTR lpszName);

// Function name	: ForceDirectory
// Description	    : create nested directories with a single command
// Return type		: static bool 
// Argument         : LPCTSTR lpDirectory
	static bool ForceDirectory(LPCTSTR lpDirectory);

// Function name	: GetFilePath
// Description	    : get the path of the given file
// Return type		: static CString 
// Argument         : LPCTSTR strFilePath
	static CString GetFilePath(LPCTSTR lpszFilePath);


// Function name	: GetFileExt
// Description	    : return the file extension
// Return type		: static CString 
// Argument         : LPCTSTR lpszFilePath
	static CString GetFileExt(LPCTSTR lpszFilePath);


// Function name	: GetFileName
// Description	    : return the name of the file (title + extension)
// Return type		: static CString 
// Argument         : LPCTSTR lpszFilePath
	static CString GetFileName(LPCTSTR lpszFilePath);

	// just a helper for a various purposes
	static const DWORD* GetCRCTable();
	// function for a VERY advanced use - you normally never need that
	CZipStorage* GetStorage(){return &m_storage;}
	
protected:
	void EmptyPtrList();
	void CryptDecodeBuffer(DWORD uCount);
	void CryptEncodeBuffer();
	void CryptEncode(char &c);
	void CryptCryptHeader(long iCrc, CZipAutoBuffer& buf);
	DWORD CryptCRC32(DWORD l, char c);
	void CryptDecode(char &c);
	char CryptDecryptByte();
	bool CryptCheck();
	void CryptUpdateKeys(char c);
	void CryptInitKeys();
	CZipAutoBuffer m_pszPassword;
	DWORD m_keys[3];

	static int CompareWords(const void *pArg1, const void *pArg2);
	bool IsDirectory(DWORD uAttr);
	void DeleteInternal(WORD uIndex);
	DWORD RemovePackedFile(DWORD uStartOffset, DWORD uEndOffset);
	CZipFileHeader* CurrentFile();
	void CheckForError(int iErr);
	CZipInternalInfo m_info;
	CZipStorage m_storage;
	CPtrList m_list;
	static void* myalloc(void* opaque, UINT items, UINT size);
	static void myfree(void* opaque, void* address);
	enum {extract = -1, nothing, compress};
	// 0 - no file inside archive opened
	// -1 - current opened for extract
	// 1 - new opened for compression
	char m_iFileOpened;
	void ThrowError(int err);
	CZipCentralDir m_centralDir;
	static TCHAR m_gszCopyright[];
};

#endif // !defined(AFX_ZIPARCHIVE_H__A7F528A6_1872_4071_BE66_D56CC2DDE0E6__INCLUDED_)
