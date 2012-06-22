// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>
#include <algorithm>
#include <stdio.h>
#include <stdlib.h>
#include <string>

using namespace std;

// Don't use stderr for errors because VS has large buffers on them, leading
// to confusing error output.
static void Fatal(const wchar_t* msg) {
  wprintf(L"supalink fatal error: %s\n", msg);
  exit(1);
}

static wstring ErrorMessageToString(DWORD err) {
  wchar_t* msg_buf = NULL;
  DWORD rc = FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                               FORMAT_MESSAGE_FROM_SYSTEM,
                           NULL,
                           err,
                           MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                           reinterpret_cast<LPTSTR>(&msg_buf),
                           0,
                           NULL);
  if (!rc)
    return L"unknown error";
  wstring ret(msg_buf);
  LocalFree(msg_buf);
  return ret;
}

static const wchar_t* const g_search_for[] = {
  L"link.exe\" ",
  L"link\" ",
  L"link.exe ",
  L"link ",
};

static void Fallback(const wchar_t* msg = NULL) {
  if (msg) {
    wprintf(L"supalink failed (%s), trying to fallback to standard link.\n",
            msg);
    wprintf(L"Original command line: %s\n", GetCommandLine());
    fflush(stdout);
  }

  STARTUPINFO startup_info = { sizeof(STARTUPINFO) };
  PROCESS_INFORMATION process_info;
  DWORD exit_code;

  GetStartupInfo(&startup_info);

  wstring orig_cmd(GetCommandLine());
  wstring cmd;
  wstring replace_with = L"link.exe.supalink_orig.exe";
  wstring orig_lowercase;

  // Avoid searching for case-variations by making a lower-case copy.
  transform(orig_cmd.begin(),
            orig_cmd.end(),
            back_inserter(orig_lowercase),
            tolower);

  for (size_t i = 0; i < ARRAYSIZE(g_search_for); ++i) {
    wstring linkexe = g_search_for[i];
    wstring::size_type at = orig_lowercase.find(linkexe, 0);
    if (at == wstring::npos)
      continue;
    if (linkexe[linkexe.size() - 2] == L'"')
      replace_with += L"\" ";
    else
      replace_with += L" ";
    cmd = orig_cmd.replace(at, linkexe.size(), replace_with);
    break;
  }
  if (cmd == L"") {
    wprintf(L"Original run '%s'\n", orig_cmd.c_str());
    Fatal(L"Couldn't find link.exe (or similar) in command line");
  }

  if (getenv("SUPALINK_DEBUG")) {
    wprintf(L"  running '%s'\n", cmd.c_str());
    fflush(stdout);
  }
  if (!CreateProcess(NULL,
                     reinterpret_cast<LPWSTR>(const_cast<wchar_t *>(
                             cmd.c_str())),
                     NULL,
                     NULL,
                     TRUE,
                     0,
                     NULL,
                     NULL,
                     &startup_info, &process_info)) {
    wstring error = ErrorMessageToString(GetLastError());
    Fatal(error.c_str());
  }
  CloseHandle(process_info.hThread);
  WaitForSingleObject(process_info.hProcess, INFINITE);
  GetExitCodeProcess(process_info.hProcess, &exit_code);
  CloseHandle(process_info.hProcess);
  exit(exit_code);
}

wstring SlurpFile(const wchar_t* path) {
  FILE* f = _wfopen(path, L"rb, ccs=UNICODE");
  if (!f) Fallback(L"couldn't read file");
  fseek(f, 0, SEEK_END);
  long len = ftell(f);
  rewind(f);
  wchar_t* data = reinterpret_cast<wchar_t*>(malloc(len));
  fread(data, 1, len, f);
  fclose(f);
  wstring ret(data, len/sizeof(wchar_t));
  free(data);
  return ret;
}

void DumpFile(const wchar_t* path, wstring& contents) {
  FILE* f = _wfopen(path, L"wb, ccs=UTF-16LE");
  if (!f) Fallback(L"couldn't write file");

  fwrite(contents.c_str(), sizeof(wchar_t), contents.size(), f);
  if (ferror(f))
    Fatal(L"failed during response rewrite");
  fclose(f);
}

// Input command line is assumed to be of the form:
//
// link.exe @C:\src\...\RSP00003045884740.rsp /NOLOGO /ERRORREPORT:PROMPT
//
// Specifically, we parse & hack the contents of argv[1] and pass the rest
// onwards.
int wmain(int argc, wchar_t** argv) {
  ULONGLONG start_time = 0, end_time;

  int rsp_file_index = -1;

  if (argc < 2)
    Fallback(L"too few commmand line args");

  for (int i = 1; i < argc; ++i) {
    if (argv[i][0] == L'@') {
      rsp_file_index = i;
      break;
    }
  }

  if (rsp_file_index == -1)
    Fallback(L"couldn't find a response file in argv");

  if (_wgetenv(L"SUPALINK_DEBUG"))
    start_time = GetTickCount64();

  wstring rsp = SlurpFile(&argv[rsp_file_index][1]);

  // The first line of this file is all we try to fix. It's a bunch of
  // quoted space separated items. Simplest thing seems to be replacing " "
  // with "\n". So, just slurp the file, replace, spit it out to the same
  // file and continue on our way.

  // Took about .5s when using the naive .replace loop to replace " " with
  // "\r\n" so write the silly loop instead.
  wstring fixed;
  fixed.reserve(rsp.size() * 2);

  for (const wchar_t* s = rsp.c_str(); *s;) {
    if (*s == '"' && *(s + 1) == ' ' && *(s + 2) == '"') {
      fixed += L"\"\r\n\"";
      s += 3;
    } else {
      fixed += *s++;
    }
  }

  DumpFile(&argv[rsp_file_index][1], fixed);

  if (_wgetenv(L"SUPALINK_DEBUG")) {
    wstring backup_copy(&argv[rsp_file_index][1]);
    backup_copy += L".copy";
    DumpFile(backup_copy.c_str(), fixed);

    end_time = GetTickCount64();

    wprintf(L"  took %.2fs to modify @rsp file\n",
            (end_time - start_time) / 1000.0);
  }

  Fallback();
}
