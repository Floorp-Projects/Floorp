// Main.cpp

#include "StdAfx.h"

#include <initguid.h>

#include "Common/StringConvert.h"
#include "Common/Random.h"
#include "Common/TextConfig.h"
#include "Common/CommandLineParser.h"

#include "Windows/FileDir.h"
#include "Windows/FileIO.h"
#include "Windows/FileFind.h"
#include "Windows/FileName.h"
#include "Windows/DLL.h"
#include "Windows/ResourceString.h"

#include "../../IPassword.h"
#include "../../ICoder.h"
#include "../../Archive/IArchive.h"
#include "../../UI/Explorer/MyMessages.h"

// #include "../../UI/GUI/ExtractGUI.h"

#include "ExtractEngine.h"

#include "resource.h"

using namespace NWindows;

HINSTANCE g_hInstance;

static LPCTSTR kTempDirPrefix = TEXT("7zS"); 

#define _SHELL_EXECUTE

/* BEGIN Mozilla customizations */

static char const *
FindStrInBuf(char const * buf, size_t bufLen, char const * str)
{
  size_t index = 0;
  while (index < bufLen) {
    char const * result = strstr(buf + index, str);
    if (result) {
      return result;
    }
    while ((buf[index] != '\0') && (index < bufLen)) {
      index++;
    }
    index++;
  }
  return NULL;
}

static bool
ReadPostSigningDataFromView(char const * view, DWORD size, AString& data)
{
  // Find the offset and length of the certificate table,
  // so we know the valid range to look for the token.
  if (size < (0x3c + sizeof(UInt32))) {
    return false;
  }
  UInt32 PEHeaderOffset = *(UInt32*)(view + 0x3c);
  UInt32 optionalHeaderOffset = PEHeaderOffset + 24;
  UInt32 certDirEntryOffset = 0;
  if (size < (optionalHeaderOffset + sizeof(UInt16))) {
    return false;
  }
  UInt16 magic = *(UInt16*)(view + optionalHeaderOffset);
  if (magic == 0x010b) {
    // 32-bit executable
    certDirEntryOffset = optionalHeaderOffset + 128;
  } else if (magic == 0x020b) {
    // 64-bit executable; certain header fields are wider
    certDirEntryOffset = optionalHeaderOffset + 144;
  } else {
    // Unknown executable
    return false;
  }
  if (size < certDirEntryOffset + 8) {
    return false;
  }
  UInt32 certTableOffset = *(UInt32*)(view + certDirEntryOffset);
  UInt32 certTableLen = *(UInt32*)(view + certDirEntryOffset + sizeof(UInt32));
  if (certTableOffset == 0 || certTableLen == 0 ||
      size < (certTableOffset + certTableLen)) {
    return false;  
  }

  char const token[] = "__MOZCUSTOM__:";
  // We're searching for a string inside a binary blob,
  // so a normal strstr that bails on the first NUL won't work.
  char const * tokenPos = FindStrInBuf(view + certTableOffset,
                                       certTableLen, token);
  if (tokenPos) {
    size_t tokenLen = (sizeof(token) / sizeof(token[0])) - 1;
    data = AString(tokenPos + tokenLen);
    return true;
  }
  return false;
}

static bool
ReadPostSigningData(UString exePath, AString& data)
{
  bool retval = false;
  HANDLE exeFile = CreateFileW(exePath, GENERIC_READ, FILE_SHARE_READ, NULL,
                               OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
  if (exeFile != INVALID_HANDLE_VALUE) {
    HANDLE mapping = CreateFileMapping(exeFile, NULL, PAGE_READONLY, 0, 0, NULL);
    if (mapping != INVALID_HANDLE_VALUE) {
      // MSDN claims the return value on failure is NULL,
      // but I've also seen it returned on success, so double-check.
      if (mapping || GetLastError() == ERROR_SUCCESS) {
        char * view = (char*)MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0);
        if (view) {
          DWORD fileSize = GetFileSize(exeFile, NULL);
          retval = ReadPostSigningDataFromView(view, fileSize, data);
        }
        CloseHandle(mapping);
      }
    }
    CloseHandle(exeFile);
  }
  return retval;
}

/* END Mozilla customizations */


