/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Synchronous front-end for the JavaScript OS.File library.
 * Unix implementation.
 *
 * This front-end is meant to be imported by a worker thread.
 */

{
  if (typeof Components != "undefined") {
    // We do not wish osfile_unix_front.jsm to be used directly as a main thread
    // module yet.

    throw new Error("osfile_unix_front.jsm cannot be used from the main thread yet");
  }
  importScripts("resource://gre/modules/osfile/osfile_shared.jsm");
  importScripts("resource://gre/modules/osfile/osfile_unix_back.jsm");
  (function(exports) {
     "use strict";

     // exports.OS.Unix is created by osfile_unix_back.jsm
     if (exports.OS.File) {
       return; // Avoid double-initialization
     }
     exports.OS.Unix.File._init();
     let Const = exports.OS.Constants.libc;
     let UnixFile = exports.OS.Unix.File;
     let LOG = OS.Shared.LOG.bind(OS.Shared, "Unix front-end");

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
         operation = operation || "unknown operation";
         throw new File.Error(operation, Const.EBADF);
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
           // Call |close(fd)|, detach finalizer
           if (UnixFile.close(fd) == -1) {
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
         return throw_on_negative("read",
           UnixFile.read(this.fd, buffer, nbytes)
         );
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
         return throw_on_negative("write",
           UnixFile.write(this.fd, buffer, nbytes)
         );
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
         // OS.File.POS_START == OS.Constants.libc.SEEK_SET
         // OS.File.POS_CURRENT == OS.Constants.libc.SEEK_CUR
         // OS.File.POS_END == OS.Constants.libc.SEEK_END
         whence = (whence == undefined)?OS.Constants.libc.SEEK_SET:whence;
         return throw_on_negative("setPosition",
           UnixFile.lseek(this.fd, pos, whence)
         );
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
      * |unixErrno|, which holds the OS-specific error
      * constant. If you need this level of detail, you may match the value
      * of this field against the error constants of |OS.Constants.libc|.
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
     File.Error = function(operation, errno) {
       operation = operation || "unknown operation";
       OS.Shared.Error.call(this, operation);
       this.unixErrno = errno || ctypes.errno;
     };
     File.Error.prototype = new OS.Shared.Error();
     File.Error.prototype.toString = function toString() {
       return "Unix error " + this.unixErrno +
         " during operation " + this.operation +
         " (" + UnixFile.strerror(this.unixErrno).readString() + ")";
     };

     /**
      * |true| if the error was raised because a file or directory
      * already exists, |false| otherwise.
      */
     Object.defineProperty(File.Error.prototype, "becauseExists", {
       get: function becauseExists() {
         return this.unixErrno == OS.Constants.libc.EEXISTS;
       }
     });
     /**
      * |true| if the error was raised because a file or directory
      * does not exist, |false| otherwise.
      */
     Object.defineProperty(File.Error.prototype, "becauseNoSuchFile", {
       get: function becauseNoSuchFile() {
         return this.unixErrno == OS.Constants.libc.ENOENT;
       }
     });

     // Constant used to normalize options.
     const noOptions = {};

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
     File.open = function Unix_open(path, mode, options) {
       options = options || noOptions;
       let omode = options.unixMode || DEFAULT_UNIX_MODE;
       let flags;
       if (options.unixFlags) {
         flags = options.unixFlags;
       } else {
         mode = OS.Shared._aux.normalizeOpenMode(mode);
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
         } else /*append*/ {
           if (mode.existing) {
             flags |= Const.O_APPEND;
           } else {
             flags |= Const.O_APPEND | Const.O_CREAT;
           }
         }
       }
       return error_or_file(UnixFile.open(path, flags, omode));
     };

     /**
      * Remove an existing file.
      *
      * @param {string} path The name of the file.
      */
     File.remove = function remove(path) {
       throw_on_negative("remove",
         UnixFile.unlink(path)
       );
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
       File.copy = function copyfile(sourcePath, destPath, options) {
         options = options || noOptions;
         let flags = Const.COPYFILE_DATA;
         if (options.noOverwrite) {
           flags |= Const.COPYFILE_EXCL;
         }
         throw_on_negative("copy",
           UnixFile.copyfile(sourcePath, destPath, null, flags)
         );
       };

       // This implementation uses |copyfile(3)|, from the BSD library.
       // Adding moving of hierarchies and/or attributes is just a flag
       // away.
       File.move = function movefile(sourcePath, destPath, options) {
         // This implementation uses |copyfile(3)|, from the BSD library.
         // It can move directory hierarchies.
         options = options || noOptions;
         let flags = Const.COPYFILE_DATA | Const.COPYFILE_MOVE;
         if (options.noOverwrite) {
           flags |= Const.COPYFILE_EXCL;
         }
         throw_on_negative("move",
           UnixFile.copyfile(sourcePath, destPath, null, flags)
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
        *
        * @throws {OS.File.Error} In case of error.
        */
       let pump;

       // A buffer used by |pump_userland|
       let pump_buffer = null;

       // An implementation of |pump| using |read|/|write|
       let pump_userland = function pump_userland(source, dest, options) {
         let options = options || noOptions;
         let bufSize = options.bufSize || 4096;
         let nbytes = options.nbytes || Infinity;
         if (!pump_buffer || pump_buffer.length < bufSize) {
           pump_buffer = new (ctypes.ArrayType(ctypes.char))(bufSize);
         }
         let read = source.read.bind(source);
         let write = dest.write.bind(dest);
         // Perform actual copy
         let total_read = 0;
         while (true) {
           let chunk_size = Math.min(nbytes, bufSize);
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
         pump = function pump_splice(source, dest, options) {
           let options = options || noOptions;
           let nbytes = options.nbytes || Infinity;
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
               let bytes_read = throw_on_negative("pump",
                 UnixFile.splice(source_fd, null,
                 pipe_write, null, chunk_size, 0)
               );
               if (!bytes_read) {
                 break;
               }
               total_read += bytes_read;
               let bytes_written = throw_on_negative(
                 "pump",
                 UnixFile.splice(pipe_read, null,
                   dest_fd, null, bytes_read,
                     (bytes_read == chunk_size)?Const.SPLICE_F_MORE:0
               ));
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
                 source.setPosition(-total_read, OS.File.POS_CURRENT);
               }
               if (total_written) {
                 dest.setPosition(-total_written, OS.File.POS_CURRENT);
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
     File.copy = function copy(sourcePath, destPath, options) {
       options = options || noOptions;
       let source, dest;
       let result;
       try {
         source = File.open(sourcePath);
         if (options.noOverwrite) {
           dest = File.open(destPath, {create:true});
         } else {
           dest = File.open(destPath, {write:true});
         }
         result = pump(source, dest, options);
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

     // Implement |move| using |rename| (wherever possible) or |copy|
     // (if files are on distinct devices).
     File.move = function move(sourcePath, destPath, options) {
       // An implementation using |rename| whenever possible or
       // |File.pump| when required, for other Unices.
       // It can move directories on one file system, not
       // across file systems

       options = options || noOptions;

       // If necessary, fail if the destination file exists
       if (options.noOverwrite) {
         let fd = UnixFile.open(destPath, Const.O_RDONLY, 0);
         if (fd != -1) {
           fd.dispose();
           // The file exists and we have access
           throw new File.Error("move", Const.EEXIST);
         } else if (ctypes.errno == Const.EACCESS) {
           // The file exists and we don't have access
           throw new File.Error("move", Const.EEXIST);
         }
       }

       // If we can, rename the file
       let result = UnixFile.rename(sourcePath, destPath);
       if (result != -1)
         return;

       // If the error is not EXDEV ("not on the same device"),
       // throw it.
       if (ctypes.errno != Const.EXDEV) {
         throw new File.Error();
       }

         // Otherwise, copy and remove.
         File.copy(sourcePath, destPath, options);
         // FIXME: Clean-up in case of copy error?
         File.remove(sourcePath);
       };

     } // End of definition of copy/move


     /**
      * Get/set the current directory.
      */
     Object.defineProperty(File, "curDir", {
         set: function(path) {
           throw_on_negative("curDir",
             UnixFile.chdir(path)
           );
         },
         get: function() {
           let path = UnixFile.getwd(null);
           if (path.isNull()) {
             throw new File.Error("getwd");
           }
           return ctypes.CDataFinalizer(path, UnixFile.free);
         }
       }
     );

     // Utility functions

     /**
      * Turn the result of |open| into an Error or a File
      */
     function error_or_file(maybe) {
       if (maybe == -1) {
         throw new File.Error("open");
       }
       return new File(maybe);
     }

     /**
      * Utility function to sort errors represented as "-1" from successes.
      *
      * @param {number} result The result of the operation that may
      * represent either an error or a success. If -1, this function raises
      * an error holding ctypes.errno, otherwise it returns |result|.
      * @param {string=} operation The name of the operation. If unspecified,
      * the name of the caller function.
      */
     function throw_on_negative(operation, result) {
       if (result < 0) {
         throw new File.Error(operation);
       }
       return result;
     }

     File.POS_START = exports.OS.Constants.libc.SEEK_SET;
     File.POS_CURRENT = exports.OS.Constants.libc.SEEK_CUR;
     File.POS_END = exports.OS.Constants.libc.SEEK_END;

     File.Unix = exports.OS.Unix.File;
     exports.OS.File = File;
   })(this);
}
