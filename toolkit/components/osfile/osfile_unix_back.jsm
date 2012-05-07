/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This file can be used in the following contexts:
 *
 *  1. included from a non-osfile worker thread using importScript
 *   (it serves to define a synchronous API for that worker thread)
 *   (bug 707679)
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
    // We do not wish osfile_unix_back.jsm to be used directly as a main thread
    // module yet. When time comes, it will be loaded by a combination of
    // a main thread front-end/worker thread implementation that makes sure
    // that we are not executing synchronous IO code in the main thread.

    throw new Error("osfile_unix_back.jsm cannot be used from the main thread yet");
  }
  importScripts("resource://gre/modules/osfile/osfile_shared.jsm");
  (function(exports) {
     "use strict";
     if (!exports.OS) {
       exports.OS = {};
     }
     if (!exports.OS.Unix) {
       exports.OS.Unix = {};
     }
     if (exports.OS.Unix.File) {
       return; // Avoid double-initialization
     }
     exports.OS.Unix.File = {};

     let LOG = OS.Shared.LOG.bind(OS.Shared, "Unix");

     // Open libc
     let libc;
     let libc_candidates =  [ "libSystem.dylib",
                              "libc.so.6",
                              "libc.so" ];
     for (let i = 0; i < libc_candidates.length; ++i) {
       try {
         libc = ctypes.open(libc_candidates[i]);
         break;
       } catch (x) {
         LOG("Could not open libc "+libc_candidates[i]);
       }
     }
     if (!libc) {
       throw new Error("Could not open any libc.");
     }

     /**
      * Initialize the Unix module.
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
       let OSUnix = exports.OS.Unix;
       let UnixFile = exports.OS.Unix.File;
       if (!exports.OS.Types) {
         exports.OS.Types = {};
       }
       let Type = exports.OS.Shared.Type;
       let Types = Type;

       // Initialize types that require additional OS-specific
       // support - either finalization or matching against
       // OS-specific constants.

       /**
        * A file descriptor.
        */
       Types.fd =
         new Type("fd",
                  ctypes.int,
                  function(fd_int) {
                    return ctypes.CDataFinalizer(fd_int, _close);
                  });

       /**
        * A C integer holding -1 in case of error or a file descriptor
        * in case of success.
        */
       Types.negativeone_or_fd =
         new Type("negativeone_or_fd",
                  ctypes.int,
                  function(fd_int, operation) {
                    if (fd_int == -1) {
                      return -1;
                    }
                    return ctypes.CDataFinalizer(fd_int, _close);
                  });

       /**
        * A C integer holding -1 in case of error or a meaningless value
        * in case of success.
        */
       Types.negativeone_or_nothing =
         new Type("negativeone_or_nothing",
                  ctypes.int);

       /**
        * A C integer holding -1 in case of error or a positive integer
        * in case of success.
        */
       Types.negativeone_or_ssize_t =
         new Type("negativeone_or_ssize_t",
                  ctypes.ssize_t,
                  Type.ssize_t.convert_from_c);

       /**
        * A C string
        */
       Types.null_or_string =
         new Type("null_or_string",
                  ctypes.char.ptr);

       Types.string =
         new Type("string",
                  ctypes.char.ptr);


       // Note: support for strings in js-ctypes is very limited.
       // Once bug 552551 has progressed, we should extend this
       // type using ctypes.readString/ctypes.writeString


       // Declare libc functions as functions of |OS.Unix.File|

       // Finalizer-related functions
       let _close =
         libc.declare("close", ctypes.default_abi,
                        /*return */ctypes.int,
                        /*fd*/     ctypes.int);

       UnixFile.close = function close(fd) {
         // Detach the finalizer and call |_close|.
         return fd.dispose();
       };

       UnixFile.free =
         libc.declare("free", ctypes.default_abi,
                       /*return*/ ctypes.void_t,
                       /*ptr*/    ctypes.voidptr_t);

       // Other functions
       UnixFile.access =
         declareFFI("access", ctypes.default_abi,
                    /*return*/ Types.negativeone_or_nothing,
                    /*path*/   Types.string,
                    /*mode*/   Types.int);

       UnixFile.chdir =
         declareFFI("chdir", ctypes.default_abi,
                    /*return*/ Types.negativeone_or_nothing,
                    /*path*/   Types.string);

       UnixFile.chmod =
         declareFFI("chmod", ctypes.default_abi,
                    /*return*/ Types.negativeone_or_nothing,
                    /*path*/   Types.string,
                    /*mode*/   Types.mode_t);

       UnixFile.chown =
         declareFFI("chown", ctypes.default_abi,
                    /*return*/ Types.negativeone_or_nothing,
                    /*path*/   Types.string,
                    /*uid*/    Types.uid_t,
                    /*gid*/    Types.gid_t);

       UnixFile.copyfile =
         declareFFI("copyfile", ctypes.default_abi,
                    /*return*/ Types.negativeone_or_nothing,
                    /*source*/ Types.string,
                    /*dest*/   Types.string,
                    /*state*/  Types.void_t.in_ptr, // Ignored atm
                    /*flags*/  Types.uint32_t);

       UnixFile.dup =
         declareFFI("dup", ctypes.default_abi,
                    /*return*/ Types.negativeone_or_fd,
                    /*fd*/     Types.fd);

       UnixFile.chdir =
         declareFFI("chdir", ctypes.default_abi,
                    /*return*/ Types.negativeone_or_nothing,
                    /*path*/   Types.string);

       UnixFile.fchdir =
         declareFFI("fchdir", ctypes.default_abi,
                    /*return*/ Types.negativeone_or_nothing,
                    /*fd*/     Types.fd);

       UnixFile.fchown =
         declareFFI("fchown", ctypes.default_abi,
                    /*return*/ Types.negativeone_or_nothing,
                    /*fd*/     Types.fd,
                    /*uid_t*/  Types.uid_t,
                    /*gid_t*/  Types.gid_t);

       UnixFile.fsync =
         declareFFI("fsync", ctypes.default_abi,
                    /*return*/ Types.negativeone_or_nothing,
                    /*fd*/     Types.fd);

       UnixFile.getcwd =
         declareFFI("getcwd", ctypes.default_abi,
                    /*return*/ Types.null_or_string,
                    /*buf*/    Types.char.out_ptr,
                    /*size*/   Types.size_t);

       UnixFile.getwd =
         declareFFI("getwd", ctypes.default_abi,
                    /*return*/ Types.null_or_string,
                    /*buf*/    Types.char.out_ptr);

       // Two variants of |getwd| which allocate the memory
       // dynamically.

       // Linux/Android version
       UnixFile.get_current_dir_name =
         declareFFI("get_current_dir_name", ctypes.default_abi,
                    /*return*/ Types.null_or_string.releaseWith(UnixFile.free));

       // MacOS/BSD version (will return NULL on Linux/Android)
       UnixFile.getwd_auto =
         declareFFI("getwd", ctypes.default_abi,
                    /*return*/ Types.null_or_string.releaseWith(UnixFile.free),
                    /*buf*/    Types.void_t.in_ptr);

       UnixFile.fdatasync =
         declareFFI("fdatasync", ctypes.default_abi,
                    /*return*/ Types.negativeone_or_nothing,
                    /*fd*/     Types.fd); // Note: MacOS/BSD-specific

       UnixFile.ftruncate =
         declareFFI("ftruncate", ctypes.default_abi,
                    /*return*/ Types.negativeone_or_nothing,
                    /*fd*/     Types.fd,
                    /*length*/ Types.off_t);

       UnixFile.lchown =
         declareFFI("lchown", ctypes.default_abi,
                    /*return*/ Types.negativeone_or_nothing,
                    /*path*/   Types.string,
                    /*uid_t*/  Types.uid_t,
                    /*gid_t*/  Types.gid_t);

       UnixFile.link =
         declareFFI("link", ctypes.default_abi,
                    /*return*/ Types.negativeone_or_nothing,
                    /*source*/ Types.string,
                    /*dest*/   Types.string);

       UnixFile.lseek =
         declareFFI("lseek", ctypes.default_abi,
                    /*return*/ Types.off_t,
                    /*fd*/     Types.fd,
                    /*offset*/ Types.off_t,
                    /*whence*/ Types.int);

       UnixFile.mkstemp =
         declareFFI("mkstemp", ctypes.default_abi,
                    /*return*/ Types.null_or_string,
                    /*template*/Types.string);

       UnixFile.open =
         declareFFI("open", ctypes.default_abi,
                    /*return*/Types.negativeone_or_fd,
                    /*path*/  Types.string,
                    /*oflags*/Types.int,
                    /*mode*/  Types.int);

       UnixFile.pread =
         declareFFI("pread", ctypes.default_abi,
                    /*return*/ Types.negativeone_or_ssize_t,
                    /*fd*/     Types.fd,
                    /*buf*/    Types.char.out_ptr,
                    /*nbytes*/ Types.size_t,
                    /*offset*/ Types.off_t);

       UnixFile.pwrite =
         declareFFI("pwrite", ctypes.default_abi,
                    /*return*/ Types.negativeone_or_ssize_t,
                    /*fd*/     Types.fd,
                    /*buf*/    Types.char.in_ptr,
                    /*nbytes*/ Types.size_t,
                    /*offset*/ Types.off_t);

       UnixFile.read =
         declareFFI("read", ctypes.default_abi,
                    /*return*/Types.negativeone_or_ssize_t,
                    /*fd*/    Types.fd,
                    /*buf*/   Types.char.out_ptr,
                    /*nbytes*/Types.size_t);

       UnixFile.rename =
         declareFFI("rename", ctypes.default_abi,
                    /*return*/ Types.negativeone_or_nothing,
                    /*old*/    Types.string,
                    /*new*/    Types.string);

       UnixFile.splice =
         declareFFI("splice", ctypes.default_abi,
                    /*return*/ Types.long,
                    /*fd_in*/  Types.fd,
                    /*off_in*/ Types.off_t.in_ptr,
                    /*fd_out*/ Types.fd,
                    /*off_out*/Types.off_t.in_ptr,
                    /*len*/    Types.size_t,
                    /*flags*/  Types.unsigned_int); // Linux/Android-specific

       UnixFile.strerror =
         declareFFI("strerror", ctypes.default_abi,
                    /*return*/ Types.null_or_string,
                    /*errnum*/ Types.int);

       UnixFile.symlink =
         declareFFI("symlink", ctypes.default_abi,
                    /*return*/ Types.negativeone_or_nothing,
                    /*source*/ Types.string,
                    /*dest*/   Types.string);

       UnixFile.truncate =
         declareFFI("truncate", ctypes.default_abi,
                    /*return*/Types.negativeone_or_nothing,
                    /*path*/  Types.string,
                    /*length*/ Types.off_t);

       UnixFile.unlink =
         declareFFI("unlink", ctypes.default_abi,
                    /*return*/ Types.negativeone_or_nothing,
                    /*path*/   Types.string);

       UnixFile.write =
         declareFFI("write", ctypes.default_abi,
                    /*return*/ Types.negativeone_or_ssize_t,
                    /*fd*/     Types.fd,
                    /*buf*/    Types.char.in_ptr,
                    /*nbytes*/ Types.size_t);

       // Weird cases that require special treatment

       // We cannot make a C array of CDataFinalizer, so
       // pipe cannot be directly defined as a C function.

       let _pipe =
         declareFFI("pipe", ctypes.default_abi,
                    /*return*/ Types.negativeone_or_nothing,
                    /*fds*/    Types.int.out_ptr);

       // A shared per-thread buffer used to communicate with |pipe|
       let _pipebuf = new (ctypes.ArrayType(ctypes.int, 2))();

       UnixFile.pipe = function pipe(array) {
         let result = _pipe(_pipebuf);
         if (result == -1) {
           return result;
         }
         array[0] = ctypes.CDataFinalizer(_pipebuf[0], _close);
         array[1] = ctypes.CDataFinalizer(_pipebuf[1], _close);
         return result;
       };

       // Export useful stuff for extensibility

       exports.OS.Unix.libc = libc;
       exports.OS.Unix.declareFFI = declareFFI;

     };
     exports.OS.Unix.File._init = init;
   })(this);
}
