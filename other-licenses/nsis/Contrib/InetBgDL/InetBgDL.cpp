//
// Copyright (C) Anders Kjersem. Licensed under the zlib/libpng license
//

// This file is intended to be compiled with MSVC's Omit Default Library Name (/Zl)
// option enabled, in order to keep the file size low for bundling this DLL with
// the stub installer. That means that any code requiring the C runtime will fail
// to link. You'll see a couple of odd-looking things here for this reason; they
// should all be called out with comments.

#include "InetBgDL.h"

#define USERAGENT _T("NSIS InetBgDL (Mozilla)")

#define STATUS_COMPLETEDALL 0
#define STATUS_INITIAL 202
#define STATUS_CONNECTING STATUS_INITIAL //102
#define STATUS_DOWNLOADING STATUS_INITIAL
#define STATUS_ERR_GETLASTERROR 418 //HTTP: I'm a teapot: Win32 error code in $3
#define STATUS_ERR_LOCALFILEWRITEERROR 450 //HTTP: MS parental control extension
#define STATUS_ERR_CANCELLED 499
#define STATUS_ERR_CONNECTION_LOST 1000

typedef DWORD FILESIZE_T; // Limit to 4GB for now...
#define FILESIZE_UNKNOWN (-1)

#define MAX_STRLEN 1024

HINSTANCE g_hInst;
NSIS::stack_t*g_pLocations = NULL;
HANDLE g_hThread = NULL;
HANDLE g_hGETStartedEvent = NULL;
HINTERNET g_hInetSes = NULL;
HINTERNET g_hInetFile = NULL;
volatile UINT g_FilesTotal = 0;
volatile UINT g_FilesCompleted = 0;
volatile UINT g_Status = STATUS_INITIAL;
volatile FILESIZE_T g_cbCurrXF;
volatile FILESIZE_T g_cbCurrTot = FILESIZE_UNKNOWN;
CRITICAL_SECTION g_CritLock;
UINT g_N_CCH;
PTSTR g_N_Vars;
TCHAR g_ServerIP[128] = { _T('\0') };

DWORD g_ConnectTimeout = 0;
DWORD g_ReceiveTimeout = 0;

// Setup a buffer of size 256KiB to store the downloaded data.
constexpr UINT g_cbBufXF = 262144;
// This buffer is only needed inside TaskThreadProc(), but declaring it on
// the stack there triggers a runtime stack size check, which is implemented
// by a C runtime library function, so we have to avoid the compiler wanting
// to build that check by not having any large stack buffers.
BYTE g_bufXF[g_cbBufXF];

#define NSISPI_INITGLOBALS(N_CCH, N_Vars) do { \
  g_N_CCH = N_CCH; \
  g_N_Vars = N_Vars; \
  } while(0)

#define ONELOCKTORULETHEMALL
#ifdef ONELOCKTORULETHEMALL
#define TaskLock_AcquireExclusive() EnterCriticalSection(&g_CritLock)
#define TaskLock_ReleaseExclusive() LeaveCriticalSection(&g_CritLock)
#define StatsLock_AcquireExclusive() TaskLock_AcquireExclusive()
#define StatsLock_ReleaseExclusive() TaskLock_ReleaseExclusive()
#define StatsLock_AcquireShared() StatsLock_AcquireExclusive()
#define StatsLock_ReleaseShared() StatsLock_ReleaseExclusive()
#endif

// Normally we would just call the C library wcstol, but since we can't use the
// C runtime, we'll supply our own function as an understudy.
static DWORD
MyTStrToL(TCHAR const* str)
{
  if (!str) {
    return 0;
  }

  int len = lstrlen(str);
  DWORD place = 1;
  DWORD rv = 0;
  for (int i = len - 1; i >= 0; --i) {
    int digit = str[i] - 0x30;
    rv += digit * place;
    place *= 10;
  }
  return rv;
}

