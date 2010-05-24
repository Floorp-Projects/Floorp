/* -*- Mode: C++; c-basic-offset: 2; tab-width: 8; indent-tabs-mode: nil; -*- */
/*****************************************************************************
 *
 * This 7z Library is based the 7z Client and 7z Standalone Extracting Plugin
 * code from the LZMA SDK.
 * It is in the public domain (see http://www.7-zip.org/sdk.html).
 *
 * Any copyright in these files held by contributors to the Mozilla Project is
 * also dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 *
 * Contributor(s):
 *   Alex Pakhotin <alexp@mozilla.com>
 *
 *****************************************************************************/

#include "Common/MyWindows.h"
#include "Common/NewHandler.h"

#include "Common/IntToString.h"
#include "Common/MyInitGuid.h"
#include "Common/StringConvert.h"

#include "Windows/DLL.h"
#include "Windows/FileDir.h"
#include "Windows/FileFind.h"
#include "Windows/FileName.h"
#include "Windows/NtCheck.h"
#include "Windows/PropVariant.h"
#include "Windows/PropVariantConversions.h"

#include "7zip/Common/FileStreams.h"

#include "7zip/ICoder.h"
#include "7zip/Archive/IArchive.h"

#include "7zip/IPassword.h"
#include "7zip/MyVersion.h"

// Used for global structures initialization
#include "../C/7zCrc.h"
#include "7zip/Common/RegisterArc.h"
#include "7zip/Common/RegisterCodec.h"
#include "7zip/Archive/7z/7zHandler.h"
#include "7zip/Compress/Bcj2Coder.h"
#include "7zip/Compress/BcjCoder.h"
#include "7zip/Compress/CopyCoder.h"
#include "7zip/Compress/Lzma2Decoder.h"
#include "7zip/Compress/LzmaDecoder.h"

#include "7zLib.h"

using namespace NWindows;

STDAPI CreateArchiver(const GUID *classID, const GUID *iid, void **outObject);

DEFINE_GUID(CLSID_CArchiveHandler,
  0x23170F69, 0x40C1, 0x278A, 0x10, 0x00, 0x00, 0x01, 0x10, 0x00, 0x00, 0x00);

DEFINE_GUID(CLSID_CFormat7z,
  0x23170F69, 0x40C1, 0x278A, 0x10, 0x00, 0x00, 0x01, 0x10, 0x07, 0x00, 0x00);

// Global static structures copied here from *Register.cpp files
// Static global variable defined in a module didn't work when used in a library

// 7z
static IInArchive *CreateArc() { return new NArchive::N7z::CHandler; }
static CArcInfo g_ArcInfo =
{ L"7z", L"7z", 0, 7, {'7', 'z', 0xBC, 0xAF, 0x27, 0x1C}, 6, false, CreateArc, NULL };

// BCJ2
static void *CreateCodecBCJ2() { return (void *)(ICompressCoder2 *)(new NCompress::NBcj2::CDecoder()); }

static CCodecInfo g_CodecInfoBCJ2 =
{ CreateCodecBCJ2, NULL, 0x0303011B, L"BCJ2", 4, false };

// BCJ
static void *CreateCodecBCJ() { return (void *)(ICompressFilter *)(new CBCJ_x86_Decoder()); }

static CCodecInfo g_CodecInfoBCJ =
{ CreateCodecBCJ, NULL, 0x03030103, L"BCJ", 1, true };

// Copy
static void *CreateCodecCopy() { return (void *)(ICompressCoder *)(new NCompress::CCopyCoder); }

static CCodecInfo g_CodecInfoCopy =
{ CreateCodecCopy, CreateCodecCopy, 0x00, L"Copy", 1, false };

// LZMA2
static void *CreateCodecLZMA2() { return (void *)(ICompressCoder *)(new NCompress::NLzma2::CDecoder); }

static CCodecInfo g_CodecInfoLZMA2 =
{ CreateCodecLZMA2, NULL, 0x21, L"LZMA2", 1, false };

// LZMA
static void *CreateCodecLZMA() { return (void *)(ICompressCoder *)(new NCompress::NLzma::CDecoder); }

static CCodecInfo g_CodecInfoLZMA =
{ CreateCodecLZMA, NULL, 0x030101, L"LZMA", 1, false };

