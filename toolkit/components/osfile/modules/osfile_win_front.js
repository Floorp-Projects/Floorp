/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Synchronous front-end for the JavaScript OS.File library.
 * Windows implementation.
 *
 * This front-end is meant to be imported by a worker thread.
 */

/* eslint-env mozilla/chrome-worker, node */
/* global OS */

// eslint-disable-next-line no-lone-blocks
{
  if (typeof Components != "undefined") {
    // We do not wish osfile_win_front.js to be used directly as a main thread
    // module yet.
    throw new Error(
      "osfile_win_front.js cannot be used from the main thread yet"
    );
  }

  (function(exports) {
    "use strict";

    // exports.OS.Win is created by osfile_win_back.js
    if (exports.OS && exports.OS.File) {
      return; // Avoid double-initialization
    }

    let SharedAll = require("resource://gre/modules/osfile/osfile_shared_allthreads.jsm");
    let Path = require("resource://gre/modules/osfile/ospath.jsm");
    let SysAll = require("resource://gre/modules/osfile/osfile_win_allthreads.jsm");
    exports.OS.Win.File._init();
    let Const = exports.OS.Constants.Win;
    let WinFile = exports.OS.Win.File;
    let Type = WinFile.Type;

    // Mutable thread-global data
    // In the Windows implementation, methods |read| and |write|
    // require passing a pointer to an uint32 to determine how many
    // bytes have been read/written. In C, this is a benigne operation,
    // but in js-ctypes, this has a cost. Rather than re-allocating a
    // C uint32 and a C uint32* for each |read|/|write|, we take advantage
    // of the fact that the state is thread-private -- hence that two
    // |read|/|write| operations cannot take place at the same time --
    // and we use the following global mutable values:
    let gBytesRead = new ctypes.uint32_t(0);
    let gBytesReadPtr = gBytesRead.address();
    let gBytesWritten = new ctypes.uint32_t(0);
    let gBytesWrittenPtr = gBytesWritten.address();

    // Same story for GetFileInformationByHandle
    let gFileInfo = new Type.FILE_INFORMATION.implementation();
    let gFileInfoPtr = gFileInfo.address();

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
        let result = WinFile._CloseHandle(fd);
        if (typeof fd == "object" && "forget" in fd) {
          fd.forget();
        }
        if (result == -1) {
          this._closeResult = new File.Error(
            "close",
            ctypes.winLastError,
            this._path
          );
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
    File.prototype._read = function _read(buffer, nbytes, options) {
      // |gBytesReadPtr| is a pointer to |gBytesRead|.
      throw_on_zero(
        "read",
        WinFile.ReadFile(this.fd, buffer, nbytes, gBytesReadPtr, null),
        this._path
      );
      return gBytesRead.value;
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
    File.prototype._write = function _write(buffer, nbytes, options) {
      if (this._appendMode) {
        // Need to manually seek on Windows, as O_APPEND is not supported.
        // This is, of course, a race, but there is no real way around this.
        this.setPosition(0, File.POS_END);
      }
      // |gBytesWrittenPtr| is a pointer to |gBytesWritten|.
      throw_on_zero(
        "write",
        WinFile.WriteFile(this.fd, buffer, nbytes, gBytesWrittenPtr, null),
        this._path
      );
      return gBytesWritten.value;
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
        whence = Const.FILE_BEGIN;
      }
      let pos64 = ctypes.Int64(pos);
      // Per MSDN, while |lDistanceToMove| (low) is declared as int32_t, when
      // providing |lDistanceToMoveHigh| as well, it should countain the
      // bottom 32 bits of the 64-bit integer. Hence the following |posLo|
      // cast is OK.
      let posLo = new ctypes.uint32_t(ctypes.Int64.lo(pos64));
      posLo = ctypes.cast(posLo, ctypes.int32_t);
      let posHi = new ctypes.int32_t(ctypes.Int64.hi(pos64));
      let result = WinFile.SetFilePointer(
        this.fd,
        posLo.value,
        posHi.address(),
        whence
      );
      // INVALID_SET_FILE_POINTER might be still a valid result, as it
      // represents the lower 32 bit of the int64 result. MSDN says to check
      // both, INVALID_SET_FILE_POINTER and a non-zero winLastError.
      if (result == Const.INVALID_SET_FILE_POINTER && ctypes.winLastError) {
        throw new File.Error("setPosition", ctypes.winLastError, this._path);
      }
      pos64 = ctypes.Int64.join(posHi.value, result);
      return Type.int64_t.project(pos64);
    };

    /**
     * Fetch the information on the file.
     *
     * @return File.Info The information on |this| file.
     */
    File.prototype.stat = function stat() {
      throw_on_zero(
        "stat",
        WinFile.GetFileInformationByHandle(this.fd, gFileInfoPtr),
        this._path
      );
      return new File.Info(gFileInfo, this._path);
    };

    /**
     * Set the last access and modification date of the file.
     * The time stamp resolution is 1 second at best, but might be worse
     * depending on the platform.
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
    File.prototype.setDates = function setDates(accessDate, modificationDate) {
      accessDate = Date_to_FILETIME(
        "File.prototype.setDates",
        accessDate,
        this._path
      );
      modificationDate = Date_to_FILETIME(
        "File.prototype.setDates",
        modificationDate,
        this._path
      );
      throw_on_zero(
        "setDates",
        WinFile.SetFileTime(
          this.fd,
          null,
          accessDate.address(),
          modificationDate.address()
        ),
        this._path
      );
    };

    /**
     * Set the file's access permission bits.
     */
    File.prototype.setPermissions = function setPermissions(options = {}) {
      if (!("winAttributes" in options)) {
        return;
      }
      let oldAttributes = WinFile.GetFileAttributes(this._path);
      if (oldAttributes == Const.INVALID_FILE_ATTRIBUTES) {
        throw new File.Error("setPermissions", ctypes.winLastError, this._path);
      }
      let newAttributes = toFileAttributes(
        options.winAttributes,
        oldAttributes
      );
      throw_on_zero(
        "setPermissions",
        WinFile.SetFileAttributes(this._path, newAttributes),
        this._path
      );
    };

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
      throw_on_zero("flush", WinFile.FlushFileBuffers(this.fd), this._path);
    };

    // The default sharing mode for opening files: files are not
    // locked against being reopened for reading/writing or against
    // being deleted by the same process or another process.
    // This is consistent with the default Unix policy.
    const DEFAULT_SHARE =
      Const.FILE_SHARE_READ | Const.FILE_SHARE_WRITE | Const.FILE_SHARE_DELETE;

    // The default flags for opening files.
    const DEFAULT_FLAGS = Const.FILE_ATTRIBUTE_NORMAL;

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
     * - {number} winShare If specified, a share mode, as per
     * Windows function |CreateFile|. You can build it from
     * constants |OS.Constants.Win.FILE_SHARE_*|. If unspecified,
     * the file uses the default sharing policy: it can be opened
     * for reading and/or writing and it can be removed by other
     * processes and by the same process.
     * - {number} winSecurity If specified, Windows security
     * attributes, as per Windows function |CreateFile|. If unspecified,
     * no security attributes.
     * - {number} winAccess If specified, Windows access mode, as
     * per Windows function |CreateFile|. This also requires option
     * |winDisposition| and this replaces argument |mode|. If unspecified,
     * uses the string |mode|.
     * - {number} winDisposition If specified, Windows disposition mode,
     * as per Windows function |CreateFile|. This also requires option
     * |winAccess| and this replaces argument |mode|. If unspecified,
     * uses the string |mode|.
     *
     * @return {File} A file object.
     * @throws {OS.File.Error} If the file could not be opened.
     */
    File.open = function Win_open(path, mode = {}, options = {}) {
      let share =
        options.winShare !== undefined ? options.winShare : DEFAULT_SHARE;
      let security = options.winSecurity || null;
      let flags =
        options.winFlags !== undefined ? options.winFlags : DEFAULT_FLAGS;
      let template = options.winTemplate ? options.winTemplate._fd : null;
      let access;
      let disposition;

      mode = OS.Shared.AbstractFile.normalizeOpenMode(mode);

      // The following option isn't a generic implementation of access to paths
      // of arbitrary lengths. It allows for the specific case of writing to an
      // Alternate Data Stream on a file whose path length is already close to
      // MAX_PATH. This implementation is safe with a full path as input, if
      // the first part of the path comes from local configuration and the
      // file without the ADS was successfully opened before, so we know the
      // path is valid.
      if (options.winAllowLengthBeyondMaxPathWithCaveats) {
        // Use the \\?\ syntax to allow lengths beyond MAX_PATH. This limited
        // implementation only supports a DOS local path or UNC path as input.
        let isUNC =
          path.length >= 2 &&
          (path[0] == "\\" || path[0] == "/") &&
          (path[1] == "\\" || path[1] == "/");
        let pathToUse = "\\\\?\\" + (isUNC ? "UNC\\" + path.slice(2) : path);
        // Use GetFullPathName to normalize slashes into backslashes. This is
        // required because CreateFile won't do this for the \\?\ syntax.
        let buffer_size = 512;
        let array = new (ctypes.ArrayType(ctypes.char16_t, buffer_size))();
        let expected_size = throw_on_zero(
          "open",
          WinFile.GetFullPathName(pathToUse, buffer_size, array, 0)
        );
        if (expected_size > buffer_size) {
          // We don't need to allow an arbitrary path length for now.
          throw new File.Error("open", ctypes.winLastError, path);
        }
        path = array.readString();
      }

      if ("winAccess" in options && "winDisposition" in options) {
        access = options.winAccess;
        disposition = options.winDisposition;
      } else if (
        ("winAccess" in options && !("winDisposition" in options)) ||
        (!("winAccess" in options) && "winDisposition" in options)
      ) {
        throw new TypeError(
          "OS.File.open requires either both options " +
            "winAccess and winDisposition or neither"
        );
      } else {
        if (mode.read) {
          access |= Const.GENERIC_READ;
        }
        if (mode.write) {
          access |= Const.GENERIC_WRITE;
        }
        // Finally, handle create/existing/trunc
        if (mode.trunc) {
          if (mode.existing) {
            // It seems that Const.TRUNCATE_EXISTING is broken
            // in presence of links (source, anyone?). We need
            // to open normally, then perform truncation manually.
            disposition = Const.OPEN_EXISTING;
          } else {
            disposition = Const.CREATE_ALWAYS;
          }
        } else if (mode.create) {
          disposition = Const.CREATE_NEW;
        } else if (mode.read && !mode.write) {
          disposition = Const.OPEN_EXISTING;
        } else if (mode.existing) {
          disposition = Const.OPEN_EXISTING;
        } else {
          disposition = Const.OPEN_ALWAYS;
        }
      }

      let file = error_or_file(
        WinFile.CreateFile(
          path,
          access,
          share,
          security,
          disposition,
          flags,
          template
        ),
        path
      );

      file._appendMode = !!mode.append;

      if (!(mode.trunc && mode.existing)) {
        return file;
      }
      // Now, perform manual truncation
      file.setPosition(0, File.POS_START);
      throw_on_zero("open", WinFile.SetEndOfFile(file.fd), path);
      return file;
    };

    /**
     * Checks if a file or directory exists
     *
     * @param {string} path The path to the file.
     *
     * @return {bool} true if the file exists, false otherwise.
     */
    File.exists = function Win_exists(path) {
      try {
        let file = File.open(path, FILE_STAT_MODE, FILE_STAT_OPTIONS);
        file.close();
        return true;
      } catch (x) {
        return false;
      }
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
      if (WinFile.DeleteFile(path)) {
        return;
      }

      if (
        ctypes.winLastError == Const.ERROR_FILE_NOT_FOUND ||
        ctypes.winLastError == Const.ERROR_PATH_NOT_FOUND
      ) {
        if (!("ignoreAbsent" in options) || options.ignoreAbsent) {
          return;
        }
      } else if (ctypes.winLastError == Const.ERROR_ACCESS_DENIED) {
        // Save winLastError before another ctypes call.
        let lastError = ctypes.winLastError;
        let attributes = WinFile.GetFileAttributes(path);
        if (attributes != Const.INVALID_FILE_ATTRIBUTES) {
          if (!(attributes & Const.FILE_ATTRIBUTE_READONLY)) {
            throw new File.Error("remove", lastError, path);
          }
          let newAttributes = attributes & ~Const.FILE_ATTRIBUTE_READONLY;
          if (
            WinFile.SetFileAttributes(path, newAttributes) &&
            WinFile.DeleteFile(path)
          ) {
            return;
          }
        }
      }

      throw new File.Error("remove", ctypes.winLastError, path);
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
      let result = WinFile.RemoveDirectory(path);
      if (!result) {
        if (
          (!("ignoreAbsent" in options) || options.ignoreAbsent) &&
          ctypes.winLastError == Const.ERROR_FILE_NOT_FOUND
        ) {
          return;
        }
        throw new File.Error("removeEmptyDir", ctypes.winLastError, path);
      }
    };

    /**
     * Create a directory and, optionally, its parent directories.
     *
     * @param {string} path The name of the directory.
     * @param {*=} options Additional options. This
     * implementation interprets the following fields:
     *
     * - {C pointer} winSecurity If specified, security attributes
     * as per winapi function |CreateDirectory|. If unspecified,
     * use the default security descriptor, inherited from the
     * parent directory.
     * - {bool} ignoreExisting If |false|, throw an error if the directory
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
      let security = options.winSecurity || null;
      let result = WinFile.CreateDirectory(path, security);

      if (result) {
        return;
      }

      if ("ignoreExisting" in options && !options.ignoreExisting) {
        throw new File.Error("makeDir", ctypes.winLastError, path);
      }

      if (ctypes.winLastError == Const.ERROR_ALREADY_EXISTS) {
        return;
      }

      // If the user has no access, but it's a root directory, no error should be thrown
      let splitPath = OS.Path.split(path);
      // Removing last component if it's empty
      // An empty last component is caused by trailing slashes in path
      // This is always the case with root directories
      if (splitPath.components[splitPath.components.length - 1].length === 0) {
        splitPath.components.pop();
      }
      // One component consisting of a drive letter implies a directory root.
      if (
        ctypes.winLastError == Const.ERROR_ACCESS_DENIED &&
        splitPath.winDrive &&
        splitPath.components.length === 1
      ) {
        return;
      }

      throw new File.Error("makeDir", ctypes.winLastError, path);
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
     * @option {bool} noOverwrite - If true, this function will fail if
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
    File.copy = function copy(sourcePath, destPath, options = {}) {
      throw_on_zero(
        "copy",
        WinFile.CopyFile(sourcePath, destPath, options.noOverwrite || false),
        sourcePath
      );
    };

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
     * |sourcePath| and |destPath| are not situated on the same drive.
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
    File.move = function move(sourcePath, destPath, options = {}) {
      let flags = 0;
      if (!options.noCopy) {
        flags = Const.MOVEFILE_COPY_ALLOWED;
      }
      if (!options.noOverwrite) {
        flags = flags | Const.MOVEFILE_REPLACE_EXISTING;
      }
      throw_on_zero(
        "move",
        WinFile.MoveFileEx(sourcePath, destPath, flags),
        sourcePath
      );

      // Inherit NTFS permissions from the destination directory
      // if possible.
      if (Path.dirname(sourcePath) === Path.dirname(destPath)) {
        // Skip if the move operation was the simple rename,
        return;
      }
      // The function may fail for various reasons (e.g. not all
      // filesystems support NTFS permissions or the user may not
      // have the enough rights to read/write permissions).
      // However we can safely ignore errors. The file was already
      // moved. Setting permissions is not mandatory.
      let dacl = new ctypes.voidptr_t();
      let sd = new ctypes.voidptr_t();
      WinFile.GetNamedSecurityInfo(
        destPath,
        Const.SE_FILE_OBJECT,
        Const.DACL_SECURITY_INFORMATION,
        null /* sidOwner*/,
        null /* sidGroup*/,
        dacl.address(),
        null /* sacl*/,
        sd.address()
      );
      // dacl will be set only if the function succeeds.
      if (!dacl.isNull()) {
        WinFile.SetNamedSecurityInfo(
          destPath,
          Const.SE_FILE_OBJECT,
          Const.DACL_SECURITY_INFORMATION |
            Const.UNPROTECTED_DACL_SECURITY_INFORMATION,
          null /* sidOwner*/,
          null /* sidGroup*/,
          dacl,
          null /* sacl*/
        );
      }
      // sd will be set only if the function succeeds.
      if (!sd.isNull()) {
        WinFile.LocalFree(Type.HLOCAL.cast(sd));
      }
    };

    /**
     * A global value used to receive data during time conversions.
     */
    let gSystemTime = new Type.SystemTime.implementation();
    let gSystemTimePtr = gSystemTime.address();

    /**
     * Utility function: convert a FILETIME to a JavaScript Date.
     */
    let FILETIME_to_Date = function FILETIME_to_Date(fileTime, path) {
      if (fileTime == null) {
        throw new TypeError("Expecting a non-null filetime");
      }
      throw_on_zero(
        "FILETIME_to_Date",
        WinFile.FileTimeToSystemTime(fileTime.address(), gSystemTimePtr),
        path
      );
      // Windows counts hours, minutes, seconds from UTC,
      // JS counts from local time, so we need to go through UTC.
      let utc = Date.UTC(
        gSystemTime.wYear,
        gSystemTime.wMonth - 1,
        /* Windows counts months from 1, JS from 0*/ gSystemTime.wDay,
        gSystemTime.wHour,
        gSystemTime.wMinute,
        gSystemTime.wSecond,
        gSystemTime.wMilliSeconds
      );
      return new Date(utc);
    };

    /**
     * Utility function: convert Javascript Date to FileTime.
     *
     * @param {string} fn Name of the calling function.
     * @param {Date,number} date The date to be converted. If omitted or null,
     * then the current date will be used. If numeric, assumed to be the date
     * in milliseconds since epoch.
     */
    let Date_to_FILETIME = function Date_to_FILETIME(fn, date, path) {
      if (typeof date === "number") {
        date = new Date(date);
      } else if (!date) {
        date = new Date();
      } else if (typeof date.getUTCFullYear !== "function") {
        throw new TypeError(
          "|date| parameter of " +
            fn +
            " must be a " +
            "|Date| instance or number"
        );
      }
      gSystemTime.wYear = date.getUTCFullYear();
      // Windows counts months from 1, JS from 0.
      gSystemTime.wMonth = date.getUTCMonth() + 1;
      gSystemTime.wDay = date.getUTCDate();
      gSystemTime.wHour = date.getUTCHours();
      gSystemTime.wMinute = date.getUTCMinutes();
      gSystemTime.wSecond = date.getUTCSeconds();
      gSystemTime.wMilliseconds = date.getUTCMilliseconds();
      let result = new OS.Shared.Type.FILETIME.implementation();
      throw_on_zero(
        "Date_to_FILETIME",
        WinFile.SystemTimeToFileTime(gSystemTimePtr, result.address()),
        path
      );
      return result;
    };

    /**
     * Iterate on one directory.
     *
     * This iterator will not enter subdirectories.
     *
     * @param {string} path The directory upon which to iterate.
     * @param {*=} options An object that may contain the following field:
     * @option {string} winPattern Windows file name pattern; if set,
     * only files matching this pattern are returned.
     *
     * @throws {File.Error} If |path| does not represent a directory or
     * if the directory cannot be iterated.
     * @constructor
     */
    File.DirectoryIterator = function DirectoryIterator(path, options) {
      exports.OS.Shared.AbstractFile.AbstractIterator.call(this);
      if (options && options.winPattern) {
        this._pattern = path + "\\" + options.winPattern;
      } else {
        this._pattern = path + "\\*";
      }
      this._path = path;

      // Pre-open the first item.
      this._first = true;
      this._findData = new Type.FindData.implementation();
      this._findDataPtr = this._findData.address();
      this._handle = WinFile.FindFirstFile(this._pattern, this._findDataPtr);
      if (this._handle == Const.INVALID_HANDLE_VALUE) {
        let error = ctypes.winLastError;
        this._findData = null;
        this._findDataPtr = null;
        if (error == Const.ERROR_FILE_NOT_FOUND) {
          // Directory is empty, let's behave as if it were closed
          SharedAll.LOG("Directory is empty");
          this._closed = true;
          this._exists = true;
        } else if (error == Const.ERROR_PATH_NOT_FOUND) {
          // Directory does not exist, let's throw if we attempt to walk it
          SharedAll.LOG("Directory does not exist");
          this._closed = true;
          this._exists = false;
        } else {
          throw new File.Error("DirectoryIterator", error, this._path);
        }
      } else {
        this._closed = false;
        this._exists = true;
      }
    };

    File.DirectoryIterator.prototype = Object.create(
      exports.OS.Shared.AbstractFile.AbstractIterator.prototype
    );

    /**
     * Fetch the next entry in the directory.
     *
     * @return null If we have reached the end of the directory.
     */
    File.DirectoryIterator.prototype._next = function _next() {
      // Bailout if the directory does not exist
      if (!this._exists) {
        throw File.Error.noSuchFile(
          "DirectoryIterator.prototype.next",
          this._path
        );
      }
      // Bailout if the iterator is closed.
      if (this._closed) {
        return null;
      }
      // If this is the first entry, we have obtained it already
      // during construction.
      if (this._first) {
        this._first = false;
        return this._findData;
      }

      if (WinFile.FindNextFile(this._handle, this._findDataPtr)) {
        return this._findData;
      }
      let error = ctypes.winLastError;
      this.close();
      if (error == Const.ERROR_NO_MORE_FILES) {
        return null;
      }
      throw new File.Error("iter (FindNextFile)", error, this._path);
    };

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
      // FIXME: If we start supporting "\\?\"-prefixed paths, do not forget
      // that "." and ".." are absolutely normal file names if _path starts
      // with such prefix
      for (let entry = this._next(); entry != null; entry = this._next()) {
        let name = entry.cFileName.readString();
        if (name == "." || name == "..") {
          continue;
        }
        return {
          value: new File.DirectoryIterator.Entry(entry, this._path),
          done: false,
        };
      }
      return { value: undefined, done: true };
    };

    File.DirectoryIterator.prototype.close = function close() {
      if (this._closed) {
        return;
      }
      this._closed = true;
      if (this._handle) {
        // We might not have a handle if the iterator is closed
        // before being used.
        throw_on_zero("FindClose", WinFile.FindClose(this._handle), this._path);
        this._handle = null;
      }
    };

    /**
     * Determine whether the directory exists.
     *
     * @return {boolean}
     */
    File.DirectoryIterator.prototype.exists = function exists() {
      return this._exists;
    };

    File.DirectoryIterator.Entry = function Entry(win_entry, parent) {
      if (
        !win_entry.dwFileAttributes ||
        !win_entry.ftLastAccessTime ||
        !win_entry.ftLastWriteTime
      ) {
        throw new TypeError();
      }

      // Copy the relevant part of |win_entry| to ensure that
      // our data is not overwritten prematurely.
      let isDir = !!(
        win_entry.dwFileAttributes & Const.FILE_ATTRIBUTE_DIRECTORY
      );
      let isSymLink = !!(
        win_entry.dwFileAttributes & Const.FILE_ATTRIBUTE_REPARSE_POINT
      );

      let winLastWriteDate = FILETIME_to_Date(
        win_entry.ftLastWriteTime,
        this._path
      );
      let winLastAccessDate = FILETIME_to_Date(
        win_entry.ftLastAccessTime,
        this._path
      );

      let name = win_entry.cFileName.readString();
      if (!name) {
        throw new TypeError("Empty name");
      }

      if (!parent) {
        throw new TypeError("Empty parent");
      }
      this._parent = parent;

      let path = Path.join(this._parent, name);

      SysAll.AbstractEntry.call(
        this,
        isDir,
        isSymLink,
        name,
        winLastWriteDate,
        winLastAccessDate,
        path
      );
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

    /**
     * Information on a file.
     *
     * To obtain the latest information on a file, use |File.stat|
     * (for an unopened file) or |File.prototype.stat| (for an
     * already opened file).
     *
     * @constructor
     */
    File.Info = function Info(stat, path) {
      let isDir = !!(stat.dwFileAttributes & Const.FILE_ATTRIBUTE_DIRECTORY);
      let isSymLink = !!(
        stat.dwFileAttributes & Const.FILE_ATTRIBUTE_REPARSE_POINT
      );

      let lastAccessDate = FILETIME_to_Date(stat.ftLastAccessTime, this._path);
      let lastWriteDate = FILETIME_to_Date(stat.ftLastWriteTime, this._path);

      let value = ctypes.UInt64.join(stat.nFileSizeHigh, stat.nFileSizeLow);
      let size = Type.uint64_t.importFromC(value);
      let winAttributes = {
        readOnly: !!(stat.dwFileAttributes & Const.FILE_ATTRIBUTE_READONLY),
        system: !!(stat.dwFileAttributes & Const.FILE_ATTRIBUTE_SYSTEM),
        hidden: !!(stat.dwFileAttributes & Const.FILE_ATTRIBUTE_HIDDEN),
      };

      SysAll.AbstractInfo.call(
        this,
        path,
        isDir,
        isSymLink,
        size,
        lastAccessDate,
        lastWriteDate,
        winAttributes
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
     * Performance note: if you have opened the file already,
     * method |File.prototype.stat| is generally much faster
     * than method |File.stat|.
     *
     * Platform-specific note: under Windows, if the file is
     * already opened without sharing of the read capability,
     * this function will fail.
     *
     * @return {File.Information}
     */
    File.stat = function stat(path) {
      let file = File.open(path, FILE_STAT_MODE, FILE_STAT_OPTIONS);
      try {
        return file.stat();
      } finally {
        file.close();
      }
    };
    // All of the following is required to ensure that File.stat
    // also works on directories.
    const FILE_STAT_MODE = {
      read: true,
    };
    const FILE_STAT_OPTIONS = {
      // Directories can be opened neither for reading(!) nor for writing
      winAccess: 0,
      // Directories can only be opened with backup semantics(!)
      winFlags: Const.FILE_FLAG_BACKUP_SEMANTICS,
      winDisposition: Const.OPEN_EXISTING,
    };

    /**
     * Set the file's access permission bits.
     */
    File.setPermissions = function setPermissions(path, options = {}) {
      if (!("winAttributes" in options)) {
        return;
      }
      let oldAttributes = WinFile.GetFileAttributes(path);
      if (oldAttributes == Const.INVALID_FILE_ATTRIBUTES) {
        throw new File.Error("setPermissions", ctypes.winLastError, path);
      }
      let newAttributes = toFileAttributes(
        options.winAttributes,
        oldAttributes
      );
      throw_on_zero(
        "setPermissions",
        WinFile.SetFileAttributes(path, newAttributes),
        path
      );
    };

    /**
     * Set the last access and modification date of the file.
     * The time stamp resolution is 1 second at best, but might be worse
     * depending on the platform.
     *
     * Performance note: if you have opened the file already in write mode,
     * method |File.prototype.stat| is generally much faster
     * than method |File.stat|.
     *
     * Platform-specific note: under Windows, if the file is
     * already opened without sharing of the write capability,
     * this function will fail.
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
      let file = File.open(path, FILE_SETDATES_MODE, FILE_SETDATES_OPTIONS);
      try {
        return file.setDates(accessDate, modificationDate);
      } finally {
        file.close();
      }
    };
    // All of the following is required to ensure that File.setDates
    // also works on directories.
    const FILE_SETDATES_MODE = {
      write: true,
    };
    const FILE_SETDATES_OPTIONS = {
      winAccess: Const.GENERIC_WRITE,
      // Directories can only be opened with backup semantics(!)
      winFlags: Const.FILE_FLAG_BACKUP_SEMANTICS,
      winDisposition: Const.OPEN_EXISTING,
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
     */
    File.removeDir = function(path, options = {}) {
      // We can't use File.stat here because it will follow the symlink.
      let attributes = WinFile.GetFileAttributes(path);
      if (attributes == Const.INVALID_FILE_ATTRIBUTES) {
        if (
          (!("ignoreAbsent" in options) || options.ignoreAbsent) &&
          ctypes.winLastError == Const.ERROR_FILE_NOT_FOUND
        ) {
          return;
        }
        throw new File.Error("removeEmptyDir", ctypes.winLastError, path);
      }
      if (attributes & Const.FILE_ATTRIBUTE_REPARSE_POINT) {
        // Unlike Unix symlinks, NTFS junctions or NTFS symlinks to
        // directories are directories themselves. OS.File.remove()
        // will not work for them.
        OS.File.removeEmptyDir(path, options);
        return;
      }
      exports.OS.Shared.AbstractFile.removeRecursive(path, options);
    };

    /**
     * Get the current directory by getCurrentDirectory.
     */
    File.getCurrentDirectory = function getCurrentDirectory() {
      // This function is more complicated than one could hope.
      //
      // This is due to two facts:
      // - the maximal length of a path under Windows is not completely
      //  specified (there is a constant MAX_PATH, but it is quite possible
      //  to create paths that are much larger, see bug 744413);
      // - if we attempt to call |GetCurrentDirectory| with a buffer that
      //  is too short, it returns the length of the current directory, but
      //  this length might be insufficient by the time we can call again
      //  the function with a larger buffer, in the (unlikely but possible)
      //  case in which the process changes directory to a directory with
      //  a longer name between both calls.
      //
      let buffer_size = 4096;
      while (true) {
        let array = new (ctypes.ArrayType(ctypes.char16_t, buffer_size))();
        let expected_size = throw_on_zero(
          "getCurrentDirectory",
          WinFile.GetCurrentDirectory(buffer_size, array)
        );
        if (expected_size <= buffer_size) {
          return array.readString();
        }
        // At this point, we are in a case in which our buffer was not
        // large enough to hold the name of the current directory.
        // Consequently, we need to increase the size of the buffer.
        // Note that, even in crazy scenarios, the loop will eventually
        // converge, as the length of the paths cannot increase infinitely.
        buffer_size = expected_size + 1 /* to store \0 */;
      }
    };

    /**
     * Set the current directory by setCurrentDirectory.
     */
    File.setCurrentDirectory = function setCurrentDirectory(path) {
      throw_on_zero(
        "setCurrentDirectory",
        WinFile.SetCurrentDirectory(path),
        path
      );
    };

    /**
     * Get/set the current directory by |curDir|.
     */
    Object.defineProperty(File, "curDir", {
      set(path) {
        this.setCurrentDirectory(path);
      },
      get() {
        return this.getCurrentDirectory();
      },
    });

    // Utility functions, used for error-handling

    /**
     * Turn the result of |open| into an Error or a File
     * @param {number} maybe The result of the |open| operation that may
     * represent either an error or a success. If -1, this function raises
     * an error holding ctypes.winLastError, otherwise it returns the opened file.
     * @param {string=} path The path of the file.
     */
    function error_or_file(maybe, path) {
      if (maybe == Const.INVALID_HANDLE_VALUE) {
        throw new File.Error("open", ctypes.winLastError, path);
      }
      return new File(maybe, path);
    }

    /**
     * Utility function to sort errors represented as "0" from successes.
     *
     * @param {string=} operation The name of the operation. If unspecified,
     * the name of the caller function.
     * @param {number} result The result of the operation that may
     * represent either an error or a success. If 0, this function raises
     * an error holding ctypes.winLastError, otherwise it returns |result|.
     * @param {string=} path The path of the file.
     */
    function throw_on_zero(operation, result, path) {
      if (result == 0) {
        throw new File.Error(operation, ctypes.winLastError, path);
      }
      return result;
    }

    /**
     * Helper used by both versions of setPermissions
     */
    function toFileAttributes(winAttributes, oldDwAttrs) {
      if ("readOnly" in winAttributes) {
        if (winAttributes.readOnly) {
          oldDwAttrs |= Const.FILE_ATTRIBUTE_READONLY;
        } else {
          oldDwAttrs &= ~Const.FILE_ATTRIBUTE_READONLY;
        }
      }
      if ("system" in winAttributes) {
        if (winAttributes.system) {
          oldDwAttrs |= Const.FILE_ATTRIBUTE_SYSTEM;
        } else {
          oldDwAttrs &= ~Const.FILE_ATTRIBUTE_SYSTEM;
        }
      }
      if ("hidden" in winAttributes) {
        if (winAttributes.hidden) {
          oldDwAttrs |= Const.FILE_ATTRIBUTE_HIDDEN;
        } else {
          oldDwAttrs &= ~Const.FILE_ATTRIBUTE_HIDDEN;
        }
      }
      return oldDwAttrs;
    }

    File.Win = exports.OS.Win.File;
    File.Error = SysAll.Error;
    exports.OS.File = File;
    exports.OS.Shared.Type = Type;

    Object.defineProperty(File, "POS_START", { value: SysAll.POS_START });
    Object.defineProperty(File, "POS_CURRENT", { value: SysAll.POS_CURRENT });
    Object.defineProperty(File, "POS_END", { value: SysAll.POS_END });
  })(this);
}