static bool ReadDataString(LPCWSTR fileName, LPCSTR startID, 
    LPCSTR endID, AString &stringResult)
{
  stringResult.Empty();
  NFile::NIO::CInFile inFile;
  if (!inFile.Open(fileName))
    return false;
  const int kBufferSize = (1 << 12);

  Byte buffer[kBufferSize];
  int signatureStartSize = lstrlenA(startID);
  int signatureEndSize = lstrlenA(endID);
  
  UInt32 numBytesPrev = 0;
  bool writeMode = false;
  UInt64 posTotal = 0;
  while(true)
  {
    if (posTotal > (1 << 20))
      return (stringResult.IsEmpty());
    UInt32 numReadBytes = kBufferSize - numBytesPrev;
    UInt32 processedSize;
    if (!inFile.Read(buffer + numBytesPrev, numReadBytes, processedSize))
      return false;
    if (processedSize == 0)
      return true;
    UInt32 numBytesInBuffer = numBytesPrev + processedSize;
    UInt32 pos = 0;
    while (true)
    { 
      if (writeMode)
      {
        if (pos > numBytesInBuffer - signatureEndSize)
          break;
        if (memcmp(buffer + pos, endID, signatureEndSize) == 0)
          return true;
        char b = buffer[pos];
        if (b == 0)
          return false;
        stringResult += b;
        pos++;
      }
      else
      {
        if (pos > numBytesInBuffer - signatureStartSize)
          break;
        if (memcmp(buffer + pos, startID, signatureStartSize) == 0)
        {
          writeMode = true;
          pos += signatureStartSize;
        }
        else
          pos++;
      }
    }
    numBytesPrev = numBytesInBuffer - pos;
    posTotal += pos;
    memmove(buffer, buffer + pos, numBytesPrev);
  }
}

static char kStartID[] = ",!@Install@!UTF-8!";
static char kEndID[] = ",!@InstallEnd@!";

class CInstallIDInit
{
public:
  CInstallIDInit()
  {
    kStartID[0] = ';';
    kEndID[0] = ';';
  };
} g_CInstallIDInit;


class CCurrentDirRestorer
{
  CSysString m_CurrentDirectory;
public:
  CCurrentDirRestorer()
    { NFile::NDirectory::MyGetCurrentDirectory(m_CurrentDirectory); }
  ~CCurrentDirRestorer()
    { RestoreDirectory();}
  bool RestoreDirectory()
    { return BOOLToBool(::SetCurrentDirectory(m_CurrentDirectory)); }
};

#ifndef _UNICODE
bool g_IsNT = false;
static inline bool IsItWindowsNT()
{
  OSVERSIONINFO versionInfo;
  versionInfo.dwOSVersionInfoSize = sizeof(versionInfo);
  if (!::GetVersionEx(&versionInfo)) 
    return false;
  return (versionInfo.dwPlatformId == VER_PLATFORM_WIN32_NT);
}
#endif

// Delayed load libraries are loaded when the first symbol is used.
// The following ensures that we load the delayed loaded libraries from the
// system directory.
struct AutoLoadSystemDependencies
{
  AutoLoadSystemDependencies()
  {
    static LPCWSTR delayDLLs[] = { L"dwmapi.dll", L"cryptbase.dll",
                                   L"SHCore.dll", L"uxtheme.dll",
                                   L"oleacc.dll", L"apphelp.dll" };
    WCHAR systemDirectory[MAX_PATH + 1] = { L'\0' };
    // If GetSystemDirectory fails we accept that we'll load the DLLs from the
    // normal search path.
    GetSystemDirectoryW(systemDirectory, MAX_PATH + 1);
    size_t systemDirLen = wcslen(systemDirectory);

    // Make the system directory path terminate with a slash
    if (systemDirectory[systemDirLen - 1] != L'\\' && systemDirLen) {
      systemDirectory[systemDirLen] = L'\\';
      ++systemDirLen;
      // No need to re-NULL terminate
    }

    // For each known DLL ensure it is loaded from the system32 directory
    for (size_t i = 0; i < sizeof(delayDLLs) / sizeof(delayDLLs[0]); ++i) {
      size_t fileLen = wcslen(delayDLLs[i]);
      wcsncpy(systemDirectory + systemDirLen, delayDLLs[i],
      MAX_PATH - systemDirLen);
      if (systemDirLen + fileLen <= MAX_PATH) {
        systemDirectory[systemDirLen + fileLen] = L'\0';
      } else {
        systemDirectory[MAX_PATH] = L'\0';
      }
      LPCWSTR fullModulePath = systemDirectory; // just for code readability
      LoadLibraryW(fullModulePath);
    }
  }
} loadDLLs;

BOOL
RemoveCurrentDirFromSearchPath()
{
  // kernel32.dll is in the knownDLL list so it is safe to load without a full path
  HMODULE kernel32 = LoadLibraryW(L"kernel32.dll");
  if (!kernel32) {
    return FALSE;
  }

  typedef BOOL (WINAPI *SetDllDirectoryType)(LPCWSTR);
  SetDllDirectoryType SetDllDirectoryFn =
    (SetDllDirectoryType)GetProcAddress(kernel32, "SetDllDirectoryW");
  if (!SetDllDirectoryFn) {
    FreeLibrary(kernel32);
    return FALSE;
  }

  // If this call fails we can't do much about it, so ignore it.
  // It is unlikely to fail and this is just a precaution anyway.
  SetDllDirectoryFn(L"");
  FreeLibrary(kernel32);
  return TRUE;
}

