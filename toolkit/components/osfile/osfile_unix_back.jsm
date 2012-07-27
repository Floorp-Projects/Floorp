/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

{
  if (typeof Components != "undefined") {
    // We do not wish osfile_unix_back.jsm to be used directly as a main thread
    // module yet. When time comes, it will be loaded by a combination of
    // a main thread front-end/worker thread implementation that makes sure
    // that we are not executing synchronous IO code in the main thread.

    throw new Error("osfile_unix_back.jsm cannot be used from the main thread yet");
  }
  importScripts("resource://gre/modules/osfile/osfile_shared_allthreads.jsm");
  importScripts("resource://gre/modules/osfile/osfile_unix_allthreads.jsm");
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

     let LOG = exports.OS.Shared.LOG.bind(OS.Shared, "Unix", "back");
     let libc = exports.OS.Shared.Unix.libc;

     /**
      * Initialize the Unix module.
      *
      * @param {function=} declareFFI
      */
     // FIXME: Both |init| and |aDeclareFFI| are deprecated, we should remove them
     let init = function init(aDeclareFFI) {
       let declareFFI;
       if (aDeclareFFI) {
         declareFFI = aDeclareFFI.bind(null, libc);
       } else {
         declareFFI = exports.OS.Shared.Unix.declareFFI;
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

       /**
        * Type |mode_t|
        */
       Types.mode_t =
         Types.intn_t(OS.Constants.libc.OSFILE_SIZEOF_MODE_T).withName("mode_t");

       /**
        * Type |time_t|
        */
       Types.time_t =
         Types.intn_t(OS.Constants.libc.OSFILE_SIZEOF_TIME_T).withName("time_t");

       Types.DIR =
         new Type("DIR",
                  ctypes.StructType("DIR"));

       Types.null_or_DIR_ptr =
         new Type("null_or_DIR*",
                  Types.DIR.out_ptr.implementation,
                  function(dir, operation) {
                    if (dir == null || dir.isNull()) {
                      return null;
                    }
                    return ctypes.CDataFinalizer(dir, _close_dir);
                  });

       // Structure |dirent|
       // Building this type is rather complicated, as its layout varies between
       // variants of Unix. For this reason, we rely on a number of constants
       // (computed in C from the C data structures) that give us the layout.
       // The structure we compute looks like
       //  { int8_t[...] before_d_type; // ignored content
       //    int8_t      d_type       ;
       //    int8_t[...] before_d_name; // ignored content
       //    char[...]   d_name;
       //    };
       {
         let dirent = new OS.Shared.HollowStructure("dirent",
           OS.Constants.libc.OSFILE_SIZEOF_DIRENT);
         dirent.add_field_at(OS.Constants.libc.OSFILE_OFFSETOF_DIRENT_D_TYPE,
           "d_type", ctypes.uint8_t);
         dirent.add_field_at(OS.Constants.libc.OSFILE_OFFSETOF_DIRENT_D_NAME,
           "d_name", ctypes.ArrayType(ctypes.char, OS.Constants.libc.OSFILE_SIZEOF_DIRENT_D_NAME));

         // We now have built |dirent|.
         Types.dirent = dirent.getType();
         LOG("dirent is: " + Types.dirent.implementation.toSource());
       }
       Types.null_or_dirent_ptr =
         new Type("null_of_dirent",
                  Types.dirent.out_ptr.implementation);

       // Structure |stat|
       // Same technique
       {
         let stat = new OS.Shared.HollowStructure("stat",
           OS.Constants.libc.OSFILE_SIZEOF_STAT);
         stat.add_field_at(OS.Constants.libc.OSFILE_OFFSETOF_STAT_ST_MODE,
                        "st_mode", Types.mode_t.implementation);
         stat.add_field_at(OS.Constants.libc.OSFILE_OFFSETOF_STAT_ST_UID,
                          "st_uid", ctypes.int);
         stat.add_field_at(OS.Constants.libc.OSFILE_OFFSETOF_STAT_ST_GID,
                          "st_gid", ctypes.int);

         // Here, things get complicated with different data structures.
         // Some platforms have |time_t st_atime| and some platforms have
         // |timespec st_atimespec|. However, since |timespec| starts with
         // a |time_t|, followed by nanoseconds, we just cheat and pretend
         // that everybody has |time_t st_atime|, possibly followed by padding
         stat.add_field_at(OS.Constants.libc.OSFILE_OFFSETOF_STAT_ST_ATIME,
                          "st_atime", Types.time_t.implementation);
         stat.add_field_at(OS.Constants.libc.OSFILE_OFFSETOF_STAT_ST_MTIME,
                          "st_mtime", Types.time_t.implementation);
         stat.add_field_at(OS.Constants.libc.OSFILE_OFFSETOF_STAT_ST_CTIME,
                          "st_ctime", Types.time_t.implementation);

         stat.add_field_at(OS.Constants.libc.OSFILE_OFFSETOF_STAT_ST_SIZE,
                        "st_size", Types.size_t.implementation);
         Types.stat = stat.getType();
       }


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

       let _close_dir =
         libc.declare("closedir", ctypes.default_abi,
                        /*return */ctypes.int,
                        /*dirp*/   Types.DIR.in_ptr.implementation);

       UnixFile.closedir = function closedir(fd) {
         // Detach the finalizer and call |_close_dir|.
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

       if (OS.Constants.libc._DARWIN_FEATURE_64_BIT_INODE) {
         UnixFile.fstat =
           declareFFI("fstat$INODE64", ctypes.default_abi,
                      /*return*/ Types.negativeone_or_nothing,
                      /*path*/   Types.fd,
                      /*buf*/    Types.stat.out_ptr
                     );
       } else {
         UnixFile.fstat =
           declareFFI("fstat", ctypes.default_abi,
                      /*return*/ Types.negativeone_or_nothing,
                      /*path*/   Types.fd,
                      /*buf*/    Types.stat.out_ptr
                     );
       }

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

       UnixFile.mkdir =
         declareFFI("mkdir", ctypes.default_abi,
                    /*return*/ Types.int,
                    /*path*/ Types.string,
                    /*mode*/ Types.int);

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

       UnixFile.opendir =
         declareFFI("opendir", ctypes.default_abi,
                    /*return*/ Types.null_or_DIR_ptr,
                    /*path*/   Types.string);

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

       if (OS.Constants.libc._DARWIN_FEATURE_64_BIT_INODE) {
         // Special case for MacOS X 10.5+
         // Symbol name "readdir" still exists but is used for a
         // deprecated function that does not match the
         // constants of |OS.Constants.libc|.
         UnixFile.readdir =
           declareFFI("readdir$INODE64", ctypes.default_abi,
                     /*return*/Types.null_or_dirent_ptr,
                      /*dir*/   Types.DIR.in_ptr); // For MacOS X
       } else {
         UnixFile.readdir =
           declareFFI("readdir", ctypes.default_abi,
                      /*return*/Types.null_or_dirent_ptr,
                      /*dir*/   Types.DIR.in_ptr); // Other Unices
       }

       UnixFile.rename =
         declareFFI("rename", ctypes.default_abi,
                    /*return*/ Types.negativeone_or_nothing,
                    /*old*/    Types.string,
                    /*new*/    Types.string);

       UnixFile.rmdir =
         declareFFI("rmdir", ctypes.default_abi,
                    /*return*/ Types.int,
                    /*path*/   Types.string);

       UnixFile.splice =
         declareFFI("splice", ctypes.default_abi,
                    /*return*/ Types.long,
                    /*fd_in*/  Types.fd,
                    /*off_in*/ Types.off_t.in_ptr,
                    /*fd_out*/ Types.fd,
                    /*off_out*/Types.off_t.in_ptr,
                    /*len*/    Types.size_t,
                    /*flags*/  Types.unsigned_int); // Linux/Android-specific

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

       // OSes use a variety of hacks to differentiate between
       // 32-bits and 64-bits versions of |stat|, |lstat|, |fstat|.
       if (OS.Constants.libc._DARWIN_FEATURE_64_BIT_INODE) {
         // MacOS X 64-bits
         UnixFile.stat =
           declareFFI("stat$INODE64", ctypes.default_abi,
                      /*return*/ Types.negativeone_or_nothing,
                      /*path*/   Types.string,
                      /*buf*/    Types.stat.out_ptr
                     );
         UnixFile.lstat =
           declareFFI("lstat$INODE64", ctypes.default_abi,
                      /*return*/ Types.negativeone_or_nothing,
                      /*path*/   Types.string,
                      /*buf*/    Types.stat.out_ptr
                     );
         UnixFile.fstat =
           declareFFI("fstat$INODE64", ctypes.default_abi,
                      /*return*/ Types.negativeone_or_nothing,
                      /*path*/   Types.fd,
                      /*buf*/    Types.stat.out_ptr
                     );
       } else if (OS.Constants.libc._STAT_VER != undefined) {
         const ver = OS.Constants.libc._STAT_VER;
         // Linux, all widths
         let xstat =
           declareFFI("__xstat", ctypes.default_abi,
                      /*return*/    Types.negativeone_or_nothing,
                      /*_stat_ver*/ Types.int,
                      /*path*/      Types.string,
                      /*buf*/       Types.stat.out_ptr);
         let lxstat =
           declareFFI("__lxstat", ctypes.default_abi,
                      /*return*/    Types.negativeone_or_nothing,
                      /*_stat_ver*/ Types.int,
                      /*path*/      Types.string,
                      /*buf*/       Types.stat.out_ptr);
         let fxstat =
           declareFFI("__fxstat", ctypes.default_abi,
                      /*return*/    Types.negativeone_or_nothing,
                      /*_stat_ver*/ Types.int,
                      /*fd*/        Types.fd,
                      /*buf*/       Types.stat.out_ptr);

         UnixFile.stat = function stat(path, buf) {
           return xstat(ver, path, buf);
         };
         UnixFile.lstat = function stat(path, buf) {
           return lxstat(ver, path, buf);
         };
         UnixFile.fstat = function stat(fd, buf) {
           return fxstat(ver, fd, buf);
         };
       } else {
         // Mac OS X 32-bits, other Unix
         UnixFile.stat =
           declareFFI("stat", ctypes.default_abi,
                      /*return*/ Types.negativeone_or_nothing,
                      /*path*/   Types.string,
                      /*buf*/    Types.stat.out_ptr
                     );
         UnixFile.lstat =
           declareFFI("lstat", ctypes.default_abi,
                      /*return*/ Types.negativeone_or_nothing,
                      /*path*/   Types.string,
                      /*buf*/    Types.stat.out_ptr
                     );
         UnixFile.fstat =
           declareFFI("fstat", ctypes.default_abi,
                      /*return*/ Types.negativeone_or_nothing,
                      /*fd*/     Types.fd,
                      /*buf*/    Types.stat.out_ptr
                     );
       }

       // We cannot make a C array of CDataFinalizer, so
       // pipe cannot be directly defined as a C function.

       let _pipe =
         libc.declare("pipe", ctypes.default_abi,
                    /*return*/ ctypes.int,
                    /*fds*/    ctypes.ArrayType(ctypes.int, 2));

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
     };
     exports.OS.Unix.File._init = init;
   })(this);
}
