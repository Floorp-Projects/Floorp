// ZipInternalInfo.h: interface for the CZipInternalInfo class.
//
////////////////////////////////////////////////////////////////////////////////
//  Copyright (C) 2000 Tadeusz Dracz.
//  For conditions of distribution and use, see copyright notice in ZipArchive.h
////////////////////////////////////////////////////////////////////////////////

#if !defined(AFX_ZIPINTERNALINFO_H__C6749101_590C_4F74_8121_B82E3BE9FA44__INCLUDED_)
#define AFX_ZIPINTERNALINFO_H__C6749101_590C_4F74_8121_B82E3BE9FA44__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
#include "ZipAutoBuffer.h"
#include "zlib.h"

class CZipInternalInfo  
{
public:
	DWORD m_iBufferSize;
	z_stream m_stream;
	DWORD m_uUncomprLeft;
	DWORD m_uComprLeft;
	DWORD m_uCrc32;
	void Init();
	CZipAutoBuffer m_pBuffer;
	CZipInternalInfo();
	virtual ~CZipInternalInfo();

};

#endif // !defined(AFX_ZIPINTERNALINFO_H__C6749101_590C_4F74_8121_B82E3BE9FA44__INCLUDED_)
