// ZipStorage.cpp: implementation of the CZipStorage class.
//
////////////////////////////////////////////////////////////////////////////////
//  Copyright (C) 2000 Tadeusz Dracz.
//  For conditions of distribution and use, see copyright notice in ZipArchive.h
////////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "ZipStorage.h"
#include "ZipArchive.h"

//////////////////////////////////////////////////////////////////////
// disk spanning objectives:
// - sinature at the first disk at the beginning
// - headers and central dir records not divided between disks
// - each file has a data descriptor preceded by the signature
//	(bit 3 set in flag);


char CZipStorage::m_gszExtHeaderSignat[] = {0x50, 0x4b, 0x07, 0x08};
CZipStorage::CZipStorage()
{
	m_pCallbackData = m_pZIPCALLBACKFUN = NULL;
	m_iWriteBufferSize = 65535;
	m_iCurrentDisk = -1;
	m_pFile = NULL;
}

CZipStorage::~CZipStorage()
{

}

DWORD CZipStorage::Read(void *pBuf, DWORD iSize, bool bAtOnce)
{
	if (iSize == 0)
		return 0;
	DWORD iRead = 0;
	while (!iRead)
	{
		iRead = m_pFile->Read(pBuf, iSize);
		if (!iRead)
			if (IsSpanMode())
				ChangeDisk(m_iCurrentDisk + 1);
			else
				ThrowError(ZIP_BADZIPFILE);
	}

	if (iRead == iSize)
		return iRead;
	else if (bAtOnce || !IsSpanMode())
		ThrowError(ZIP_BADZIPFILE);

	while (iRead < iSize)
	{
		ChangeDisk(m_iCurrentDisk + 1);
		UINT iNewRead = m_pFile->Read((char*)pBuf + iRead, iSize - iRead);
		if (!iNewRead && iRead < iSize)
			ThrowError(ZIP_BADZIPFILE);
		iRead += iNewRead;
	}

	return iRead;
}

void CZipStorage::Open(LPCTSTR szPathName, int iMode, int iVolumeSize)
{
	m_pWriteBuffer.Allocate(m_iWriteBufferSize); 
	m_uBytesInWriteBuffer = 0;
	m_bNewSpan = false;
	m_pFile = &m_internalfile;

	if ((iMode == CZipArchive::create) ||(iMode == CZipArchive::createSpan)) // create new archive
	{
		m_iCurrentDisk = 0;
		if (iMode == CZipArchive::create)
		{
			m_iSpanMode = noSpan;
			OpenFile(szPathName, CFile::modeCreate | CFile::modeReadWrite);
		}
		else // create disk spanning archive
		{
			m_bNewSpan = true;
			m_iBytesWritten = 0;
			if (iVolumeSize <= 0) // pkzip span
			{
				if (!m_pZIPCALLBACKFUN)
					ThrowError(ZIP_NOCALLBACK);
				if (!CZipArchive::IsDriveRemovable(szPathName))
					ThrowError(ZIP_NONREMOVABLE);
				m_iSpanMode = pkzipSpan;
			}
			else
			{
				m_iTdSpanData = iVolumeSize;
				m_iSpanMode = tdSpan;
			}

			NextDisk(4, szPathName);
			Write(m_gszExtHeaderSignat, 4, true);
		}
	}
	else // open existing
	{
		OpenFile(szPathName, CFile::modeNoTruncate | ((iMode == CZipArchive::openReadOnly) ? CFile::modeRead : CFile::modeReadWrite));
		// m_uData, m_bAllowModif i m_iSpanMode ustalane automatycznie podczas odczytu central dir
		m_iSpanMode = iVolumeSize == 0 ? suggestedAuto : suggestedTd;
	}
		
}


void CZipStorage::Open(CMemFile& mf, int iMode)
{
	m_pWriteBuffer.Allocate(m_iWriteBufferSize); 
	m_uBytesInWriteBuffer = 0;
	m_bNewSpan = false;
	m_pFile = &mf;

	if (iMode == CZipArchive::create)
	{
		m_iCurrentDisk = 0;
		m_iSpanMode = noSpan;
		mf.SetLength(0);
	}
	else // open existing
	{
		mf.SeekToBegin();
		m_iSpanMode = suggestedAuto;
	}
}


int CZipStorage::IsSpanMode()
{
	return m_iSpanMode == noSpan ? 0 : (m_bNewSpan ? 1 : -1);
}

void CZipStorage::ChangeDisk(int iNumber)
{
	if (iNumber == m_iCurrentDisk)
		return;

	ASSERT(m_iSpanMode != noSpan);
	m_iCurrentDisk = iNumber;
	OpenFile(m_iSpanMode == pkzipSpan ? ChangePkzipRead() : ChangeTdRead(),
		CFile::modeNoTruncate | CFile::modeRead);
}

