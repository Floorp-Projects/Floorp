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
     let LOG = SharedAll.LOG.bind(SharedAll, "Unix", "back");
     let libc = SysAll.libc;
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
         declareFFI = SysAll.declareFFI;
       }
       let declareLazyFFI = SharedAll.declareLazyFFI;

       // Initialize types that require additional OS-specific
       // support - either finalization or matching against
       // OS-specific constants.
       let Type = Object.create(SysAll.Type);
       let SysFile = exports.OS.Win.File = { Type: Type };

       // Initialize types

       /**
        * A C integer holding INVALID_HANDLE_VALUE in case of error or
        * a file descriptor in case of success.
        */
       Type.HANDLE =
         Type.voidptr_t.withName("HANDLE");
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
       SharedAll.defineLazyGetter(Type.file_HANDLE,
         "finalizeHANDLE",
         function() {
           return SysFile._CloseHandle;
         });

       Type.find_HANDLE = Type.HANDLE.withName("find HANDLE");
       SharedAll.defineLazyGetter(Type.find_HANDLE,
         "finalizeHANDLE",
         function() {
           return SysFile._FindClose;
         });

       Type.DWORD = Type.uint32_t.withName("DWORD");

       /**
        * A special type used to represent flags passed as DWORDs to a function.
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
       Type.zero_or_DWORD =
         Type.DWORD.withName("zero_or_DWORD");

       /**
        * A C integer holding 0 in case of error, any other value in
        * case of success.
        */
       Type.zero_or_nothing =
         Type.int.withName("zero_or_nothing");

       Type.SECURITY_ATTRIBUTES =
         Type.void_t.withName("SECURITY_ATTRIBUTES");

       Type.FILETIME =
         new SharedAll.Type("FILETIME",
                  ctypes.StructType("FILETIME", [
                  { lo: Type.DWORD.implementation },
                  { hi: Type.DWORD.implementation }]));

       Type.FindData =
         new SharedAll.Type("FIND_DATA",
                  ctypes.StructType("FIND_DATA", [
                    { dwFileAttributes: ctypes.uint32_t },
                    { ftCreationTime:   Type.FILETIME.implementation },
                    { ftLastAccessTime: Type.FILETIME.implementation },
                    { ftLastWriteTime:  Type.FILETIME.implementation },
                    { nFileSizeHigh:    Type.DWORD.implementation },
                    { nFileSizeLow:     Type.DWORD.implementation },
                    { dwReserved0:      Type.DWORD.implementation },
                    { dwReserved1:      Type.DWORD.implementation },
                    { cFileName:        ctypes.ArrayType(ctypes.jschar, Const.MAX_PATH) },
                    { cAlternateFileName: ctypes.ArrayType(ctypes.jschar, 14) }
                      ]));

       Type.FILE_INFORMATION =
         new SharedAll.Type("FILE_INFORMATION",
                  ctypes.StructType("FILE_INFORMATION", [
                    { dwFileAttributes: ctypes.uint32_t },
                    { ftCreationTime:   Type.FILETIME.implementation },
                    { ftLastAccessTime: Type.FILETIME.implementation },
                    { ftLastWriteTime:  Type.FILETIME.implementation },
                    { dwVolumeSerialNumber: ctypes.uint32_t },
                    { nFileSizeHigh:    Type.DWORD.implementation },
                    { nFileSizeLow:     Type.DWORD.implementation },
                    { nNumberOfLinks:   ctypes.uint32_t },
                    { nFileIndex: ctypes.uint64_t }
                   ]));

       Type.SystemTime =
         new SharedAll.Type("SystemTime",
                  ctypes.StructType("SystemTime", [
                  { wYear:      ctypes.int16_t },
                  { wMonth:     ctypes.int16_t },
                  { wDayOfWeek: ctypes.int16_t },
                  { wDay:       ctypes.int16_t },
                  { wHour:      ctypes.int16_t },
                  { wMinute:    ctypes.int16_t },
                  { wSecond:    ctypes.int16_t },
                  { wMilliSeconds: ctypes.int16_t }
                  ]));

       // Special case: these functions are used by the
       // finalizer
       SharedAll.declareLazy(SysFile, "_CloseHandle", libc,
                      "CloseHandle", ctypes.winapi_abi,
                        /*return */ctypes.bool,
                        /*handle*/ ctypes.voidptr_t);

       SysFile.CloseHandle = function(fd) {
         if (fd == INVALID_HANDLE) {
           return true;
         } else {
           return fd.dispose(); // Returns the value of |CloseHandle|.
         }
       };

       SharedAll.declareLazy(SysFile, "_FindClose", libc,
                      "FindClose", ctypes.winapi_abi,
                        /*return */ctypes.bool,
                        /*handle*/ ctypes.voidptr_t);

       SysFile.FindClose = function(handle) {
         if (handle == INVALID_HANDLE) {
           return true;
         } else {
           return handle.dispose(); // Returns the value of |FindClose|.
         }
       };

       // Declare libc functions as functions of |OS.Win.File|

       declareLazyFFI(SysFile, "CopyFile", libc,
         "CopyFileW", ctypes.winapi_abi,
                    /*return*/ Type.zero_or_nothing,
                    /*sourcePath*/ Type.path,
                    /*destPath*/   Type.path,
                    /*bailIfExist*/Type.bool);

       declareLazyFFI(SysFile, "CreateDirectory", libc,
         "CreateDirectoryW", ctypes.winapi_abi,
                    /*return*/ Type.zero_or_nothing,
                    /*name*/   Type.jschar.in_ptr,
                    /*security*/Type.SECURITY_ATTRIBUTES.in_ptr);

       declareLazyFFI(SysFile, "CreateFile", libc,
         "CreateFileW", ctypes.winapi_abi,
                    /*return*/  Type.file_HANDLE,
                    /*name*/    Type.path,
                    /*access*/  Type.DWORD_FLAGS,
                    /*share*/   Type.DWORD_FLAGS,
                    /*security*/Type.SECURITY_ATTRIBUTES.in_ptr,
                    /*creation*/Type.DWORD_FLAGS,
                    /*flags*/   Type.DWORD_FLAGS,
                    /*template*/Type.HANDLE);

       declareLazyFFI(SysFile, "DeleteFile", libc,
         "DeleteFileW", ctypes.winapi_abi,
                    /*return*/ Type.zero_or_nothing,
                    /*path*/   Type.path);

       declareLazyFFI(SysFile, "FileTimeToSystemTime", libc,
         "FileTimeToSystemTime", ctypes.winapi_abi,
                    /*return*/ Type.zero_or_nothing,
                    /*filetime*/Type.FILETIME.in_ptr,
                    /*systime*/ Type.SystemTime.out_ptr);

       declareLazyFFI(SysFile, "SystemTimeToFileTime", libc,
         "SystemTimeToFileTime", ctypes.winapi_abi,
                    /*return*/   Type.zero_or_nothing,
                    /*systime*/  Type.SystemTime.in_ptr,
                    /*filetime*/ Type.FILETIME.out_ptr);

       declareLazyFFI(SysFile, "FindFirstFile", libc,
         "FindFirstFileW", ctypes.winapi_abi,
                    /*return*/ Type.find_HANDLE,
                    /*pattern*/Type.path,
                    /*data*/   Type.FindData.out_ptr);

       declareLazyFFI(SysFile, "FindNextFile", libc,
         "FindNextFileW", ctypes.winapi_abi,
                    /*return*/ Type.zero_or_nothing,
                    /*prev*/   Type.find_HANDLE,
                    /*data*/   Type.FindData.out_ptr);

       declareLazyFFI(SysFile, "FormatMessage", libc,
         "FormatMessageW", ctypes.winapi_abi,
                    /*return*/ Type.DWORD,
                    /*flags*/  Type.DWORD_FLAGS,
                    /*source*/ Type.void_t.in_ptr,
                    /*msgid*/  Type.DWORD_FLAGS,
                    /*langid*/ Type.DWORD_FLAGS,
                    /*buf*/    Type.out_wstring,
                    /*size*/   Type.DWORD,
                    /*Arguments*/Type.void_t.in_ptr
                   );

       declareLazyFFI(SysFile, "GetCurrentDirectory", libc,
         "GetCurrentDirectoryW", ctypes.winapi_abi,
                    /*return*/ Type.zero_or_DWORD,
                    /*length*/ Type.DWORD,
                    /*buf*/    Type.out_path
                   );

       declareLazyFFI(SysFile, "GetFileInformationByHandle", libc,
         "GetFileInformationByHandle", ctypes.winapi_abi,
                    /*return*/ Type.zero_or_nothing,
                    /*handle*/ Type.HANDLE,
                    /*info*/   Type.FILE_INFORMATION.out_ptr);

       declareLazyFFI(SysFile, "MoveFileEx", libc,
         "MoveFileExW", ctypes.winapi_abi,
                    /*return*/   Type.zero_or_nothing,
                    /*sourcePath*/ Type.path,
                    /*destPath*/ Type.path,
                    /*flags*/    Type.DWORD_FLAGS
                   );

       declareLazyFFI(SysFile, "ReadFile", libc,
         "ReadFile", ctypes.winapi_abi,
                    /*return*/ Type.zero_or_nothing,
                    /*file*/   Type.HANDLE,
                    /*buffer*/ Type.voidptr_t,
                    /*nbytes*/ Type.DWORD,
                    /*nbytes_read*/Type.DWORD.out_ptr,
                    /*overlapped*/Type.void_t.inout_ptr // FIXME: Implement?
         );

       declareLazyFFI(SysFile, "RemoveDirectory", libc,
         "RemoveDirectoryW", ctypes.winapi_abi,
                    /*return*/ Type.zero_or_nothing,
                    /*path*/   Type.path);

       declareLazyFFI(SysFile, "SetCurrentDirectory", libc,
         "SetCurrentDirectoryW", ctypes.winapi_abi,
                    /*return*/ Type.zero_or_nothing,
                    /*path*/   Type.path
                   );

       declareLazyFFI(SysFile, "SetEndOfFile", libc,
         "SetEndOfFile", ctypes.winapi_abi,
                    /*return*/ Type.zero_or_nothing,
                    /*file*/   Type.HANDLE);

       declareLazyFFI(SysFile, "SetFilePointer", libc,
         "SetFilePointer", ctypes.winapi_abi,
                    /*return*/ Type.DWORD,
                    /*file*/   Type.HANDLE,
                    /*distlow*/Type.long,
                    /*disthi*/ Type.long.in_ptr,
                    /*method*/ Type.DWORD);

       declareLazyFFI(SysFile, "SetFileTime", libc,
         "SetFileTime",  ctypes.winapi_abi,
                    /*return*/   Type.zero_or_nothing,
                    /*file*/     Type.HANDLE,
                    /*creation*/ Type.FILETIME.in_ptr,
                    /*access*/   Type.FILETIME.in_ptr,
                    /*write*/    Type.FILETIME.in_ptr);


       declareLazyFFI(SysFile, "WriteFile", libc,
         "WriteFile", ctypes.winapi_abi,
                    /*return*/ Type.zero_or_nothing,
                    /*file*/   Type.HANDLE,
                    /*buffer*/ Type.voidptr_t,
                    /*nbytes*/ Type.DWORD,
                    /*nbytes_wr*/Type.DWORD.out_ptr,
                    /*overlapped*/Type.void_t.inout_ptr // FIXME: Implement?
         );

        declareLazyFFI(SysFile, "FlushFileBuffers", libc,
          "FlushFileBuffers", ctypes.winapi_abi,
                     /*return*/ Type.zero_or_nothing,
                     /*file*/   Type.HANDLE);

        declareLazyFFI(SysFile, "GetFileAttributes", libc,
          "GetFileAttributesW", ctypes.winapi_abi,
                     /*return*/   Type.DWORD_FLAGS,
                     /*fileName*/ Type.path);

        declareLazyFFI(SysFile, "SetFileAttributes", libc,
          "SetFileAttributesW", ctypes.winapi_abi,
                     /*return*/         Type.zero_or_nothing,
                     /*fileName*/       Type.path,
                     /*fileAttributes*/ Type.DWORD_FLAGS);
     };

     exports.OS.Win = {
       File: {
           _init:  init
       }
     };
   })(this);
}
