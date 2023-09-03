/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/chrome-worker, node */
/* global OS */

// eslint-disable-next-line no-lone-blocks
{
  if (typeof Components != "undefined") {
    // We do not wish osfile_unix_back.js to be used directly as a main thread
    // module yet. When time comes, it will be loaded by a combination of
    // a main thread front-end/worker thread implementation that makes sure
    // that we are not executing synchronous IO code in the main thread.

    throw new Error(
      "osfile_unix_back.js cannot be used from the main thread yet"
    );
  }
  (function(exports) {
    "use strict";
    if (exports.OS && exports.OS.Unix && exports.OS.Unix.File) {
      return; // Avoid double initialization
    }

    let SharedAll = require("resource://gre/modules/osfile/osfile_shared_allthreads.jsm");
    let SysAll = require("resource://gre/modules/osfile/osfile_unix_allthreads.jsm");
    SharedAll.LOG.bind(SharedAll, "Unix", "back");
    let libc = SysAll.libc;
    let Const = SharedAll.Constants.libc;

    /**
     * Initialize the Unix module.
     *
     * @param {function=} declareFFI
     */
    // FIXME: Both |init| and |aDeclareFFI| are deprecated, we should remove them
    let init = function init(aDeclareFFI) {
      if (aDeclareFFI) {
        aDeclareFFI.bind(null, libc);
      } else {
        SysAll.declareFFI;
      }
      SharedAll.declareLazyFFI;

      // Initialize types that require additional OS-specific
      // support - either finalization or matching against
      // OS-specific constants.
      let Type = Object.create(SysAll.Type);
      let SysFile = (exports.OS.Unix.File = { Type });

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
      Type.negativeone_or_fd.importFromC = function importFromC(fd_int) {
        if (fd_int == -1) {
          return -1;
        }
        return ctypes.CDataFinalizer(fd_int, SysFile._close);
      };

      /**
       * A C integer holding -1 in case of error or a meaningless value
       * in case of success.
       */
      Type.negativeone_or_nothing = Type.int.withName("negativeone_or_nothing");

      /**
       * A C integer holding -1 in case of error or a positive integer
       * in case of success.
       */
      Type.negativeone_or_ssize_t = Type.ssize_t.withName(
        "negativeone_or_ssize_t"
      );

      /**
       * Various libc integer types
       */
      Type.mode_t = Type.intn_t(Const.OSFILE_SIZEOF_MODE_T).withName("mode_t");
      Type.uid_t = Type.intn_t(Const.OSFILE_SIZEOF_UID_T).withName("uid_t");
      Type.gid_t = Type.intn_t(Const.OSFILE_SIZEOF_GID_T).withName("gid_t");

      /**
       * Type |time_t|
       */
      Type.time_t = Type.intn_t(Const.OSFILE_SIZEOF_TIME_T).withName("time_t");

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

        let dirent = new SharedAll.HollowStructure(
          "dirent",
          Const.OSFILE_SIZEOF_DIRENT + d_name_extra_size
        );
        if (Const.OSFILE_OFFSETOF_DIRENT_D_TYPE != undefined) {
          // |dirent| doesn't have d_type on some platforms (e.g. Solaris).
          dirent.add_field_at(
            Const.OSFILE_OFFSETOF_DIRENT_D_TYPE,
            "d_type",
            ctypes.uint8_t
          );
        }
        dirent.add_field_at(
          Const.OSFILE_OFFSETOF_DIRENT_D_NAME,
          "d_name",
          ctypes.ArrayType(
            ctypes.char,
            Const.OSFILE_SIZEOF_DIRENT_D_NAME + d_name_extra_size
          )
        );

        // We now have built |dirent|.
        Type.dirent = dirent.getType();
      }
      Type.null_or_dirent_ptr = new SharedAll.Type(
        "null_of_dirent",
        Type.dirent.out_ptr.implementation
      );

      // Structure |stat|
      // Same technique
      {
        let stat = new SharedAll.HollowStructure(
          "stat",
          Const.OSFILE_SIZEOF_STAT
        );
        stat.add_field_at(
          Const.OSFILE_OFFSETOF_STAT_ST_MODE,
          "st_mode",
          Type.mode_t.implementation
        );
        stat.add_field_at(
          Const.OSFILE_OFFSETOF_STAT_ST_UID,
          "st_uid",
          Type.uid_t.implementation
        );
        stat.add_field_at(
          Const.OSFILE_OFFSETOF_STAT_ST_GID,
          "st_gid",
          Type.gid_t.implementation
        );

        // Here, things get complicated with different data structures.
        // Some platforms have |time_t st_atime| and some platforms have
        // |timespec st_atimespec|. However, since |timespec| starts with
        // a |time_t|, followed by nanoseconds, we just cheat and pretend
        // that everybody has |time_t st_atime|, possibly followed by padding
        stat.add_field_at(
          Const.OSFILE_OFFSETOF_STAT_ST_ATIME,
          "st_atime",
          Type.time_t.implementation
        );
        stat.add_field_at(
          Const.OSFILE_OFFSETOF_STAT_ST_MTIME,
          "st_mtime",
          Type.time_t.implementation
        );
        stat.add_field_at(
          Const.OSFILE_OFFSETOF_STAT_ST_CTIME,
          "st_ctime",
          Type.time_t.implementation
        );

        stat.add_field_at(
          Const.OSFILE_OFFSETOF_STAT_ST_SIZE,
          "st_size",
          Type.off_t.implementation
        );
        Type.stat = stat.getType();
      }

      // Structure |DIR|
      if ("OSFILE_SIZEOF_DIR" in Const) {
        // On platforms for which we need to access the fields of DIR
        // directly (e.g. because certain functions are implemented
        // as macros), we need to define DIR as a hollow structure.
        let DIR = new SharedAll.HollowStructure("DIR", Const.OSFILE_SIZEOF_DIR);

        DIR.add_field_at(
          Const.OSFILE_OFFSETOF_DIR_DD_FD,
          "dd_fd",
          Type.fd.implementation
        );

        Type.DIR = DIR.getType();
      } else {
        // On other platforms, we keep DIR as a blackbox
        Type.DIR = new SharedAll.Type("DIR", ctypes.StructType("DIR"));
      }

      Type.null_or_DIR_ptr = Type.DIR.out_ptr.withName("null_or_DIR*");
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
          Const.OSFILE_SIZEOF_TIMEVAL
        );
        timeval.add_field_at(
          Const.OSFILE_OFFSETOF_TIMEVAL_TV_SEC,
          "tv_sec",
          Type.long.implementation
        );
        timeval.add_field_at(
          Const.OSFILE_OFFSETOF_TIMEVAL_TV_USEC,
          "tv_usec",
          Type.long.implementation
        );
        Type.timeval = timeval.getType();
        Type.timevals = new SharedAll.Type(
          "two timevals",
          ctypes.ArrayType(Type.timeval.implementation, 2)
        );
      }

      // Types fsblkcnt_t and fsfilcnt_t, used by structure |statvfs|
      Type.fsblkcnt_t = Type.uintn_t(Const.OSFILE_SIZEOF_FSBLKCNT_T).withName(
        "fsblkcnt_t"
      );
      // There is no guarantee of the size or order of members in sys-header structs
      // It mostly is "unsigned long", but can be "unsigned int" as well.
      // So it has its own "type".
      // NOTE: This is still only partially correct, as signedness is also not guaranteed,
      //       so assuming an unsigned int might still be wrong here.
      //       But unsigned seems to have worked all those years, even though its signed
      //       on various platforms.
      Type.statvfs_f_frsize = Type.uintn_t(
        Const.OSFILE_SIZEOF_STATVFS_F_FRSIZE
      ).withName("statvfs_f_rsize");

      // Structure |statvfs|
      // Use an hollow structure
      {
        let statvfs = new SharedAll.HollowStructure(
          "statvfs",
          Const.OSFILE_SIZEOF_STATVFS
        );

        statvfs.add_field_at(
          Const.OSFILE_OFFSETOF_STATVFS_F_FRSIZE,
          "f_frsize",
          Type.statvfs_f_frsize.implementation
        );
        statvfs.add_field_at(
          Const.OSFILE_OFFSETOF_STATVFS_F_BAVAIL,
          "f_bavail",
          Type.fsblkcnt_t.implementation
        );

        Type.statvfs = statvfs.getType();
      }

      // Declare libc functions as functions of |OS.Unix.File|

      // Finalizer-related functions
      libc.declareLazy(
        SysFile,
        "_close",
        "close",
        ctypes.default_abi,
        /* return */ ctypes.int,
        ctypes.int
      );

      SysFile.close = function close(fd) {
        // Detach the finalizer and call |_close|.
        return fd.dispose();
      };

      libc.declareLazy(
        SysFile,
        "_close_dir",
        "closedir",
        ctypes.default_abi,
        /* return */ ctypes.int,
        Type.DIR.in_ptr.implementation
      );

      SysFile.closedir = function closedir(fd) {
        // Detach the finalizer and call |_close_dir|.
        return fd.dispose();
      };

      {
        // Symbol free() is special.
        // We override the definition of free() on several platforms.
        let default_lib = new SharedAll.Library("default_lib", "a.out");

        // On platforms for which we override free(), nspr defines
        // a special library name "a.out" that will resolve to the
        // correct implementation free().
        // If it turns out we don't have an a.out library or a.out
        // doesn't contain free, use the ordinary libc free.

        default_lib.declareLazyWithFallback(
          libc,
          SysFile,
          "free",
          "free",
          ctypes.default_abi,
          /* return*/ ctypes.void_t,
          ctypes.voidptr_t
        );
      }

      // Other functions
      libc.declareLazyFFI(
        SysFile,
        "access",
        "access",
        ctypes.default_abi,
        /* return*/ Type.negativeone_or_nothing,
        Type.path,
        Type.int
      );

      libc.declareLazyFFI(
        SysFile,
        "chmod",
        "chmod",
        ctypes.default_abi,
        /* return*/ Type.negativeone_or_nothing,
        Type.path,
        Type.mode_t
      );

      libc.declareLazyFFI(
        SysFile,
        "chown",
        "chown",
        ctypes.default_abi,
        /* return*/ Type.negativeone_or_nothing,
        Type.path,
        Type.uid_t,
        Type.gid_t
      );

      libc.declareLazyFFI(
        SysFile,
        "copyfile",
        "copyfile",
        ctypes.default_abi,
        /* return*/ Type.negativeone_or_nothing,
        /* source*/ Type.path,
        Type.path,
        Type.void_t.in_ptr,
        Type.uint32_t
      );

      libc.declareLazyFFI(
        SysFile,
        "dup",
        "dup",
        ctypes.default_abi,
        /* return*/ Type.negativeone_or_fd,
        Type.fd
      );

      if ("OSFILE_SIZEOF_DIR" in Const) {
        // On platforms for which |dirfd| is a macro
        SysFile.dirfd = function dirfd(DIRp) {
          return Type.DIR.in_ptr.implementation(DIRp).contents.dd_fd;
        };
      } else {
        // On platforms for which |dirfd| is a function
        libc.declareLazyFFI(
          SysFile,
          "dirfd",
          "dirfd",
          ctypes.default_abi,
          /* return*/ Type.negativeone_or_fd,
          Type.DIR.in_ptr
        );
      }

      libc.declareLazyFFI(
        SysFile,
        "chdir",
        "chdir",
        ctypes.default_abi,
        /* return*/ Type.negativeone_or_nothing,
        Type.path
      );

      libc.declareLazyFFI(
        SysFile,
        "fchdir",
        "fchdir",
        ctypes.default_abi,
        /* return*/ Type.negativeone_or_nothing,
        Type.fd
      );

      libc.declareLazyFFI(
        SysFile,
        "fchmod",
        "fchmod",
        ctypes.default_abi,
        /* return*/ Type.negativeone_or_nothing,
        Type.fd,
        Type.mode_t
      );

      libc.declareLazyFFI(
        SysFile,
        "fchown",
        "fchown",
        ctypes.default_abi,
        /* return*/ Type.negativeone_or_nothing,
        Type.fd,
        Type.uid_t,
        Type.gid_t
      );

      libc.declareLazyFFI(
        SysFile,
        "fsync",
        "fsync",
        ctypes.default_abi,
        /* return*/ Type.negativeone_or_nothing,
        Type.fd
      );

      libc.declareLazyFFI(
        SysFile,
        "getcwd",
        "getcwd",
        ctypes.default_abi,
        /* return*/ Type.out_path,
        Type.out_path,
        Type.size_t
      );

      libc.declareLazyFFI(
        SysFile,
        "getwd",
        "getwd",
        ctypes.default_abi,
        /* return*/ Type.out_path,
        Type.out_path
      );

      // Two variants of |getwd| which allocate the memory
      // dynamically.

      // Linux/Android version
      libc.declareLazyFFI(
        SysFile,
        "get_current_dir_name",
        "get_current_dir_name",
        ctypes.default_abi,
        /* return*/ Type.out_path.releaseWithLazy(() => SysFile.free)
      );

      // MacOS/BSD version (will return NULL on Linux/Android)
      libc.declareLazyFFI(
        SysFile,
        "getwd_auto",
        "getwd",
        ctypes.default_abi,
        /* return*/ Type.out_path.releaseWithLazy(() => SysFile.free),
        Type.void_t.out_ptr
      );

      if (OS.Constants.Sys.Name == "Darwin") {
        // At the time of writing we only need this on MacOS. If we generalize
        // this, be sure to do so with the other xattr functions also.
        libc.declareLazyFFI(
          SysFile,
          "getxattr",
          "getxattr",
          ctypes.default_abi,
          /* return*/ Type.int,
          Type.path,
          Type.cstring,
          Type.void_t.out_ptr,
          Type.size_t,
          Type.uint32_t,
          Type.int
        );
      }

      libc.declareLazyFFI(
        SysFile,
        "fdatasync",
        "fdatasync",
        ctypes.default_abi,
        /* return*/ Type.negativeone_or_nothing,
        Type.fd
      ); // Note: MacOS/BSD-specific

      libc.declareLazyFFI(
        SysFile,
        "ftruncate",
        "ftruncate",
        ctypes.default_abi,
        /* return*/ Type.negativeone_or_nothing,
        Type.fd,
        /* length*/ Type.off_t
      );

      libc.declareLazyFFI(
        SysFile,
        "lchown",
        "lchown",
        ctypes.default_abi,
        /* return*/ Type.negativeone_or_nothing,
        Type.path,
        Type.uid_t,
        Type.gid_t
      );

      libc.declareLazyFFI(
        SysFile,
        "link",
        "link",
        ctypes.default_abi,
        /* return*/ Type.negativeone_or_nothing,
        /* source*/ Type.path,
        Type.path
      );

      libc.declareLazyFFI(
        SysFile,
        "lseek",
        "lseek",
        ctypes.default_abi,
        /* return*/ Type.off_t,
        Type.fd,
        /* offset*/ Type.off_t,
        /* whence*/ Type.int
      );

      libc.declareLazyFFI(
        SysFile,
        "mkdir",
        "mkdir",
        ctypes.default_abi,
        /* return*/ Type.int,
        /* path*/ Type.path,
        /* mode*/ Type.int
      );

      libc.declareLazyFFI(
        SysFile,
        "mkstemp",
        "mkstemp",
        ctypes.default_abi,
        Type.fd,
        /* template*/ Type.out_path
      );

      libc.declareLazyFFI(
        SysFile,
        "open",
        "open",
        ctypes.default_abi,
        /* return*/ Type.negativeone_or_fd,
        Type.path,
        /* oflags*/ Type.int,
        "..."
      );

      if (OS.Constants.Sys.Name == "NetBSD") {
        libc.declareLazyFFI(
          SysFile,
          "opendir",
          "__opendir30",
          ctypes.default_abi,
          /* return*/ Type.null_or_DIR_ptr,
          Type.path
        );
      } else {
        libc.declareLazyFFI(
          SysFile,
          "opendir",
          "opendir",
          ctypes.default_abi,
          /* return*/ Type.null_or_DIR_ptr,
          Type.path
        );
      }

      libc.declareLazyFFI(
        SysFile,
        "pread",
        "pread",
        ctypes.default_abi,
        /* return*/ Type.negativeone_or_ssize_t,
        Type.fd,
        Type.void_t.out_ptr,
        /* nbytes*/ Type.size_t,
        /* offset*/ Type.off_t
      );

      libc.declareLazyFFI(
        SysFile,
        "pwrite",
        "pwrite",
        ctypes.default_abi,
        /* return*/ Type.negativeone_or_ssize_t,
        Type.fd,
        Type.void_t.in_ptr,
        /* nbytes*/ Type.size_t,
        /* offset*/ Type.off_t
      );

      libc.declareLazyFFI(
        SysFile,
        "read",
        "read",
        ctypes.default_abi,
        /* return*/ Type.negativeone_or_ssize_t,
        Type.fd,
        Type.void_t.out_ptr,
        /* nbytes*/ Type.size_t
      );

      libc.declareLazyFFI(
        SysFile,
        "posix_fadvise",
        "posix_fadvise",
        ctypes.default_abi,
        /* return*/ Type.int,
        Type.fd,
        /* offset*/ Type.off_t,
        Type.off_t,
        /* advise*/ Type.int
      );

      if (Const._DARWIN_INODE64_SYMBOLS) {
        // Special case for MacOS X 10.5+
        // Symbol name "readdir" still exists but is used for a
        // deprecated function that does not match the
        // constants of |Const|.
        libc.declareLazyFFI(
          SysFile,
          "readdir",
          "readdir$INODE64",
          ctypes.default_abi,
          /* return*/ Type.null_or_dirent_ptr,
          Type.DIR.in_ptr
        ); // For MacOS X
      } else if (OS.Constants.Sys.Name == "NetBSD") {
        libc.declareLazyFFI(
          SysFile,
          "readdir",
          "__readdir30",
          ctypes.default_abi,
          /* return*/ Type.null_or_dirent_ptr,
          Type.DIR.in_ptr
        ); // Other Unices
      } else {
        libc.declareLazyFFI(
          SysFile,
          "readdir",
          "readdir",
          ctypes.default_abi,
          /* return*/ Type.null_or_dirent_ptr,
          Type.DIR.in_ptr
        ); // Other Unices
      }

      if (OS.Constants.Sys.Name == "Darwin") {
        // At the time of writing we only need this on MacOS. If we generalize
        // this, be sure to do so with the other xattr functions also.
        libc.declareLazyFFI(
          SysFile,
          "removexattr",
          "removexattr",
          ctypes.default_abi,
          /* return*/ Type.negativeone_or_nothing,
          Type.path,
          Type.cstring,
          Type.int
        );
      }

      libc.declareLazyFFI(
        SysFile,
        "rename",
        "rename",
        ctypes.default_abi,
        /* return*/ Type.negativeone_or_nothing,
        Type.path,
        Type.path
      );

      libc.declareLazyFFI(
        SysFile,
        "rmdir",
        "rmdir",
        ctypes.default_abi,
        /* return*/ Type.int,
        Type.path
      );

      if (OS.Constants.Sys.Name == "Darwin") {
        // At the time of writing we only need this on MacOS. If we generalize
        // this, be sure to do so with the other xattr functions also.
        libc.declareLazyFFI(
          SysFile,
          "setxattr",
          "setxattr",
          ctypes.default_abi,
          /* return*/ Type.negativeone_or_nothing,
          Type.path,
          Type.cstring,
          Type.void_t.in_ptr,
          Type.size_t,
          Type.uint32_t,
          Type.int
        );
      }

      libc.declareLazyFFI(
        SysFile,
        "splice",
        "splice",
        ctypes.default_abi,
        /* return*/ Type.long,
        Type.fd,
        /* off_in*/ Type.off_t.in_ptr,
        /* fd_out*/ Type.fd,
        /* off_out*/ Type.off_t.in_ptr,
        Type.size_t,
        Type.unsigned_int
      ); // Linux/Android-specific

      libc.declareLazyFFI(
        SysFile,
        "statfs",
        "statfs",
        ctypes.default_abi,
        /* return*/ Type.negativeone_or_nothing,
        Type.path,
        Type.statvfs.out_ptr
      ); // Android,B2G

      libc.declareLazyFFI(
        SysFile,
        "statvfs",
        "statvfs",
        ctypes.default_abi,
        /* return*/ Type.negativeone_or_nothing,
        Type.path,
        Type.statvfs.out_ptr
      ); // Other platforms

      libc.declareLazyFFI(
        SysFile,
        "symlink",
        "symlink",
        ctypes.default_abi,
        /* return*/ Type.negativeone_or_nothing,
        /* source*/ Type.path,
        Type.path
      );

      libc.declareLazyFFI(
        SysFile,
        "truncate",
        "truncate",
        ctypes.default_abi,
        /* return*/ Type.negativeone_or_nothing,
        Type.path,
        /* length*/ Type.off_t
      );

      libc.declareLazyFFI(
        SysFile,
        "unlink",
        "unlink",
        ctypes.default_abi,
        /* return */ Type.negativeone_or_nothing,
        /* path */ Type.path
      );

      libc.declareLazyFFI(
        SysFile,
        "write",
        "write",
        ctypes.default_abi,
        /* return */ Type.negativeone_or_ssize_t,
        /* fd */ Type.fd,
        /* buf */ Type.void_t.in_ptr,
        /* nbytes */ Type.size_t
      );

      // Weird cases that require special treatment

      // OSes use a variety of hacks to differentiate between
      // 32-bits and 64-bits versions of |stat|, |lstat|, |fstat|.
      if (Const._DARWIN_INODE64_SYMBOLS) {
        // MacOS X 64-bits
        libc.declareLazyFFI(
          SysFile,
          "stat",
          "stat$INODE64",
          ctypes.default_abi,
          /* return */ Type.negativeone_or_nothing,
          /* path */ Type.path,
          /* buf */ Type.stat.out_ptr
        );
        libc.declareLazyFFI(
          SysFile,
          "lstat",
          "lstat$INODE64",
          ctypes.default_abi,
          /* return */ Type.negativeone_or_nothing,
          /* path */ Type.path,
          /* buf */ Type.stat.out_ptr
        );
        libc.declareLazyFFI(
          SysFile,
          "fstat",
          "fstat$INODE64",
          ctypes.default_abi,
          /* return */ Type.negativeone_or_nothing,
          /* path */ Type.fd,
          /* buf */ Type.stat.out_ptr
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
        libc.declareLazyFFI(
          Stat,
          "xstat",
          xstat_name,
          ctypes.default_abi,
          /* return */ Type.negativeone_or_nothing,
          /* _stat_ver */ Type.int,
          /* path */ Type.path,
          /* buf */ Type.stat.out_ptr
        );
        libc.declareLazyFFI(
          Stat,
          "lxstat",
          lxstat_name,
          ctypes.default_abi,
          /* return */ Type.negativeone_or_nothing,
          /* _stat_ver */ Type.int,
          /* path */ Type.path,
          /* buf */ Type.stat.out_ptr
        );
        libc.declareLazyFFI(
          Stat,
          "fxstat",
          fxstat_name,
          ctypes.default_abi,
          /* return */ Type.negativeone_or_nothing,
          /* _stat_ver */ Type.int,
          /* fd */ Type.fd,
          /* buf */ Type.stat.out_ptr
        );

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
        libc.declareLazyFFI(
          SysFile,
          "stat",
          "__stat50",
          ctypes.default_abi,
          /* return */ Type.negativeone_or_nothing,
          /* path */ Type.path,
          /* buf */ Type.stat.out_ptr
        );
        libc.declareLazyFFI(
          SysFile,
          "lstat",
          "__lstat50",
          ctypes.default_abi,
          /* return */ Type.negativeone_or_nothing,
          /* path */ Type.path,
          /* buf */ Type.stat.out_ptr
        );
        libc.declareLazyFFI(
          SysFile,
          "fstat",
          "__fstat50",
          ctypes.default_abi,
          /* return */ Type.negativeone_or_nothing,
          /* fd */ Type.fd,
          /* buf */ Type.stat.out_ptr
        );
      } else {
        // Mac OS X 32-bits, other Unix
        libc.declareLazyFFI(
          SysFile,
          "stat",
          "stat",
          ctypes.default_abi,
          /* return */ Type.negativeone_or_nothing,
          /* path */ Type.path,
          /* buf */ Type.stat.out_ptr
        );
        libc.declareLazyFFI(
          SysFile,
          "lstat",
          "lstat",
          ctypes.default_abi,
          /* return */ Type.negativeone_or_nothing,
          /* path */ Type.path,
          /* buf */ Type.stat.out_ptr
        );
        libc.declareLazyFFI(
          SysFile,
          "fstat",
          "fstat",
          ctypes.default_abi,
          /* return */ Type.negativeone_or_nothing,
          /* fd */ Type.fd,
          /* buf */ Type.stat.out_ptr
        );
      }

      // We cannot make a C array of CDataFinalizer, so
      // pipe cannot be directly defined as a C function.

      let Pipe = {};
      libc.declareLazyFFI(
        Pipe,
        "_pipe",
        "pipe",
        ctypes.default_abi,
        /* return */ Type.negativeone_or_nothing,
        /* fds */ new SharedAll.Type(
          "two file descriptors",
          ctypes.ArrayType(ctypes.int, 2)
        )
      );

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
        libc.declareLazyFFI(
          SysFile,
          "utimes",
          "__utimes50",
          ctypes.default_abi,
          /* return */ Type.negativeone_or_nothing,
          /* path */ Type.path,
          /* timeval[2] */ Type.timevals.out_ptr
        );
      } else {
        libc.declareLazyFFI(
          SysFile,
          "utimes",
          "utimes",
          ctypes.default_abi,
          /* return */ Type.negativeone_or_nothing,
          /* path */ Type.path,
          /* timeval[2] */ Type.timevals.out_ptr
        );
      }
      if (OS.Constants.Sys.Name == "NetBSD") {
        libc.declareLazyFFI(
          SysFile,
          "futimes",
          "__futimes50",
          ctypes.default_abi,
          /* return */ Type.negativeone_or_nothing,
          /* fd */ Type.fd,
          /* timeval[2] */ Type.timevals.out_ptr
        );
      } else {
        libc.declareLazyFFI(
          SysFile,
          "futimes",
          "futimes",
          ctypes.default_abi,
          /* return */ Type.negativeone_or_nothing,
          /* fd */ Type.fd,
          /* timeval[2] */ Type.timevals.out_ptr
        );
      }
    };

    exports.OS.Unix = {
      File: {
        _init: init,
      },
    };
  })(this);
}