void CZipStorage::ThrowError(int err)
{
	AfxThrowZipException(err, m_pFile->GetFilePath());
}

bool CZipStorage::OpenFile(LPCTSTR lpszName, UINT uFlags, bool bThrow)
{
	CFileException* e = new CFileException;
	BOOL bRet = m_pFile->Open(lpszName, uFlags | CFile::shareDenyWrite, e);
	if (!bRet && bThrow)
		throw e;
	e->Delete();
	return bRet != 0;
}

// zapobiega konstrukcji ChangeDisk(++m_iCurrentDisk)
void CZipStorage::SetCurrentDisk(int iNumber)
{
	m_iCurrentDisk = iNumber;
}

int CZipStorage::GetCurrentDisk()
{
	return m_iCurrentDisk;
}

CString CZipStorage::ChangePkzipRead()
{
	CString szTemp = m_pFile->GetFilePath();
	m_pFile->Close();
	CallCallback(-1 , szTemp);
	return szTemp;
}

CString CZipStorage::ChangeTdRead()
{
	CString szTemp = GetTdVolumeName(m_iCurrentDisk == m_iTdSpanData);
	m_pFile->Close();
	return szTemp;
}

void CZipStorage::Close(bool bAfterException)
{
	if (!bAfterException)
	{
		Flush();
		if ((m_iSpanMode == tdSpan) && (m_bNewSpan))
		{
			// give to the last volume the zip extension
			CString szFileName = m_pFile->GetFilePath();
			CString szNewFileName = GetTdVolumeName(true);
			m_pFile->Close();
			if (CZipArchive::FileExists(szNewFileName))
				CFile::Remove(szNewFileName);
			CFile::Rename(szFileName, szNewFileName);
		}
		else
#ifdef _DEBUG // to prevent assertion if the file is already closed
 		if (m_pFile->m_hFile != (UINT)CFile::hFileNull)
#endif
				m_pFile->Close();
	}
	else
#ifdef _DEBUG // to prevent assertion if the file is already closed
 		if (m_pFile->m_hFile != (UINT)CFile::hFileNull)
#endif
				m_pFile->Close();


	m_pWriteBuffer.Release();
	m_iCurrentDisk = -1;
	m_iSpanMode = noSpan;
	m_pFile = NULL;
}

CString CZipStorage::GetTdVolumeName(bool bLast, LPCTSTR lpszZipName)
{
	CString szFilePath = lpszZipName ? lpszZipName : m_pFile->GetFilePath();
	CString szPath = CZipArchive::GetFilePath(szFilePath);
	CString szName = CZipArchive::GetFileTitle(szFilePath);
	CString szExt;
	if (bLast)
		szExt = _T("zip");
	else
		szExt.Format(_T("%.3d"), m_iCurrentDisk);
	return szPath + szName + _T(".") + szExt;
}

void CZipStorage::NextDisk(int iNeeded, LPCTSTR lpszFileName)
{
	Flush();
	ASSERT(m_iSpanMode != noSpan);
	if (m_iBytesWritten)
	{
		m_iBytesWritten = 0;
		m_iCurrentDisk++;
		if (m_iCurrentDisk >= 999)
			ThrowError(ZIP_TOOMANYVOLUMES);
	} 
	CString szFileName;
	bool bPkSpan = (m_iSpanMode == pkzipSpan);
	if (bPkSpan)
		szFileName  = lpszFileName ? lpszFileName : m_pFile->GetFilePath();
	else
		szFileName =  GetTdVolumeName(false, lpszFileName);

#ifdef _DEBUG // to prevent assertion if the file is already closed
	if (m_pFile->m_hFile != (UINT)CFile::hFileNull)
#endif
		m_pFile->Close(); // if it is closed, so it will not close

	if (bPkSpan)
	{
		int iCode = iNeeded;
		while (true)
		{
			CallCallback(iCode, szFileName);
			if (CZipArchive::FileExists(szFileName))
				iCode = -2;
			else
			{
				CString label;
				label.Format(_T("pkback# %.3d"), m_iCurrentDisk + 1);
				if (!SetVolumeLabel(CZipArchive::GetDrive(szFileName), label)) /*not write label*/
					iCode = -3;
				else if (!OpenFile(szFileName, CFile::modeCreate | CFile::modeReadWrite, false))
					iCode = -4;
				else
					break;
			}

		}
		m_uCurrentVolSize = GetFreeVolumeSpace();
	}
	else
	{
		m_uCurrentVolSize = m_iTdSpanData;
		OpenFile(szFileName, CFile::modeCreate | CFile::modeReadWrite);
	}
}

void CZipStorage::CallCallback(int iCode, CString szTemp)
{
	ASSERT(m_pZIPCALLBACKFUN);
	if (!(*m_pZIPCALLBACKFUN)(m_iCurrentDisk + 1, iCode, m_pCallbackData))
		throw new CZipException(CZipException::aborted, szTemp);
}

