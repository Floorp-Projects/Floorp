/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/* exported LIBC, Win, createPipe, libc */

const LIBC = OS.Constants.libc;

const Win = OS.Constants.Win;

const LIBC_CHOICES = ["kernel32.dll"];

const win32 = {
  // On Windows 64, winapi_abi is an alias for default_abi.
  WINAPI: ctypes.winapi_abi,

  BYTE: ctypes.uint8_t,
  WORD: ctypes.uint16_t,
  DWORD: ctypes.uint32_t,

  UINT: ctypes.unsigned_int,
  UCHAR: ctypes.unsigned_char,

  BOOL: ctypes.bool,

  HANDLE: ctypes.voidptr_t,
  PVOID: ctypes.voidptr_t,
  LPVOID: ctypes.voidptr_t,

  CHAR: ctypes.char,
  WCHAR: ctypes.jschar,

  ULONG_PTR: ctypes.uintptr_t,
};

Object.assign(win32, {
  LPSTR: win32.CHAR.ptr,
  LPWSTR: win32.WCHAR.ptr,

  LPBYTE: win32.BYTE.ptr,
  LPDWORD: win32.DWORD.ptr,
  LPHANDLE: win32.HANDLE.ptr,
});

Object.assign(win32, {
  LPCSTR: win32.LPSTR,
  LPCWSTR: win32.LPWSTR,
  LPCVOID: win32.LPVOID,
});

Object.assign(win32, {
  CREATE_NEW_CONSOLE: 0x00000010,
  CREATE_UNICODE_ENVIRONMENT: 0x00000400,
  CREATE_NO_WINDOW: 0x08000000,

  STARTF_USESTDHANDLES: 0x0100,

  DUPLICATE_CLOSE_SOURCE: 0x01,
  DUPLICATE_SAME_ACCESS: 0x02,

  ERROR_HANDLE_EOF: 38,
  ERROR_BROKEN_PIPE: 109,

  FILE_FLAG_OVERLAPPED: 0x40000000,

  PIPE_TYPE_BYTE: 0x00,

  PIPE_ACCESS_INBOUND: 0x01,
  PIPE_ACCESS_OUTBOUND: 0x02,
  PIPE_ACCESS_DUPLEX: 0x03,

  PIPE_WAIT: 0x00,
  PIPE_NOWAIT: 0x01,

  STILL_ACTIVE: 259,

  // These constants are 32-bit unsigned integers, but Windows defines
  // them as negative integers cast to an unsigned type.
  STD_INPUT_HANDLE: -10 + 0x100000000,
  STD_OUTPUT_HANDLE: -11 + 0x100000000,
  STD_ERROR_HANDLE: -12 + 0x100000000,

  WAIT_TIMEOUT: 0x00000102,
  WAIT_FAILED: 0xffffffff,
});

Object.assign(win32, {
  OVERLAPPED: new ctypes.StructType("OVERLAPPED", [
     {"Internal": win32.ULONG_PTR},
     {"InternalHigh": win32.ULONG_PTR},
     {"Offset": win32.DWORD},
     {"OffsetHigh": win32.DWORD},
     {"hEvent": win32.HANDLE},
  ]),

  PROCESS_INFORMATION: new ctypes.StructType("PROCESS_INFORMATION", [
    {"hProcess": win32.HANDLE},
    {"hThread": win32.HANDLE},
    {"dwProcessId": win32.DWORD},
    {"dwThreadId": win32.DWORD},
  ]),

  SECURITY_ATTRIBUTES: new ctypes.StructType("SECURITY_ATTRIBUTES", [
    {"nLength": win32.DWORD},
    {"lpSecurityDescriptor": win32.LPVOID},
    {"bInheritHandle": win32.BOOL},
  ]),

  STARTUPINFOW: new ctypes.StructType("STARTUPINFOW", [
    {"cb": win32.DWORD},
    {"lpReserved": win32.LPWSTR},
    {"lpDesktop": win32.LPWSTR},
    {"lpTitle": win32.LPWSTR},
    {"dwX": win32.DWORD},
    {"dwY": win32.DWORD},
    {"dwXSize": win32.DWORD},
    {"dwYSize": win32.DWORD},
    {"dwXCountChars": win32.DWORD},
    {"dwYCountChars": win32.DWORD},
    {"dwFillAttribute": win32.DWORD},
    {"dwFlags": win32.DWORD},
    {"wShowWindow": win32.WORD},
    {"cbReserved2": win32.WORD},
    {"lpReserved2": win32.LPBYTE},
    {"hStdInput": win32.HANDLE},
    {"hStdOutput": win32.HANDLE},
    {"hStdError": win32.HANDLE},
  ]),
});