// Initialize all global structures
static void Initialize7z()
{
  static bool bInitialized = false;

  if (bInitialized)
    return;

  CrcGenerateTable();

  RegisterArc(&g_ArcInfo);

  RegisterCodec(&g_CodecInfoBCJ2);
  RegisterCodec(&g_CodecInfoBCJ);
  RegisterCodec(&g_CodecInfoCopy);
  RegisterCodec(&g_CodecInfoLZMA2);
  RegisterCodec(&g_CodecInfoLZMA);

  bInitialized = true;
}

#ifdef _CONSOLE
#include "stdio.h"

void PrintString(const UString &s)
{
  printf("%s", (LPCSTR)GetOemString(s));
}

void PrintString(const AString &s)
{
  printf("%s", (LPCSTR)s);
}

void PrintNewLine()
{
  PrintString("\n");
}

void PrintStringLn(const AString &s)
{
  PrintString(s);
  PrintNewLine();
}

void PrintError(const AString &s)
{
  PrintNewLine();
  PrintString(s);
  PrintNewLine();
}
#else

#define PrintString(s)
#define PrintString(s)
#define PrintNewLine()
#define PrintStringLn(s)

static UString g_sError;

void PrintError(const AString &s)
{
  g_sError += GetUnicodeString(s) + L"\n";
}

const wchar_t* GetExtractorError()
{
  return (const wchar_t*)g_sError;
}

#endif

static HRESULT IsArchiveItemProp(IInArchive *archive, UInt32 index, PROPID propID, bool &result)
{
  NCOM::CPropVariant prop;
  RINOK(archive->GetProperty(index, propID, &prop));
  if (prop.vt == VT_BOOL)
    result = VARIANT_BOOLToBool(prop.boolVal);
  else if (prop.vt == VT_EMPTY)
    result = false;
  else
    return E_FAIL;
  return S_OK;
}

static HRESULT IsArchiveItemFolder(IInArchive *archive, UInt32 index, bool &result)
{
  return IsArchiveItemProp(archive, index, kpidIsDir, result);
}


static const wchar_t *kEmptyFileAlias = L"[Content]";


//////////////////////////////////////////////////////////////
// Archive Open callback class

class CArchiveOpenCallback:
  public IArchiveOpenCallback,
  public ICryptoGetTextPassword,
  public CMyUnknownImp
{
public:
  MY_UNKNOWN_IMP1(ICryptoGetTextPassword)

  STDMETHOD(SetTotal)(const UInt64 *files, const UInt64 *bytes);
  STDMETHOD(SetCompleted)(const UInt64 *files, const UInt64 *bytes);

  STDMETHOD(CryptoGetTextPassword)(BSTR *password);

  bool PasswordIsDefined;
  UString Password;

  CArchiveOpenCallback() : PasswordIsDefined(false) {}
};

STDMETHODIMP CArchiveOpenCallback::SetTotal(const UInt64 * /* files */, const UInt64 * /* bytes */)
{
  return S_OK;
}

STDMETHODIMP CArchiveOpenCallback::SetCompleted(const UInt64 * /* files */, const UInt64 * /* bytes */)
{
  return S_OK;
}
  
STDMETHODIMP CArchiveOpenCallback::CryptoGetTextPassword(BSTR *password)
{
  if (!PasswordIsDefined)
  {
    // You can ask real password here from user
    // Password = GetPassword(OutStream);
    // PasswordIsDefined = true;
    PrintError("Password is not defined");
    return E_ABORT;
  }
  return StringToBstr(Password, password);
}


//////////////////////////////////////////////////////////////
// Archive Extracting callback class

static const wchar_t *kCantDeleteOutputFile = L"ERROR: Can not delete output file ";

static const char *kTestingString    =  "Testing     ";
static const char *kExtractingString =  "Extracting  ";
static const char *kSkippingString   =  "Skipping    ";

static const char *kUnsupportedMethod = "Unsupported Method";
static const char *kCRCFailed = "CRC Failed";
static const char *kDataError = "Data Error";
static const char *kUnknownError = "Unknown Error";