int APIENTRY WinMain(
  HINSTANCE hInstance,
  HINSTANCE hPrevInstance,
  LPSTR lpCmdLine,
  int nCmdShow)
{
  // Disable current directory from being in the search path.
  // This call does not help with implicitly loaded DLLs.
  if (!RemoveCurrentDirFromSearchPath()) {
    WCHAR minOSTitle[512] = { '\0' };
    WCHAR minOSText[512] = { '\0' };
    LoadStringW(NULL, IDS_MIN_OS_TITLE, minOSTitle,
                sizeof(minOSTitle) / sizeof(minOSTitle[0]));
    LoadStringW(NULL, IDS_MIN_OS_TEXT, minOSText,
                sizeof(minOSText) / sizeof(minOSText[0]));
    MessageBoxW(NULL, minOSText, minOSTitle, MB_OK | MB_ICONERROR);
    return 1;
  }

  g_hInstance = (HINSTANCE)hInstance;
  #ifndef _UNICODE
  g_IsNT = IsItWindowsNT();
  #endif
  InitCommonControls();

  UString archiveName, switches;
  #ifdef _SHELL_EXECUTE
  UString executeFile, executeParameters;
  #endif
  NCommandLineParser::SplitCommandLine(GetCommandLineW(), archiveName, switches);

  UString fullPath;
  NDLL::MyGetModuleFileName(g_hInstance, fullPath);

  switches.Trim();
  bool assumeYes = false;
  if (switches.Left(2).CompareNoCase(UString(L"-y")) == 0)
  {
    assumeYes = true;
    switches = switches.Mid(2);
    switches.Trim();
  }

  /* BEGIN Mozilla customizations */
  bool showProgress = true;
  bool extractOnly = false;
  if (switches.Left(3).CompareNoCase(UString(L"-ms")) == 0 ||
      switches.Left(4).CompareNoCase(UString(L"/ini")) == 0 ||
      switches.Left(2).CompareNoCase(UString(L"/s")) == 0) {
    showProgress = false;
  } else if (switches.Left(12).CompareNoCase(UString(L"/extractdir=")) == 0) {
    assumeYes = true;
    showProgress = false;
    extractOnly = true;
  }
  /* END Mozilla customizations */

  AString config;
  if (!ReadDataString(fullPath, kStartID, kEndID, config))
  {
    if (!assumeYes)
      MyMessageBox(L"Can't load config info");
    return 1;
  }

  UString dirPrefix = L".\\";
  UString appLaunched;
  if (!config.IsEmpty())
  {
    CObjectVector<CTextConfigPair> pairs;
    if (!GetTextConfig(config, pairs))
    {
      if (!assumeYes)
        MyMessageBox(L"Config failed");
      return 1;
    }
    UString friendlyName = GetTextConfigValue(pairs, L"Title");
    UString installPrompt = GetTextConfigValue(pairs, L"BeginPrompt");
    UString progress = GetTextConfigValue(pairs, L"Progress");
    if (progress.CompareNoCase(L"no") == 0)
      showProgress = false;
    int index = FindTextConfigItem(pairs, L"Directory");
    if (index >= 0)
      dirPrefix = pairs[index].String;
    if (!installPrompt.IsEmpty() && !assumeYes)
    {
      if (MessageBoxW(0, installPrompt, friendlyName, MB_YESNO | 
          MB_ICONQUESTION) != IDYES)
        return 0;
    }
    appLaunched = GetTextConfigValue(pairs, L"RunProgram");
    
    #ifdef _SHELL_EXECUTE
    executeFile = GetTextConfigValue(pairs, L"ExecuteFile");
    executeParameters = GetTextConfigValue(pairs, L"ExecuteParameters") + switches;
    #endif
  }

  NFile::NDirectory::CTempDirectory tempDir;
  /* Mozilla customizations - Added !extractOnly */
  if (!extractOnly && !tempDir.Create(kTempDirPrefix))
  {
    if (!assumeYes)
      MyMessageBox(L"Can not create temp folder archive");
    return 1;
  }

  /* BEGIN Mozilla customizations */
  UString tempDirPath = (extractOnly ? switches.Mid(12) : GetUnicodeString(tempDir.GetPath()));
  /* END Mozilla customizations */

  COpenCallbackGUI openCallback;

  bool isCorrupt = false;
  UString errorMessage;
  HRESULT result = ExtractArchive(fullPath, tempDirPath, &openCallback, showProgress, 
      isCorrupt, errorMessage);

  if (result != S_OK)
  {
    if (!assumeYes)
    {
      if (result == S_FALSE || isCorrupt)
      {
        errorMessage = NWindows::MyLoadStringW(IDS_EXTRACTION_ERROR_MESSAGE);
        result = E_FAIL;
      }
      if (result != E_ABORT && !errorMessage.IsEmpty())
        ::MessageBoxW(0, errorMessage, NWindows::MyLoadStringW(IDS_EXTRACTION_ERROR_TITLE), MB_ICONERROR);
    }
    return 1;
  }

  /* BEGIN Mozilla customizations */
  // Retrieve and store any data added to this file after signing.
  {
    AString postSigningData;
    if (ReadPostSigningData(fullPath, postSigningData)) {
      NFile::NName::CParsedPath postSigningDataFilePath;
      postSigningDataFilePath.ParsePath(tempDirPath);
      postSigningDataFilePath.PathParts.Add(L"postSigningData");

      NFile::NIO::COutFile postSigningDataFile;
      postSigningDataFile.Create(postSigningDataFilePath.MergePath(), true);

      UInt32 written = 0;
      postSigningDataFile.Write(postSigningData, postSigningData.Length(), written);
    }
  }

  if (extractOnly) {
    return 0;
  }
  /* END Mozilla customizations */

  CCurrentDirRestorer currentDirRestorer;

  if (!SetCurrentDirectory(tempDir.GetPath()))
    return 1;
  
  HANDLE hProcess = 0;
#ifdef _SHELL_EXECUTE
  if (!executeFile.IsEmpty())
  {
    CSysString filePath = GetSystemString(executeFile);
    SHELLEXECUTEINFO execInfo;
    execInfo.cbSize = sizeof(execInfo);
    execInfo.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_FLAG_DDEWAIT;
    execInfo.hwnd = NULL;
    execInfo.lpVerb = NULL;
    execInfo.lpFile = filePath;

    if (!switches.IsEmpty())
      executeParameters += switches;

    CSysString parametersSys = GetSystemString(executeParameters);
    if (parametersSys.IsEmpty())
      execInfo.lpParameters = NULL;
    else
      execInfo.lpParameters = parametersSys;

    execInfo.lpDirectory = NULL;
    execInfo.nShow = SW_SHOWNORMAL;
    execInfo.hProcess = 0;
    bool success = BOOLToBool(::ShellExecuteEx(&execInfo));
    result = (UINT32)execInfo.hInstApp;
    if(result <= 32)
    {
      if (!assumeYes)
        MyMessageBox(L"Can not open file");
      return 1;
    }
    hProcess = execInfo.hProcess;
  }
  else
#endif
  {
    if (appLaunched.IsEmpty())
    {
      appLaunched = L"setup.exe";
      if (!NFile::NFind::DoesFileExist(GetSystemString(appLaunched)))
      {
        if (!assumeYes)
          MyMessageBox(L"Can not find setup.exe");
        return 1;
      }
    }
    
    {
      UString s2 = tempDirPath;
      NFile::NName::NormalizeDirPathPrefix(s2);
      appLaunched.Replace(L"%%T\\", s2);
    }
    
    appLaunched.Replace(L"%%T", tempDirPath);

    if (!switches.IsEmpty())
    {
      appLaunched += L' ';
      appLaunched += switches;
    }
    STARTUPINFO startupInfo;
    startupInfo.cb = sizeof(startupInfo);
    startupInfo.lpReserved = 0;
    startupInfo.lpDesktop = 0;
    startupInfo.lpTitle = 0;
    startupInfo.dwFlags = 0;
    startupInfo.cbReserved2 = 0;
    startupInfo.lpReserved2 = 0;
    
    PROCESS_INFORMATION processInformation;
    
    CSysString appLaunchedSys = GetSystemString(dirPrefix + appLaunched);
    
    BOOL createResult = CreateProcess(NULL, (LPTSTR)(LPCTSTR)appLaunchedSys, 
      NULL, NULL, FALSE, 0, NULL, NULL /*tempDir.GetPath() */, 
      &startupInfo, &processInformation);
    if (createResult == 0)
    {
      if (!assumeYes)
        ShowLastErrorMessage();
      return 1;
    }
    ::CloseHandle(processInformation.hThread);
    hProcess = processInformation.hProcess;
  }
  if (hProcess != 0)
  {
    WaitForSingleObject(hProcess, INFINITE);
    ::CloseHandle(hProcess);
  }
  return 0;
}