var libc = new Library("libc", LIBC_CHOICES, {
  CloseHandle: [
    win32.WINAPI,
    win32.BOOL,
    win32.HANDLE, /* hObject */
  ],

  CreateEventW: [
    win32.WINAPI,
    win32.HANDLE,
    win32.SECURITY_ATTRIBUTES.ptr, /* opt lpEventAttributes */
    win32.BOOL, /* bManualReset */
    win32.BOOL, /* bInitialState */
    win32.LPWSTR, /* lpName */
  ],

  CreateFileW: [
    win32.WINAPI,
    win32.HANDLE,
    win32.LPWSTR, /* lpFileName */
    win32.DWORD, /* dwDesiredAccess */
    win32.DWORD, /* dwShareMode */
    win32.SECURITY_ATTRIBUTES.ptr, /* opt lpSecurityAttributes */
    win32.DWORD, /* dwCreationDisposition */
    win32.DWORD, /* dwFlagsAndAttributes */
    win32.HANDLE, /* opt hTemplateFile */
  ],

  CreateNamedPipeW: [
    win32.WINAPI,
    win32.HANDLE,
    win32.LPWSTR, /* lpName */
    win32.DWORD, /* dwOpenMode */
    win32.DWORD, /* dwPipeMode */
    win32.DWORD, /* nMaxInstances */
    win32.DWORD, /* nOutBufferSize */
    win32.DWORD, /* nInBufferSize */
    win32.DWORD, /* nDefaultTimeOut */
    win32.SECURITY_ATTRIBUTES.ptr, /* opt lpSecurityAttributes */
  ],

  CreatePipe: [
    win32.WINAPI,
    win32.BOOL,
    win32.LPHANDLE, /* out hReadPipe */
    win32.LPHANDLE, /* out hWritePipe */
    win32.SECURITY_ATTRIBUTES.ptr, /* opt lpPipeAttributes */
    win32.DWORD, /* nSize */
  ],

  CreateProcessW: [
    win32.WINAPI,
    win32.BOOL,
    win32.LPCWSTR, /* lpApplicationName */
    win32.LPWSTR, /* lpCommandLine */
    win32.SECURITY_ATTRIBUTES.ptr, /* lpProcessAttributes */
    win32.SECURITY_ATTRIBUTES.ptr, /* lpThreadAttributes */
    win32.BOOL, /* bInheritHandle */
    win32.DWORD, /* dwCreationFlags */
    win32.LPVOID, /* opt lpEnvironment */
    win32.LPCWSTR, /* opt lpCurrentDirectory */
    win32.STARTUPINFOW.ptr, /* lpStartupInfo */
    win32.PROCESS_INFORMATION.ptr, /* out lpProcessInformation */
  ],

  DuplicateHandle: [
    win32.WINAPI,
    win32.BOOL,
    win32.HANDLE, /* hSourceProcessHandle */
    win32.HANDLE, /* hSourceHandle */
    win32.HANDLE, /* hTargetProcessHandle */
    win32.LPHANDLE, /* out lpTargetHandle */
    win32.DWORD, /* dwDesiredAccess */
    win32.BOOL, /* bInheritHandle */
    win32.DWORD, /* dwOptions */
  ],

  FreeEnvironmentStringsW: [
    win32.WINAPI,
    win32.BOOL,
    win32.LPCWSTR, /* lpszEnvironmentBlock */
  ],

  GetCurrentProcess: [
    win32.WINAPI,
    win32.HANDLE,
  ],

  GetCurrentProcessId: [
    win32.WINAPI,
    win32.DWORD,
  ],

  GetEnvironmentStringsW: [
    win32.WINAPI,
    win32.LPCWSTR,
  ],

  GetExitCodeProcess: [
    win32.WINAPI,
    win32.BOOL,
    win32.HANDLE, /* hProcess */
    win32.LPDWORD, /* lpExitCode */
  ],

  GetOverlappedResult: [
    win32.WINAPI,
    win32.BOOL,
    win32.HANDLE, /* hFile */
    win32.OVERLAPPED.ptr, /* lpOverlapped */
    win32.LPDWORD, /* lpNumberOfBytesTransferred */
    win32.BOOL, /* bWait */
  ],

  GetStdHandle: [
    win32.WINAPI,
    win32.HANDLE,
    win32.DWORD, /* nStdHandle */
  ],

  ReadFile: [
    win32.WINAPI,
    win32.BOOL,
    win32.HANDLE, /* hFile */
    win32.LPVOID, /* out lpBuffer */
    win32.DWORD, /* nNumberOfBytesToRead */
    win32.LPDWORD, /* opt out lpNumberOfBytesRead */
    win32.OVERLAPPED.ptr, /* opt in/out lpOverlapped */
  ],

  TerminateProcess: [
    win32.WINAPI,
    win32.BOOL,
    win32.HANDLE, /* hProcess */
    win32.UINT, /* uExitCode */
  ],

  WaitForMultipleObjects: [
    win32.WINAPI,
    win32.DWORD,
    win32.DWORD, /* nCount */
    win32.HANDLE.ptr, /* hHandles */
    win32.BOOL, /* bWaitAll */
    win32.DWORD, /* dwMilliseconds */
  ],

  WaitForSingleObject: [
    win32.WINAPI,
    win32.DWORD,
    win32.HANDLE, /* hHandle */
    win32.BOOL, /* bWaitAll */
    win32.DWORD, /* dwMilliseconds */
  ],

  WriteFile: [
    win32.WINAPI,
    win32.BOOL,
    win32.HANDLE, /* hFile */
    win32.LPCVOID, /* lpBuffer */
    win32.DWORD, /* nNumberOfBytesToRead */
    win32.LPDWORD, /* opt out lpNumberOfBytesWritten */
    win32.OVERLAPPED.ptr, /* opt in/out lpOverlapped */
  ],
});