class CArchiveExtractCallback:
  public IArchiveExtractCallback,
  public ICryptoGetTextPassword,
  public CMyUnknownImp
{
public:
  MY_UNKNOWN_IMP1(ICryptoGetTextPassword)

  // IProgress
  STDMETHOD(SetTotal)(UInt64 size);
  STDMETHOD(SetCompleted)(const UInt64 *completeValue);

  // IArchiveExtractCallback
  STDMETHOD(GetStream)(UInt32 index, ISequentialOutStream **outStream, Int32 askExtractMode);
  STDMETHOD(PrepareOperation)(Int32 askExtractMode);
  STDMETHOD(SetOperationResult)(Int32 resultEOperationResult);

  // ICryptoGetTextPassword
  STDMETHOD(CryptoGetTextPassword)(BSTR *aPassword);

private:
  CMyComPtr<IInArchive> _archiveHandler;
  UString _directoryPath;  // Output directory
  UString _filePath;       // name inside archive
  UString _diskFilePath;   // full path to file on disk
  bool _extractMode;
  struct CProcessedFileInfo
  {
    FILETIME MTime;
    UInt32 Attrib;
    bool isDir;
    bool AttribDefined;
    bool MTimeDefined;
  } _processedFileInfo;

  COutFileStream *_outFileStreamSpec;
  CMyComPtr<ISequentialOutStream> _outFileStream;

  SzExtractProgressCallback *_progressCallback;
  UInt32 _numItemsTotal;
  UInt32 _numItemsExtracted;

public:
  void Init(IInArchive *archiveHandler, const UString &directoryPath, SzExtractProgressCallback *progressCallback);

  UInt64 NumErrors;
  bool PasswordIsDefined;
  UString Password;

  CArchiveExtractCallback() : PasswordIsDefined(false) {}
};

void CArchiveExtractCallback::Init(IInArchive *archiveHandler, const UString &directoryPath, SzExtractProgressCallback *progressCallback)
{
  NumErrors = 0;
  _archiveHandler = archiveHandler;
  _directoryPath = directoryPath;
  NFile::NName::NormalizeDirPathPrefix(_directoryPath);
  _progressCallback = progressCallback;
  _numItemsTotal = 0;
  _numItemsExtracted = 0;
  archiveHandler->GetNumberOfItems(&_numItemsTotal);
}

// SetTotal and SetCompleted callback methods show progress
// based on the input buffer, which does not really correspond
// to the actual extraction progress.
// Current implementation uses number of files as the progress indicator,
// which gives better result with an archive containing a lot of files.
STDMETHODIMP CArchiveExtractCallback::SetTotal(UInt64 /* size */)
{
/*
  char s[30];
  ConvertUInt64ToString(size, s);
  PrintString(AString("\n--- Total: "));
  PrintStringLn(s);
*/
  return S_OK;
}

STDMETHODIMP CArchiveExtractCallback::SetCompleted(const UInt64 * /* completeValue */)
{
/*
  char s[30];
  ConvertUInt64ToString(*completeValue, s);
  PrintString(AString("\n--- Completed: "));
  PrintStringLn(s);
*/
  return S_OK;
}

