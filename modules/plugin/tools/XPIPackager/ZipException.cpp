// ZipException.cpp: implementation of the CZipException class.
//
////////////////////////////////////////////////////////////////////////////////
//  Copyright (C) 2000 Tadeusz Dracz.
//  For conditions of distribution and use, see copyright notice in ZipArchive.h
////////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "ZipException.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNAMIC( CZipException, CException)

CZipException::CZipException(int iCause, LPCTSTR lpszZipName):CException(TRUE)
{
	m_iCause = iCause;

	if (lpszZipName)
		m_szFileName = lpszZipName;	
}

CZipException::~CZipException()
{

}

void AfxThrowZipException(int iZipError, LPCTSTR lpszZipName)
{
	throw new CZipException(CZipException::ZipErrToCause(iZipError), lpszZipName);
}

int CZipException::ZipErrToCause(int iZipError)
{
	switch (iZipError)
	{
	case 2://Z_NEED_DICT:
		return CZipException::needDict;
	case 1://Z_STREAM_END:
		return CZipException::streamEnd;
	case -1://Z_ERRNO:
		return CZipException::errNo;
	case -2://Z_STREAM_ERROR:
		return CZipException::streamError;
	case -3://Z_DATA_ERROR:
		return CZipException::dataError;
	case -4://Z_MEM_ERROR:
		return CZipException::memError;
	case -5://Z_BUF_ERROR:
		return CZipException::bufError;
	case -6://Z_VERSION_ERROR:
		return CZipException::versionError;
	case ZIP_BADZIPFILE:
		return CZipException::badZipFile;
	case ZIP_BADCRC:
		return CZipException::badCrc;
	case ZIP_ABORTED:
		return CZipException::aborted;
	case ZIP_NOCALLBACK:
		return CZipException::noCallback;
	case ZIP_NONREMOVABLE:
		return CZipException::nonRemovable;
	case ZIP_TOOMANYVOLUMES:
		return CZipException::tooManyVolumes;
	case ZIP_TOOLONGFILENAME:
		return CZipException::tooLongFileName;
	case ZIP_BADPASSWORD:
		return CZipException::badPassword;
	case ZIP_CDIR_NOTFOUND:
		return CZipException::cdirNotFound;


	default:
		return CZipException::generic;
	}
	
}
