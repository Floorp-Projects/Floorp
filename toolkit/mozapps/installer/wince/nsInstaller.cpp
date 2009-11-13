/* -*- Mode: C++; c-basic-offset: 2; tab-width: 8; indent-tabs-mode: nil; -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Fennec Installer for WinCE.
 *
 * The Initial Developer of the Original Code is The Mozilla Foundation.
 *
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alex Pakhotin <alexp@mozilla.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/**
 *
 * nsInstaller.cpp : Mozilla Fennec Installer
 *
 */

#include <windows.h>
#include <DeviceResolutionAware.h>
#include "nsSetupStrings.h"
#include "nsInstaller.h"
#include "ns7zipExtractor.h"
#include "nsInstallerDlg.h"

const WCHAR c_sStringsFile[] = L"setup.ini";

// Global Variables:
nsSetupStrings Strings;

HINSTANCE g_hInst;
WCHAR     g_sSourcePath[MAX_PATH];
WCHAR     g_sExeFullFileName[MAX_PATH];
WCHAR     g_sExeFileName[MAX_PATH];
WCHAR     g_sInstallDir[MAX_PATH];
DWORD     g_dwExeSize;

// Forward declarations of functions included in this code module:
BOOL InitInstance(HINSTANCE, int);
DWORD GetExeSize();
BOOL PostExtract();
BOOL LoadStrings();

///////////////////////////////////////////////////////////////////////////////
//
// WinMain
//
///////////////////////////////////////////////////////////////////////////////
int WINAPI WinMain(HINSTANCE hInstance,
                   HINSTANCE hPrevInstance,
                   LPTSTR    lpCmdLine,
                   int       nCmdShow)
{
  int nResult = -1;

  if (!InitInstance(hInstance, nCmdShow)) 
    return nResult;

  nsInstallerDlg::GetInstance()->Init(g_hInst);
  nResult = nsInstallerDlg::GetInstance()->DoModal();
  if (nResult == IDOK)
    wcscpy(g_sInstallDir, nsInstallerDlg::GetInstance()->GetExtractPath());

  return nResult;
}

///////////////////////////////////////////////////////////////////////////////
//
// InitInstance
//
///////////////////////////////////////////////////////////////////////////////
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
  g_hInst = hInstance;

  InitCommonControls();

  g_sInstallDir[0] = L'\0';
  g_sExeFileName[0] = L'\0';
  if (GetModuleFileName(NULL, g_sExeFullFileName, MAX_PATH) == 0)
    return FALSE;

  wcscpy(g_sSourcePath, g_sExeFullFileName);
  WCHAR *sSlash = wcsrchr(g_sSourcePath, L'\\');
  wcscpy(g_sExeFileName, sSlash + 1);
  *sSlash = L'\0'; // cut the file name

  g_dwExeSize = GetExeSize();

  return LoadStrings();
}

///////////////////////////////////////////////////////////////////////////////
//
// GetExeSize
//
///////////////////////////////////////////////////////////////////////////////

DWORD AbsoluteSeek(HANDLE, DWORD);
void  ReadBytes(HANDLE, LPVOID, DWORD);

DWORD GetExeSize()
{
  DWORD dwSize = 0;
  HANDLE hImage;

  DWORD  dwSectionOffset;
  DWORD  dwCoffHeaderOffset;
  DWORD  moreDosHeader[16];

  ULONG  NTSignature;

  IMAGE_DOS_HEADER      image_dos_header;
  IMAGE_FILE_HEADER     image_file_header;
  IMAGE_OPTIONAL_HEADER image_optional_header;
  IMAGE_SECTION_HEADER  image_section_header;

  // Open the EXE file.
  hImage = CreateFile(g_sExeFullFileName, GENERIC_READ, FILE_SHARE_READ, NULL,
                      OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

  if (INVALID_HANDLE_VALUE == hImage)
  {
    ErrorMsg(L"Could not open the EXE");
    return 0;
  }

  // Read the MS-DOS image header.
  ReadBytes(hImage, &image_dos_header, sizeof(image_dos_header));

  if (IMAGE_DOS_SIGNATURE != image_dos_header.e_magic)
  {
    ErrorMsg(L"Unknown file format.");
    return 0;
  }

  // Read more MS-DOS header.
  ReadBytes(hImage, moreDosHeader, sizeof(moreDosHeader));

  // Get actual COFF header.
  dwCoffHeaderOffset = AbsoluteSeek(hImage, image_dos_header.e_lfanew) + sizeof(ULONG);

  ReadBytes (hImage, &NTSignature, sizeof(NTSignature));

  if (IMAGE_NT_SIGNATURE != NTSignature)
  {
    ErrorMsg(L"Missing NT signature. Unknown file type.");
    return 0;
  }

  dwSectionOffset = dwCoffHeaderOffset + IMAGE_SIZEOF_FILE_HEADER + IMAGE_SIZEOF_NT_OPTIONAL_HEADER;

  ReadBytes(hImage, &image_file_header, IMAGE_SIZEOF_FILE_HEADER);

  // Read optional header.
  ReadBytes(hImage, &image_optional_header, IMAGE_SIZEOF_NT_OPTIONAL_HEADER);

  WORD nNumSections = image_file_header.NumberOfSections;

  for (WORD i = 0; i < nNumSections; i++)
  {
    ReadBytes(hImage, &image_section_header, sizeof(IMAGE_SECTION_HEADER));

    DWORD dwRawData = image_section_header.PointerToRawData + image_section_header.SizeOfRawData;
    if (dwRawData > dwSize)
      dwSize = dwRawData;
  }

  CloseHandle(hImage);

  return dwSize;
}

DWORD AbsoluteSeek(HANDLE hFile, DWORD offset)
{
  DWORD newOffset;

  if ((newOffset = SetFilePointer(hFile, offset, NULL, FILE_BEGIN)) == 0xFFFFFFFF)
    ErrorMsg(L"SetFilePointer failed");

  return newOffset;
}

void ReadBytes(HANDLE hFile, LPVOID buffer, DWORD size)
{
  DWORD bytes;

  if (!ReadFile(hFile, buffer, size, &bytes, NULL))
    ErrorMsg(L"ReadFile failed");
  else if (size != bytes)
    ErrorMsg(L"Read the wrong number of bytes");
}

//////////////////////////////////////////////////////////////////////////
//
// Read strings resource from the attached archive
//
//////////////////////////////////////////////////////////////////////////
BOOL LoadStrings()
{
  WCHAR *sExtractPath = L"\\Temp\\";
  WCHAR sExtractedStringsFile[MAX_PATH];
  _snwprintf(sExtractedStringsFile, MAX_PATH, L"%s%s", sExtractPath, c_sStringsFile);

  ns7zipExtractor archiveExtractor(g_sExeFullFileName, g_dwExeSize, NULL);

  int nResult = archiveExtractor.ExtractFile(c_sStringsFile, sExtractPath);

  if (nResult != 0)
    return FALSE;

  BOOL bResult = Strings.LoadStrings(sExtractedStringsFile);
  DeleteFile(sExtractedStringsFile);

  return bResult;
}
