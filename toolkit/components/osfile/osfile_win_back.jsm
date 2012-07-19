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
  importScripts("resource://gre/modules/osfile/osfile_shared.jsm");

  (function(exports) {
     "use strict";
     if (!exports.OS) {
       exports.OS = {};
     }
     if (!exports.OS.Win) {
       exports.OS.Win = {};
     }
     if (exports.OS.Win.File) {
       return; // Avoid double-initialization
     }
     exports.OS.Win.File = {};

     let LOG = OS.Shared.LOG.bind(OS.Shared, "OS.Win.File");

     let libc = ctypes.open("kernel32.dll");
     if (!libc) {
       throw new Error("Could not open kernel32.dll");
     }

     /**
      * Initialize the Windows module.
      *
      * @param {function=} declareFFI
      */
     let init = function init(aDeclareFFI) {
       let declareFFI;
       if (aDeclareFFI) {
         declareFFI = aDeclareFFI.bind(null, libc);
       } else {
         declareFFI = OS.Shared.declareFFI.bind(null, libc);
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

       Types.HANDLE =
         new Type("HANDLE",
                  ctypes.voidptr_t);

       /**
        * A C integer holding INVALID_HANDLE_VALUE in case of error or
        * a file descriptor in case of success.
        */
       Types.maybe_HANDLE =
         new Type("maybe_HANDLE",
           Types.HANDLE.implementation,
           function (maybe) {
             if (ctypes.cast(maybe, ctypes.int).value == invalid_handle) {
               // Ensure that API clients can effectively compare against
               // Const.INVALID_HANDLE_VALUE. Without this cast,
               // == would always return |false|.
               return invalid_handle;
             }
             return ctypes.CDataFinalizer(maybe, _CloseHandle);
           });

       /**
        * A C integer holding INVALID_HANDLE_VALUE in case of error or
        * a file descriptor in case of success.
        */
       Types.maybe_find_HANDLE =
         new Type("maybe_find_HANDLE",
           Types.HANDLE.implementation,
           function (maybe) {
             if (ctypes.cast(maybe, ctypes.int).value == invalid_handle) {
               // Ensure that API clients can effectively compare against
               // Const.INVALID_HANDLE_VALUE. Without this cast,
               // == would always return |false|.
               return invalid_handle;
             }
             return ctypes.CDataFinalizer(maybe, _FindClose);
           });

       let invalid_handle = exports.OS.Constants.Win.INVALID_HANDLE_VALUE;

       Types.DWORD = Types.int32_t;

       /**
        * A C integer holding -1 in case of error or a positive integer
        * in case of success.
        */
       Types.negative_or_DWORD =
         new Type("negative_or_DWORD",
                  ctypes.int32_t);

       /**
        * A C integer holding 0 in case of error or a positive integer
        * in case of success.
        */
       Types.zero_or_DWORD =
         new Type("zero_or_DWORD",
                  ctypes.int32_t);

       /**
        * A C integer holding 0 in case of error, any other value in
        * case of success.
        */
       Types.zero_or_nothing =
         new Type("zero_or_nothing",
                  Types.bool.implementation);

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
       let _CloseHandle =
         libc.declare("CloseHandle", ctypes.winapi_abi,
                        /*return */ctypes.bool,
                        /*handle*/ ctypes.voidptr_t);

       WinFile.CloseHandle = function(fd) {
         return fd.dispose(); // Returns the value of |CloseHandle|.
       };

       let _FindClose =
         libc.declare("CloseHandle", ctypes.winapi_abi,
                        /*return */ctypes.bool,
                        /*handle*/ ctypes.voidptr_t);

       WinFile.FindClose = function(handle) {
         return handle.dispose(); // Returns the value of |CloseHandle|.
       };

       // Declare libc functions as functions of |OS.Win.File|

       WinFile.CopyFile =
         declareFFI("CopyFileW", ctypes.winapi_abi,
                    /*return*/ Types.zero_or_nothing,
                    /*sourcePath*/ Types.jschar.in_ptr,
                    /*destPath*/   Types.jschar.in_ptr,
                    /*bailIfExist*/Types.bool);

       WinFile.CreateFile =
         declareFFI("CreateFileW", ctypes.winapi_abi,
                    /*return*/  Types.maybe_HANDLE,
                    /*name*/    Types.jschar.in_ptr,
                    /*access*/  Types.DWORD,
                    /*share*/   Types.DWORD,
                    /*security*/Types.void_t.in_ptr,// FIXME: Implement?
                    /*creation*/Types.DWORD,
                    /*flags*/   Types.DWORD,
                    /*template*/Types.HANDLE);

       WinFile.DeleteFile =
         declareFFI("DeleteFileW", ctypes.winapi_abi,
                    /*return*/ Types.zero_or_nothing,
                    /*path*/   Types.jschar.in_ptr);

       WinFile.FileTimeToSystemTime =
         declareFFI("FileTimeToSystemTime", ctypes.winapi_abi,
                    /*return*/ Types.zero_or_nothing,
                    /*filetime*/Types.FILETIME.in_ptr,
                    /*systime*/ Types.SystemTime.out_ptr);

       WinFile.FindFirstFile =
         declareFFI("FindFirstFileW", ctypes.winapi_abi,
                    /*return*/ Types.maybe_find_HANDLE,
                    /*pattern*/Types.jschar.in_ptr,
                    /*data*/   Types.FindData.out_ptr);

       WinFile.FindNextFile =
         declareFFI("FindNextFileW", ctypes.winapi_abi,
                    /*return*/ Types.zero_or_nothing,
                    /*prev*/   Types.HANDLE,
                    /*data*/   Types.FindData.out_ptr);

       WinFile.FormatMessage =
         declareFFI("FormatMessageW", ctypes.winapi_abi,
                    /*return*/ Types.DWORD,
                    /*flags*/  Types.DWORD,
                    /*source*/ Types.void_t.in_ptr,
                    /*msgid*/  Types.DWORD,
                    /*langid*/ Types.DWORD,
                    /*buf*/    Types.jschar.out_ptr,
                    /*size*/   Types.DWORD,
                    /*Arguments*/Types.void_t.in_ptr
                   );

       WinFile.GetCurrentDirectory =
         declareFFI("GetCurrentDirectoryW", ctypes.winapi_abi,
                    /*return*/ Types.zero_or_DWORD,
                    /*length*/ Types.DWORD,
                    /*buf*/    Types.jschar.out_ptr
                   );

       WinFile.MoveFileEx =
         declareFFI("MoveFileExW", ctypes.winapi_abi,
                    /*return*/   Types.zero_or_nothing,
                    /*sourcePath*/ Types.jschar.in_ptr,
                    /*destPath*/ Types.jschar.in_ptr,
                    /*flags*/    Types.DWORD
                   );

       WinFile.ReadFile =
         declareFFI("ReadFile", ctypes.winapi_abi,
                    /*return*/ Types.zero_or_nothing,
                    /*file*/   Types.HANDLE,
                    /*buffer*/ Types.char.out_ptr,
                    /*nbytes*/ Types.DWORD,
                    /*nbytes_read*/Types.DWORD.out_ptr,
                    /*overlapped*/Types.void_t.inout_ptr // FIXME: Implement?
         );

       WinFile.RemoveDirectory =
         declareFFI("RemoveDirectoryW", ctypes.winapi_abi,
                    /*return*/ Types.zero_or_nothing,
                    /*path*/   Types.jschar.in_ptr);

       WinFile.SetCurrentDirectory =
         declareFFI("SetCurrentDirectoryW", ctypes.winapi_abi,
                    /*return*/ Types.zero_or_nothing,
                    /*path*/   Types.jschar.in_ptr
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
                    /*buffer*/ Types.char.in_ptr,
                    /*nbytes*/ Types.DWORD,
                    /*nbytes_wr*/Types.DWORD.out_ptr,
                    /*overlapped*/Types.void_t.inout_ptr // FIXME: Implement?
         );

       // Export useful stuff for extensibility

       exports.OS.Win.libc = libc;
       exports.OS.Win.declareFFI = declareFFI;
     };
     exports.OS.Win.File._init = init;
   })(this);
}