let nextNamedPipeId = 0;

win32.Handle = function(handle) {
  return ctypes.CDataFinalizer(win32.HANDLE(handle), libc.CloseHandle);
};

win32.createPipe = function(secAttr, readFlags = 0, writeFlags = 0, size = 0) {
  readFlags |= win32.PIPE_ACCESS_INBOUND;
  writeFlags |= Win.FILE_ATTRIBUTE_NORMAL;

  if (size == 0) {
    size = 4096;
  }

  let pid = libc.GetCurrentProcessId();
  let pipeName = String.raw`\\.\Pipe\SubProcessPipe.${pid}.${nextNamedPipeId++}`;

  let readHandle = libc.CreateNamedPipeW(
    pipeName, readFlags,
    win32.PIPE_TYPE_BYTE | win32.PIPE_WAIT,
    1, /* number of connections */
    size, /* output buffer size */
    size,  /* input buffer size */
    0, /* timeout */
    secAttr.address());

  let isInvalid = handle => String(handle) == String(win32.HANDLE(Win.INVALID_HANDLE_VALUE));

  if (isInvalid(readHandle)) {
    return [];
  }

  let writeHandle = libc.CreateFileW(
    pipeName, Win.GENERIC_WRITE, 0, secAttr.address(),
    Win.OPEN_EXISTING, writeFlags, null);

  if (isInvalid(writeHandle)) {
    libc.CloseHandle(readHandle);
    return [];
  }

  return [win32.Handle(readHandle),
          win32.Handle(writeHandle)];
};