PTSTR NSIS_SetRegStr(UINT Reg, LPCTSTR Value)
{
  PTSTR s = g_N_Vars + (Reg * g_N_CCH);
  lstrcpy(s, Value);
  return s;
}
#define NSIS_SetRegStrEmpty(r) NSIS_SetRegStr(r, _T(""))
void NSIS_SetRegUINT(UINT Reg, UINT Value)
{
  TCHAR buf[32];
  wsprintf(buf, _T("%u"), Value);
  NSIS_SetRegStr(Reg, buf);
}
#define StackFreeItem(pI) GlobalFree(pI)
NSIS::stack_t* StackPopItem(NSIS::stack_t**ppST)
{
  if (*ppST)
  {
    NSIS::stack_t*pItem = *ppST;
    *ppST = pItem->next;
    return pItem;
  }
  return NULL;
}

void Reset()
{
  // The g_hGETStartedEvent event is used to make sure that the Get() call will
  // acquire the lock before the Reset() call acquires the lock.
  if (g_hGETStartedEvent) {
    TRACE(_T("InetBgDl: waiting on g_hGETStartedEvent\n"));
    WaitForSingleObject(g_hGETStartedEvent, INFINITE);
    CloseHandle(g_hGETStartedEvent);
    g_hGETStartedEvent = NULL;
  }

  TaskLock_AcquireExclusive();
#ifndef ONELOCKTORULETHEMALL
  StatsLock_AcquireExclusive();
#endif
  g_FilesTotal = 0; // This causes the Task thread to exit the transfer loop
  if (g_hThread)
  {
    TRACE(_T("InetBgDl: waiting on g_hThread\n"));
    if (WAIT_OBJECT_0 != WaitForSingleObject(g_hThread, 5 * 1000))
    {
      TRACE(_T("InetBgDl: terminating g_hThread\n"));
      // Suspend the thread so that it's not still trying to use these handles
      // that we're about to close out from under it.
      SuspendThread(g_hThread);
      if (g_hInetFile) {
        InternetCloseHandle(g_hInetFile);
        g_hInetFile = nullptr;
      }
      if (g_hInetSes) {
        InternetCloseHandle(g_hInetSes);
        g_hInetSes = nullptr;
      }
      TerminateThread(g_hThread, ERROR_OPERATION_ABORTED);
    }
    CloseHandle(g_hThread);
    g_hThread = NULL;
  }
  g_FilesTotal = 0;
  g_FilesCompleted = 0;
  g_Status = STATUS_INITIAL;
#ifndef ONELOCKTORULETHEMALL
  StatsLock_ReleaseExclusive();
#endif
  for (NSIS::stack_t*pTmpTast,*pTask = g_pLocations; pTask ;)
  {
    pTmpTast = pTask;
    pTask = pTask->next;
    StackFreeItem(pTmpTast);
  }
  g_pLocations = NULL;
  TaskLock_ReleaseExclusive();
}

UINT_PTR __cdecl NSISPluginCallback(UINT Event)
{
  switch(Event)
  {
  case NSPIM_UNLOAD:
    Reset();
    break;
  }
  return NULL;
}

