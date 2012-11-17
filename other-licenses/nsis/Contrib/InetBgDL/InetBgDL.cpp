//
// Copyright (C) Anders Kjersem. Licensed under the zlib/libpng license
//

#include "InetBgDL.h"

#define USERAGENT _T("NSIS InetBgDL (Mozilla)")

#define STATUS_COMPLETEDALL 0
#define STATUS_INITIAL 202
#define STATUS_CONNECTING STATUS_INITIAL //102
#define STATUS_DOWNLOADING STATUS_INITIAL
#define STATUS_ERR_GETLASTERROR 418 //HTTP: I'm a teapot: Win32 error code in $3
#define STATUS_ERR_LOCALFILEWRITEERROR 450 //HTTP: MS parental control extension
#define STATUS_ERR_CANCELLED 499

typedef DWORD FILESIZE_T; // Limit to 4GB for now...
#define FILESIZE_UNKNOWN (-1)

HINSTANCE g_hInst;
NSIS::stack_t*g_pLocations = NULL;
HANDLE g_hThread = NULL;
HANDLE g_hGETStartedEvent = NULL;
volatile UINT g_FilesTotal = 0;
volatile UINT g_FilesCompleted = 0;
volatile UINT g_Status = STATUS_INITIAL;
volatile FILESIZE_T g_cbCurrXF;
volatile FILESIZE_T g_cbCurrTot = FILESIZE_UNKNOWN;
CRITICAL_SECTION g_CritLock;
UINT g_N_CCH;
PTSTR g_N_Vars;

