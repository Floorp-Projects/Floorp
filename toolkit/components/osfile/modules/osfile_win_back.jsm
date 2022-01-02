/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This file can be used in the following contexts:
 *
 *  1. included from a non-osfile worker thread using importScript
 *   (it serves to define a synchronous API for that worker thread)
 *   (bug 707681)
 *
 *  2. included from the main thread using Components.utils.import
 *   (it serves to define the asynchronous API, whose implementation
 *    resides in the worker thread)
 *   (bug 729057)
 *
 * 3. included from the osfile worker thread using importScript
 *   (it serves to define the implementation of the asynchronous API)
 *   (bug 729057)
 */

/* eslint-env mozilla/chrome-worker, node */

// eslint-disable-next-line no-lone-blocks
{
  if (typeof Components != "undefined") {
    // We do not wish osfile_win.jsm to be used directly as a main thread
    // module yet. When time comes, it will be loaded by a combination of
    // a main thread front-end/worker thread implementation that makes sure
    // that we are not executing synchronous IO code in the main thread.

    throw new Error("osfile_win.jsm cannot be used from the main thread yet");
  }

  (function(exports) {
    "use strict";
    if (exports.OS && exports.OS.Win && exports.OS.Win.File) {
      return; // Avoid double initialization
    }

    let SharedAll = require("resource://gre/modules/osfile/osfile_shared_allthreads.jsm");
    let SysAll = require("resource://gre/modules/osfile/osfile_win_allthreads.jsm");
    SharedAll.LOG.bind(SharedAll, "Unix", "back");
    let libc = SysAll.libc;
    let advapi32 = new SharedAll.Library("advapi32", "advapi32.dll");
    let Const = SharedAll.Constants.Win;

    /**
     * Initialize the Windows module.
     *
     * @param {function=} declareFFI
     */
    // FIXME: Both |init| and |aDeclareFFI| are deprecated, we should remove them
    let init = function init(aDeclareFFI) {
      let declareFFI;
      if (aDeclareFFI) {
        declareFFI = aDeclareFFI.bind(null, libc);
      } else {
        declareFFI = SysAll.declareFFI; // eslint-disable-line no-unused-vars
      }
      let declareLazyFFI = SharedAll.declareLazyFFI; // eslint-disable-line no-unused-vars

      // Initialize types that require additional OS-specific
      // support - either finalization or matching against
      // OS-specific constants.
      let Type = Object.create(SysAll.Type);
      let SysFile = (exports.OS.Win.File = { Type });

      // Initialize types

      /**
       * A C integer holding INVALID_HANDLE_VALUE in case of error or
       * a file descriptor in case of success.
       */
      Type.HANDLE = Type.voidptr_t.withName("HANDLE");
      Type.HANDLE.importFromC = function importFromC(maybe) {
        if (Type.int.cast(maybe).value == INVALID_HANDLE) {
          // Ensure that API clients can effectively compare against
          // Const.INVALID_HANDLE_VALUE. Without this cast,
          // == would always return |false|.
          return INVALID_HANDLE;
        }
        return ctypes.CDataFinalizer(maybe, this.finalizeHANDLE);
      };
      Type.HANDLE.finalizeHANDLE = function placeholder() {
        throw new Error("finalizeHANDLE should be implemented");
      };
      let INVALID_HANDLE = Const.INVALID_HANDLE_VALUE;

      Type.file_HANDLE = Type.HANDLE.withName("file HANDLE");
      SharedAll.defineLazyGetter(
        Type.file_HANDLE,
        "finalizeHANDLE",
        function() {
          return SysFile._CloseHandle;
        }
      );

      Type.find_HANDLE = Type.HANDLE.withName("find HANDLE");
      SharedAll.defineLazyGetter(
        Type.find_HANDLE,
        "finalizeHANDLE",
        function() {
          return SysFile._FindClose;
        }
      );

      Type.DWORD = Type.uint32_t.withName("DWORD");

      /* A special type used to represent flags passed as DWORDs to a function.
       * In JavaScript, bitwise manipulation of numbers, such as or-ing flags,
       * can produce negative numbers. Since DWORD is unsigned, these negative
       * numbers simply cannot be converted to DWORD. For this reason, whenever
       * bit manipulation is called for, you should rather use DWORD_FLAGS,
       * which is represented as a signed integer, hence has the correct
       * semantics.
       */
      Type.DWORD_FLAGS = Type.int32_t.withName("DWORD_FLAGS");

      /**
       * A C integer holding 0 in case of error or a positive integer
       * in case of success.
       */
      Type.zero_or_DWORD = Type.DWORD.withName("zero_or_DWORD");

      /**
       * A C integer holding 0 in case of error, any other value in
       * case of success.
       */
      Type.zero_or_nothing = Type.int.withName("zero_or_nothing");

      /**
       * A C integer holding flags related to NTFS security.
       */
      Type.SECURITY_ATTRIBUTES = Type.void_t.withName("SECURITY_ATTRIBUTES");

      /**
       * A C integer holding pointers related to NTFS security.
       */
      Type.PSID = Type.voidptr_t.withName("PSID");

      Type.PACL = Type.voidptr_t.withName("PACL");

      Type.PSECURITY_DESCRIPTOR = Type.voidptr_t.withName(
        "PSECURITY_DESCRIPTOR"
      );

      /**
       * A C integer holding Win32 local memory handle.
       */
      Type.HLOCAL = Type.voidptr_t.withName("HLOCAL");

      Type.FILETIME = new SharedAll.Type(
        "FILETIME",
        ctypes.StructType("FILETIME", [
          { lo: Type.DWORD.implementation },
          { hi: Type.DWORD.implementation },
        ])
      );

      Type.FindData = new SharedAll.Type(
        "FIND_DATA",
        ctypes.StructType("FIND_DATA", [
          { dwFileAttributes: ctypes.uint32_t },
          { ftCreationTime: Type.FILETIME.implementation },
          { ftLastAccessTime: Type.FILETIME.implementation },
          { ftLastWriteTime: Type.FILETIME.implementation },
          { nFileSizeHigh: Type.DWORD.implementation },
          { nFileSizeLow: Type.DWORD.implementation },
          { dwReserved0: Type.DWORD.implementation },
          { dwReserved1: Type.DWORD.implementation },
          { cFileName: ctypes.ArrayType(ctypes.char16_t, Const.MAX_PATH) },
          { cAlternateFileName: ctypes.ArrayType(ctypes.char16_t, 14) },
        ])
      );

      Type.FILE_INFORMATION = new SharedAll.Type(
        "FILE_INFORMATION",
        ctypes.StructType("FILE_INFORMATION", [
          { dwFileAttributes: ctypes.uint32_t },
          { ftCreationTime: Type.FILETIME.implementation },
          { ftLastAccessTime: Type.FILETIME.implementation },
          { ftLastWriteTime: Type.FILETIME.implementation },
          { dwVolumeSerialNumber: ctypes.uint32_t },
          { nFileSizeHigh: Type.DWORD.implementation },
          { nFileSizeLow: Type.DWORD.implementation },
          { nNumberOfLinks: ctypes.uint32_t },
          { nFileIndex: ctypes.uint64_t },
        ])
      );

      Type.SystemTime = new SharedAll.Type(
        "SystemTime",
        ctypes.StructType("SystemTime", [
          { wYear: ctypes.int16_t },
          { wMonth: ctypes.int16_t },
          { wDayOfWeek: ctypes.int16_t },
          { wDay: ctypes.int16_t },
          { wHour: ctypes.int16_t },
          { wMinute: ctypes.int16_t },
          { wSecond: ctypes.int16_t },
          { wMilliSeconds: ctypes.int16_t },
        ])
      );

      // Special case: these functions are used by the
      // finalizer
      libc.declareLazy(
        SysFile,
        "_CloseHandle",
        "CloseHandle",
        ctypes.winapi_abi,
        /* return */ ctypes.bool,
        /* handle*/ ctypes.voidptr_t
      );

      SysFile.CloseHandle = function(fd) {
        if (fd == INVALID_HANDLE) {
          return true;
        }
        return fd.dispose(); // Returns the value of |CloseHandle|.
      };

      libc.declareLazy(
        SysFile,
        "_FindClose",
        "FindClose",
        ctypes.winapi_abi,
        /* return */ ctypes.bool,
        /* handle*/ ctypes.voidptr_t
      );

      SysFile.FindClose = function(handle) {
        if (handle == INVALID_HANDLE) {
          return true;
        }
        return handle.dispose(); // Returns the value of |FindClose|.
      };

      // Declare libc functions as functions of |OS.Win.File|

      libc.declareLazyFFI(
        SysFile,
        "CopyFile",
        "CopyFileW",
        ctypes.winapi_abi,
        /* return*/ Type.zero_or_nothing,
        /* sourcePath*/ Type.path,
        Type.path,
        /* bailIfExist*/ Type.bool
      );

      libc.declareLazyFFI(
        SysFile,
        "CreateDirectory",
        "CreateDirectoryW",
        ctypes.winapi_abi,
        /* return*/ Type.zero_or_nothing,
        Type.char16_t.in_ptr,
        /* security*/ Type.SECURITY_ATTRIBUTES.in_ptr
      );

      libc.declareLazyFFI(
        SysFile,
        "CreateFile",
        "CreateFileW",
        ctypes.winapi_abi,
        Type.file_HANDLE,
        Type.path,
        Type.DWORD_FLAGS,
        Type.DWORD_FLAGS,
        /* security*/ Type.SECURITY_ATTRIBUTES.in_ptr,
        /* creation*/ Type.DWORD_FLAGS,
        Type.DWORD_FLAGS,
        /* template*/ Type.HANDLE
      );

      libc.declareLazyFFI(
        SysFile,
        "DeleteFile",
        "DeleteFileW",
        ctypes.winapi_abi,
        /* return*/ Type.zero_or_nothing,
        Type.path
      );

      libc.declareLazyFFI(
        SysFile,
        "FileTimeToSystemTime",
        "FileTimeToSystemTime",
        ctypes.winapi_abi,
        /* return*/ Type.zero_or_nothing,
        /* filetime*/ Type.FILETIME.in_ptr,
        /* systime*/ Type.SystemTime.out_ptr
      );

      libc.declareLazyFFI(
        SysFile,
        "SystemTimeToFileTime",
        "SystemTimeToFileTime",
        ctypes.winapi_abi,
        Type.zero_or_nothing,
        Type.SystemTime.in_ptr,
        /* filetime*/ Type.FILETIME.out_ptr
      );

      libc.declareLazyFFI(
        SysFile,
        "FindFirstFile",
        "FindFirstFileW",
        ctypes.winapi_abi,
        /* return*/ Type.find_HANDLE,
        /* pattern*/ Type.path,
        Type.FindData.out_ptr
      );

      libc.declareLazyFFI(
        SysFile,
        "FindNextFile",
        "FindNextFileW",
        ctypes.winapi_abi,
        /* return*/ Type.zero_or_nothing,
        Type.find_HANDLE,
        Type.FindData.out_ptr
      );

      libc.declareLazyFFI(
        SysFile,
        "FormatMessage",
        "FormatMessageW",
        ctypes.winapi_abi,
        /* return*/ Type.DWORD,
        Type.DWORD_FLAGS,
        /* source*/ Type.void_t.in_ptr,
        Type.DWORD_FLAGS,
        /* langid*/ Type.DWORD_FLAGS,
        Type.out_wstring,
        Type.DWORD,
        /* Arguments*/ Type.void_t.in_ptr
      );

      libc.declareLazyFFI(
        SysFile,
        "GetCurrentDirectory",
        "GetCurrentDirectoryW",
        ctypes.winapi_abi,
        /* return*/ Type.zero_or_DWORD,
        /* length*/ Type.DWORD,
        Type.out_path
      );

      libc.declareLazyFFI(
        SysFile,
        "GetFullPathName",
        "GetFullPathNameW",
        ctypes.winapi_abi,
        Type.zero_or_DWORD,
        /* fileName*/ Type.path,
        Type.DWORD,
        Type.out_path,
        /* filePart*/ Type.DWORD
      );

      libc.declareLazyFFI(
        SysFile,
        "GetDiskFreeSpaceEx",
        "GetDiskFreeSpaceExW",
        ctypes.winapi_abi,
        /* return*/ Type.zero_or_nothing,
        /* directoryName*/ Type.path,
        /* freeBytesForUser*/ Type.uint64_t.out_ptr,
        /* totalBytesForUser*/ Type.uint64_t.out_ptr,
        /* freeTotalBytesOnDrive*/ Type.uint64_t.out_ptr
      );

      libc.declareLazyFFI(
        SysFile,
        "GetFileInformationByHandle",
        "GetFileInformationByHandle",
        ctypes.winapi_abi,
        /* return*/ Type.zero_or_nothing,
        /* handle*/ Type.HANDLE,
        Type.FILE_INFORMATION.out_ptr
      );

      libc.declareLazyFFI(
        SysFile,
        "MoveFileEx",
        "MoveFileExW",
        ctypes.winapi_abi,
        Type.zero_or_nothing,
        /* sourcePath*/ Type.path,
        /* destPath*/ Type.path,
        Type.DWORD
      );

      libc.declareLazyFFI(
        SysFile,
        "ReadFile",
        "ReadFile",
        ctypes.winapi_abi,
        /* return*/ Type.zero_or_nothing,
        Type.HANDLE,
        /* buffer*/ Type.voidptr_t,
        /* nbytes*/ Type.DWORD,
        /* nbytes_read*/ Type.DWORD.out_ptr,
        /* overlapped*/ Type.void_t.inout_ptr // FIXME: Implement?
      );

      libc.declareLazyFFI(
        SysFile,
        "RemoveDirectory",
        "RemoveDirectoryW",
        ctypes.winapi_abi,
        /* return*/ Type.zero_or_nothing,
        Type.path
      );

      libc.declareLazyFFI(
        SysFile,
        "SetCurrentDirectory",
        "SetCurrentDirectoryW",
        ctypes.winapi_abi,
        /* return*/ Type.zero_or_nothing,
        Type.path
      );

      libc.declareLazyFFI(
        SysFile,
        "SetEndOfFile",
        "SetEndOfFile",
        ctypes.winapi_abi,
        /* return*/ Type.zero_or_nothing,
        Type.HANDLE
      );

      libc.declareLazyFFI(
        SysFile,
        "SetFilePointer",
        "SetFilePointer",
        ctypes.winapi_abi,
        /* return*/ Type.DWORD,
        Type.HANDLE,
        /* distlow*/ Type.long,
        /* disthi*/ Type.long.in_ptr,
        /* method*/ Type.DWORD
      );

      libc.declareLazyFFI(
        SysFile,
        "SetFileTime",
        "SetFileTime",
        ctypes.winapi_abi,
        Type.zero_or_nothing,
        Type.HANDLE,
        /* creation*/ Type.FILETIME.in_ptr,
        Type.FILETIME.in_ptr,
        Type.FILETIME.in_ptr
      );

      libc.declareLazyFFI(
        SysFile,
        "WriteFile",
        "WriteFile",
        ctypes.winapi_abi,
        /* return*/ Type.zero_or_nothing,
        Type.HANDLE,
        /* buffer*/ Type.voidptr_t,
        /* nbytes*/ Type.DWORD,
        /* nbytes_wr*/ Type.DWORD.out_ptr,
        /* overlapped*/ Type.void_t.inout_ptr // FIXME: Implement?
      );

      libc.declareLazyFFI(
        SysFile,
        "FlushFileBuffers",
        "FlushFileBuffers",
        ctypes.winapi_abi,
        /* return*/ Type.zero_or_nothing,
        Type.HANDLE
      );

      libc.declareLazyFFI(
        SysFile,
        "GetFileAttributes",
        "GetFileAttributesW",
        ctypes.winapi_abi,
        Type.DWORD_FLAGS,
        /* fileName*/ Type.path
      );

      libc.declareLazyFFI(
        SysFile,
        "SetFileAttributes",
        "SetFileAttributesW",
        ctypes.winapi_abi,
        Type.zero_or_nothing,
        Type.path,
        /* fileAttributes*/ Type.DWORD_FLAGS
      );

      advapi32.declareLazyFFI(
        SysFile,
        "GetNamedSecurityInfo",
        "GetNamedSecurityInfoW",
        ctypes.winapi_abi,
        Type.DWORD,
        Type.path,
        Type.DWORD,
        /* securityInfo*/ Type.DWORD,
        Type.PSID.out_ptr,
        Type.PSID.out_ptr,
        Type.PACL.out_ptr,
        Type.PACL.out_ptr,
        /* securityDesc*/ Type.PSECURITY_DESCRIPTOR.out_ptr
      );

      advapi32.declareLazyFFI(
        SysFile,
        "SetNamedSecurityInfo",
        "SetNamedSecurityInfoW",
        ctypes.winapi_abi,
        Type.DWORD,
        Type.path,
        Type.DWORD,
        /* securityInfo*/ Type.DWORD,
        Type.PSID,
        Type.PSID,
        Type.PACL,
        Type.PACL
      );

      libc.declareLazyFFI(
        SysFile,
        "LocalFree",
        "LocalFree",
        ctypes.winapi_abi,
        Type.HLOCAL,
        Type.HLOCAL
      );
    };

    exports.OS.Win = {
      File: {
        _init: init,
      },
    };
  })(this);
}
