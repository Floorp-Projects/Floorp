/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Synchronous front-end for the JavaScript OS.File library.
 * Windows implementation.
 *
 * This front-end is meant to be imported by a worker thread.
 */

{
  if (typeof Components != "undefined") {
    // We do not wish osfile_win_front.jsm to be used directly as a main thread
    // module yet.
    throw new Error("osfile_win_front.jsm cannot be used from the main thread yet");
  }

  importScripts("resource://gre/modules/osfile/osfile_shared.jsm");
  importScripts("resource://gre/modules/osfile/osfile_win_back.jsm");
  importScripts("resource://gre/modules/osfile/ospath_win_back.jsm");

  (function(exports) {
     "use strict";

     // exports.OS.Win is created by osfile_win_back.jsm
     if (exports.OS.File) {
       return; // Avoid double-initialization
     }
     exports.OS.Win.File._init();
     let Const = exports.OS.Constants.Win;
     let WinFile = exports.OS.Win.File;
     let LOG = OS.Shared.LOG.bind(OS.Shared, "Win front-end");

     // Mutable thread-global data
     // In the Windows implementation, methods |read| and |write|
     // require passing a pointer to an int32 to determine how many
     // bytes have been read/written. In C, this is a benigne operation,
     // but in js-ctypes, this has a cost. Rather than re-allocating a
     // C int32 and a C int32* for each |read|/|write|, we take advantage
     // of the fact that the state is thread-private -- hence that two
     // |read|/|write| operations cannot take place at the same time --
     // and we use the following global mutable values:
     let gBytesRead = new ctypes.int32_t(-1);
     let gBytesReadPtr = gBytesRead.address();
     let gBytesWritten = new ctypes.int32_t(-1);
     let gBytesWrittenPtr = gBytesWritten.address();

     /**
      * Representation of a file.
      *
      * You generally do not need to call this constructor yourself. Rather,
      * to open a file, use function |OS.File.open|.
      *
      * @param fd A OS-specific file descriptor.
      * @constructor
      */
     let File = function File(fd) {
       this._fd = fd;
     };
     File.prototype = {
       /**
        * If the file is open, this returns the file descriptor.
        * Otherwise, throw a |File.Error|.
        */
       get fd() {
         return this._fd;
       },

       // Placeholder getter, used to replace |get fd| once
       // the file is closed.
       _nofd: function nofd(operation) {
         operation = operation ||
             this._nofd.caller.name ||
             "unknown operation";
         throw new File.Error(operation, Const.INVALID_HANDLE_VALUE);
       },

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
       close: function close() {
         if (this._fd) {
           let fd = this._fd;
           this._fd = null;
           delete this.fd;
           Object.defineProperty(this, "fd", {get: File.prototype._nofd});
           // Call CloseHandle(fd), detach finalizer
           if (fd.dispose() == 0) {
             this._closeResult = new File.Error("close", ctypes.errno);
           }
         }
         if (this._closeResult) {
           throw this._closeResult;
         }
         return;
       },
       _closeResult: null,

       /**
        * Read some bytes from a file.
        *
        * @param {ArrayBuffer} buffer A buffer for holding the data
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
       read: function read(buffer, nbytes, options) {
         // |gBytesReadPtr| is a pointer to |gBytesRead|.
         throw_on_zero("read",
           WinFile.ReadFile(this.fd, buffer, nbytes, gBytesReadPtr, null)
         );
         return gBytesRead.value;
       },

       /**
        * Write some bytes to a file.
        *
        * @param {ArrayBuffer} buffer A buffer holding the data that must be
        * written.
        * @param {number} nbytes The number of bytes to write. It must not
        * exceed the size of |buffer| in bytes.
        * @param {*=} options Additional options for writing. Ignored in
        * this implementation.
        *
        * @return {number} The number of bytes effectively written.
        * @throws {OS.File.Error} In case of I/O error.
        */
       write: function write(buffer, nbytes, options) {
         // |gBytesWrittenPtr| is a pointer to |gBytesWritten|.
         throw_on_zero("write",
           WinFile.WriteFile(this.fd, buffer, nbytes, gBytesWrittenPtr, null)
         );
         return gBytesWritten.value;
       },

       /**
        * Return the current position in the file.
        */
       getPosition: function getPosition(pos) {
         return this.setPosition(0, File.POS_CURRENT);
       },

       /**
        * Change the current position in the file.
        *
        * @param {number} pos The new position. Whether this position
        * is considered from the current position, from the start of
        * the file or from the end of the file is determined by
        * argument |whence|.  Note that |pos| may exceed the length of
        * the file.
        * @param {number=} whence The reference position. If omitted or
        * |OS.File.POS_START|, |pos| is taken from the start of the file.
        * If |OS.File.POS_CURRENT|, |pos| is taken from the current position
        * in the file. If |OS.File.POS_END|, |pos| is taken from the end of
        * the file.
        *
        * @return The new position in the file.
        */
       setPosition: function setPosition(pos, whence) {
         // We are cheating to avoid one unnecessary conversion:
         // In this implementation,
         // OS.File.POS_START == OS.Constants.Win.FILE_BEGIN
         // OS.File.POS_CURRENT == OS.Constants.Win.FILE_CURRENT
         // OS.File.POS_END == OS.Constants.Win.FILE_END
         whence = (whence == undefined)?Const.FILE_BEGIN:whence;
         return throw_on_negative("setPosition",
	   WinFile.SetFilePointer(this.fd, pos, null, whence));
       }
     };

     /**
      * A File-related error.
      *
      * To obtain a human-readable error message, use method |toString|.
      * To determine the cause of the error, use the various |becauseX|
      * getters. To determine the operation that failed, use field
      * |operation|.
      *
      * Additionally, this implementation offers a field
      * |winLastError|, which holds the OS-specific error
      * constant. If you need this level of detail, you may match the value
      * of this field against the error constants of |OS.Constants.Win|.
      *
      * @param {string=} operation The operation that failed. If unspecified,
      * the name of the calling function is taken to be the operation that
      * failed.
      * @param {number=} lastError The OS-specific constant detailing the
      * reason of the error. If unspecified, this is fetched from the system
      * status.
      *
      * @constructor
      * @extends {OS.Shared.Error}
      */
     File.Error = function FileError(operation, lastError) {
       operation = operation || File.Error.caller.name || "unknown operation";
       OS.Shared.Error.call(this, operation);
       this.winLastError = lastError || ctypes.winLastError;
     };
     File.Error.prototype = new OS.Shared.Error();
     File.Error.prototype.toString = function toString() {
         let buf = new (ctypes.ArrayType(ctypes.jschar, 1024))();
         let result = WinFile.FormatMessage(
           OS.Constants.Win.FORMAT_MESSAGE_FROM_SYSTEM |
           OS.Constants.Win.FORMAT_MESSAGE_IGNORE_INSERTS,
           null,
           /* The error number */ this.winLastError,
           /* Default language */ 0,
           /* Output buffer*/     buf,
           /* Minimum size of buffer */ 1024,
           /* Format args*/       null
         );
         if (!result) {
           buf = "additional error " +
             ctypes.winLastError +
             " while fetching system error message";
         }
         return "Win error " + this.winLastError + " during operation "
           + this.operation + " (" + buf.readString() + " )";
     };

     /**
      * |true| if the error was raised because a file or directory
      * already exists, |false| otherwise.
      */
     Object.defineProperty(File.Error.prototype, "becauseExists", {
       get: function becauseExists() {
         return this.winLastError == OS.Constants.Win.ERROR_FILE_EXISTS;
       }
     });
     /**
      * |true| if the error was raised because a file or directory
      * does not exist, |false| otherwise.
      */
     Object.defineProperty(File.Error.prototype, "becauseNoSuchFile", {
       get: function becauseNoSuchFile() {
         return this.winLastError == OS.Constants.Win.ERROR_FILE_NOT_FOUND;
       }
     });

     // Constant used to normalize options.
     const noOptions = {};

     // The default sharing mode for opening files: files are not
     // locked against being reopened for reading/writing or against
     // being deleted by the same process or another process.
     // This is consistent with the default Unix policy.
     const DEFAULT_SHARE = Const.FILE_SHARE_READ |
       Const.FILE_SHARE_WRITE | Const.FILE_SHARE_DELETE;

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
      *  on the other fields of |mode|. If neither |truncate| nor
      *  |create| is specified, the file is opened for appending.
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
     File.open = function Win_open(path, mode, options) {
       options = options || noOptions;

       let share = options.winShare || DEFAULT_SHARE;
       let security = options.winSecurity || null;
       let flags = options.winFlags || DEFAULT_FLAGS;
       let template = options.winTemplate?options.winTemplate._fd:null;
       let access;
       let disposition;
       if ("winAccess" in options && "winDisposition" in options) {
         access = options.winAccess;
         disposition = options.winDisposition;
       } else if (("winAccess" in options && !("winDisposition" in options))
                 ||(!("winAccess" in options) && "winDisposition" in options)) {
         throw new TypeError("OS.File.open requires either both options " +
           "winAccess and winDisposition or neither");
       } else {
         mode = OS.Shared._aux.normalizeOpenMode(mode);
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
         } else /*append*/ {
           if (mode.existing) {
             disposition = Const.OPEN_EXISTING;
           } else {
             disposition = Const.OPEN_ALWAYS;
           }
         }
       }
       let file = error_or_file(WinFile.CreateFile(path,
         access, share, security, disposition, flags, template));
       if (!(mode.trunc && mode.existing)) {
         return file;
       }
       // Now, perform manual truncation
       file.setPosition(0, File.POS_START);
       throw_on_zero("open",
         WinFile.SetEndOfFile(file.fd));
       return file;
     };

     /**
      * Remove an existing file.
      *
      * @param {string} path The name of the file.
      * @throws {OS.File.Error} In case of I/O error.
      */
     File.remove = function remove(path) {
       throw_on_zero("remove",
         WinFile.DeleteFile(path));
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
     File.copy = function copy(sourcePath, destPath, options) {
       options = options || noOptions;
       throw_on_zero("copy",
         WinFile.CopyFile(sourcePath, destPath, options.noOverwrite || false)
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
     File.move = function move(sourcePath, destPath, options) {
       options = options || noOptions;
       let flags;
       if (options.noOverwrite) {
         flags = Const.MOVEFILE_COPY_ALLOWED;
       } else {
         flags = Const.MOVEFILE_COPY_ALLOWED | Const.MOVEFILE_REPLACE_EXISTING;
       }
       throw_on_zero("move",
         WinFile.MoveFileEx(sourcePath, destPath, flags)
       );
     };

     /**
      * A global value used to receive data during a
      * |FindFirstFile|/|FindNextFile|.
      */
     let gFindData = new OS.Shared.Type.FindData.implementation();
     let gFindDataPtr = gFindData.address();

     /**
      * A global value used to receive data during time conversions.
      */
     let gSystemTime = new OS.Shared.Type.SystemTime.implementation();
     let gSystemTimePtr = gSystemTime.address();

     /**
      * Utility function: convert a FILETIME to a JavaScript Date.
      */
     let FILETIME_to_Date = function FILETIME_to_Date(fileTime) {
       LOG("fileTimeToDate:", fileTime);
       if (fileTime == null) {
         throw new TypeError("Expecting a non-null filetime");
       }
       LOG("fileTimeToDate normalized:", fileTime);
       throw_on_zero("FILETIME_to_Date", WinFile.FileTimeToSystemTime(fileTime.address(),
                                                  gSystemTimePtr));
       return new Date(gSystemTime.wYear, gSystemTime.wMonth,
                       gSystemTime.wDay, gSystemTime.wHour,
                       gSystemTime.wMinute, gSystemTime.wSecond,
                       gSystemTime.wMilliSeconds);
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
       if (options && options.winPattern) {
         this._pattern = path + "\\" + options.winPattern;
       } else {
         this._pattern = path + "\\*";
       }
       this._handle = null;
       this._path = path;
       this._started = false;
     };
     File.DirectoryIterator.prototype = {
       __iterator__: function __iterator__() {
         return this;
       },

       /**
        * Fetch the next entry in the directory.
        *
        * @return null If we have reached the end of the directory.
        */
       _next: function _next() {
         // Iterator is not fully initialized yet. Finish
         // initialization.
         if (!this._started) {
            this._started = true;
            this._handle = WinFile.FindFirstFile(this._pattern, gFindDataPtr);
            if (this._handle == null) {
              let error = ctypes.winLastError;
              if (error == Const.ERROR_FILE_NOT_FOUND) {
                this.close();
                return null;
              } else {
                throw new File.Error("iter (FindFirstFile)", error);
              }
            }
            return gFindData;
         }

         // We have closed this iterator already.
         if (!this._handle) {
           return null;
         }

         if (WinFile.FindNextFile(this._handle, gFindDataPtr)) {
           return gFindData;
         } else {
           let error = ctypes.winLastError;
           this.close();
           if (error == Const.ERROR_NO_MORE_FILES) {
              return null;
           } else {
              throw new File.Error("iter (FindNextFile)", error);
           }
         }
       },
       /**
        * Return the next entry in the directory, if any such entry is
        * available.
        *
        * Skip special directories "." and "..".
        *
        * @return {File.Entry} The next entry in the directory.
        * @throws {StopIteration} Once all files in the directory have been
        * encountered.
        */
       next: function next() {
         // FIXME: If we start supporting "\\?\"-prefixed paths, do not forget
         // that "." and ".." are absolutely normal file names if _path starts
         // with such prefix
         for (let entry = this._next(); entry != null; entry = this._next()) {
           let name = entry.cFileName.readString();
           if (name == "." || name == "..") {
             continue;
           }
           return new File.DirectoryIterator.Entry(entry, this._path);
         }
         throw StopIteration;
       },
       close: function close() {
         if (!this._handle) {
           return;
         }
         WinFile.FindClose(this._handle);
         this._handle = null;
       }
     };
     File.DirectoryIterator.Entry = function Entry(win_entry, parent) {
       // Copy the relevant part of |win_entry| to ensure that
       // our data is not overwritten prematurely.
       if (!win_entry.dwFileAttributes) {
         throw new TypeError();
       }
       this._dwFileAttributes = win_entry.dwFileAttributes;
       this._name = win_entry.cFileName.readString();
       if (!this._name) {
         throw new TypeError("Empty name");
       }
       this._ftCreationTime = win_entry.ftCreationTime;
       if (!win_entry.ftCreationTime) {
         throw new TypeError();
       }
       this._ftLastAccessTime = win_entry.ftLastAccessTime;
       if (!win_entry.ftLastAccessTime) {
         throw new TypeError();
       }
       this._ftLastWriteTime = win_entry.ftLastWriteTime;
       if (!win_entry.ftLastWriteTime) {
         throw new TypeError();
       }
       if (!parent) {
         throw new TypeError("Empty parent");
       }
       this._parent = parent;
     };
     File.DirectoryIterator.Entry.prototype = {
       /**
        * |true| if the entry is a directory, |false| otherwise
        */
       get isDir() {
         return this._dwFileAttributes & Const.FILE_ATTRIBUTE_DIRECTORY;
       },
       /**
        * |true| if the entry is a symbolic link, |false| otherwise
        */
       get isLink() {
         return this._dwFileAttributes & Const.FILE_ATTRIBUTE_REPARSE_POINT;
       },
       /**
        * The name of the entry.
        * @type {string}
        */
       get name() {
         return this._name;
       },
       /**
        * The creation time of this file.
        * @type {Date}
        */
       get winCreationTime() {
         let date = FILETIME_to_Date(this._ftCreationTime);
         delete this.winCreationTime;
         Object.defineProperty(this, "winCreationTime", {value: date});
         return date;
       },
       /**
        * The last modification time of this file.
        * @type {Date}
        */
       get winLastWriteTime() {
         let date = FILETIME_to_Date(this._ftLastWriteTime);
         delete this.winLastWriteTime;
         Object.defineProperty(this, "winLastWriteTime", {value: date});
         return date;
       },
       /**
        * The last access time of this file.
        * @type {Date}
        */
       get winLastAccessTime() {
         let date = FILETIME_to_Date(this._ftLastAccessTime);
         delete this.winLastAccessTime;
         Object.defineProperty(this, "winLastAccessTime", {value: date});
         return date;
       },
       /**
        * The full path to the entry.
        * @type {string}
        */
       get path() {
         delete this.path;
         let path = OS.Win.Path.join(this._parent, this.name);
         Object.defineProperty(this, "path", {value: path});
         return path;
       }
     };

     /**
      * Get/set the current directory.
      */
     Object.defineProperty(File, "curDir", {
         set: function(path) {
           throw_on_zero("set curDir",
             WinFile.SetCurrentDirectory(path));
         },
         get: function() {
           // This function is more complicated than one could hope.
           //
           // This is due to two facts:
           // - the maximal length of a path under Windows is not completely
           //  specified (there is a constant MAX_PATH, but it is quite possible
           //  to create paths that are much larger, see bug 744413);
           // - if we attempt to call |GetCurrentDirectory| with a buffer that
           //  is too short, it returns the length of the current directory, but
           //  this length might be insufficient by the time we can call again
           //  the function with a larger buffer, in the (unlikely byt possible)
           //  case in which the process changes directory to a directory with
           //  a longer name between both calls.

           let buffer_size = 4096;
           while (true) {
             let array = new (ctypes.ArrayType(ctypes.jschar, buffer_size))();
             let expected_size = throw_on_zero("get curDir",
               WinFile.GetCurrentDirectory(buffer_size, array)
             );
             if (expected_size <= buffer_size) {
               return array.readString();
             }
             // At this point, we are in a case in which our buffer was not
             // large enough to hold the name of the current directory.
             // Consequently

             // Note that, even in crazy scenarios, the loop will eventually
             // converge, as the length of the paths cannot increase infinitely.
             buffer_size = expected_size;
           }
         }
       }
     );

     // Utility functions, used for error-handling
     function error_or_file(maybe) {
       if (maybe == exports.OS.Constants.Win.INVALID_HANDLE_VALUE) {
         throw new File.Error("open");
       }
       return new File(maybe);
     }
     function throw_on_zero(operation, result) {
       if (result == 0) {
         throw new File.Error(operation);
       }
       return result;
     }
     function throw_on_negative(operation, result) {
       if (result < 0) {
         throw new File.Error(operation);
       }
       return result;
     }
     function throw_on_null(operation, result) {
       if (result == null || (result.isNull && result.isNull())) {
         throw new File.Error(operation);
       }
       return result;
     }




     // Constants

     // Constants for File.prototype.setPosition
     File.POS_START = Const.FILE_BEGIN;
     File.POS_CURRENT = Const.FILE_CURRENT;
     File.POS_END = Const.FILE_END;

     File.Win = exports.OS.Win.File;
     exports.OS.File = File;
   })(this);
}
