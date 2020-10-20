/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// To explain some of the oddities:
// This plugin avoids linking against a runtime that might not be present, thus
// it avoids standard library functions.
// NSIS requires GlobalAlloc/GlobalFree for its interfaces, and I use them for
// other allocations (vs e.g. HeapAlloc) for the sake of consistency.

#include <Windows.h>
#include <Wininet.h>

#define AGENT_NAME L"HttpPostFile plugin"

PBYTE LoadFileData(LPWSTR fileName, DWORD& cbData);
bool HttpPost(LPURL_COMPONENTS pUrl, LPWSTR contentTypeHeader, PBYTE data,
              DWORD cbData);

// NSIS API
typedef struct _stack_t {
  struct _stack_t* next;
  WCHAR text[1];
} stack_t;

// Unlink and return the topmost element of the stack, if any.
static stack_t* popstack(stack_t** stacktop) {
  if (!stacktop || !*stacktop) return nullptr;
  stack_t* element = *stacktop;
  *stacktop = element->next;
  element->next = nullptr;
  return element;
}

// Allocate a new stack element (with space for `stringsize`), copy the string,
// add to the top of the stack.
static void pushstring(LPCWSTR str, stack_t** stacktop,
                       unsigned int stringsize) {
  stack_t* element;
  if (!stacktop) return;

  // The allocation here has space for stringsize+1 WCHARs, because stack_t.text
  // is 1 element long. This is consistent with the NSIS ExDLL example, though
  // inconsistent with the comment that says the array "should be the length of
  // g_stringsize when allocating". I'm sticking to consistency with
  // the code, and erring towards having a larger buffer than necessary.

  element = (stack_t*)GlobalAlloc(
      GPTR, (sizeof(stack_t) + stringsize * sizeof(*str)));
  lstrcpynW(element->text, str, stringsize);
  element->next = *stacktop;
  *stacktop = element;
}

BOOL APIENTRY DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
  // No initialization or cleanup is needed.
  return TRUE;
}

extern "C" {

// HttpPostFile::Post <File> <Content-Type header with \r\n> <URL>
//
// e.g. HttpPostFile "C:\blah.json" "Content-Type: application/json$\r$\n"
// "https://example.com"
//
// Leaves a result string on the stack, "success" if the POST was successful, an
// error message otherwise.
// The status code from the server is not checked, as long as we got some
// response the result will be "success". The response is read, but discarded.
void __declspec(dllexport)
    Post(HWND hwndParent, int string_size, char* /* variables */,
         stack_t** stacktop, void* /* extra_parameters */) {
  static const URL_COMPONENTS kZeroComponents = {0};
  const WCHAR* errorMsg = L"error";

  DWORD cbData = INVALID_FILE_SIZE;
  PBYTE data = nullptr;

  // Copy a constant, because initializing an automatic variable with {0} ends
  // up linking to memset, which isn't available.
  URL_COMPONENTS components = kZeroComponents;

  // Get args, taking ownership of the strings from the stack, to avoid
  // allocating and copying strings.
  stack_t* postFileName = popstack(stacktop);
  stack_t* contentTypeHeader = popstack(stacktop);
  stack_t* url = popstack(stacktop);

  if (!postFileName || !contentTypeHeader || !url) {
    errorMsg = L"error getting arguments";
    goto finish;
  }

  data = LoadFileData(postFileName->text, cbData);
  if (!data || cbData == INVALID_FILE_SIZE) {
    errorMsg = L"error reading file";
    goto finish;
  }

  {
    // This length is used to allocate for the host name and path components,
    // which should be no longer than the source URL.
    int urlBufLen = lstrlenW(url->text) + 1;

    components.dwStructSize = sizeof(components);
    components.dwHostNameLength = urlBufLen;
    components.dwUrlPathLength = urlBufLen;
    components.lpszHostName =
        (LPWSTR)GlobalAlloc(GPTR, urlBufLen * sizeof(WCHAR));
    components.lpszUrlPath =
        (LPWSTR)GlobalAlloc(GPTR, urlBufLen * sizeof(WCHAR));
  }

  errorMsg = L"error parsing URL";
  if (components.lpszHostName && components.lpszUrlPath &&
      InternetCrackUrl(url->text, 0, 0, &components) &&
      (components.nScheme == INTERNET_SCHEME_HTTP ||
       components.nScheme == INTERNET_SCHEME_HTTPS)) {
    errorMsg = L"error sending HTTP request";
    if (HttpPost(&components, contentTypeHeader->text, data, cbData)) {
      // success!
      errorMsg = nullptr;
    }
  }

finish:
  if (components.lpszUrlPath) {
    GlobalFree(components.lpszUrlPath);
  }
  if (components.lpszHostName) {
    GlobalFree(components.lpszHostName);
  }
  if (data) {
    GlobalFree(data);
  }

  // Free args taken from the NSIS stack
  if (url) {
    GlobalFree(url);
  }
  if (contentTypeHeader) {
    GlobalFree(contentTypeHeader);
  }
  if (postFileName) {
    GlobalFree(postFileName);
  }

  if (errorMsg) {
    pushstring(errorMsg, stacktop, string_size);
  } else {
    pushstring(L"success", stacktop, string_size);
  }
}
}

