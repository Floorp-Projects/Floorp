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

       // Special case: these functions are used by the
       // finalizer
       let _CloseHandle =
         libc.declare("CloseHandle", ctypes.winapi_abi,
                        /*return */ctypes.bool,
                        /*handle*/ ctypes.voidptr_t);

       WinFile.CloseHandle = function(fd) {
         return fd.dispose(); // Returns the value of |CloseHandle|.
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

        /**
         * Utility function: check that a path is not a UNC path,
         * as the current implementation of |Path| does not support
         * UNC grammars.
         *
         * We consider that any path starting with "\\\\" is a UNC path.
         */
        let ensureNotUNC = function ensureNotUNC(path) {
           if (!path) {
              throw new TypeError("Expecting a non-null path");
           }
           if (path.length >= 2 && path[0] == "\\" && path[1] == "\\") {
              throw new Error("Module Path cannot handle UNC-formatted paths yet: " + path);
           }
        };

        /**
         * Utility function: Remove any leading/trailing backslashes
         * from a string.
         */
        let trimBackslashes = function trimBackslashes(string) {
          return string.replace(/^\\+|\\+$/g,'');
        };

        /**
         * Handling native paths.
         *
         * This module contains a number of functions destined to simplify
         * working with native paths through a cross-platform API. Functions
         * of this module will only work with the following assumptions:
         *
         * - paths are valid;
         * - paths are defined with one of the grammars that this module can
         *   parse (see later);
         * - all path concatenations go through function |join|.
         *
         * Limitations of this implementation.
         *
         * Windows supports 6 distinct grammars for paths. For the moment, this
         * implementation supports the following subset:
         *
         * - drivename:backslash-separated components
         * - backslash-separated components
         *
         * Additionally, |normalize| can convert a path containing slash-
         * separated components to a path containing backslash-separated
         * components.
         */
       exports.OS.Win.Path = {
         /**
          * Return the final part of the path.
          * The final part of the path is everything after the last "\\".
          */
         basename: function basename(path) {
           ensureNotUNC(path);
           return path.slice(Math.max(path.lastIndexOf("\\"),
             path.lastIndexOf(":")) + 1);
         },

         /**
          * Return the directory part of the path.  The directory part
          * of the path is everything before the last "\\", including
          * the drive/server name.
          *
          * If the path contains no directory, return the drive letter,
          * or "." if the path contains no drive letter or if option
          * |winNoDrive| is set.
          *
          * @param {string} path The path.
          * @param {*=} options Platform-specific options controlling the behavior
          * of this function. This implementation supports the following options:
          *  - |winNoDrive| If |true|, also remove the letter from the path name.
          */
         dirname: function dirname(path, options) {
           ensureNotUNC(path);
           let noDrive = (options && options.winNoDrive);

           // Find the last occurrence of "\\"
           let index = path.lastIndexOf("\\");
           if (index == -1) {
             // If there is no directory component...
             if (!noDrive) {
               // Return the drive path if possible, falling back to "."
               return this.winGetDrive(path) || ".";
             } else {
               // Or just "."
               return ".";
             }
           }

           // Ignore any occurrence of "\\: immediately before that one
           while (index >= 0 && path[index] == "\\") {
             --index;
           }

           // Compute what is left, removing the drive name if necessary
           let start;
           if (noDrive) {
             start = (this.winGetDrive(path) || "").length;
           } else {
             start = 0;
           }
           return path.slice(start, index + 1);
         },

         /**
          * Join path components.
          * This is the recommended manner of getting the path of a file/subdirectory
          * in a directory.
          *
          * Example: Obtaining $TMP/foo/bar in an OS-independent manner
          *  var tmpDir = OS.Path.to("TmpD");
          *  var path = OS.Path.join(tmpDir, "foo", "bar");
          *
          * Under Windows, this will return "$TMP\foo\bar".
          */
         join: function join(path /*...*/) {
           let paths = [];
           let root;
           let absolute = false;
           for each(let subpath in arguments) {
             let drive = this.winGetDrive(subpath);
             let abs   = this.winIsAbsolute(subpath);
             if (drive) {
               root = drive;
               paths = [trimBackslashes(subpath.slice(drive.length))];
               absolute = abs;
             } else if (abs) {
               paths = [trimBackslashes(subpath)];
               absolute = true;
             } else {
               paths.push(trimBackslashes(subpath));
             }
           }
           let result = "";
           if (root) {
             result += root;
           }
           if (absolute) {
             result += "\\";
           }
           result += paths.join("\\");
           return result;
         },

         /**
          * Return the drive letter of a path, or |null| if the
          * path does not contain a drive letter.
          */
         winGetDrive: function winGetDrive(path) {
           ensureNotUNC(path);
           let index = path.indexOf(":");
           if (index <= 0) return null;
           return path.slice(0, index + 1);
         },

         /**
          * Return |true| if the path is absolute, |false| otherwise.
          *
          * We consider that a path is absolute if it starts with "\\"
          * or "driveletter:\\".
          */
         winIsAbsolute: function winIsAbsolute(path) {
           ensureNotUNC(path);
           return this._winIsAbsolute(path);
         },
         /**
          * As |winIsAbsolute|, but does not check for UNC.
          * Useful mostly as an internal utility, for normalization.
          */
         _winIsAbsolute: function _winIsAbsolute(path) {
           let index = path.indexOf(":");
           return path.length > index + 1 && path[index + 1] == "\\";
         },

         /**
          * Normalize a path by removing any unneeded ".", "..", "\\".
          * Also convert any "/" to a "\\".
          */
         normalize: function normalize(path) {
           let stack = [];

           // Remove the drive (we will put it back at the end)
           let root = this.winGetDrive(path);
           if (root) {
             path = path.slice(root.length);
           }

           // Remember whether we need to restore a leading "\\".
           let absolute = this._winIsAbsolute(path);

           // Normalize "/" to "\\"
           path = path.replace("/", "\\");

           // And now, fill |stack| from the components,
           // popping whenever there is a ".."
           path.split("\\").forEach(function loop(v) {
             switch (v) {
             case "":  case ".": // Ignore
               break;
             case "..":
               if (stack.length == 0) {
                 if (absolute) {
                   throw new Error("Path is ill-formed: attempting to go past root");
                 } else {
                  stack.push("..");
                 }
               } else {
                 stack.pop();
               }
               break;
             default:
               stack.push(v);
             }
           });

           // Put everything back together
           let result = stack.join("\\");
           if (absolute) {
             result = "\\" + result;
           }
           if (root) {
             result = root + result;
           }
           return result;
         },

         /**
          * Return the components of a path.
          * You should generally apply this function to a normalized path.
          *
          * @return {{
          *   {bool} absolute |true| if the path is absolute, |false| otherwise
          *   {array} components the string components of the path
          *   {string?} winDrive the drive or server for this path
          * }}
          *
          * Other implementations may add additional OS-specific informations.
          */
         split: function split(path) {
           return {
             absolute: this.winIsAbsolute(path),
             winDrive: this.winGetDrive(path),
             components: path.split("\\")
           };
         }
       };

       // Export useful stuff for extensibility

       exports.OS.Win.libc = libc;
       exports.OS.Win.declareFFI = declareFFI;
     };
     exports.OS.Win.File._init = init;
   })(this);
}