DWORD CZipStorage::GetFreeVolumeSpace()
{
	ASSERT (m_iSpanMode == pkzipSpan);
	DWORD SectorsPerCluster, BytesPerSector, NumberOfFreeClusters, TotalNumberOfClusters;		
	if (!GetDiskFreeSpace(
		CZipArchive::GetDrive(m_pFile->GetFilePath()),
		&SectorsPerCluster,
		&BytesPerSector,
		&NumberOfFreeClusters,
		&TotalNumberOfClusters))
			return 0;
	_int64 total = SectorsPerCluster * BytesPerSector * NumberOfFreeClusters;
	return (DWORD)total;
}


void CZipStorage::UpdateSpanMode(WORD uLastDisk)
{
	m_iCurrentDisk = uLastDisk;
	if (uLastDisk)
	{
		// disk spanning detected

		if (m_iSpanMode == suggestedAuto)
			m_iSpanMode = CZipArchive::IsDriveRemovable(m_pFile->GetFilePath()) ? 
				pkzipSpan : tdSpan;
		else
			m_iSpanMode = tdSpan;

		if (m_iSpanMode == pkzipSpan)
		{
			if (!m_pZIPCALLBACKFUN)
					ThrowError(ZIP_NOCALLBACK);
		}
		else /*if (m_iSpanMode == tdSpan)*/
			m_iTdSpanData = uLastDisk; // disk with .zip extension ( the last one)
			
		m_pWriteBuffer.Release(); // no need for this in this case
	}
	else 
		m_iSpanMode = noSpan;

}

void CZipStorage::Write(void *pBuf, DWORD iSize, bool bAtOnce)
{
	if (!IsSpanMode())
		WriteInternalBuffer((char*)pBuf, iSize);
	else
	{
		// if not at once, one byte is enough free space
		DWORD iNeeded = bAtOnce ? iSize : 1; 
		DWORD uTotal = 0;

		while (uTotal < iSize)
		{
			DWORD uFree;
			while ((uFree = VolumeLeft()) < iNeeded)
			{
				if ((m_iSpanMode == tdSpan) && !m_iBytesWritten && !m_uBytesInWriteBuffer)
					// in the tdSpan mode, if the size of the archive is less 
					// than the size of the packet to be written at once,
					// increase once the size of the volume
					m_uCurrentVolSize = iNeeded;
				else
					NextDisk(iNeeded);
			}

			DWORD uLeftToWrite = iSize - uTotal;
			DWORD uToWrite = uFree < uLeftToWrite ? uFree : uLeftToWrite;
			WriteInternalBuffer((char*)pBuf + uTotal, uToWrite);
			if (bAtOnce)
				return;
			else
				uTotal += uToWrite;
		}

	}
}


void CZipStorage::WriteInternalBuffer(char *pBuf, DWORD uSize)
{
	DWORD uWritten = 0;
	while (uWritten < uSize)
	{
		DWORD uFreeInBuffer = GetFreeInBuffer();
		if (uFreeInBuffer == 0)
		{
			Flush();
			uFreeInBuffer = m_pWriteBuffer.GetSize();
		}
		DWORD uLeftToWrite = uSize - uWritten;
		DWORD uToCopy = uLeftToWrite < uFreeInBuffer ? uLeftToWrite : uFreeInBuffer;
		memcpy(m_pWriteBuffer + m_uBytesInWriteBuffer, pBuf + uWritten, uToCopy);
		uWritten += uToCopy;
		m_uBytesInWriteBuffer += uToCopy;
	}
}

DWORD CZipStorage::VolumeLeft()
{
	// for pkzip span m_uCurrentVolSize is updated after each flush()
	return m_uCurrentVolSize  - m_uBytesInWriteBuffer - ((m_iSpanMode == pkzipSpan) ? 0 : m_iBytesWritten);
}

void CZipStorage::Flush()
{
	m_iBytesWritten += m_uBytesInWriteBuffer;
	if (m_uBytesInWriteBuffer)
	{
		m_pFile->Write(m_pWriteBuffer, m_uBytesInWriteBuffer);
		m_uBytesInWriteBuffer = 0;
	}
	if (m_iSpanMode == pkzipSpan) 
		// after writting it is difficult to predict the free space due to 
		// not completly written clusters, write operation may start from 
		// the new cluster
		m_uCurrentVolSize = GetFreeVolumeSpace();
}

DWORD CZipStorage::GetPosition()
{
	return m_pFile->GetPosition() + m_uBytesInWriteBuffer;
}


DWORD CZipStorage::GetFreeInBuffer()
{
	return m_pWriteBuffer.GetSize() - m_uBytesInWriteBuffer;
}