void __stdcall InetStatusCallback(HINTERNET hInternet, DWORD_PTR dwContext,
                                  DWORD dwInternetStatus,
                                  LPVOID lpvStatusInformation,
                                  DWORD dwStatusInformationLength)
{
  if (dwInternetStatus == INTERNET_STATUS_NAME_RESOLVED) {
    // If we're in the process of being reset, don't try to update g_ServerIP;
    // there's no need for it, and Reset() will be holding the StatsLock, so
    // we'll hang here and block the reset if we try to acquire it.
    if (g_FilesTotal != 0) {
      // The documentation states the IP address is a PCTSTR but it is usually a
      // PCSTR and only sometimes a PCTSTR.
      StatsLock_AcquireExclusive();
      wsprintf(g_ServerIP, _T("%S"), lpvStatusInformation);
      if (lstrlen(g_ServerIP) == 1)
      {
        wsprintf(g_ServerIP, _T("%s"), lpvStatusInformation);
      }
      StatsLock_ReleaseExclusive();
    }
  }

#if defined(PLUGIN_DEBUG)
  switch (dwInternetStatus)
  {
    case INTERNET_STATUS_RESOLVING_NAME:
      TRACE(_T("InetBgDl: INTERNET_STATUS_RESOLVING_NAME (%d), name=%s\n"),
            dwStatusInformationLength, lpvStatusInformation);
      break;
    case INTERNET_STATUS_NAME_RESOLVED:
      TRACE(_T("InetBgDl: INTERNET_STATUS_NAME_RESOLVED (%d), resolved name=%s\n"),
            dwStatusInformationLength, g_ServerIP);
      break;
    case INTERNET_STATUS_CONNECTING_TO_SERVER:
      TRACE(_T("InetBgDl: INTERNET_STATUS_CONNECTING_TO_SERVER (%d)\n"),
            dwStatusInformationLength);
      break;
    case INTERNET_STATUS_CONNECTED_TO_SERVER:
      TRACE(_T("InetBgDl: INTERNET_STATUS_CONNECTED_TO_SERVER (%d)\n"),
            dwStatusInformationLength);
      break;
    case INTERNET_STATUS_SENDING_REQUEST:
      TRACE(_T("InetBgDl: INTERNET_STATUS_SENDING_REQUEST (%d)\n"),
               dwStatusInformationLength);
      break;
    case INTERNET_STATUS_REQUEST_SENT:
      TRACE(_T("InetBgDl: INTERNET_STATUS_REQUEST_SENT (%d), bytes sent=%d\n"),
             dwStatusInformationLength, lpvStatusInformation);
      break;
    case INTERNET_STATUS_RECEIVING_RESPONSE:
      TRACE(_T("InetBgDl: INTERNET_STATUS_RECEIVING_RESPONSE (%d)\n"),
            dwStatusInformationLength);
      break;
    case INTERNET_STATUS_RESPONSE_RECEIVED:
      TRACE(_T("InetBgDl: INTERNET_STATUS_RESPONSE_RECEIVED (%d)\n"),
            dwStatusInformationLength);
      break;
    case INTERNET_STATUS_CTL_RESPONSE_RECEIVED:
      TRACE(_T("InetBgDl: INTERNET_STATUS_CTL_RESPONSE_RECEIVED (%d)\n"),
            dwStatusInformationLength);
      break;
    case INTERNET_STATUS_PREFETCH:
      TRACE(_T("InetBgDl: INTERNET_STATUS_PREFETCH (%d)\n"),
            dwStatusInformationLength);
      break;
    case INTERNET_STATUS_CLOSING_CONNECTION:
      TRACE(_T("InetBgDl: INTERNET_STATUS_CLOSING_CONNECTION (%d)\n"),
            dwStatusInformationLength);
      break;
    case INTERNET_STATUS_CONNECTION_CLOSED:
      TRACE(_T("InetBgDl: INTERNET_STATUS_CONNECTION_CLOSED (%d)\n"),
            dwStatusInformationLength);
      break;
    case INTERNET_STATUS_HANDLE_CREATED:
      TRACE(_T("InetBgDl: INTERNET_STATUS_HANDLE_CREATED (%d)\n"),
            dwStatusInformationLength);
      break;
    case INTERNET_STATUS_HANDLE_CLOSING:
      TRACE(_T("InetBgDl: INTERNET_STATUS_HANDLE_CLOSING (%d)\n"),
            dwStatusInformationLength);
      break;
    case INTERNET_STATUS_DETECTING_PROXY:
      TRACE(_T("InetBgDl: INTERNET_STATUS_DETECTING_PROXY (%d)\n"),
            dwStatusInformationLength);
      break;
    case INTERNET_STATUS_REQUEST_COMPLETE:
      TRACE(_T("InetBgDl: INTERNET_STATUS_REQUEST_COMPLETE (%d)\n"),
            dwStatusInformationLength);
      break;
    case INTERNET_STATUS_REDIRECT:
      TRACE(_T("InetBgDl: INTERNET_STATUS_REDIRECT (%d), new url=%s\n"),
            dwStatusInformationLength, lpvStatusInformation);
      break;
    case INTERNET_STATUS_INTERMEDIATE_RESPONSE:
      TRACE(_T("InetBgDl: INTERNET_STATUS_INTERMEDIATE_RESPONSE (%d)\n"),
            dwStatusInformationLength);
      break;
    case INTERNET_STATUS_USER_INPUT_REQUIRED:
      TRACE(_T("InetBgDl: INTERNET_STATUS_USER_INPUT_REQUIRED (%d)\n"),
            dwStatusInformationLength);
      break;
    case INTERNET_STATUS_STATE_CHANGE:
      TRACE(_T("InetBgDl: INTERNET_STATUS_STATE_CHANGE (%d)\n"),
            dwStatusInformationLength);
      break;
    case INTERNET_STATUS_COOKIE_SENT:
      TRACE(_T("InetBgDl: INTERNET_STATUS_COOKIE_SENT (%d)\n"),
            dwStatusInformationLength);
      break;
    case INTERNET_STATUS_COOKIE_RECEIVED:
      TRACE(_T("InetBgDl: INTERNET_STATUS_COOKIE_RECEIVED (%d)\n"),
            dwStatusInformationLength);
      break;
    case INTERNET_STATUS_PRIVACY_IMPACTED:
      TRACE(_T("InetBgDl: INTERNET_STATUS_PRIVACY_IMPACTED (%d)\n"),
            dwStatusInformationLength);
      break;
    case INTERNET_STATUS_P3P_HEADER:
      TRACE(_T("InetBgDl: INTERNET_STATUS_P3P_HEADER (%d)\n"),
            dwStatusInformationLength);
      break;
    case INTERNET_STATUS_P3P_POLICYREF:
      TRACE(_T("InetBgDl: INTERNET_STATUS_P3P_POLICYREF (%d)\n"),
            dwStatusInformationLength);
      break;
    case INTERNET_STATUS_COOKIE_HISTORY:
      TRACE(_T("InetBgDl: INTERNET_STATUS_COOKIE_HISTORY (%d)\n"),
            dwStatusInformationLength);
      break;
    default:
      TRACE(_T("InetBgDl: Unknown Status %d\n"), dwInternetStatus);
      break;
  }
#endif
}

