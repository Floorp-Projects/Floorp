/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Synchronous front-end for the JavaScript OS.File library.
 * Unix implementation.
 *
 * This front-end is meant to be imported by a worker thread.
 */

/* eslint-env mozilla/chrome-worker, node */
/* global OS */

// eslint-disable-next-line no-lone-blocks
{
  if (typeof Components != "undefined") {
    // We do not wish osfile_unix_front.js to be used directly as a main thread
    // module yet.

    throw new Error(
      "osfile_unix_front.js cannot be used from the main thread yet"
    );
  }
  (function(exports) {
    "use strict";

    // exports.OS.Unix is created by osfile_unix_back.js
    if (exports.OS && exports.OS.File) {
      return; // Avoid double-initialization
    }

    let SharedAll = require("resource://gre/modules/osfile/osfile_shared_allthreads.jsm");
    let Path = require("resource://gre/modules/osfile/ospath.jsm");
    let SysAll = require("resource://gre/modules/osfile/osfile_unix_allthreads.jsm");
    exports.OS.Unix.File._init();
    SharedAll.LOG.bind(SharedAll, "Unix front-end");
    let Const = SharedAll.Constants.libc;
    let UnixFile = exports.OS.Unix.File;
    let Type = UnixFile.Type;

    /**
     * Representation of a file.
     *
     * You generally do not need to call this constructor yourself. Rather,
     * to open a file, use function |OS.File.open|.
     *
     * @param fd A OS-specific file descriptor.
     * @param {string} path File path of the file handle, used for error-reporting.
     * @constructor
     */
    let File = function File(fd, path) {
      exports.OS.Shared.AbstractFile.call(this, fd, path);
      this._closeResult = null;
    };
    File.prototype = Object.create(exports.OS.Shared.AbstractFile.prototype);

    /**
     * Close the file.
     *
     * This method has no effect if the file is already closed. However,
     * if the first call to |close| has thrown an error, further calls
     * will throw the same error.
     *
     * @throws File.Error If closing the file revealed an error that could
     * not be reported earlier.
     */
    File.prototype.close = function close() {
      if (this._fd) {
        let fd = this._fd;
        this._fd = null;
        // Call |close(fd)|, detach finalizer if any
        // (|fd| may not be a CDataFinalizer if it has been
        // instantiated from a controller thread).
        let result = UnixFile._close(fd);
        if (typeof fd == "object" && "forget" in fd) {
          fd.forget();
        }
        if (result == -1) {
          this._closeResult = new File.Error("close", ctypes.errno, this._path);
        }
      }
      if (this._closeResult) {
        throw this._closeResult;
      }
    };

    /**
     * Read some bytes from a file.
     *
     * @param {C pointer} buffer A buffer for holding the data
     * once it is read.
     * @param {number} nbytes The number of bytes to read. It must not
     * exceed the size of |buffer| in bytes but it may exceed the number
     * of bytes unread in the file.
     * @param {*=} options Additional options for reading. Ignored in
     * this implementation.
     *
     * @return {number} The number of bytes effectively read. If zero,
     * the end of the file has been reached.
     * @throws {OS.File.Error} In case of I/O error.
     */
    File.prototype._read = function _read(buffer, nbytes, options = {}) {
      // Populate the page cache with data from a file so the subsequent reads
      // from that file will not block on disk I/O.
      if (
        typeof UnixFile.posix_fadvise === "function" &&
        (options.sequential || !("sequential" in options))
      ) {
        UnixFile.posix_fadvise(
          this.fd,
          0,
          nbytes,
          OS.Constants.libc.POSIX_FADV_SEQUENTIAL
        );
      }
      return throw_on_negative(
        "read",
        UnixFile.read(this.fd, buffer, nbytes),
        this._path
      );
    };

    /**
     * Write some bytes to a file.
     *
     * @param {Typed array} buffer A buffer holding the data that must be
     * written.
     * @param {number} nbytes The number of bytes to write. It must not
     * exceed the size of |buffer| in bytes.
     * @param {*=} options Additional options for writing. Ignored in
     * this implementation.
     *
     * @return {number} The number of bytes effectively written.
     * @throws {OS.File.Error} In case of I/O error.
     */
    File.prototype._write = function _write(buffer, nbytes, options = {}) {
      return throw_on_negative(
        "write",
        UnixFile.write(this.fd, buffer, nbytes),
        this._path
      );
    };

    /**
     * Return the current position in the file.
     */
    File.prototype.getPosition = function getPosition(pos) {
      return this.setPosition(0, File.POS_CURRENT);
    };

    /**
     * Change the current position in the file.
     *
     * @param {number} pos The new position. Whether this position
     * is considered from the current position, from the start of
     * the file or from the end of the file is determined by
     * argument |whence|.  Note that |pos| may exceed the length of
     * the file.
     * @param {number=} whence The reference position. If omitted
     * or |OS.File.POS_START|, |pos| is relative to the start of the
     * file.  If |OS.File.POS_CURRENT|, |pos| is relative to the
     * current position in the file. If |OS.File.POS_END|, |pos| is
     * relative to the end of the file.
     *
     * @return The new position in the file.
     */
    File.prototype.setPosition = function setPosition(pos, whence) {
      if (whence === undefined) {
        whence = Const.SEEK_SET;
      }
      return throw_on_negative(
        "setPosition",
        UnixFile.lseek(this.fd, pos, whence),
        this._path
      );
    };

    /**
     * Fetch the information on the file.
     *
     * @return File.Info The information on |this| file.
     */
    File.prototype.stat = function stat() {
      throw_on_negative(
        "stat",
        UnixFile.fstat(this.fd, gStatDataPtr),
        this._path
      );
      return new File.Info(gStatData, this._path);
    };

    /**
     * Set the file's access permissions.
     *
     * This operation is likely to fail if applied to a file that was
     * not created by the currently running program (more precisely,
     * if it was created by a program running under a different OS-level
     * user account).  It may also fail, or silently do nothing, if the
     * filesystem containing the file does not support access permissions.
     *
     * @param {*=} options Object specifying the requested permissions:
     *
     * - {number} unixMode The POSIX file mode to set on the file.  If omitted,
     *  the POSIX file mode is reset to the default used by |OS.file.open|.  If
     *  specified, the permissions will respect the process umask as if they
     *  had been specified as arguments of |OS.File.open|, unless the
     *  |unixHonorUmask| parameter tells otherwise.
     * - {bool} unixHonorUmask If omitted or true, any |unixMode| value is
     *  modified by the process umask, as |OS.File.open| would have done.  If
     *  false, the exact value of |unixMode| will be applied.
     */
    File.prototype.setPermissions = function setPermissions(options = {}) {
      throw_on_negative(
        "setPermissions",
        UnixFile.fchmod(this.fd, unixMode(options)),
        this._path
      );
    };

    /**
     * Set the last access and modification date of the file.
     * The time stamp resolution is 1 second at best, but might be worse
     * depending on the platform.
     *
     * WARNING: This method is not implemented on Android/B2G. On Android/B2G,
     * you should use File.setDates instead.
     *
     * @param {Date,number=} accessDate The last access date. If numeric,
     * milliseconds since epoch. If omitted or null, then the current date
     * will be used.
     * @param {Date,number=} modificationDate The last modification date. If
     * numeric, milliseconds since epoch. If omitted or null, then the current
     * date will be used.
     *
     * @throws {TypeError} In case of invalid parameters.
     * @throws {OS.File.Error} In case of I/O error.
     */
    if (SharedAll.Constants.Sys.Name != "Android") {
      File.prototype.setDates = function(accessDate, modificationDate) {
        let { /* value, */ ptr } = datesToTimevals(
          accessDate,
          modificationDate
        );
        throw_on_negative(
          "setDates",
          UnixFile.futimes(this.fd, ptr),
          this._path
        );
      };
    }

    /**
     * Flushes the file's buffers and causes all buffered data
     * to be written.
     * Disk flushes are very expensive and therefore should be used carefully,
     * sparingly and only in scenarios where it is vital that data survives
     * system crashes. Even though the function will be executed off the
     * main-thread, it might still affect the overall performance of any
     * running application.
     *
     * @throws {OS.File.Error} In case of I/O error.
     */
    File.prototype.flush = function flush() {
      throw_on_negative("flush", UnixFile.fsync(this.fd), this._path);
    };

    // The default unix mode for opening (0600)
    const DEFAULT_UNIX_MODE = 384;

    /**
     * Open a file
     *
     * @param {string} path The path to the file.
     * @param {*=} mode The opening mode for the file, as
     * an object that may contain the following fields:
     *
     * - {bool} truncate If |true|, the file will be opened
     *  for writing. If the file does not exist, it will be
     *  created. If the file exists, its contents will be
     *  erased. Cannot be specified with |create|.
     * - {bool} create If |true|, the file will be opened
     *  for writing. If the file exists, this function fails.
     *  If the file does not exist, it will be created. Cannot
     *  be specified with |truncate| or |existing|.
     * - {bool} existing. If the file does not exist, this function
     *  fails. Cannot be specified with |create|.
     * - {bool} read If |true|, the file will be opened for
     *  reading. The file may also be opened for writing, depending
     *  on the other fields of |mode|.
     * - {bool} write If |true|, the file will be opened for
     *  writing. The file may also be opened for reading, depending
     *  on the other fields of |mode|.
     * - {bool} append If |true|, the file will be opened for appending,
     *  meaning the equivalent of |.setPosition(0, POS_END)| is executed
     *  before each write. The default is |true|, i.e. opening a file for
     *  appending. Specify |append: false| to open the file in regular mode.
     *
     * If neither |truncate|, |create| or |write| is specified, the file
     * is opened for reading.
     *
     * Note that |false|, |null| or |undefined| flags are simply ignored.
     *
     * @param {*=} options Additional options for file opening. This
     * implementation interprets the following fields:
     *
     * - {number} unixFlags If specified, file opening flags, as
     *  per libc function |open|. Replaces |mode|.
     * - {number} unixMode If specified, a file creation mode,
     *  as per libc function |open|. If unspecified, files are
     *  created with a default mode of 0600 (file is private to the
     *  user, the user can read and write).
     *
     * @return {File} A file object.
     * @throws {OS.File.Error} If the file could not be opened.
     */
    File.open = function Unix_open(path, mode, options = {}) {
      // We don't need to filter for the umask because "open" does this for us.
      let omode =
        options.unixMode !== undefined ? options.unixMode : DEFAULT_UNIX_MODE;
      let flags;
      if (options.unixFlags !== undefined) {
        flags = options.unixFlags;
      } else {
        mode = OS.Shared.AbstractFile.normalizeOpenMode(mode);
        // Handle read/write
        if (!mode.write) {
          flags = Const.O_RDONLY;
        } else if (mode.read) {
          flags = Const.O_RDWR;
        } else {
          flags = Const.O_WRONLY;
        }
        // Finally, handle create/existing/trunc
        if (mode.trunc) {
          if (mode.existing) {
            flags |= Const.O_TRUNC;
          } else {
            flags |= Const.O_CREAT | Const.O_TRUNC;
          }
        } else if (mode.create) {
          flags |= Const.O_CREAT | Const.O_EXCL;
        } else if (mode.read && !mode.write) {
          // flags are sufficient
        } else if (!mode.existing) {
          flags |= Const.O_CREAT;
        }
        if (mode.append) {
          flags |= Const.O_APPEND;
        }
      }
      return error_or_file(UnixFile.open(path, flags, ctypes.int(omode)), path);
    };

    /**
     * Checks if a file exists
     *
     * @param {string} path The path to the file.
     *
     * @return {bool} true if the file exists, false otherwise.
     */
    File.exists = function Unix_exists(path) {
      if (UnixFile.access(path, Const.F_OK) == -1) {
        return false;
      }
      return true;
    };

    /**
     * Remove an existing file.
     *
     * @param {string} path The name of the file.
     * @param {*=} options Additional options.
     *   - {bool} ignoreAbsent If |false|, throw an error if the file does
     *     not exist. |true| by default.
     *
     * @throws {OS.File.Error} In case of I/O error.
     */
    File.remove = function remove(path, options = {}) {
      let result = UnixFile.unlink(path);
      if (result == -1) {
        if (
          (!("ignoreAbsent" in options) || options.ignoreAbsent) &&
          ctypes.errno == Const.ENOENT
        ) {
          return;
        }
        throw new File.Error("remove", ctypes.errno, path);
      }
    };

    /**
     * Remove an empty directory.
     *
     * @param {string} path The name of the directory to remove.
     * @param {*=} options Additional options.
     *   - {bool} ignoreAbsent If |false|, throw an error if the directory
     *     does not exist. |true| by default
     */
    File.removeEmptyDir = function removeEmptyDir(path, options = {}) {
      let result = UnixFile.rmdir(path);
      if (result == -1) {
        if (
          (!("ignoreAbsent" in options) || options.ignoreAbsent) &&
          ctypes.errno == Const.ENOENT
        ) {
          return;
        }
        throw new File.Error("removeEmptyDir", ctypes.errno, path);
      }
    };

    /**
     * Default mode for opening directories.
     */
    const DEFAULT_UNIX_MODE_DIR = Const.S_IRWXU;

    /**
     * Create a directory.
     *
     * @param {string} path The name of the directory.
     * @param {*=} options Additional options. This
     * implementation interprets the following fields:
     *
     * - {number} unixMode If specified, a file creation mode,
     * as per libc function |mkdir|. If unspecified, dirs are
     * created with a default mode of 0700 (dir is private to
     * the user, the user can read, write and execute).
     * - {bool} ignoreExisting If |false|, throw error if the directory
     * already exists. |true| by default
     * - {string} from If specified, the call to |makeDir| creates all the
     * ancestors of |path| that are descendants of |from|. Note that |from|
     * and its existing descendants must be user-writeable and that |path|
     * must be a descendant of |from|.
     * Example:
     *   makeDir(Path.join(profileDir, "foo", "bar"), { from: profileDir });
     *  creates directories profileDir/foo, profileDir/foo/bar
     */
    File._makeDir = function makeDir(path, options = {}) {
      let omode =
        options.unixMode !== undefined
          ? options.unixMode
          : DEFAULT_UNIX_MODE_DIR;
      let result = UnixFile.mkdir(path, omode);
      if (result == -1) {
        if (
          (!("ignoreExisting" in options) || options.ignoreExisting) &&
          (ctypes.errno == Const.EEXIST || ctypes.errno == Const.EISDIR)
        ) {
          return;
        }
        throw new File.Error("makeDir", ctypes.errno, path);
      }
    };

    /**
     * Copy a file to a destination.
     *
     * @param {string} sourcePath The platform-specific path at which
     * the file may currently be found.
     * @param {string} destPath The platform-specific path at which the
     * file should be copied.
     * @param {*=} options An object which may contain the following fields:
     *
     * @option {bool} noOverwrite - If set, this function will fail if
     * a file already exists at |destPath|. Otherwise, if this file exists,
     * it will be erased silently.
     *
     * @throws {OS.File.Error} In case of any error.
     *
     * General note: The behavior of this function is defined only when
     * it is called on a single file. If it is called on a directory, the
     * behavior is undefined and may not be the same across all platforms.
     *
     * General note: The behavior of this function with respect to metadata
     * is unspecified. Metadata may or may not be copied with the file. The
     * behavior may not be the same across all platforms.
     */
    File.copy = null;

    /**
     * Move a file to a destination.
     *
     * @param {string} sourcePath The platform-specific path at which
     * the file may currently be found.
     * @param {string} destPath The platform-specific path at which the
     * file should be moved.
     * @param {*=} options An object which may contain the following fields:
     *
     * @option {bool} noOverwrite - If set, this function will fail if
     * a file already exists at |destPath|. Otherwise, if this file exists,
     * it will be erased silently.
     * @option {bool} noCopy - If set, this function will fail if the
     * operation is more sophisticated than a simple renaming, i.e. if
     * |sourcePath| and |destPath| are not situated on the same device.
     *
     * @throws {OS.File.Error} In case of any error.
     *
     * General note: The behavior of this function is defined only when
     * it is called on a single file. If it is called on a directory, the
     * behavior is undefined and may not be the same across all platforms.
     *
     * General note: The behavior of this function with respect to metadata
     * is unspecified. Metadata may or may not be moved with the file. The
     * behavior may not be the same across all platforms.
     */
    File.move = null;

    if (UnixFile.copyfile) {
      // This implementation uses |copyfile(3)|, from the BSD library.
      // Adding copying of hierarchies and/or attributes is just a flag
      // away.
      File.copy = function copyfile(sourcePath, destPath, options = {}) {
        let flags = Const.COPYFILE_DATA;
        if (options.noOverwrite) {
          flags |= Const.COPYFILE_EXCL;
        }
        throw_on_negative(
          "copy",
          UnixFile.copyfile(sourcePath, destPath, null, flags),
          sourcePath
        );
      };
    } else {
      // If the OS does not implement file copying for us, we need to
      // implement it ourselves. For this purpose, we need to define
      // a pumping function.

      /**
       * Copy bytes from one file to another one.
       *
       * @param {File} source The file containing the data to be copied. It
       * should be opened for reading.
       * @param {File} dest The file to which the data should be written. It
       * should be opened for writing.
       * @param {*=} options An object which may contain the following fields:
       *
       * @option {number} nbytes The maximal number of bytes to
       * copy. If unspecified, copy everything from the current
       * position.
       * @option {number} bufSize A hint regarding the size of the
       * buffer to use for copying. The implementation may decide to
       * ignore this hint.
       * @option {bool} unixUserland Will force the copy operation to be
       * caried out in user land, instead of using optimized syscalls such
       * as splice(2).
       *
       * @throws {OS.File.Error} In case of error.
       */
      let pump;

      // A buffer used by |pump_userland|
      let pump_buffer = null;

      // An implementation of |pump| using |read|/|write|
      let pump_userland = function pump_userland(source, dest, options = {}) {
        let bufSize = options.bufSize > 0 ? options.bufSize : 4096;
        let nbytes = options.nbytes > 0 ? options.nbytes : Infinity;
        if (!pump_buffer || pump_buffer.length < bufSize) {
          pump_buffer = new (ctypes.ArrayType(ctypes.char))(bufSize);
        }
        let read = source._read.bind(source);
        let write = dest._write.bind(dest);
        // Perform actual copy
        let total_read = 0;
        while (true) {
          let bytes_just_read = read(pump_buffer, bufSize);
          if (bytes_just_read == 0) {
            return total_read;
          }
          total_read += bytes_just_read;
          let bytes_written = 0;
          do {
            bytes_written += write(
              pump_buffer.addressOfElement(bytes_written),
              bytes_just_read - bytes_written
            );
          } while (bytes_written < bytes_just_read);
          nbytes -= bytes_written;
          if (nbytes <= 0) {
            return total_read;
          }
        }
      };

      // Fortunately, under Linux, that pumping function can be optimized.
      if (UnixFile.splice) {
        const BUFSIZE = 1 << 17;

        // An implementation of |pump| using |splice| (for Linux/Android)
        pump = function pump_splice(source, dest, options = {}) {
          let nbytes = options.nbytes > 0 ? options.nbytes : Infinity;
          let pipe = [];
          throw_on_negative("pump", UnixFile.pipe(pipe));
          let pipe_read = pipe[0];
          let pipe_write = pipe[1];
          let source_fd = source.fd;
          let dest_fd = dest.fd;
          let total_read = 0;
          let total_written = 0;
          try {
            while (true) {
              let chunk_size = Math.min(nbytes, BUFSIZE);
              let bytes_read = throw_on_negative(
                "pump",
                UnixFile.splice(
                  source_fd,
                  null,
                  pipe_write,
                  null,
                  chunk_size,
                  0
                )
              );
              if (!bytes_read) {
                break;
              }
              total_read += bytes_read;
              let bytes_written = throw_on_negative(
                "pump",
                UnixFile.splice(
                  pipe_read,
                  null,
                  dest_fd,
                  null,
                  bytes_read,
                  bytes_read == chunk_size ? Const.SPLICE_F_MORE : 0
                )
              );
              if (!bytes_written) {
                // This should never happen
                throw new Error("Internal error: pipe disconnected");
              }
              total_written += bytes_written;
              nbytes -= bytes_read;
              if (!nbytes) {
                break;
              }
            }
            return total_written;
          } catch (x) {
            if (x.unixErrno == Const.EINVAL) {
              // We *might* be on a file system that does not support splice.
              // Try again with a fallback pump.
              if (total_read) {
                source.setPosition(-total_read, File.POS_CURRENT);
              }
              if (total_written) {
                dest.setPosition(-total_written, File.POS_CURRENT);
              }
              return pump_userland(source, dest, options);
            }
            throw x;
          } finally {
            pipe_read.dispose();
            pipe_write.dispose();
          }
        };
      } else {
        // Fallback implementation of pump for other Unix platforms.
        pump = pump_userland;
      }

      // Implement |copy| using |pump|.
      // This implementation would require some work before being able to
      // copy directories
      File.copy = function copy(sourcePath, destPath, options = {}) {
        let source, dest;
        try {
          source = File.open(sourcePath);
          // Need to open the output file with |append:false|, or else |splice|
          // won't work.
          if (options.noOverwrite) {
            dest = File.open(destPath, { create: true, append: false });
          } else {
            dest = File.open(destPath, { trunc: true, append: false });
          }
          if (options.unixUserland) {
            pump_userland(source, dest, options);
          } else {
            pump(source, dest, options);
          }
        } catch (x) {
          if (dest) {
            dest.close();
          }
          if (source) {
            source.close();
          }
          throw x;
        }
      };
    } // End of definition of copy

    // Implement |move| using |rename| (wherever possible) or |copy|
    // (if files are on distinct devices).
    File.move = function move(sourcePath, destPath, options = {}) {
      // An implementation using |rename| whenever possible or
      // |File.pump| when required, for other Unices.
      // It can move directories on one file system, not
      // across file systems

      // If necessary, fail if the destination file exists
      if (options.noOverwrite) {
        let fd = UnixFile.open(destPath, Const.O_RDONLY);
        if (fd != -1) {
          fd.dispose();
          // The file exists and we have access
          throw new File.Error("move", Const.EEXIST, sourcePath);
        } else if (ctypes.errno == Const.EACCESS) {
          // The file exists and we don't have access
          throw new File.Error("move", Const.EEXIST, sourcePath);
        }
      }

      // If we can, rename the file.
      let result = UnixFile.rename(sourcePath, destPath);
      if (result != -1) {
        // Succeeded.
        return;
      }

      // In some cases, we cannot rename, e.g. because we're crossing
      // devices. In such cases, if permitted, we'll need to copy then
      // erase the original.
      if (options.noCopy) {
        throw new File.Error("move", ctypes.errno, sourcePath);
      }

      File.copy(sourcePath, destPath, options);
      // Note that we do not attempt to clean-up in case of copy error.
      // I'm sure that there are edge cases in which this could end up
      // removing an important file by accident. I'd rather leave
      // a file lying around by error than removing a critical file.

      File.remove(sourcePath);
    };

    File.unixSymLink = function unixSymLink(sourcePath, destPath) {
      throw_on_negative(
        "symlink",
        UnixFile.symlink(sourcePath, destPath),
        sourcePath
      );
    };

    /**
     * Iterate on one directory.
     *
     * This iterator will not enter subdirectories.
     *
     * @param {string} path The directory upon which to iterate.
     * @param {*=} options Ignored in this implementation.
     *
     * @throws {File.Error} If |path| does not represent a directory or
     * if the directory cannot be iterated.
     * @constructor
     */
    File.DirectoryIterator = function DirectoryIterator(path, options) {
      exports.OS.Shared.AbstractFile.AbstractIterator.call(this);
      this._path = path;
      this._dir = UnixFile.opendir(this._path);
      if (this._dir == null) {
        let error = ctypes.errno;
        if (error != Const.ENOENT) {
          throw new File.Error("DirectoryIterator", error, path);
        }
        this._exists = false;
        this._closed = true;
      } else {
        this._exists = true;
        this._closed = false;
      }
    };
    File.DirectoryIterator.prototype = Object.create(
      exports.OS.Shared.AbstractFile.AbstractIterator.prototype
    );

    /**
     * Return the next entry in the directory, if any such entry is
     * available.
     *
     * Skip special directories "." and "..".
     *
     * @return By definition of the iterator protocol, either
     * `{value: {File.Entry}, done: false}` if there is an unvisited entry
     * in the directory, or `{value: undefined, done: true}`, otherwise.
     */
    File.DirectoryIterator.prototype.next = function next() {
      if (!this._exists) {
        throw File.Error.noSuchFile(
          "DirectoryIterator.prototype.next",
          this._path
        );
      }
      if (this._closed) {
        return { value: undefined, done: true };
      }
      for (
        let entry = UnixFile.readdir(this._dir);
        entry != null && !entry.isNull();
        entry = UnixFile.readdir(this._dir)
      ) {
        let contents = entry.contents;
        let name = contents.d_name.readString();
        if (name == "." || name == "..") {
          continue;
        }

        let isDir, isSymLink;
        if (
          !("d_type" in contents) ||
          !("DT_UNKNOWN" in Const) ||
          contents.d_type == Const.DT_UNKNOWN
        ) {
          // File type information is not available in d_type. The cases are:
          // 1. |dirent| doesn't have d_type on some platforms (e.g. Solaris).
          // 2. DT_UNKNOWN and other DT_ constants are not defined.
          // 3. d_type is set to unknown (e.g. not supported by the
          //    filesystem).
          let path = Path.join(this._path, name);
          throw_on_negative(
            "lstat",
            UnixFile.lstat(path, gStatDataPtr),
            this._path
          );
          isDir = (gStatData.st_mode & Const.S_IFMT) == Const.S_IFDIR;
          isSymLink = (gStatData.st_mode & Const.S_IFMT) == Const.S_IFLNK;
        } else {
          isDir = contents.d_type == Const.DT_DIR;
          isSymLink = contents.d_type == Const.DT_LNK;
        }

        return {
          value: new File.DirectoryIterator.Entry(
            isDir,
            isSymLink,
            name,
            this._path
          ),
          done: false,
        };
      }
      this.close();
      return { value: undefined, done: true };
    };

    /**
     * Close the iterator and recover all resources.
     * You should call this once you have finished iterating on a directory.
     */
    File.DirectoryIterator.prototype.close = function close() {
      if (this._closed) {
        return;
      }
      this._closed = true;
      UnixFile.closedir(this._dir);
      this._dir = null;
    };

    /**
     * Determine whether the directory exists.
     *
     * @return {boolean}
     */
    File.DirectoryIterator.prototype.exists = function exists() {
      return this._exists;
    };

    /**
     * Return directory as |File|
     */
    File.DirectoryIterator.prototype.unixAsFile = function unixAsFile() {
      if (!this._dir) {
        throw File.Error.closed("unixAsFile", this._path);
      }
      return error_or_file(UnixFile.dirfd(this._dir), this._path);
    };

    /**
     * An entry in a directory.
     */
    File.DirectoryIterator.Entry = function Entry(
      isDir,
      isSymLink,
      name,
      parent
    ) {
      // Copy the relevant part of |unix_entry| to ensure that
      // our data is not overwritten prematurely.
      this._parent = parent;
      let path = Path.join(this._parent, name);

      SysAll.AbstractEntry.call(this, isDir, isSymLink, name, path);
    };
    File.DirectoryIterator.Entry.prototype = Object.create(
      SysAll.AbstractEntry.prototype
    );

    /**
     * Return a version of an instance of
     * File.DirectoryIterator.Entry that can be sent from a worker
     * thread to the main thread. Note that deserialization is
     * asymmetric and returns an object with a different
     * implementation.
     */
    File.DirectoryIterator.Entry.toMsg = function toMsg(value) {
      if (!(value instanceof File.DirectoryIterator.Entry)) {
        throw new TypeError(
          "parameter of " +
            "File.DirectoryIterator.Entry.toMsg must be a " +
            "File.DirectoryIterator.Entry"
        );
      }
      let serialized = {};
      for (let key in File.DirectoryIterator.Entry.prototype) {
        serialized[key] = value[key];
      }
      return serialized;
    };

    let gStatData = new Type.stat.implementation();
    let gStatDataPtr = gStatData.address();

    let MODE_MASK = 4095; /* = 07777*/
    File.Info = function Info(stat, path) {
      let isDir = (stat.st_mode & Const.S_IFMT) == Const.S_IFDIR;
      let isSymLink = (stat.st_mode & Const.S_IFMT) == Const.S_IFLNK;
      let size = Type.off_t.importFromC(stat.st_size);

      let lastAccessDate = new Date(stat.st_atime * 1000);
      let lastModificationDate = new Date(stat.st_mtime * 1000);
      let unixLastStatusChangeDate = new Date(stat.st_ctime * 1000);

      let unixOwner = Type.uid_t.importFromC(stat.st_uid);
      let unixGroup = Type.gid_t.importFromC(stat.st_gid);
      let unixMode = Type.mode_t.importFromC(stat.st_mode & MODE_MASK);

      SysAll.AbstractInfo.call(
        this,
        path,
        isDir,
        isSymLink,
        size,
        lastAccessDate,
        lastModificationDate,
        unixLastStatusChangeDate,
        unixOwner,
        unixGroup,
        unixMode
      );
    };
    File.Info.prototype = Object.create(SysAll.AbstractInfo.prototype);

    /**
     * Return a version of an instance of File.Info that can be sent
     * from a worker thread to the main thread. Note that deserialization
     * is asymmetric and returns an object with a different implementation.
     */
    File.Info.toMsg = function toMsg(stat) {
      if (!(stat instanceof File.Info)) {
        throw new TypeError("parameter of File.Info.toMsg must be a File.Info");
      }
      let serialized = {};
      for (let key in File.Info.prototype) {
        serialized[key] = stat[key];
      }
      return serialized;
    };

    /**
     * Fetch the information on a file.
     *
     * @param {string} path The full name of the file to open.
     * @param {*=} options Additional options. In this implementation:
     *
     * - {bool} unixNoFollowingLinks If set and |true|, if |path|
     * represents a symbolic link, the call will return the information
     * of the link itself, rather than that of the target file.
     *
     * @return {File.Information}
     */
    File.stat = function stat(path, options = {}) {
      if (options.unixNoFollowingLinks) {
        throw_on_negative("stat", UnixFile.lstat(path, gStatDataPtr), path);
      } else {
        throw_on_negative("stat", UnixFile.stat(path, gStatDataPtr), path);
      }
      return new File.Info(gStatData, path);
    };

    /**
     * Set the file's access permissions.
     *
     * This operation is likely to fail if applied to a file that was
     * not created by the currently running program (more precisely,
     * if it was created by a program running under a different OS-level
     * user account).  It may also fail, or silently do nothing, if the
     * filesystem containing the file does not support access permissions.
     *
     * @param {string} path The name of the file to reset the permissions of.
     * @param {*=} options Object specifying the requested permissions:
     *
     * - {number} unixMode The POSIX file mode to set on the file.  If omitted,
     *  the POSIX file mode is reset to the default used by |OS.file.open|.  If
     *  specified, the permissions will respect the process umask as if they
     *  had been specified as arguments of |OS.File.open|, unless the
     *  |unixHonorUmask| parameter tells otherwise.
     * - {bool} unixHonorUmask If omitted or true, any |unixMode| value is
     *  modified by the process umask, as |OS.File.open| would have done.  If
     *  false, the exact value of |unixMode| will be applied.
     */
    File.setPermissions = function setPermissions(path, options = {}) {
      throw_on_negative(
        "setPermissions",
        UnixFile.chmod(path, unixMode(options)),
        path
      );
    };

    /**
     * Convert an access date and a modification date to an array
     * of two |timeval|.
     */
    function datesToTimevals(accessDate, modificationDate) {
      accessDate = normalizeDate("File.setDates", accessDate);
      modificationDate = normalizeDate("File.setDates", modificationDate);

      let timevals = new Type.timevals.implementation();
      let timevalsPtr = timevals.address();

      // JavaScript date values are expressed in milliseconds since epoch.
      // Split this up into second and microsecond components.
      timevals[0].tv_sec = (accessDate / 1000) | 0;
      timevals[0].tv_usec = ((accessDate % 1000) * 1000) | 0;
      timevals[1].tv_sec = (modificationDate / 1000) | 0;
      timevals[1].tv_usec = ((modificationDate % 1000) * 1000) | 0;

      return { value: timevals, ptr: timevalsPtr };
    }

    /**
     * Set the last access and modification date of the file.
     * The time stamp resolution is 1 second at best, but might be worse
     * depending on the platform.
     *
     * @param {string} path The full name of the file to set the dates for.
     * @param {Date,number=} accessDate The last access date. If numeric,
     * milliseconds since epoch. If omitted or null, then the current date
     * will be used.
     * @param {Date,number=} modificationDate The last modification date. If
     * numeric, milliseconds since epoch. If omitted or null, then the current
     * date will be used.
     *
     * @throws {TypeError} In case of invalid paramters.
     * @throws {OS.File.Error} In case of I/O error.
     */
    File.setDates = function setDates(path, accessDate, modificationDate) {
      let { /* value, */ ptr } = datesToTimevals(accessDate, modificationDate);
      throw_on_negative("setDates", UnixFile.utimes(path, ptr), path);
    };

    File.read = exports.OS.Shared.AbstractFile.read;
    File.writeAtomic = exports.OS.Shared.AbstractFile.writeAtomic;
    File.openUnique = exports.OS.Shared.AbstractFile.openUnique;
    File.makeDir = exports.OS.Shared.AbstractFile.makeDir;

    /**
     * Remove an existing directory and its contents.
     *
     * @param {string} path The name of the directory.
     * @param {*=} options Additional options.
     *   - {bool} ignoreAbsent If |false|, throw an error if the directory doesn't
     *     exist. |true| by default.
     *   - {boolean} ignorePermissions If |true|, remove the file even when lacking write
     *     permission.
     *
     * @throws {OS.File.Error} In case of I/O error, in particular if |path| is
     *         not a directory.
     *
     * Note: This function will remove a symlink even if it points a directory.
     */
    File.removeDir = function(path, options = {}) {
      let isSymLink;
      try {
        let info = File.stat(path, { unixNoFollowingLinks: true });
        isSymLink = info.isSymLink;
      } catch (e) {
        if (
          (!("ignoreAbsent" in options) || options.ignoreAbsent) &&
          ctypes.errno == Const.ENOENT
        ) {
          return;
        }
        throw e;
      }
      if (isSymLink) {
        // A Unix symlink itself is not a directory even if it points
        // a directory.
        File.remove(path, options);
        return;
      }
      exports.OS.Shared.AbstractFile.removeRecursive(path, options);
    };

    /**
     * Get the current directory by getCurrentDirectory.
     */
    File.getCurrentDirectory = function getCurrentDirectory() {
      let path, buf;
      if (UnixFile.get_current_dir_name) {
        path = UnixFile.get_current_dir_name();
      } else if (UnixFile.getwd_auto) {
        path = UnixFile.getwd_auto(null);
      } else {
        for (let length = Const.PATH_MAX; !path; length *= 2) {
          buf = new (ctypes.char.array(length))();
          path = UnixFile.getcwd(buf, length);
        }
      }
      throw_on_null("getCurrentDirectory", path);
      return path.readString();
    };

    /**
     * Get/set the current directory.
     */
    Object.defineProperty(File, "curDir", {
      set(path) {
        this.setCurrentDirectory(path);
      },
      get() {
        return this.getCurrentDirectory();
      },
    });

    // Utility functions

    /**
     * Turn the result of |open| into an Error or a File
     * @param {number} maybe The result of the |open| operation that may
     * represent either an error or a success. If -1, this function raises
     * an error holding ctypes.errno, otherwise it returns the opened file.
     * @param {string=} path The path of the file.
     */
    function error_or_file(maybe, path) {
      if (maybe == -1) {
        throw new File.Error("open", ctypes.errno, path);
      }
      return new File(maybe, path);
    }

    /**
     * Utility function to sort errors represented as "-1" from successes.
     *
     * @param {string=} operation The name of the operation. If unspecified,
     * the name of the caller function.
     * @param {number} result The result of the operation that may
     * represent either an error or a success. If -1, this function raises
     * an error holding ctypes.errno, otherwise it returns |result|.
     * @param {string=} path The path of the file.
     */
    function throw_on_negative(operation, result, path) {
      if (result < 0) {
        throw new File.Error(operation, ctypes.errno, path);
      }
      return result;
    }

    /**
     * Utility function to sort errors represented as |null| from successes.
     *
     * @param {string=} operation The name of the operation. If unspecified,
     * the name of the caller function.
     * @param {pointer} result The result of the operation that may
     * represent either an error or a success. If |null|, this function raises
     * an error holding ctypes.errno, otherwise it returns |result|.
     * @param {string=} path The path of the file.
     */
    function throw_on_null(operation, result, path) {
      if (result == null || (result.isNull && result.isNull())) {
        throw new File.Error(operation, ctypes.errno, path);
      }
      return result;
    }

    /**
     * Normalize and verify a Date or numeric date value.
     *
     * @param {string} fn Function name of the calling function.
     * @param {Date,number} date The date to normalize. If omitted or null,
     * then the current date will be used.
     *
     * @throws {TypeError} Invalid date provided.
     *
     * @return {number} Sanitized, numeric date in milliseconds since epoch.
     */
    function normalizeDate(fn, date) {
      if (typeof date !== "number" && !date) {
        // |date| was Omitted or null.
        date = Date.now();
      } else if (typeof date.getTime === "function") {
        // Input might be a date or date-like object.
        date = date.getTime();
      }

      if (typeof date !== "number" || Number.isNaN(date)) {
        throw new TypeError(
          "|date| parameter of " +
            fn +
            " must be a " +
            "|Date| instance or number"
        );
      }
      return date;
    }

    /**
     * Helper used by both versions of setPermissions.
     */
    function unixMode(options) {
      let mode =
        options.unixMode !== undefined ? options.unixMode : DEFAULT_UNIX_MODE;
      let unixHonorUmask = true;
      if ("unixHonorUmask" in options) {
        unixHonorUmask = options.unixHonorUmask;
      }
      if (unixHonorUmask) {
        mode &= ~SharedAll.Constants.Sys.umask;
      }
      return mode;
    }

    File.Unix = exports.OS.Unix.File;
    File.Error = SysAll.Error;
    exports.OS.File = File;
    exports.OS.Shared.Type = Type;

    Object.defineProperty(File, "POS_START", { value: SysAll.POS_START });
    Object.defineProperty(File, "POS_CURRENT", { value: SysAll.POS_CURRENT });
    Object.defineProperty(File, "POS_END", { value: SysAll.POS_END });
  })(this);
}