STDMETHODIMP CArchiveExtractCallback::GetStream(UInt32 index,
    ISequentialOutStream **outStream, Int32 askExtractMode)
{
  *outStream = 0;
  _outFileStream.Release();

  {
    // Get Name
    NCOM::CPropVariant prop;
    RINOK(_archiveHandler->GetProperty(index, kpidPath, &prop));
    
    UString fullPath;
    if (prop.vt == VT_EMPTY)
      fullPath = kEmptyFileAlias;
    else
    {
      if (prop.vt != VT_BSTR)
        return E_FAIL;
      fullPath = prop.bstrVal;
    }
    _filePath = fullPath;
  }

  if (askExtractMode != NArchive::NExtract::NAskMode::kExtract)
    return S_OK;

  {
    // Get Attrib
    NCOM::CPropVariant prop;
    RINOK(_archiveHandler->GetProperty(index, kpidAttrib, &prop));
    if (prop.vt == VT_EMPTY)
    {
      _processedFileInfo.Attrib = 0;
      _processedFileInfo.AttribDefined = false;
    }
    else
    {
      if (prop.vt != VT_UI4)
        return E_FAIL;
      _processedFileInfo.Attrib = prop.ulVal;
      _processedFileInfo.AttribDefined = true;
    }
  }

  RINOK(IsArchiveItemFolder(_archiveHandler, index, _processedFileInfo.isDir));

  {
    // Get Modified Time
    NCOM::CPropVariant prop;
    RINOK(_archiveHandler->GetProperty(index, kpidMTime, &prop));
    _processedFileInfo.MTimeDefined = false;
    switch(prop.vt)
    {
      case VT_EMPTY:
        // _processedFileInfo.MTime = _utcMTimeDefault;
        break;
      case VT_FILETIME:
        _processedFileInfo.MTime = prop.filetime;
        _processedFileInfo.MTimeDefined = true;
        break;
      default:
        return E_FAIL;
    }

  }
  /*
  {
    // Get Size
    NCOM::CPropVariant prop;
    RINOK(_archiveHandler->GetProperty(index, kpidSize, &prop));
    bool newFileSizeDefined = (prop.vt != VT_EMPTY);
    UInt64 newFileSize;
    if (newFileSizeDefined)
      newFileSize = ConvertPropVariantToUInt64(prop);
  }
  */
  
  {
    // Create folders for file
    int slashPos = _filePath.ReverseFind(WCHAR_PATH_SEPARATOR);
    if (slashPos >= 0)
      NFile::NDirectory::CreateComplexDirectory(_directoryPath + _filePath.Left(slashPos));
  }

  UString fullProcessedPath = _directoryPath + _filePath;
  _diskFilePath = fullProcessedPath;

  if (_processedFileInfo.isDir)
  {
    NFile::NDirectory::CreateComplexDirectory(fullProcessedPath);
  }
  else
  {
    NFile::NFind::CFileInfoW fi;
    if (fi.Find(fullProcessedPath))
    {
      if (!NFile::NDirectory::DeleteFileAlways(fullProcessedPath))
      {
        PrintString(UString(kCantDeleteOutputFile) + fullProcessedPath);
        return E_ABORT;
      }
    }
    
    _outFileStreamSpec = new COutFileStream;
    CMyComPtr<ISequentialOutStream> outStreamLoc(_outFileStreamSpec);
    if (!_outFileStreamSpec->Open(fullProcessedPath, CREATE_ALWAYS))
    {
      PrintString((UString)L"can not open output file " + fullProcessedPath);
      return E_ABORT;
    }
    _outFileStream = outStreamLoc;
    *outStream = outStreamLoc.Detach();
  }
  return S_OK;
}

STDMETHODIMP CArchiveExtractCallback::PrepareOperation(Int32 askExtractMode)
{
  _extractMode = false;
  switch (askExtractMode)
  {
    case NArchive::NExtract::NAskMode::kExtract:  _extractMode = true; break;
  };
  switch (askExtractMode)
  {
    case NArchive::NExtract::NAskMode::kExtract:  PrintString(kExtractingString); break;
    case NArchive::NExtract::NAskMode::kTest:  PrintString(kTestingString); break;
    case NArchive::NExtract::NAskMode::kSkip:  PrintString(kSkippingString); break;
  };
  PrintString(_filePath);
  _numItemsExtracted++;
  if (_progressCallback)
  {
    _progressCallback(_numItemsExtracted * 100 / _numItemsTotal);
  }
  return S_OK;
}

STDMETHODIMP CArchiveExtractCallback::SetOperationResult(Int32 operationResult)
{
  switch(operationResult)
  {
    case NArchive::NExtract::NOperationResult::kOK:
      break;
    default:
    {
      NumErrors++;
      PrintString("     ");
      switch(operationResult)
      {
        case NArchive::NExtract::NOperationResult::kUnSupportedMethod:
          PrintString(kUnsupportedMethod);
          break;
        case NArchive::NExtract::NOperationResult::kCRCError:
          PrintString(kCRCFailed);
          break;
        case NArchive::NExtract::NOperationResult::kDataError:
          PrintString(kDataError);
          break;
        default:
          PrintString(kUnknownError);
      }
    }
  }

  if (_outFileStream != NULL)
  {
    if (_processedFileInfo.MTimeDefined)
      _outFileStreamSpec->SetMTime(&_processedFileInfo.MTime);
    RINOK(_outFileStreamSpec->Close());
  }
  _outFileStream.Release();
  if (_extractMode && _processedFileInfo.AttribDefined)
    NFile::NDirectory::MySetFileAttributes(_diskFilePath, _processedFileInfo.Attrib);
  PrintNewLine();
  return S_OK;
}


STDMETHODIMP CArchiveExtractCallback::CryptoGetTextPassword(BSTR *password)
{
  if (!PasswordIsDefined)
  {
    // You can ask real password here from user
    // Password = GetPassword(OutStream);
    // PasswordIsDefined = true;
    PrintError("Password is not defined");
    return E_ABORT;
  }
  return StringToBstr(Password, password);
}

