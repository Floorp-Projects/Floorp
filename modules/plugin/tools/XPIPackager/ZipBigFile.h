// ZipBigFile.h: interface for the CZipBigFile class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_ZipBigFile_H__79E0E6BD_25D6_4B82_85C5_AB397D9EC368__INCLUDED_)
#define AFX_ZipBigFile_H__79E0E6BD_25D6_4B82_85C5_AB397D9EC368__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

class CZipBigFile : public CFile  
{
	DECLARE_DYNAMIC(CZipBigFile)
public:
	_int64 Seek(_int64 dOff, UINT nFrom);
	CZipBigFile();
	virtual ~CZipBigFile();

};

#endif // !defined(AFX_ZipBigFile_H__79E0E6BD_25D6_4B82_85C5_AB397D9EC368__INCLUDED_)
