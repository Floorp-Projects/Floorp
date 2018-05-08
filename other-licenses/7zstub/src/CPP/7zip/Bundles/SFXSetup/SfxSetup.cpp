// Main.cpp

#include "StdAfx.h"

#include "../../../Common/MyWindows.h"

#include "../../../Common/MyInitGuid.h"

#include "../../../Common/CommandLineParser.h"
#include "../../../Common/StringConvert.h"
#include "../../../Common/TextConfig.h"

#include "../../../Windows/DLL.h"
#include "../../../Windows/ErrorMsg.h"
#include "../../../Windows/FileDir.h"
#include "../../../Windows/FileFind.h"
#include "../../../Windows/FileIO.h"
#include "../../../Windows/FileName.h"
#include "../../../Windows/NtCheck.h"
#include "../../../Windows/ResourceString.h"

#include "../../UI/Explorer/MyMessages.h"

#include "ExtractEngine.h"

#include "../../../../C/DllSecur.h"

#include "resource.h"

using namespace NWindows;
using namespace NFile;
using namespace NDir;

HINSTANCE g_hInstance;

static CFSTR const kTempDirPrefix = FTEXT("7zS");

#define _SHELL_EXECUTE

static bool ReadDataString(CFSTR fileName, LPCSTR startID,
    LPCSTR endID, AString &stringResult)
{
  stringResult.Empty();
  NIO::CInFile inFile;
  if (!inFile.Open(fileName))
    return false;
  const int kBufferSize = (1 << 12);

  Byte buffer[kBufferSize];
  int signatureStartSize = MyStringLen(startID);
  int signatureEndSize = MyStringLen(endID);
  
  UInt32 numBytesPrev = 0;
  bool writeMode = false;
  UInt64 posTotal = 0;
  for (;;)
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
    for (;;)
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

static char kStartID[] = { ',','!','@','I','n','s','t','a','l','l','@','!','U','T','F','-','8','!', 0 };
static char kEndID[]   = { ',','!','@','I','n','s','t','a','l','l','E','n','d','@','!', 0 };

struct CInstallIDInit
{
  CInstallIDInit()
  {
    kStartID[0] = ';';
    kEndID[0] = ';';
  };
} g_CInstallIDInit;


#define NT_CHECK_FAIL_ACTION ShowErrorMessage(L"Unsupported Windows version"); return 1;

static void ShowErrorMessageSpec(const UString &name)
{
  UString message = NError::MyFormatMessage(::GetLastError());
  int pos = message.Find(L"%1");
  if (pos >= 0)
  {
    message.Delete(pos, 2);
    message.Insert(pos, name);
  }
  ShowErrorMessage(NULL, message);
}

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

// Delayed load libraries are loaded when the first symbol is used.
// The following ensures that we load the delayed loaded libraries from the
// system directory.
struct AutoLoadSystemDependencies
{
  AutoLoadSystemDependencies()
  {
    HMODULE module = ::GetModuleHandleW(L"kernel32.dll");
    if (module) {
      // SetDefaultDllDirectories is always available on Windows 8 and above. It
      // is also available on Windows Vista, Windows Server 2008, and
      // Windows 7 when MS KB2533623 has been applied.
      typedef BOOL (WINAPI *SetDefaultDllDirectoriesType)(DWORD);
      SetDefaultDllDirectoriesType setDefaultDllDirectories =
        (SetDefaultDllDirectoriesType) GetProcAddress(module, "SetDefaultDllDirectories");
      if (setDefaultDllDirectories) {
        setDefaultDllDirectories(0x0800 /* LOAD_LIBRARY_SEARCH_SYSTEM32 */ );
        return;
      }
    }

    static LPCWSTR delayDLLs[] = { L"uxtheme.dll", L"userenv.dll",
                                   L"setupapi.dll", L"apphelp.dll",
                                   L"propsys.dll", L"dwmapi.dll",
                                   L"cryptbase.dll", L"oleacc.dll",
                                   L"clbcatq.dll" };
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

/* END Mozilla customizations */

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE /* hPrevInstance */,
    #ifdef UNDER_CE
    LPWSTR
    #else
    LPSTR
    #endif
    /* lpCmdLine */,int /* nCmdShow */)
{
  /* BEGIN Mozilla customizations */
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
  /* END Mozilla customizations */

  g_hInstance = (HINSTANCE)hInstance;

  NT_CHECK

  // BEGIN Mozilla customizations
  // Our AutoLoadSystemDependencies (see above) does the same job as the
  // LoadSecurityDlls function, but slightly better because it runs as a static
  // initializer, and it doesn't include LOAD_LIBRARY_SEARCH_USER_DIRS in
  // the search path, which partially defeats the purpose of calling
  // SetDefaultDllDirectories at all.
  //#ifdef _WIN32
  //LoadSecurityDlls();
  //#endif
  // END Mozilla customizations

  // InitCommonControls();

  UString archiveName, switches;
  #ifdef _SHELL_EXECUTE
  UString executeFile, executeParameters;
  #endif
  NCommandLineParser::SplitCommandLine(GetCommandLineW(), archiveName, switches);

  FString fullPath;
  NDLL::MyGetModuleFileName(fullPath);

  switches.Trim();
  bool assumeYes = false;
  if (switches.IsPrefixedBy_Ascii_NoCase("-y"))
  {
    assumeYes = true;
    switches = switches.Ptr(2);
    switches.Trim();
  }

  AString config;
  if (!ReadDataString(fullPath, kStartID, kEndID, config))
  {
    if (!assumeYes)
      ShowErrorMessage(L"Can't load config info");
    return 1;
  }

  UString dirPrefix ("." STRING_PATH_SEPARATOR);
  UString appLaunched;
  bool showProgress = true;

  /* BEGIN Mozilla customizations */
  bool extractOnly = false;
  if (switches.IsPrefixedBy_NoCase(L"/extractdir=")) {
    assumeYes = true;
    showProgress = false;
    extractOnly = true;
  } else if (!switches.IsEmpty()) {
    showProgress = false;
  }
  /* END Mozilla customizations */

  if (!config.IsEmpty())
  {
    CObjectVector<CTextConfigPair> pairs;
    if (!GetTextConfig(config, pairs))
    {
      if (!assumeYes)
        ShowErrorMessage(L"Config failed");
      return 1;
    }
    UString friendlyName = GetTextConfigValue(pairs, "Title");
    UString installPrompt = GetTextConfigValue(pairs, "BeginPrompt");
    UString progress = GetTextConfigValue(pairs, "Progress");
    if (progress.IsEqualTo_Ascii_NoCase("no"))
      showProgress = false;
    int index = FindTextConfigItem(pairs, "Directory");
    if (index >= 0)
      dirPrefix = pairs[index].String;
    if (!installPrompt.IsEmpty() && !assumeYes)
    {
      if (MessageBoxW(0, installPrompt, friendlyName, MB_YESNO |
          MB_ICONQUESTION) != IDYES)
        return 0;
    }
    appLaunched = GetTextConfigValue(pairs, "RunProgram");
    
    #ifdef _SHELL_EXECUTE
    executeFile = GetTextConfigValue(pairs, "ExecuteFile");
    executeParameters = GetTextConfigValue(pairs, "ExecuteParameters");
    #endif
  }

  CTempDir tempDir;
  /* Mozilla customizations - Added !extractOnly */
  if (!extractOnly && !tempDir.Create(kTempDirPrefix))
  {
    if (!assumeYes)
      ShowErrorMessage(L"Can not create temp folder archive");
    return 1;
  }

  CCodecs *codecs = new CCodecs;
  CMyComPtr<IUnknown> compressCodecsInfo = codecs;
  {
    HRESULT result = codecs->Load();
    if (result != S_OK)
    {
      ShowErrorMessage(L"Can not load codecs");
      return 1;
    }
  }

  /* BEGIN Mozilla customizations - added extractOnly  parameter support */
  const FString tempDirPath = extractOnly ? switches.Ptr(12) : GetUnicodeString(tempDir.GetPath());
  /* END Mozilla customizations */
  // tempDirPath = L"M:\\1\\"; // to test low disk space
  {
    bool isCorrupt = false;
    UString errorMessage;
    HRESULT result = ExtractArchive(codecs, fullPath, tempDirPath, showProgress,
      isCorrupt, errorMessage);
    
    if (result != S_OK)
    {
      if (!assumeYes)
      {
        if (result == S_FALSE || isCorrupt)
        {
          NWindows::MyLoadString(IDS_EXTRACTION_ERROR_MESSAGE, errorMessage);
          result = E_FAIL;
        }
        if (result != E_ABORT)
        {
          if (errorMessage.IsEmpty())
            errorMessage = NError::MyFormatMessage(result);
          ::MessageBoxW(0, errorMessage, NWindows::MyLoadString(IDS_EXTRACTION_ERROR_TITLE), MB_ICONERROR);
        }
      }
      return 1;
    }
  }

  /* BEGIN Mozilla customizations */
  // Retrieve and store any data added to this file after signing.
  {
    AString postSigningData;
    if (ReadPostSigningData(fullPath, postSigningData)) {
      FString postSigningDataFilePath(tempDirPath);
      NFile::NName::NormalizeDirPathPrefix(postSigningDataFilePath);
      postSigningDataFilePath += L"postSigningData";

      NFile::NIO::COutFile postSigningDataFile;
      postSigningDataFile.Create(postSigningDataFilePath, true);

      UInt32 written = 0;
      postSigningDataFile.Write(postSigningData, postSigningData.Len(), written);
    }
  }

  if (extractOnly) {
    return 0;
  }
  /* END Mozilla customizations */

  #ifndef UNDER_CE
  CCurrentDirRestorer currentDirRestorer;
  if (!SetCurrentDir(tempDirPath))
    return 1;
  #endif
  
  HANDLE hProcess = 0;
#ifdef _SHELL_EXECUTE
  if (!executeFile.IsEmpty())
  {
    CSysString filePath (GetSystemString(executeFile));
    SHELLEXECUTEINFO execInfo;
    execInfo.cbSize = sizeof(execInfo);
    execInfo.fMask = SEE_MASK_NOCLOSEPROCESS
      #ifndef UNDER_CE
      | SEE_MASK_FLAG_DDEWAIT
      #endif
      ;
    execInfo.hwnd = NULL;
    execInfo.lpVerb = NULL;
    execInfo.lpFile = filePath;

    if (!switches.IsEmpty())
    {
      executeParameters.Add_Space_if_NotEmpty();
      executeParameters += switches;
    }

    CSysString parametersSys (GetSystemString(executeParameters));
    if (parametersSys.IsEmpty())
      execInfo.lpParameters = NULL;
    else
      execInfo.lpParameters = parametersSys;

    execInfo.lpDirectory = NULL;
    execInfo.nShow = SW_SHOWNORMAL;
    execInfo.hProcess = 0;
    /* BOOL success = */ ::ShellExecuteEx(&execInfo);
    UINT32 result = (UINT32)(UINT_PTR)execInfo.hInstApp;
    if (result <= 32)
    {
      if (!assumeYes)
        ShowErrorMessage(L"Can not open file");
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
      if (!NFind::DoesFileExist(us2fs(appLaunched)))
      {
        if (!assumeYes)
          ShowErrorMessage(L"Can not find setup.exe");
        return 1;
      }
    }
    
    {
      FString s2 = tempDirPath;
      NName::NormalizeDirPathPrefix(s2);
      appLaunched.Replace(L"%%T" WSTRING_PATH_SEPARATOR, fs2us(s2));
    }
    
    UString appNameForError = appLaunched; // actually we need to rtemove parameters also

    appLaunched.Replace(L"%%T", fs2us(tempDirPath));

    if (!switches.IsEmpty())
    {
      appLaunched.Add_Space();
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
    
    CSysString appLaunchedSys (GetSystemString(dirPrefix + appLaunched));
    
    BOOL createResult = CreateProcess(NULL, (LPTSTR)(LPCTSTR)appLaunchedSys,
      NULL, NULL, FALSE, 0, NULL, NULL /*tempDir.GetPath() */,
      &startupInfo, &processInformation);
    if (createResult == 0)
    {
      if (!assumeYes)
      {
        // we print name of exe file, if error message is
        // ERROR_BAD_EXE_FORMAT: "%1 is not a valid Win32 application".
        ShowErrorMessageSpec(appNameForError);
      }
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