static WRes MyCreateDir(const WCHAR *name)
{
  return CreateDirectoryW(name, NULL) ? 0 : GetLastError();
}

static WRes CreateOutputDir(const WCHAR *outputDir)
{
  WRes res = SZ_OK;
  WCHAR name[MAX_PATH];
  size_t j;
  if (outputDir == NULL || outputDir[0] == 0)
    return SZ_ERROR_PARAM;
  wcsncpy(name, outputDir, MAX_PATH-1);
  name[MAX_PATH-1] = 0;

  for (j = 1; name[j] != 0 && res == 0; j++)
  {
    if (name[j] == CHAR_PATH_SEPARATOR)
    {
      name[j] = 0;
      res = MyCreateDir(name);
      name[j] = CHAR_PATH_SEPARATOR;
    }
  }
  if (res == 0 && name[wcslen(name) - 1] != CHAR_PATH_SEPARATOR)
  {
    res = MyCreateDir(name);
  }
  return res;
}

//////////////////////////////////////////////////////////////////////////
// Main extract functions

/**
 * Extract 7z-archive
 *
 * @param archiveFileName  Name of the archive
 * @param fileToExtract    Name of the file to extract (if NULL - extract all files)
 * @param outputDir        Output directory for extracted files
 * @param progressCallback Function to be called on each file - can show the progress
 */
int SzExtract(const WCHAR *archiveName,
              const WCHAR *fileToExtract, const WCHAR *outputDir,
              SzExtractProgressCallback *progressCallback)
{
  return SzExtractSfx(archiveName, 0, fileToExtract, outputDir, progressCallback);
}

/**
 * Extract 7z-SFX-archive
 *
 * @param archiveFileName  Name of the archive
 * @param sfxStubSize      Size of the stub at the beginning of the file before the actual archive data (could be 0)
 * @param fileToExtract    Name of the file to extract (if NULL - extract all files)
 * @param outputDir        Output directory for extracted files
 * @param progressCallback Function to be called on each file to show the progress
 */
int SzExtractSfx(const WCHAR *archiveName, DWORD sfxStubSize,
              const WCHAR *fileToExtract, const WCHAR *outputDir,
              SzExtractProgressCallback *progressCallback)
{
  Initialize7z();

  CreateOutputDir(outputDir);

#ifdef _DEBUG_OUTPUT
  PrintString("Loading archive ");
  PrintString(archiveName);
  PrintNewLine();
#endif

  // Extracting
  {
    CMyComPtr<IInArchive> archive;
    if (CreateArchiver(&CLSID_CFormat7z, &IID_IInArchive, (void **)&archive) != S_OK)
    {
      PrintError("Can not get class object");
      return SZ_ERROR_FAIL;
    }

#ifdef _DEBUG_OUTPUT
    PrintStringLn("Created archiver");
#endif
    
    CInFileStream *fileSpec = new CInFileStream;
    CMyComPtr<IInStream> file = fileSpec;
    
    if (!fileSpec->Open(archiveName))
    {
      PrintError("Can not open archive file");
      return SZ_ERROR_NO_ARCHIVE;
    }
    if (sfxStubSize > 0)
      file->Seek(sfxStubSize, STREAM_SEEK_SET, NULL);

#ifdef _DEBUG_OUTPUT
    PrintStringLn("Opened file");
#endif

    {
      CArchiveOpenCallback *openCallbackSpec = new CArchiveOpenCallback;
      CMyComPtr<IArchiveOpenCallback> openCallback(openCallbackSpec);
      openCallbackSpec->PasswordIsDefined = false;
      // openCallbackSpec->PasswordIsDefined = true;
      // openCallbackSpec->Password = L"1";
      
      if (archive->Open(file, 0, openCallback) != S_OK)
      {
        PrintError("Can not open archive");
        return SZ_ERROR_NO_ARCHIVE;
      }
    }
    
#ifdef _DEBUG_OUTPUT
    PrintStringLn("Extracting...");
#endif

    CArchiveExtractCallback *extractCallbackSpec = new CArchiveExtractCallback;
    CMyComPtr<IArchiveExtractCallback> extractCallback(extractCallbackSpec);
    extractCallbackSpec->Init(archive, outputDir, progressCallback);
    extractCallbackSpec->PasswordIsDefined = false;
    // extractCallbackSpec->PasswordIsDefined = true;
    // extractCallbackSpec->Password = L"1";

    HRESULT result = S_OK;
    UInt32 numItems = 0;
    archive->GetNumberOfItems(&numItems);
    if (numItems == 0)
    {
	    PrintError("No files found in the archive");
	    return SZ_ERROR_DATA;
    }
    if (fileToExtract)
    {
      // Extract one file
      for (UInt32 i = 0; i < numItems; i++)
      {
        // Get name of file
        NWindows::NCOM::CPropVariant prop;
        archive->GetProperty(i, kpidPath, &prop);
        UString s = ConvertPropVariantToString(prop);
        if (wcscmp(fileToExtract, s) == 0)
        {
          PrintString(s);
          PrintString("\n");
          // Extract the current file
          result = archive->Extract(&i, 1, false, extractCallback);
          break;
        }
      }
    }
    else
    {
      // Extract all
      result = archive->Extract(NULL, (UInt32)(Int32)(-1), false, extractCallback);
    }
    if (result != S_OK)
    {
	    PrintError("Extract Error");
	    return SZ_ERROR_DATA;
    }
  }
  return SZ_OK;
}