DWORD g_ConnectTimeout = 0;
DWORD g_ReceiveTimeout = 0;
bool g_WantRangeRequest = false;

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
    if (WAIT_OBJECT_0 != WaitForSingleObject(g_hThread, 10 * 1000))
    {
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

/* ParseURL is a quickly thrown together simple way to parse a URL into parts.
 * It is only designed to support simple URLs and doesn't support things like
 * authorization information in the URL.
 * The format of the URL is assumed to be:
 * <protocol>://<server>:<port>/<path>
 *
 * @param url      The input URL to parse
 * @param protocol Out variable which will store the protocol of the passed url
 * @param server Out variable which will store the server of the passed url
 * @param port Out variable which will store the port of the passed url
 * @param path Out variable which will store the path of the passed url
 * @return true if successful
*/
template<size_t A, size_t B, size_t C> static bool
BasicParseURL(LPCWSTR url, wchar_t (*protocol)[A], INTERNET_PORT *port,
              wchar_t (*server)[B], wchar_t (*path)[C])
{
  ZeroMemory(*protocol, A * sizeof(wchar_t));
  ZeroMemory(*server, B * sizeof(wchar_t));
  ZeroMemory(*path, C * sizeof(wchar_t));

  const WCHAR *p = url;
  // Skip past the protocol
  int pos = 0;
  while (*p != L'\0' && *p != L'/' && *p != L':')
  {
    if (pos + 1 >= A)
      return false;
    (*protocol)[pos++] = *p++;
  }

  // Skip past the ://
  p += 3;

  *port = INTERNET_DEFAULT_HTTP_PORT;
  if (!wcsicmp(*protocol, L"https"))
  {
    *port = INTERNET_DEFAULT_HTTPS_PORT;
  }

  // Get the server
  pos = 0;
  while (*p != L'\0' && *p != L'/' && *p != L':')
  {
    if (pos + 1 >= B)
      return false;
    (*server)[pos++] = *p++;
  }

  // Get the port if specified
  if (*p == L':')
  {
    WCHAR portStr[16];
    p++;
    pos = 0;
    while (*p != L'\0' && *p != L'/')
    {
      if (pos + 1 >= 16)
        return false;
      portStr[pos++] = *p++;
    }
    portStr[pos] = '\0';
    *port = (INTERNET_PORT)_wtoi(portStr);
  }
  else
  {
    // Skip the slash after the server
    while (*p != L'\0' && *p != L'/')
    {
      p++;
    }
  }

  // Get the rest as the path
  pos = 0;
  while (*p != L'\0')
  {
    if (pos + 1 >= C)
      return false;
    (*path)[pos++] = *p++;
  }

  return true;
}

DWORD CALLBACK TaskThreadProc(LPVOID ThreadParam)
{
  NSIS::stack_t *pURL,*pFile;
  HINTERNET hInetSes = NULL, hInetCon = NULL;
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
    if (hInetSes)
    {
      InternetCloseHandle(hInetSes);
    }
    if (hInetCon)
    {
      InternetCloseHandle(hInetCon);
    }
    if (INVALID_HANDLE_VALUE != hLocalFile)
    {
      CloseHandle(hLocalFile);
    }
    StackFreeItem(pURL);
    StackFreeItem(pFile);
    return 0;
  }

  if (!hInetSes)
  {
    hInetSes = InternetOpen(USERAGENT, INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (!hInetSes)
    {
      goto diegle;
    }

    //msdn.microsoft.com/library/default.asp?url=/workshop/components/offline/offline.asp#Supporting Offline Browsing in Applications and Components
    ULONG longOpt;
    DWORD cbio = sizeof(ULONG);
    if (InternetQueryOption(hInetSes, INTERNET_OPTION_CONNECTED_STATE, &longOpt, &cbio))
    {
      if (INTERNET_STATE_DISCONNECTED_BY_USER&longOpt)
      {
        INTERNET_CONNECTED_INFO ci = {INTERNET_STATE_CONNECTED, 0};
        InternetSetOption(hInetSes, INTERNET_OPTION_CONNECTED_STATE, &ci, sizeof(ci));
      }
    }

    if(g_ConnectTimeout > 0)
    {
      InternetSetOption(hInetSes, INTERNET_OPTION_CONNECT_TIMEOUT,
                        &g_ConnectTimeout, sizeof(g_ConnectTimeout));
    }
  }

  DWORD ec = ERROR_SUCCESS;
  hLocalFile = CreateFile(pFile->text,GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_DELETE,NULL,CREATE_ALWAYS,0,NULL);
  if (INVALID_HANDLE_VALUE == hLocalFile)
  {
    goto diegle;
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

  WCHAR protocol[16];
  WCHAR server[128];
  WCHAR path[1024];
  INTERNET_PORT port;
  // Good old VC6 cannot deduce the size of these params :'(
  if (!BasicParseURL<16, 128, 1024>(pURL->text, &protocol, &port, &server, &path))
  {
    // Insufficient buffer or bad URL passed in
    goto diegle;
  }

  DWORD context;
  hInetCon = InternetConnect(hInetSes, server, port, NULL, NULL,
                             INTERNET_SERVICE_HTTP, IOUFlags,
                             (unsigned long)&context);
  if (!hInetCon)
  {
    goto diegle;
  }

  // Setup a buffer of size 256KiB to store the downloaded data.
  // Get at most 4MiB at a time from the partial HTTP Range requests.
  // Biffer buffers will be faster.
  // cbRangeReadBufXF should be a multiple of cbBufXF.
  const UINT cbBufXF = 262144;
  const UINT cbRangeReadBufXF = 4194304;
  BYTE bufXF[cbBufXF];

  // Up the default internal buffer size from 4096 to internalReadBufferSize.
  DWORD internalReadBufferSize = cbRangeReadBufXF;
  if (!InternetSetOption(hInetCon, INTERNET_OPTION_READ_BUFFER_SIZE,
                         &internalReadBufferSize, sizeof(DWORD)))
  {
    // Maybe it's too big, try half of the optimal value.  If that fails just
    // use the default.
    internalReadBufferSize /= 2;
    InternetSetOption(hInetCon, INTERNET_OPTION_READ_BUFFER_SIZE,
                      &internalReadBufferSize, sizeof(DWORD));
  }

  // Change the default timeout of 30 seconds to the specified value.
  // Is case a proxy in between caches the results, it could in theory
  // take longer to get the first chunk, so it is good to set this high.
  if (g_ReceiveTimeout)
  {
    InternetSetOption(hInetCon, INTERNET_OPTION_DATA_RECEIVE_TIMEOUT,
                      &g_ReceiveTimeout, sizeof(DWORD));
  }

  HINTERNET hInetFile;
  DWORD cbio = sizeof(DWORD);

  // Keep looping until we don't have a redirect anymore
  int redirectCount = 0;
  for (;;) {
    // Make sure we aren't stuck in some kind of infinite redirect loop.
    if (redirectCount > 15) {
      goto diegle;
    }

    // If a range request was specified, first do a HEAD request
    hInetFile = HttpOpenRequest(hInetCon, g_WantRangeRequest ? L"HEAD" : L"GET",
                                path, NULL, NULL, NULL,
                                INTERNET_FLAG_NO_AUTO_REDIRECT |
                                INTERNET_FLAG_PRAGMA_NOCACHE |
                                INTERNET_FLAG_RELOAD, 0);
    if (!hInetFile)
    {
      goto diegle;
    }

    if (!HttpSendRequest(hInetFile, NULL, 0, NULL, 0))
    {
      goto diegle;
    }

    WCHAR responseText[256];
    cbio = sizeof(responseText);
    if (!HttpQueryInfo(hInetFile,
                       HTTP_QUERY_STATUS_CODE,
                       responseText, &cbio, NULL))
    {
      goto diegle;
    }

    int statusCode = _wtoi(responseText);
    if (statusCode == HTTP_STATUS_REDIRECT ||
        statusCode == HTTP_STATUS_MOVED) {
      redirectCount++;
      WCHAR URLBuffer[2048];
      cbio = sizeof(URLBuffer);
      if (!HttpQueryInfo(hInetFile,
                         HTTP_QUERY_LOCATION,
                         (DWORD*)URLBuffer, &cbio, NULL))
      {
        goto diegle;
      }

      WCHAR protocol2[16];
      WCHAR server2[128];
      WCHAR path2[1024];
      INTERNET_PORT port2;
      BasicParseURL<16, 128, 1024>(URLBuffer, &protocol2, &port2, &server2, &path2);
      // Check if we need to reconnect to a new server
      if (wcscmp(protocol, protocol2) || wcscmp(server, server2) ||
          port != port2) {
        wcscpy(server, server2);
        port = port2;
        InternetCloseHandle(hInetCon);
        hInetCon = InternetConnect(hInetSes, server, port, NULL, NULL,
                                   INTERNET_SERVICE_HTTP, IOUFlags,
                                   (unsigned long)&context);
        if (!hInetCon)
        {
          goto diegle;
        }
      }
      wcscpy(path, path2);

      // Close the existing handle because we'll be issuing a new request
      // with the new request path.
      InternetCloseHandle(hInetFile);
      continue;
    }
    break;
  }

  // Get the file length via the Content-Length header
  FILESIZE_T cbThisFile;
  cbio = sizeof(cbThisFile);
  if (!HttpQueryInfo(hInetFile,
                     HTTP_QUERY_CONTENT_LENGTH | HTTP_QUERY_FLAG_NUMBER,
                     &cbThisFile, &cbio, NULL))
  {
    cbThisFile = FILESIZE_UNKNOWN;
  }

  // Determine if we should use byte range requests. We want to use it if:
  // 1. Server accepts byte range requests
  // 2. The size of the file is known and more than our Range buffer size.
  bool shouldUseRangeRequest = true;
  WCHAR rangeRequestAccepted[64] = { '\0' };
  cbio = sizeof(rangeRequestAccepted);
  if (cbThisFile != FILESIZE_UNKNOWN && cbThisFile <= cbRangeReadBufXF ||
      !HttpQueryInfo(hInetFile,
                     HTTP_QUERY_ACCEPT_RANGES,
                     (LPDWORD)rangeRequestAccepted, &cbio, NULL))
  {
    shouldUseRangeRequest = false;
  }
  else
  {
    shouldUseRangeRequest = wcsstr(rangeRequestAccepted, L"bytes") != 0 &&
                            cbThisFile != FILESIZE_UNKNOWN;
  }

  // If the server doesn't have range request support or doesn't have a
  // Content-Length header, then get everything all at once.
  // If the user didn't want a range request, then we already issued the GET
  // request earlier.  If the user did want a range request, then we issued only
  // a HEAD so far.
  if (g_WantRangeRequest && !shouldUseRangeRequest)
  {
    InternetCloseHandle(hInetFile);
    InternetCloseHandle(hInetCon);
    hInetFile = InternetOpenUrl(hInetSes, pURL->text,
                                NULL, 0, IOUFlags, NULL);
    if (!hInetFile)
    {
      goto diegle;
    }

    // For some reason this also needs to be set on the hInetFile and
    // not just the connection.
    if (g_ReceiveTimeout > 0)
    {
      InternetSetOption(hInetCon, INTERNET_OPTION_DATA_RECEIVE_TIMEOUT,
                        &g_ReceiveTimeout, sizeof(DWORD));
    }
  }

  for(;;)
  {
    DWORD cbio = 0,cbXF = 0;
    // If we know the file size, download it in chunks
    if (g_WantRangeRequest && shouldUseRangeRequest && cbThisFile != g_cbCurrXF)
    {
      // Close the previous request, but not the connection
      InternetCloseHandle(hInetFile);
      hInetFile = HttpOpenRequest(hInetCon, L"GET", path,
                                  NULL, NULL, NULL,
                                  INTERNET_FLAG_PRAGMA_NOCACHE |
                                  INTERNET_FLAG_RELOAD, 0);
      if (!hInetFile)
      {
        // TODO: we could add retry here to be more tolerant
        goto diegle;
      }

      // For some reason this also needs to be set on the hInetFile and
      // not just the connection.
      if (g_ReceiveTimeout > 0)
      {
        InternetSetOption(hInetCon, INTERNET_OPTION_DATA_RECEIVE_TIMEOUT,
                          &g_ReceiveTimeout, sizeof(DWORD));
      }

      WCHAR range[32];
      swprintf(range, L"Range: bytes=%d-%d", g_cbCurrXF,
               min(g_cbCurrXF + cbRangeReadBufXF, cbThisFile));
      if (!HttpSendRequest(hInetFile, range, wcslen(range), NULL, 0))
      {
        // TODO: we could add retry here to be more tolerant
        goto diegle;
      }
    }

    // Read the chunk (or full file if we don't know the size) we downloaded
    BOOL retXF;
    for (;;)
    {
      DWORD cbioThisIteration = 0;
      retXF = InternetReadFile(hInetFile, bufXF, cbBufXF, &cbioThisIteration);
      if (!retXF)
      {
        ec = GetLastError();
        TRACE1(_T("InternetReadFile failed, gle=%u\n"), ec);
        // TODO: we could add retry here to be more tolerant
        goto diegle;
      }

      // Check if we're done reading
      if (cbioThisIteration == 0)
      {
        break;
      }

      // Write what we found
      cbXF = cbioThisIteration;
      retXF = WriteFile(hLocalFile, bufXF, cbXF, &cbioThisIteration, NULL);
      if (!retXF || cbXF != cbioThisIteration)
      {
        cbio += cbioThisIteration;
        ec = GetLastError();
        break;
      }

      cbio += cbioThisIteration;
      StatsLock_AcquireExclusive();
      if (FILESIZE_UNKNOWN != cbThisFile)
      {
        g_cbCurrTot = cbThisFile;
      }
      g_cbCurrXF += cbXF;
      StatsLock_ReleaseExclusive();

      // Avoid an extra call to InternetReadFile if we already read everything
      // in the current request
      if (cbio == cbRangeReadBufXF && shouldUseRangeRequest)
      {
        break;
      }
    }

    // Check if we're done transferring the file successfully
    if (0 == cbio &&
        (cbThisFile == FILESIZE_UNKNOWN || cbThisFile == g_cbCurrXF))
    {
      ASSERT(ERROR_SUCCESS == ec);
      TRACE2(_T("InternetReadFile true with 0 cbio, cbThisFile=%d gle=%u\n"), cbThisFile, GetLastError());
      break;
    }

    // Check if we canceled the download
    if (0 == g_FilesTotal)
    {
      TRACEA("0==g_FilesTotal, aborting transfer loop...\n");
      ec = ERROR_CANCELLED;
      break;
    }
  }

  TRACE2(_T("TaskThreadProc completed %s, ec=%u\n"), pURL->text, ec);
  InternetCloseHandle(hInetFile);
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
  else
  {
    SetLastError(ec);
    goto diegle;
  }
  goto startnexttask;
}

NSISPIEXPORTFUNC Get(HWND hwndNSIS, UINT N_CCH, TCHAR*N_Vars, NSIS::stack_t**ppST, NSIS::xparams_t*pX)
{
  pX->RegisterPluginCallback(g_hInst, NSISPluginCallback);
  g_WantRangeRequest = false;
  for (;;)
  {
    NSIS::stack_t*pURL = StackPopItem(ppST);
    if (!pURL)
    {
      break;
    }

    if (lstrcmpi(pURL->text, _T("/rangerequest")) == 0)
    {
      g_WantRangeRequest = true;
      continue;
    }
    else if (lstrcmpi(pURL->text, _T("/connecttimeout")) == 0)
    {
      NSIS::stack_t*pConnectTimeout = StackPopItem(ppST);
      g_ConnectTimeout = _tcstol(pConnectTimeout->text, NULL, 10) * 1000;
      continue;
    }
    else if (lstrcmpi(pURL->text, _T("/receivetimeout")) == 0)
    {
      NSIS::stack_t*pReceiveTimeout = StackPopItem(ppST);
      g_ReceiveTimeout = _tcstol(pReceiveTimeout->text, NULL, 10) * 1000;
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
  StatsLock_ReleaseShared();
}

EXTERN_C BOOL WINAPI _DllMainCRTStartup(HMODULE hInst, UINT Reason, LPVOID pCtx)
{
  if (DLL_PROCESS_ATTACH==Reason)
  {
    g_hInst=hInst;
    InitializeCriticalSection(&g_CritLock);
  }
  return TRUE;
}

BOOL WINAPI DllMain(HINSTANCE hInst, ULONG Reason, LPVOID pCtx)
{
  return _DllMainCRTStartup(hInst, Reason, pCtx);
}

// For some reason VC6++ doesn't like wcsicmp and swprintf.
// If you use them, you get a linking error about _main
// as an unresolved external.
int main(int argc, char**argv)
{
  return 0;
}