DWORD CALLBACK TaskThreadProc(LPVOID ThreadParam)
{
  NSIS::stack_t *pURL,*pFile;
  DWORD cbio = sizeof(DWORD);
  DWORD previouslyWritten = 0, writtenThisSession = 0;
  HANDLE hLocalFile;
  bool completedFile = false;
startnexttask:
  hLocalFile = INVALID_HANDLE_VALUE;
  pFile = NULL;
  TaskLock_AcquireExclusive();
  // Now that we've acquired the lock, we can set the event to indicate this.
  // SetEvent will likely never fail, but if it does we should set it to NULL
  // to avoid anyone waiting on it.
  if (!SetEvent(g_hGETStartedEvent)) {
    CloseHandle(g_hGETStartedEvent);
    g_hGETStartedEvent = NULL;
  }
  pURL = g_pLocations;
  if (pURL)
  {
    pFile = pURL->next;
    g_pLocations = pFile->next;
  }
#ifndef ONELOCKTORULETHEMALL
  StatsLock_AcquireExclusive();
#endif
  if (completedFile)
  {
    ++g_FilesCompleted;
  }
  completedFile = false;
  g_cbCurrXF = 0;
  g_cbCurrTot = FILESIZE_UNKNOWN;
  if (!pURL)
  {
    if (g_FilesTotal)
    {
      if (g_FilesTotal == g_FilesCompleted)
      {
        g_Status = STATUS_COMPLETEDALL;
      }
    }
    g_hThread = NULL;
  }
#ifndef ONELOCKTORULETHEMALL
  StatsLock_ReleaseExclusive();
#endif
  TaskLock_ReleaseExclusive();

  if (!pURL)
  {
    if (0)
    {
diegle:
      DWORD gle = GetLastError();
      //TODO? if (ERROR_INTERNET_EXTENDED_ERROR==gle) InternetGetLastResponseInfo(...)
      g_Status = STATUS_ERR_GETLASTERROR;
    }
die:
    if (g_hInetSes)
    {
      InternetCloseHandle(g_hInetSes);
      g_hInetSes = nullptr;
    }
    if (INVALID_HANDLE_VALUE != hLocalFile)
    {
      CloseHandle(hLocalFile);
    }
    StackFreeItem(pURL);
    StackFreeItem(pFile);
    return 0;
  }

  if (!g_hInetSes)
  {
    g_hInetSes = InternetOpen(USERAGENT, INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (!g_hInetSes)
    {
      TRACE(_T("InetBgDl: InternetOpen failed with gle=%u\n"),
            GetLastError());
      goto diegle;
    }
    InternetSetStatusCallback(g_hInetSes, (INTERNET_STATUS_CALLBACK)InetStatusCallback);

    //msdn.microsoft.com/library/default.asp?url=/workshop/components/offline/offline.asp#Supporting Offline Browsing in Applications and Components
    ULONG longOpt;
    DWORD cbio = sizeof(ULONG);
    if (InternetQueryOption(g_hInetSes, INTERNET_OPTION_CONNECTED_STATE, &longOpt, &cbio))
    {
      if (INTERNET_STATE_DISCONNECTED_BY_USER&longOpt)
      {
        INTERNET_CONNECTED_INFO ci = {INTERNET_STATE_CONNECTED, 0};
        InternetSetOption(g_hInetSes, INTERNET_OPTION_CONNECTED_STATE, &ci, sizeof(ci));
      }
    }

    // Change the default connect timeout if specified.
    if(g_ConnectTimeout > 0)
    {
      InternetSetOption(g_hInetSes, INTERNET_OPTION_CONNECT_TIMEOUT,
                        &g_ConnectTimeout, sizeof(g_ConnectTimeout));
    }

    // Change the default receive timeout if specified.
    if (g_ReceiveTimeout)
    {
      InternetSetOption(g_hInetSes, INTERNET_OPTION_RECEIVE_TIMEOUT,
                        &g_ReceiveTimeout, sizeof(DWORD));
    }
  }

  DWORD ec = ERROR_SUCCESS;
  hLocalFile = CreateFile(pFile->text, GENERIC_READ | GENERIC_WRITE,
                          FILE_SHARE_READ | FILE_SHARE_DELETE,
                          NULL, OPEN_ALWAYS, 0, NULL);
  if (INVALID_HANDLE_VALUE == hLocalFile)
  {
    TRACE(_T("InetBgDl: CreateFile file handle invalid\n"));
    goto diegle;
  }
  if (GetLastError() == ERROR_ALREADY_EXISTS) {
    // Resuming a download that was started earlier and then aborted.
    previouslyWritten = GetFileSize(hLocalFile, NULL);
    g_cbCurrXF = previouslyWritten;
    SetFilePointer(hLocalFile, previouslyWritten, NULL, FILE_BEGIN);
  }

  const DWORD IOURedirFlags = INTERNET_FLAG_IGNORE_REDIRECT_TO_HTTP |
                              INTERNET_FLAG_IGNORE_REDIRECT_TO_HTTPS;
  const DWORD IOUCacheFlags = INTERNET_FLAG_RESYNCHRONIZE |
                              INTERNET_FLAG_NO_CACHE_WRITE |
                              INTERNET_FLAG_PRAGMA_NOCACHE |
                              INTERNET_FLAG_RELOAD;
  const DWORD IOUCookieFlags = INTERNET_FLAG_NO_COOKIES;
  DWORD IOUFlags = IOURedirFlags | IOUCacheFlags | IOUCookieFlags |
                   INTERNET_FLAG_NO_UI | INTERNET_FLAG_EXISTING_CONNECT;

  TCHAR *hostname = (TCHAR*) GlobalAlloc(GPTR, MAX_STRLEN * sizeof(TCHAR)),
        *urlpath = (TCHAR*) GlobalAlloc(GPTR, MAX_STRLEN * sizeof(TCHAR)),
        *extrainfo = (TCHAR*) GlobalAlloc(GPTR, MAX_STRLEN * sizeof(TCHAR));

  URL_COMPONENTS uc = { sizeof(URL_COMPONENTS), NULL, 0, (INTERNET_SCHEME)0,
                        hostname, MAX_STRLEN, (INTERNET_PORT)0, NULL, 0,
                        NULL, 0, urlpath, MAX_STRLEN, extrainfo, MAX_STRLEN};
  uc.dwHostNameLength = uc.dwUrlPathLength = uc.dwExtraInfoLength = MAX_STRLEN;

  if (!InternetCrackUrl(pURL->text, 0, ICU_ESCAPE, &uc))
  {
    // Bad url or param passed in
    TRACE(_T("InetBgDl: InternetCrackUrl false with url=%s, gle=%u\n"),
          pURL->text, GetLastError());
    goto diegle;
  }

  TRACE(_T("InetBgDl: scheme_id=%d, hostname=%s, port=%d, urlpath=%s, extrainfo=%s\n"),
        uc.nScheme, hostname, uc.nPort, urlpath, extrainfo);

  // Only http and https are supported
  if (uc.nScheme != INTERNET_SCHEME_HTTP &&
      uc.nScheme != INTERNET_SCHEME_HTTPS)
  {
    TRACE(_T("InetBgDl: only http and https is supported, aborting...\n"));
    goto diegle;
  }

  // Tell the server to pick up wherever we left off.
  TCHAR headers[32];
  // We're skipping building the C runtime to keep the file size low, so we
  // can't use a normal string initialization because that would call memset.
  headers[0] = _T('\0');
  wsprintf(headers, _T("Range: bytes=%d-\r\n"), previouslyWritten);

  TRACE(_T("InetBgDl: calling InternetOpenUrl with url=%s\n"), pURL->text);
  g_hInetFile = InternetOpenUrl(g_hInetSes, pURL->text,
                                headers, -1, IOUFlags |
                                (uc.nScheme == INTERNET_SCHEME_HTTPS ?
                                 INTERNET_FLAG_SECURE : 0), 1);
  if (!g_hInetFile)
  {
    TRACE(_T("InetBgDl: InternetOpenUrl failed with gle=%u\n"),
          GetLastError());
    goto diegle;
  }

  // Get the file length via the Content-Length header
  FILESIZE_T cbThisFile;
  cbio = sizeof(cbThisFile);
  if (!HttpQueryInfo(g_hInetFile,
                     HTTP_QUERY_CONTENT_LENGTH | HTTP_QUERY_FLAG_NUMBER,
                     &cbThisFile, &cbio, NULL))
  {
    cbThisFile = FILESIZE_UNKNOWN;
  }
  TRACE(_T("InetBgDl: file size=%d bytes\n"), cbThisFile);

  // Use a 4MiB read buffer for the connection.
  // Bigger buffers will be faster.
  // cbReadBufXF should be a multiple of g_cbBufXF.
  const UINT cbReadBufXF = 4194304;

  // Up the default internal buffer size from 4096 to internalReadBufferSize.
  DWORD internalReadBufferSize = cbReadBufXF;
  if (!InternetSetOption(g_hInetFile, INTERNET_OPTION_READ_BUFFER_SIZE,
                         &internalReadBufferSize, sizeof(DWORD)))
  {
    TRACE(_T("InetBgDl: InternetSetOption failed to set read buffer size to %u bytes, gle=%u\n"),
          internalReadBufferSize, GetLastError());

    // Maybe it's too big, try half of the optimal value.  If that fails just
    // use the default.
    internalReadBufferSize /= 2;
    if (!InternetSetOption(g_hInetFile, INTERNET_OPTION_READ_BUFFER_SIZE,
                           &internalReadBufferSize, sizeof(DWORD)))
    {
      TRACE(_T("InetBgDl: InternetSetOption failed to set read buffer size ") \
            _T("to %u bytes (using default read buffer size), gle=%u\n"),
            internalReadBufferSize, GetLastError());
    }
  }

  for(;;)
  {
    DWORD cbio = 0, cbXF = 0;
    BOOL retXF = InternetReadFile(g_hInetFile, g_bufXF, g_cbBufXF, &cbio);
    if (!retXF)
    {
      ec = GetLastError();
      TRACE(_T("InetBgDl: InternetReadFile failed, gle=%u\n"), ec);
      if (ERROR_INTERNET_CONNECTION_ABORTED == ec ||
          ERROR_INTERNET_CONNECTION_RESET == ec)
      {
        ec = ERROR_BROKEN_PIPE;
      }
      break;
    }

    if (0 == cbio)
    {
      ASSERT(ERROR_SUCCESS == ec);
      // EOF or broken connection?
      // TODO: Can InternetQueryDataAvailable detect this?

      TRACE(_T("InetBgDl: InternetReadFile true with 0 cbio, cbThisFile=%d, gle=%u\n"),
            cbThisFile, GetLastError());
      // If we haven't transferred all of the file, and we know how big the file
      // is, and we have no more data to read from the HTTP request, then set a
      // broken pipe error. Reading without StatsLock is ok in this thread.
      if (FILESIZE_UNKNOWN != cbThisFile && writtenThisSession != cbThisFile)
      {
        TRACE(_T("InetBgDl: expected Content-Length of %d bytes, ")
              _T("but transferred %d bytes\n"),
              cbThisFile, writtenThisSession);
        ec = ERROR_BROKEN_PIPE;
      }
      break;
    }

    // Check if we canceled the download
    if (0 == g_FilesTotal)
    {
      TRACE(_T("InetBgDl: 0 == g_FilesTotal, aborting transfer loop...\n"));
      ec = ERROR_CANCELLED;
      break;
    }

    cbXF = cbio;
    if (cbXF)
    {
      retXF = WriteFile(hLocalFile, g_bufXF, cbXF, &cbio, NULL);
      if (!retXF || cbXF != cbio)
      {
        ec = GetLastError();
        break;
      }

      StatsLock_AcquireExclusive();
      if (FILESIZE_UNKNOWN != cbThisFile) {
        g_cbCurrTot = cbThisFile;
      }
      writtenThisSession += cbXF;
      g_cbCurrXF += cbXF;
      StatsLock_ReleaseExclusive();
    }
  }

  TRACE(_T("InetBgDl: TaskThreadProc completed %s, ec=%u\n"), pURL->text, ec);
  InternetCloseHandle(g_hInetFile);
  g_hInetFile = nullptr;
  if (ERROR_SUCCESS == ec)
  {
    if (INVALID_HANDLE_VALUE != hLocalFile)
    {
      CloseHandle(hLocalFile);
      hLocalFile = INVALID_HANDLE_VALUE;
    }
    StackFreeItem(pURL);
    StackFreeItem(pFile);
    ++completedFile;
  }
  else if (ERROR_BROKEN_PIPE == ec)
  {
    g_Status = STATUS_ERR_CONNECTION_LOST;
    goto die;
  }
  else
  {
    TRACE(_T("InetBgDl: failed with ec=%u\n"), ec);
    SetLastError(ec);
    goto diegle;
  }
  goto startnexttask;
}

NSISPIEXPORTFUNC Get(HWND hwndNSIS, UINT N_CCH, TCHAR*N_Vars, NSIS::stack_t**ppST, NSIS::xparams_t*pX)
{
  pX->RegisterPluginCallback(g_hInst, NSISPluginCallback);
  for (;;)
  {
    NSIS::stack_t*pURL = StackPopItem(ppST);
    if (!pURL)
    {
      break;
    }

    if (lstrcmpi(pURL->text, _T("/connecttimeout")) == 0)
    {
      NSIS::stack_t*pConnectTimeout = StackPopItem(ppST);
      g_ConnectTimeout = MyTStrToL(pConnectTimeout->text) * 1000;
      continue;
    }
    else if (lstrcmpi(pURL->text, _T("/receivetimeout")) == 0)
    {
      NSIS::stack_t*pReceiveTimeout = StackPopItem(ppST);
      g_ReceiveTimeout = MyTStrToL(pReceiveTimeout->text) * 1000;
      continue;
    }
    else if (lstrcmpi(pURL->text, _T("/reset")) == 0)
    {
      StackFreeItem(pURL);
      Reset();
      continue;
    }
    else if (lstrcmpi(pURL->text, _T("/end")) == 0)
    {
freeurlandexit:
      StackFreeItem(pURL);
      break;
    }

    NSIS::stack_t*pFile = StackPopItem(ppST);
    if (!pFile)
    {
      goto freeurlandexit;
    }

    TaskLock_AcquireExclusive();

    pFile->next = NULL;
    pURL->next = pFile;
    NSIS::stack_t*pTasksTail = g_pLocations;
    while(pTasksTail && pTasksTail->next) pTasksTail = pTasksTail->next;
    if (pTasksTail)
    {
      pTasksTail->next = pURL;
    }
    else
    {
      g_pLocations = pURL;
    }

    if (!g_hThread)
    {
      DWORD tid;
      if (g_hGETStartedEvent) {
        CloseHandle(g_hGETStartedEvent);
      }
      g_hGETStartedEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
      g_hThread = CreateThread(NULL, 0, TaskThreadProc, NULL, 0, &tid);
    }

    if (!g_hThread)
    {
      goto freeurlandexit;
    }

#ifndef ONELOCKTORULETHEMALL
    StatsLock_AcquireExclusive();
#endif
    ++g_FilesTotal;
#ifndef ONELOCKTORULETHEMALL
    StatsLock_ReleaseExclusive();
#endif
    TaskLock_ReleaseExclusive();
  }
}

NSISPIEXPORTFUNC GetStats(HWND hwndNSIS, UINT N_CCH, TCHAR*N_Vars, NSIS::stack_t**ppST, NSIS::xparams_t*pX)
{
  NSISPI_INITGLOBALS(N_CCH, N_Vars);
  StatsLock_AcquireShared();
  NSIS_SetRegUINT(0, g_Status);
  NSIS_SetRegUINT(1, g_FilesCompleted);
  NSIS_SetRegUINT(2, g_FilesTotal - g_FilesCompleted);
  NSIS_SetRegUINT(3, g_cbCurrXF);
  NSIS_SetRegStrEmpty(4);
  if (FILESIZE_UNKNOWN != g_cbCurrTot)
  {
    NSIS_SetRegUINT(4, g_cbCurrTot);
  }
  NSIS_SetRegStr(5, g_ServerIP);
  StatsLock_ReleaseShared();
}

BOOL WINAPI DllMain(HINSTANCE hInst, ULONG Reason, LPVOID pCtx)
{
  if (DLL_PROCESS_ATTACH==Reason)
  {
    g_hInst=hInst;
    InitializeCriticalSection(&g_CritLock);
  }
  return TRUE;
}