/**
 * Get information about 7z-SFX-archive
 *
 * @param archiveFileName    Name of the archive
 * @param sfxStubSize        Size of the stub at the beginning of the file before the actual archive data (could be 0)
 *
 * Output parameters:
 * @param pUncompressedSize  Pointer to 64 bit integer for the total uncompressed size
 * @param pNumberOfFiles     (optional) Pointer to a number of files in the archive
 * @param pNumberOfDirs      (optional) Pointer to a number of directories in the archive
 */
int SzGetSfxArchiveInfo(const WCHAR *archiveName, const DWORD sfxStubSize,
                        ULONGLONG *pUncompressedSize, DWORD *pNumberOfFiles, DWORD *pNumberOfDirs)
{
  if (!archiveName || !pUncompressedSize)
    return SZ_ERROR_PARAM;

  *pUncompressedSize = 0;

  Initialize7z();

  CMyComPtr<IInArchive> archive;
  if (CreateArchiver(&CLSID_CFormat7z, &IID_IInArchive, (void **)&archive) != S_OK)
  {
    PrintError("Can not get class object");
    return SZ_ERROR_FAIL;
  }

  CInFileStream *fileSpec = new CInFileStream;
  CMyComPtr<IInStream> file = fileSpec;
  
  if (!fileSpec->Open(archiveName))
  {
    PrintError("Can not open archive file");
    return SZ_ERROR_NO_ARCHIVE;
  }
  if (sfxStubSize > 0)
    file->Seek(sfxStubSize, STREAM_SEEK_SET, NULL);

  CArchiveOpenCallback *openCallbackSpec = new CArchiveOpenCallback;
  CMyComPtr<IArchiveOpenCallback> openCallback(openCallbackSpec);
  openCallbackSpec->PasswordIsDefined = false;
  
  if (archive->Open(file, 0, openCallback) != S_OK)
  {
    PrintError("Can not open archive");
    return SZ_ERROR_NO_ARCHIVE;
  }
  
  UInt32 numItems = 0;
  archive->GetNumberOfItems(&numItems);
  if (numItems == 0)
  {
      PrintError("No files found in the archive");
      return SZ_ERROR_DATA;
  }

  if (pNumberOfFiles)
    *pNumberOfFiles = 0;

  if (pNumberOfDirs)
    *pNumberOfDirs = 0;

  // Iterate through all items
  for (UInt32 i = 0; i < numItems; i++)
  {
    bool isDir = false;
    RINOK(IsArchiveItemFolder(archive, i, isDir));
    if (isDir)
    {
      if (pNumberOfDirs)
        (*pNumberOfDirs)++;
      continue;
    }

    UInt64 unpackSize = 0;

    NWindows::NCOM::CPropVariant prop;
    if (archive->GetProperty(i, kpidSize, &prop) != S_OK)
    {
        PrintError("Cannot get size property value");
        return SZ_ERROR_DATA;
    }
    if (prop.vt != VT_EMPTY)
      unpackSize = ConvertPropVariantToUInt64(prop);

    (*pUncompressedSize) += unpackSize;

    if (pNumberOfFiles)
      (*pNumberOfFiles)++;
  }

  return SZ_OK;
}
