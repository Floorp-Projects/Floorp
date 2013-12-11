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
  (function(exports) {
     "use strict";
     if (exports.OS && exports.OS.Unix && exports.OS.Unix.File) {
       return; // Avoid double initialization
     }

     let SharedAll =
       require("resource://gre/modules/osfile/osfile_shared_allthreads.jsm");
     let SysAll =
       require("resource://gre/modules/osfile/osfile_unix_allthreads.jsm");
     let LOG = SharedAll.LOG.bind(SharedAll, "Unix", "back");
     let libc = SysAll.libc;
     let Const = SharedAll.Constants.libc;

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
         declareFFI = SysAll.declareFFI;
       }
       let declareLazyFFI = SharedAll.declareLazyFFI;

       // Initialize types that require additional OS-specific
       // support - either finalization or matching against
       // OS-specific constants.
       let Type = Object.create(SysAll.Type);
       let SysFile = exports.OS.Unix.File = { Type: Type };

       /**
        * A file descriptor.
        */
       Type.fd = Type.int.withName("fd");
       Type.fd.importFromC = function importFromC(fd_int) {
         return ctypes.CDataFinalizer(fd_int, SysFile._close);
       };


       /**
        * A C integer holding -1 in case of error or a file descriptor
        * in case of success.
        */
       Type.negativeone_or_fd = Type.fd.withName("negativeone_or_fd");
       Type.negativeone_or_fd.importFromC =
         function importFromC(fd_int) {
           if (fd_int == -1) {
             return -1;
           }
           return ctypes.CDataFinalizer(fd_int, SysFile._close);
         };

       /**
        * A C integer holding -1 in case of error or a meaningless value
        * in case of success.
        */
       Type.negativeone_or_nothing =
         Type.int.withName("negativeone_or_nothing");

       /**
        * A C integer holding -1 in case of error or a positive integer
        * in case of success.
        */
       Type.negativeone_or_ssize_t =
         Type.ssize_t.withName("negativeone_or_ssize_t");

       /**
        * Various libc integer types
        */
       Type.mode_t =
         Type.intn_t(Const.OSFILE_SIZEOF_MODE_T).withName("mode_t");
       Type.uid_t =
         Type.intn_t(Const.OSFILE_SIZEOF_UID_T).withName("uid_t");
       Type.gid_t =
         Type.intn_t(Const.OSFILE_SIZEOF_GID_T).withName("gid_t");

       /**
        * Type |time_t|
        */
       Type.time_t =
         Type.intn_t(Const.OSFILE_SIZEOF_TIME_T).withName("time_t");

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
         let d_name_extra_size = 0;
         if (Const.OSFILE_SIZEOF_DIRENT_D_NAME < 8) {
           // d_name is defined like "char d_name[1];" on some platforms
           // (e.g. Solaris), we need to give it more size for our structure.
           d_name_extra_size = 256;
         }

         let dirent = new SharedAll.HollowStructure("dirent",
           Const.OSFILE_SIZEOF_DIRENT + d_name_extra_size);
         if (Const.OSFILE_OFFSETOF_DIRENT_D_TYPE != undefined) {
           // |dirent| doesn't have d_type on some platforms (e.g. Solaris).
           dirent.add_field_at(Const.OSFILE_OFFSETOF_DIRENT_D_TYPE,
             "d_type", ctypes.uint8_t);
         }
         dirent.add_field_at(Const.OSFILE_OFFSETOF_DIRENT_D_NAME,
           "d_name", ctypes.ArrayType(ctypes.char,
             Const.OSFILE_SIZEOF_DIRENT_D_NAME + d_name_extra_size));

         // We now have built |dirent|.
         Type.dirent = dirent.getType();
       }
       Type.null_or_dirent_ptr =
         new SharedAll.Type("null_of_dirent",
                  Type.dirent.out_ptr.implementation);

       // Structure |stat|
       // Same technique
       {
         let stat = new SharedAll.HollowStructure("stat",
           Const.OSFILE_SIZEOF_STAT);
         stat.add_field_at(Const.OSFILE_OFFSETOF_STAT_ST_MODE,
                        "st_mode", Type.mode_t.implementation);
         stat.add_field_at(Const.OSFILE_OFFSETOF_STAT_ST_UID,
                          "st_uid", Type.uid_t.implementation);
         stat.add_field_at(Const.OSFILE_OFFSETOF_STAT_ST_GID,
                          "st_gid", Type.gid_t.implementation);

         // Here, things get complicated with different data structures.
         // Some platforms have |time_t st_atime| and some platforms have
         // |timespec st_atimespec|. However, since |timespec| starts with
         // a |time_t|, followed by nanoseconds, we just cheat and pretend
         // that everybody has |time_t st_atime|, possibly followed by padding
         stat.add_field_at(Const.OSFILE_OFFSETOF_STAT_ST_ATIME,
                          "st_atime", Type.time_t.implementation);
         stat.add_field_at(Const.OSFILE_OFFSETOF_STAT_ST_MTIME,
                          "st_mtime", Type.time_t.implementation);
         stat.add_field_at(Const.OSFILE_OFFSETOF_STAT_ST_CTIME,
                          "st_ctime", Type.time_t.implementation);

         // To complicate further, MacOS and some BSDs have a field |birthtime|
         if ("OSFILE_OFFSETOF_STAT_ST_BIRTHTIME" in Const) {
           stat.add_field_at(Const.OSFILE_OFFSETOF_STAT_ST_BIRTHTIME,
                             "st_birthtime", Type.time_t.implementation);
         }

         stat.add_field_at(Const.OSFILE_OFFSETOF_STAT_ST_SIZE,
                        "st_size", Type.off_t.implementation);
         Type.stat = stat.getType();
       }

       // Structure |DIR|
       if ("OSFILE_SIZEOF_DIR" in Const) {
         // On platforms for which we need to access the fields of DIR
         // directly (e.g. because certain functions are implemented
         // as macros), we need to define DIR as a hollow structure.
         let DIR = new SharedAll.HollowStructure(
           "DIR",
           Const.OSFILE_SIZEOF_DIR);

         DIR.add_field_at(
           Const.OSFILE_OFFSETOF_DIR_DD_FD,
           "dd_fd",
           Type.fd.implementation);

         Type.DIR = DIR.getType();
       } else {
         // On other platforms, we keep DIR as a blackbox
         Type.DIR =
           new SharedAll.Type("DIR",
             ctypes.StructType("DIR"));
       }

       Type.null_or_DIR_ptr =
         Type.DIR.out_ptr.withName("null_or_DIR*");
       Type.null_or_DIR_ptr.importFromC = function importFromC(dir) {
         if (dir == null || dir.isNull()) {
           return null;
         }
         return ctypes.CDataFinalizer(dir, SysFile._close_dir);
       };

       // Structure |timeval|
       {
         let timeval = new SharedAll.HollowStructure(
           "timeval",
           Const.OSFILE_SIZEOF_TIMEVAL);
         timeval.add_field_at(
           Const.OSFILE_OFFSETOF_TIMEVAL_TV_SEC,
           "tv_sec",
           Type.long.implementation);
         timeval.add_field_at(
           Const.OSFILE_OFFSETOF_TIMEVAL_TV_USEC,
           "tv_usec",
           Type.long.implementation);
         Type.timeval = timeval.getType();
         Type.timevals = new SharedAll.Type("two timevals",
           ctypes.ArrayType(Type.timeval.implementation, 2));
       }

       // Declare libc functions as functions of |OS.Unix.File|

       // Finalizer-related functions
       SharedAll.declareLazy(SysFile, "_close", libc,
           "close", ctypes.default_abi,
                        /*return */ctypes.int,
                        /*fd*/     ctypes.int);

       SysFile.close = function close(fd) {
         // Detach the finalizer and call |_close|.
         return fd.dispose();
       };

       SharedAll.declareLazy(SysFile, "_close_dir", libc,
                             "closedir", ctypes.default_abi,
                        /*return */ctypes.int,
                        /*dirp*/   Type.DIR.in_ptr.implementation);

       SysFile.closedir = function closedir(fd) {
         // Detach the finalizer and call |_close_dir|.
         return fd.dispose();
       };

       {
         // Symbol free() is special.
         // We override the definition of free() on several platforms.
         let default_lib = libc;
         try {
           // On platforms for which we override free(), nspr defines
           // a special library name "a.out" that will resolve to the
           // correct implementation free().
           default_lib = ctypes.open("a.out");

           SharedAll.declareLazy(SysFile, "free", default_lib,
             "free", ctypes.default_abi,
             /*return*/ ctypes.void_t,
             /*ptr*/    ctypes.voidptr_t);

         } catch (ex) {
           // We don't have an a.out library or a.out doesn't contain free.
           // Either way, use the ordinary libc free.

           SharedAll.declareLazy(SysFile, "free", libc,
             "free", ctypes.default_abi,
             /*return*/ ctypes.void_t,
             /*ptr*/    ctypes.voidptr_t);
         }
      }


       // Other functions
       declareLazyFFI(SysFile,  "access", libc, "access", ctypes.default_abi,
                    /*return*/ Type.negativeone_or_nothing,
                    /*path*/   Type.path,
                    /*mode*/   Type.int);

       declareLazyFFI(SysFile,  "chdir", libc, "chdir", ctypes.default_abi,
                    /*return*/ Type.negativeone_or_nothing,
                    /*path*/   Type.path);

       declareLazyFFI(SysFile,  "chmod", libc, "chmod", ctypes.default_abi,
                    /*return*/ Type.negativeone_or_nothing,
                    /*path*/   Type.path,
                    /*mode*/   Type.mode_t);

       declareLazyFFI(SysFile,  "chown", libc, "chown", ctypes.default_abi,
                    /*return*/ Type.negativeone_or_nothing,
                    /*path*/   Type.path,
                    /*uid*/    Type.uid_t,
                    /*gid*/    Type.gid_t);

       declareLazyFFI(SysFile,  "copyfile", libc, "copyfile", ctypes.default_abi,
                    /*return*/ Type.negativeone_or_nothing,
                    /*source*/ Type.path,
                    /*dest*/   Type.path,
                    /*state*/  Type.void_t.in_ptr, // Ignored atm
                    /*flags*/  Type.uint32_t);

       declareLazyFFI(SysFile,  "dup", libc, "dup", ctypes.default_abi,
                    /*return*/ Type.negativeone_or_fd,
                    /*fd*/     Type.fd);

       if ("OSFILE_SIZEOF_DIR" in Const) {
         // On platforms for which |dirfd| is a macro
         SysFile.dirfd =
           function dirfd(DIRp) {
             return Type.DIR.in_ptr.implementation(DIRp).contents.dd_fd;
           };
       } else {
         // On platforms for which |dirfd| is a function
         declareLazyFFI(SysFile,  "dirfd", libc, "dirfd", ctypes.default_abi,
                      /*return*/ Type.negativeone_or_fd,
                      /*dir*/    Type.DIR.in_ptr);
       }

       declareLazyFFI(SysFile,  "chdir", libc, "chdir", ctypes.default_abi,
                    /*return*/ Type.negativeone_or_nothing,
                    /*path*/   Type.path);

       declareLazyFFI(SysFile,  "fchdir", libc, "fchdir", ctypes.default_abi,
                    /*return*/ Type.negativeone_or_nothing,
                    /*fd*/     Type.fd);

       declareLazyFFI(SysFile,  "fchown", libc, "fchown", ctypes.default_abi,
                    /*return*/ Type.negativeone_or_nothing,
                    /*fd*/     Type.fd,
                    /*uid_t*/  Type.uid_t,
                    /*gid_t*/  Type.gid_t);

       declareLazyFFI(SysFile,  "fsync", libc, "fsync", ctypes.default_abi,
                    /*return*/ Type.negativeone_or_nothing,
                    /*fd*/     Type.fd);

       declareLazyFFI(SysFile,  "getcwd", libc, "getcwd", ctypes.default_abi,
                    /*return*/ Type.out_path,
                    /*buf*/    Type.out_path,
                    /*size*/   Type.size_t);

       declareLazyFFI(SysFile,  "getwd", libc, "getwd", ctypes.default_abi,
                    /*return*/ Type.out_path,
                    /*buf*/    Type.out_path);

       // Two variants of |getwd| which allocate the memory
       // dynamically.

       // Linux/Android version
       declareLazyFFI(SysFile,  "get_current_dir_name", libc,
                      "get_current_dir_name", ctypes.default_abi,
                      /*return*/ Type.out_path.releaseWith(SysFile.free));

       // MacOS/BSD version (will return NULL on Linux/Android)
       declareLazyFFI(SysFile,  "getwd_auto", libc,
                      "getwd", ctypes.default_abi,
                      /*return*/ Type.out_path.releaseWith(SysFile.free),
                      /*buf*/    Type.void_t.out_ptr);

       declareLazyFFI(SysFile,  "fdatasync", libc,
                      "fdatasync", ctypes.default_abi,
                    /*return*/ Type.negativeone_or_nothing,
                    /*fd*/     Type.fd); // Note: MacOS/BSD-specific

       declareLazyFFI(SysFile,  "ftruncate", libc,
                      "ftruncate", ctypes.default_abi,
                    /*return*/ Type.negativeone_or_nothing,
                    /*fd*/     Type.fd,
                    /*length*/ Type.off_t);


       declareLazyFFI(SysFile,  "lchown", libc, "lchown", ctypes.default_abi,
                    /*return*/ Type.negativeone_or_nothing,
                    /*path*/   Type.path,
                    /*uid_t*/  Type.uid_t,
                    /*gid_t*/  Type.gid_t);

       declareLazyFFI(SysFile,  "link", libc, "link", ctypes.default_abi,
                    /*return*/ Type.negativeone_or_nothing,
                    /*source*/ Type.path,
                    /*dest*/   Type.path);

       declareLazyFFI(SysFile,  "lseek", libc, "lseek", ctypes.default_abi,
                    /*return*/ Type.off_t,
                    /*fd*/     Type.fd,
                    /*offset*/ Type.off_t,
                    /*whence*/ Type.int);

       declareLazyFFI(SysFile,  "mkdir", libc, "mkdir", ctypes.default_abi,
                    /*return*/ Type.int,
                    /*path*/ Type.path,
                    /*mode*/ Type.int);

       declareLazyFFI(SysFile,  "mkstemp", libc, "mkstemp", ctypes.default_abi,
                    /*return*/ Type.fd,
                    /*template*/Type.out_path);

       declareLazyFFI(SysFile,  "open", libc, "open", ctypes.default_abi,
                    /*return*/Type.negativeone_or_fd,
                    /*path*/  Type.path,
                    /*oflags*/Type.int,
                    /*mode*/  Type.int);

       if (OS.Constants.Sys.Name == "NetBSD") {
          declareLazyFFI(SysFile,  "opendir", libc, "__opendir30", ctypes.default_abi,
                   /*return*/ Type.null_or_DIR_ptr,
                   /*path*/   Type.path);
       } else {
          declareLazyFFI(SysFile,  "opendir", libc, "opendir", ctypes.default_abi,
                    /*return*/ Type.null_or_DIR_ptr,
                    /*path*/   Type.path);
       }

       declareLazyFFI(SysFile,  "pread", libc, "pread", ctypes.default_abi,
                    /*return*/ Type.negativeone_or_ssize_t,
                    /*fd*/     Type.fd,
                    /*buf*/    Type.void_t.out_ptr,
                    /*nbytes*/ Type.size_t,
                    /*offset*/ Type.off_t);

       declareLazyFFI(SysFile,  "pwrite", libc, "pwrite", ctypes.default_abi,
                    /*return*/ Type.negativeone_or_ssize_t,
                    /*fd*/     Type.fd,
                    /*buf*/    Type.void_t.in_ptr,
                    /*nbytes*/ Type.size_t,
                    /*offset*/ Type.off_t);

       declareLazyFFI(SysFile,  "read", libc, "read", ctypes.default_abi,
                    /*return*/Type.negativeone_or_ssize_t,
                    /*fd*/    Type.fd,
                    /*buf*/   Type.void_t.out_ptr,
                    /*nbytes*/Type.size_t);

       declareLazyFFI(SysFile,  "posix_fadvise", libc, "posix_fadvise", ctypes.default_abi,
                    /*return*/ Type.int,
                    /*fd*/     Type.fd,
                    /*offset*/ Type.off_t,
                    /*len*/    Type.off_t,
                    /*advise*/ Type.int);

       if (Const._DARWIN_FEATURE_64_BIT_INODE) {
         // Special case for MacOS X 10.5+
         // Symbol name "readdir" still exists but is used for a
         // deprecated function that does not match the
         // constants of |Const|.
         declareLazyFFI(SysFile,  "readdir", libc, "readdir$INODE64", ctypes.default_abi,
                     /*return*/Type.null_or_dirent_ptr,
                      /*dir*/   Type.DIR.in_ptr); // For MacOS X
       } else if (OS.Constants.Sys.Name == "NetBSD") {
         declareLazyFFI(SysFile,  "readdir", libc, "__readdir30", ctypes.default_abi,
                      /*return*/Type.null_or_dirent_ptr,
                      /*dir*/   Type.DIR.in_ptr); // Other Unices
       } else {
         declareLazyFFI(SysFile,  "readdir", libc, "readdir", ctypes.default_abi,
                      /*return*/Type.null_or_dirent_ptr,
                      /*dir*/   Type.DIR.in_ptr); // Other Unices
       }

       declareLazyFFI(SysFile,  "rename", libc, "rename", ctypes.default_abi,
                    /*return*/ Type.negativeone_or_nothing,
                    /*old*/    Type.path,
                    /*new*/    Type.path);

       declareLazyFFI(SysFile,  "rmdir", libc, "rmdir", ctypes.default_abi,
                    /*return*/ Type.int,
                    /*path*/   Type.path);

       declareLazyFFI(SysFile,  "splice", libc, "splice", ctypes.default_abi,
                    /*return*/ Type.long,
                    /*fd_in*/  Type.fd,
                    /*off_in*/ Type.off_t.in_ptr,
                    /*fd_out*/ Type.fd,
                    /*off_out*/Type.off_t.in_ptr,
                    /*len*/    Type.size_t,
                    /*flags*/  Type.unsigned_int); // Linux/Android-specific

       declareLazyFFI(SysFile,  "symlink", libc, "symlink", ctypes.default_abi,
                    /*return*/ Type.negativeone_or_nothing,
                    /*source*/ Type.path,
                    /*dest*/   Type.path);

       declareLazyFFI(SysFile,  "truncate", libc, "truncate", ctypes.default_abi,
                    /*return*/Type.negativeone_or_nothing,
                    /*path*/  Type.path,
                    /*length*/ Type.off_t);

       declareLazyFFI(SysFile,  "unlink", libc, "unlink", ctypes.default_abi,
                    /*return*/ Type.negativeone_or_nothing,
                    /*path*/ Type.path);

       declareLazyFFI(SysFile,  "write", libc, "write", ctypes.default_abi,
                    /*return*/ Type.negativeone_or_ssize_t,
                    /*fd*/     Type.fd,
                    /*buf*/    Type.void_t.in_ptr,
                    /*nbytes*/ Type.size_t);

       // Weird cases that require special treatment

       // OSes use a variety of hacks to differentiate between
       // 32-bits and 64-bits versions of |stat|, |lstat|, |fstat|.
       if (Const._DARWIN_FEATURE_64_BIT_INODE) {
         // MacOS X 64-bits
         declareLazyFFI(SysFile,  "stat", libc, "stat$INODE64", ctypes.default_abi,
                      /*return*/ Type.negativeone_or_nothing,
                      /*path*/   Type.path,
                      /*buf*/    Type.stat.out_ptr
                     );
         declareLazyFFI(SysFile,  "lstat", libc, "lstat$INODE64", ctypes.default_abi,
                      /*return*/ Type.negativeone_or_nothing,
                      /*path*/   Type.path,
                      /*buf*/    Type.stat.out_ptr
                     );
         declareLazyFFI(SysFile,  "fstat", libc, "fstat$INODE64", ctypes.default_abi,
                      /*return*/ Type.negativeone_or_nothing,
                      /*path*/   Type.fd,
                      /*buf*/    Type.stat.out_ptr
                     );
       } else if (Const._STAT_VER != undefined) {
         const ver = Const._STAT_VER;
         let xstat_name, lxstat_name, fxstat_name;
         if (OS.Constants.Sys.Name == "SunOS") {
           // Solaris
           xstat_name = "_xstat";
           lxstat_name = "_lxstat";
           fxstat_name = "_fxstat";
         } else {
           // Linux, all widths
           xstat_name = "__xstat";
           lxstat_name = "__lxstat";
           fxstat_name = "__fxstat";
         }

         let Stat = {};
         declareLazyFFI(Stat,  "xstat", libc, xstat_name, ctypes.default_abi,
                      /*return*/    Type.negativeone_or_nothing,
                      /*_stat_ver*/ Type.int,
                      /*path*/      Type.path,
                      /*buf*/       Type.stat.out_ptr);
         declareLazyFFI(Stat,  "lxstat", libc, lxstat_name, ctypes.default_abi,
                      /*return*/    Type.negativeone_or_nothing,
                      /*_stat_ver*/ Type.int,
                      /*path*/      Type.path,
                      /*buf*/       Type.stat.out_ptr);
         declareLazyFFI(Stat, "fxstat", libc,
           fxstat_name, ctypes.default_abi,
                      /*return*/    Type.negativeone_or_nothing,
                      /*_stat_ver*/ Type.int,
                      /*fd*/        Type.fd,
                      /*buf*/       Type.stat.out_ptr);

         
         SysFile.stat = function stat(path, buf) {
           return Stat.xstat(ver, path, buf);
         };

         SysFile.lstat = function lstat(path, buf) {
           return Stat.lxstat(ver, path, buf);
         };

         SysFile.fstat = function fstat(fd, buf) {
           return Stat.fxstat(ver, fd, buf);
         };
       } else if (OS.Constants.Sys.Name == "NetBSD") {
         // NetBSD 5.0 and newer
         declareLazyFFI(SysFile,  "stat", libc, "__stat50", ctypes.default_abi,
                      /*return*/ Type.negativeone_or_nothing,
                      /*path*/   Type.path,
                      /*buf*/    Type.stat.out_ptr
                     );
         declareLazyFFI(SysFile,  "lstat", libc, "__lstat50", ctypes.default_abi,
                      /*return*/ Type.negativeone_or_nothing,
                      /*path*/   Type.path,
                      /*buf*/    Type.stat.out_ptr
                     );
         declareLazyFFI(SysFile,  "fstat", libc, "__fstat50", ctypes.default_abi,
                      /*return*/ Type.negativeone_or_nothing,
                      /*fd*/     Type.fd,
                      /*buf*/    Type.stat.out_ptr
                     );
       } else {
         // Mac OS X 32-bits, other Unix
         declareLazyFFI(SysFile,  "stat", libc, "stat", ctypes.default_abi,
                      /*return*/ Type.negativeone_or_nothing,
                      /*path*/   Type.path,
                      /*buf*/    Type.stat.out_ptr
                     );
         declareLazyFFI(SysFile,  "lstat", libc, "lstat", ctypes.default_abi,
                      /*return*/ Type.negativeone_or_nothing,
                      /*path*/   Type.path,
                      /*buf*/    Type.stat.out_ptr
                     );
         declareLazyFFI(SysFile,  "fstat", libc, "fstat", ctypes.default_abi,
                      /*return*/ Type.negativeone_or_nothing,
                      /*fd*/     Type.fd,
                      /*buf*/    Type.stat.out_ptr
                     );
       }

       // We cannot make a C array of CDataFinalizer, so
       // pipe cannot be directly defined as a C function.

       let Pipe = {};
       declareLazyFFI(Pipe, "_pipe", libc,
         "pipe", ctypes.default_abi,
         /*return*/ Type.negativeone_or_nothing,
         /*fds*/    new SharedAll.Type("two file descriptors",
             ctypes.ArrayType(ctypes.int, 2)));

       // A shared per-thread buffer used to communicate with |pipe|
       let _pipebuf = new (ctypes.ArrayType(ctypes.int, 2))();

       SysFile.pipe = function pipe(array) {
         let result = Pipe._pipe(_pipebuf);
         if (result == -1) {
           return result;
         }
         array[0] = ctypes.CDataFinalizer(_pipebuf[0], SysFile._close);
         array[1] = ctypes.CDataFinalizer(_pipebuf[1], SysFile._close);
         return result;
       };

       if (OS.Constants.Sys.Name == "NetBSD") {
           declareLazyFFI(SysFile, "utimes", libc, "__utimes50", ctypes.default_abi,
                      /*return*/     Type.negativeone_or_nothing,
                      /*path*/       Type.path,
                      /*timeval[2]*/ Type.timevals.out_ptr
                      );
       } else {
           declareLazyFFI(SysFile, "utimes", libc, "utimes", ctypes.default_abi,
                      /*return*/     Type.negativeone_or_nothing,
                      /*path*/       Type.path,
                      /*timeval[2]*/ Type.timevals.out_ptr
                      );
       }
       if (OS.Constants.Sys.Name == "NetBSD") {
           declareLazyFFI(SysFile, "futimes", libc, "__futimes50", ctypes.default_abi,
                      /*return*/     Type.negativeone_or_nothing,
                      /*fd*/         Type.fd,
                      /*timeval[2]*/ Type.timevals.out_ptr
                      );
       } else {
           declareLazyFFI(SysFile, "futimes", libc, "futimes", ctypes.default_abi,
                      /*return*/     Type.negativeone_or_nothing,
                      /*fd*/         Type.fd,
                      /*timeval[2]*/ Type.timevals.out_ptr
                      );
       }
     };

     exports.OS.Unix = {
       File: {
         _init: init
       }
     };
   })(this);
}