// Returns buffer with file contents on success, placing the size in cbData.
// Returns nullptr on failure.
// Caller must use GlobalFree() on the returned buffer if non-null.
PBYTE LoadFileData(LPWSTR fileName, DWORD& cbData) {
  bool success = false;

  HANDLE hPostFile = INVALID_HANDLE_VALUE;

  PBYTE data = nullptr;

  DWORD bytesRead;
  DWORD bytesReadTotal;

  hPostFile = CreateFile(fileName, GENERIC_READ, FILE_SHARE_READ, nullptr,
                         OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (hPostFile == INVALID_HANDLE_VALUE) {
    goto finish;
  }

  cbData = GetFileSize(hPostFile, NULL);
  if (cbData == INVALID_FILE_SIZE) {
    goto finish;
  }

  data = (PBYTE)GlobalAlloc(GPTR, cbData);
  if (!data) {
    goto finish;
  }

  bytesReadTotal = 0;
  do {
    if (!ReadFile(hPostFile, data + bytesReadTotal, cbData - bytesReadTotal,
                  &bytesRead, nullptr /* overlapped */)) {
      goto finish;
    }
    bytesReadTotal += bytesRead;
  } while (bytesReadTotal < cbData && bytesRead > 0);

  if (bytesReadTotal == cbData) {
    success = true;
  }

finish:
  if (!success) {
    if (data) {
      GlobalFree(data);
      data = nullptr;
    }
    cbData = INVALID_FILE_SIZE;
  }
  if (hPostFile != INVALID_HANDLE_VALUE) {
    CloseHandle(hPostFile);
    hPostFile = INVALID_HANDLE_VALUE;
  }

  return data;
}

// Returns true on success
bool HttpPost(LPURL_COMPONENTS pUrl, LPWSTR contentTypeHeader, PBYTE data,
              DWORD cbData) {
  bool success = false;

  HINTERNET hInternet = nullptr;
  HINTERNET hConnect = nullptr;
  HINTERNET hRequest = nullptr;

  hInternet = InternetOpen(AGENT_NAME, INTERNET_OPEN_TYPE_PRECONFIG,
                           nullptr,  // proxy
                           nullptr,  // proxy bypass
                           0         // flags
  );
  if (!hInternet) {
    goto finish;
  }

  hConnect = InternetConnect(hInternet, pUrl->lpszHostName, pUrl->nPort,
                             nullptr,  // userName,
                             nullptr,  // password
                             INTERNET_SERVICE_HTTP,
                             0,  // flags
                             0   // context
  );
  if (!hConnect) {
    goto finish;
  }

  {
    // NOTE: Some of these settings are perhaps unnecessary for a POST.
    DWORD httpFlags = INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_NO_COOKIES |
                      INTERNET_FLAG_NO_UI | INTERNET_FLAG_RELOAD;
    if (pUrl->nScheme == INTERNET_SCHEME_HTTPS) {
      // NOTE: nsJSON sets flags to allow redirecting HTTPS to HTTP, or HTTP to
      // HTTPS I left those out because it seemed undesirable for our use case.
      httpFlags |= INTERNET_FLAG_SECURE;
    }
    hRequest = HttpOpenRequest(hConnect, L"POST", pUrl->lpszUrlPath,
                               nullptr,  // version,
                               nullptr,  // referrer
                               nullptr,  // accept types
                               httpFlags,
                               0  // context
    );
    if (!hRequest) {
      goto finish;
    }
  }

  if (contentTypeHeader) {
    if (!HttpAddRequestHeaders(hRequest, contentTypeHeader,
                               -1L,  // headers length (count string length)
                               HTTP_ADDREQ_FLAG_ADD)) {
      goto finish;
    }
  }

  if (!HttpSendRequestW(hRequest,
                        nullptr,  // additional headers
                        0,        // headers length
                        data, cbData)) {
    goto finish;
  }

  BYTE readBuffer[1024];
  DWORD bytesRead;
  do {
    if (!InternetReadFile(hRequest, readBuffer, sizeof(readBuffer),
                          &bytesRead)) {
      goto finish;
    }
    // read data is thrown away
  } while (bytesRead > 0);

  success = true;

finish:
  if (hRequest) {
    InternetCloseHandle(hRequest);
  }
  if (hConnect) {
    InternetCloseHandle(hConnect);
  }
  if (hInternet) {
    InternetCloseHandle(hInternet);
  }

  return success;
}