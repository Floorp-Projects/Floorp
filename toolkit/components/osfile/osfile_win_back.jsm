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
     exports.OS = require("resource://gre/modules/osfile/osfile_shared_allthreads.jsm").OS;
     exports.OS.Win.File = {};

     let LOG = OS.Shared.LOG.bind(OS.Shared, "Win", "back");
     let libc = exports.OS.Shared.Win.libc;

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
         declareFFI = exports.OS.Shared.Win.declareFFI;
       }

       // Shorthands
       let OSWin = exports.OS.Win;
       let WinFile = exports.OS.Win.File;
       if (!exports.OS.Types) {
         exports.OS.Types = {};
       }
       let Type = exports.OS.Shared.Type;
       let Types = Type;

       // Initialize types

       /**
        * A C integer holding INVALID_HANDLE_VALUE in case of error or
        * a file descriptor in case of success.
        */
       Types.HANDLE =
         Types.voidptr_t.withName("HANDLE");
       Types.HANDLE.importFromC = function importFromC(maybe) {
         if (Types.int.cast(maybe).value == INVALID_HANDLE) {
           // Ensure that API clients can effectively compare against
           // Const.INVALID_HANDLE_VALUE. Without this cast,
           // == would always return |false|.
           return INVALID_HANDLE;
         }
         return ctypes.CDataFinalizer(maybe, this.finalizeHANDLE);
       };
       Types.HANDLE.finalizeHANDLE = function placeholder() {
         throw new Error("finalizeHANDLE should be implemented");
       };
       let INVALID_HANDLE = exports.OS.Constants.Win.INVALID_HANDLE_VALUE;

       Types.file_HANDLE = Types.HANDLE.withName("file HANDLE");
       exports.OS.Shared.defineLazyGetter(Types.file_HANDLE,
         "finalizeHANDLE",
         function() {
           return _CloseHandle;
         });

       Types.find_HANDLE = Types.HANDLE.withName("find HANDLE");
       exports.OS.Shared.defineLazyGetter(Types.find_HANDLE,
         "finalizeHANDLE",
         function() {
           return _FindClose;
         });

       Types.DWORD = Types.int32_t.withName("DWORD");

       /**
        * A C integer holding -1 in case of error or a positive integer
        * in case of success.
        */
       Types.negative_or_DWORD =
         Types.DWORD.withName("negative_or_DWORD");

       /**
        * A C integer holding 0 in case of error or a positive integer
        * in case of success.
        */
       Types.zero_or_DWORD =
         Types.DWORD.withName("zero_or_DWORD");

       /**
        * A C integer holding 0 in case of error, any other value in
        * case of success.
        */
       Types.zero_or_nothing =
         Types.int.withName("zero_or_nothing");

       Types.SECURITY_ATTRIBUTES =
         Types.void_t.withName("SECURITY_ATTRIBUTES");

       Types.FILETIME =
         new Type("FILETIME",
                  ctypes.StructType("FILETIME", [
                  { lo: Types.DWORD.implementation },
                  { hi: Types.DWORD.implementation }]));

       Types.FindData =
         new Type("FIND_DATA",
                  ctypes.StructType("FIND_DATA", [
                    { dwFileAttributes: ctypes.uint32_t },
                    { ftCreationTime:   Types.FILETIME.implementation },
                    { ftLastAccessTime: Types.FILETIME.implementation },
                    { ftLastWriteTime:  Types.FILETIME.implementation },
                    { nFileSizeHigh:    Types.DWORD.implementation },
                    { nFileSizeLow:     Types.DWORD.implementation },
                    { dwReserved0:      Types.DWORD.implementation },
                    { dwReserved1:      Types.DWORD.implementation },
                    { cFileName:        ctypes.ArrayType(ctypes.jschar, exports.OS.Constants.Win.MAX_PATH) },
                    { cAlternateFileName: ctypes.ArrayType(ctypes.jschar, 14) }
                      ]));

       Types.FILE_INFORMATION =
         new Type("FILE_INFORMATION",
                  ctypes.StructType("FILE_INFORMATION", [
                    { dwFileAttributes: ctypes.uint32_t },
                    { ftCreationTime:   Types.FILETIME.implementation },
                    { ftLastAccessTime: Types.FILETIME.implementation },
                    { ftLastWriteTime:  Types.FILETIME.implementation },
                    { dwVolumeSerialNumber: ctypes.uint32_t },
                    { nFileSizeHigh:    Types.DWORD.implementation },
                    { nFileSizeLow:     Types.DWORD.implementation },
                    { nNumberOfLinks:   ctypes.uint32_t },
                    { nFileIndex: ctypes.uint64_t }
                   ]));

       Types.SystemTime =
         new Type("SystemTime",
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
       let _CloseHandle = WinFile._CloseHandle =
         libc.declare("CloseHandle", ctypes.winapi_abi,
                        /*return */ctypes.bool,
                        /*handle*/ ctypes.voidptr_t);

       WinFile.CloseHandle = function(fd) {
         if (fd == INVALID_HANDLE) {
           return true;
         } else {
           return fd.dispose(); // Returns the value of |CloseHandle|.
         }
       };

       let _FindClose =
         libc.declare("FindClose", ctypes.winapi_abi,
                        /*return */ctypes.bool,
                        /*handle*/ ctypes.voidptr_t);

       WinFile.FindClose = function(handle) {
         if (handle == INVALID_HANDLE) {
           return true;
         } else {
           return handle.dispose(); // Returns the value of |FindClose|.
         }
       };

       // Declare libc functions as functions of |OS.Win.File|

       WinFile.CopyFile =
         declareFFI("CopyFileW", ctypes.winapi_abi,
                    /*return*/ Types.zero_or_nothing,
                    /*sourcePath*/ Types.path,
                    /*destPath*/   Types.path,
                    /*bailIfExist*/Types.bool);

       WinFile.CreateDirectory =
         declareFFI("CreateDirectoryW", ctypes.winapi_abi,
                    /*return*/ Types.zero_or_nothing,
                    /*name*/   Types.jschar.in_ptr,
                    /*security*/Types.SECURITY_ATTRIBUTES.in_ptr);

       WinFile.CreateFile =
         declareFFI("CreateFileW", ctypes.winapi_abi,
                    /*return*/  Types.file_HANDLE,
                    /*name*/    Types.path,
                    /*access*/  Types.DWORD,
                    /*share*/   Types.DWORD,
                    /*security*/Types.SECURITY_ATTRIBUTES.in_ptr,
                    /*creation*/Types.DWORD,
                    /*flags*/   Types.DWORD,
                    /*template*/Types.HANDLE);

       WinFile.DeleteFile =
         declareFFI("DeleteFileW", ctypes.winapi_abi,
                    /*return*/ Types.zero_or_nothing,
                    /*path*/   Types.path);

       WinFile.FileTimeToSystemTime =
         declareFFI("FileTimeToSystemTime", ctypes.winapi_abi,
                    /*return*/ Types.zero_or_nothing,
                    /*filetime*/Types.FILETIME.in_ptr,
                    /*systime*/ Types.SystemTime.out_ptr);

       WinFile.FindFirstFile =
         declareFFI("FindFirstFileW", ctypes.winapi_abi,
                    /*return*/ Types.find_HANDLE,
                    /*pattern*/Types.path,
                    /*data*/   Types.FindData.out_ptr);

       WinFile.FindNextFile =
         declareFFI("FindNextFileW", ctypes.winapi_abi,
                    /*return*/ Types.zero_or_nothing,
                    /*prev*/   Types.find_HANDLE,
                    /*data*/   Types.FindData.out_ptr);

       WinFile.FormatMessage =
         declareFFI("FormatMessageW", ctypes.winapi_abi,
                    /*return*/ Types.DWORD,
                    /*flags*/  Types.DWORD,
                    /*source*/ Types.void_t.in_ptr,
                    /*msgid*/  Types.DWORD,
                    /*langid*/ Types.DWORD,
                    /*buf*/    Types.out_wstring,
                    /*size*/   Types.DWORD,
                    /*Arguments*/Types.void_t.in_ptr
                   );

       WinFile.GetCurrentDirectory =
         declareFFI("GetCurrentDirectoryW", ctypes.winapi_abi,
                    /*return*/ Types.zero_or_DWORD,
                    /*length*/ Types.DWORD,
                    /*buf*/    Types.out_path
                   );

       WinFile.GetFileInformationByHandle =
         declareFFI("GetFileInformationByHandle", ctypes.winapi_abi,
                    /*return*/ Types.zero_or_nothing,
                    /*handle*/ Types.HANDLE,
                    /*info*/   Types.FILE_INFORMATION.out_ptr);

       WinFile.MoveFileEx =
         declareFFI("MoveFileExW", ctypes.winapi_abi,
                    /*return*/   Types.zero_or_nothing,
                    /*sourcePath*/ Types.path,
                    /*destPath*/ Types.path,
                    /*flags*/    Types.DWORD
                   );

       WinFile.ReadFile =
         declareFFI("ReadFile", ctypes.winapi_abi,
                    /*return*/ Types.zero_or_nothing,
                    /*file*/   Types.HANDLE,
                    /*buffer*/ Types.voidptr_t,
                    /*nbytes*/ Types.DWORD,
                    /*nbytes_read*/Types.DWORD.out_ptr,
                    /*overlapped*/Types.void_t.inout_ptr // FIXME: Implement?
         );

       WinFile.RemoveDirectory =
         declareFFI("RemoveDirectoryW", ctypes.winapi_abi,
                    /*return*/ Types.zero_or_nothing,
                    /*path*/   Types.path);

       WinFile.SetCurrentDirectory =
         declareFFI("SetCurrentDirectoryW", ctypes.winapi_abi,
                    /*return*/ Types.zero_or_nothing,
                    /*path*/   Types.path
                   );

       WinFile.SetEndOfFile =
         declareFFI("SetEndOfFile", ctypes.winapi_abi,
                    /*return*/ Types.zero_or_nothing,
                    /*file*/   Types.HANDLE);

       WinFile.SetFilePointer =
         declareFFI("SetFilePointer", ctypes.winapi_abi,
                    /*return*/ Types.negative_or_DWORD,
                    /*file*/   Types.HANDLE,
                    /*distlow*/Types.long,
                    /*disthi*/ Types.long.in_ptr,
                    /*method*/ Types.DWORD);

       WinFile.WriteFile =
         declareFFI("WriteFile", ctypes.winapi_abi,
                    /*return*/ Types.zero_or_nothing,
                    /*file*/   Types.HANDLE,
                    /*buffer*/ Types.voidptr_t,
                    /*nbytes*/ Types.DWORD,
                    /*nbytes_wr*/Types.DWORD.out_ptr,
                    /*overlapped*/Types.void_t.inout_ptr // FIXME: Implement?
         );

        WinFile.FlushFileBuffers =
          declareFFI("FlushFileBuffers", ctypes.winapi_abi,
                     /*return*/ Types.zero_or_nothing,
                     /*file*/   Types.HANDLE);
     };
     exports.OS.Win.File._init = init;
   })(this);
}
