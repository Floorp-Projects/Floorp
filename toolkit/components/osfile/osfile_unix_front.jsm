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
       exports.OS.Shared.AbstractFile.call(this, fd);
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
           this._closeResult = new File.Error("close");
         }
       }
       if (this._closeResult) {
         throw this._closeResult;
       }
       return;
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
       return throw_on_negative("read",
         UnixFile.read(this.fd, buffer, nbytes)
       );
     };

     /**
      * Write some bytes to a file.
      *
      * @param {C pointer} buffer A buffer holding the data that must be
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
       return throw_on_negative("write",
         UnixFile.write(this.fd, buffer, nbytes)
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
       return throw_on_negative("setPosition",
         UnixFile.lseek(this.fd, pos, whence)
       );
     };

     /**
      * Fetch the information on the file.
      *
      * @return File.Info The information on |this| file.
      */
     File.prototype.stat = function stat() {
       throw_on_negative("stat", UnixFile.fstat(this.fd, gStatDataPtr));
         return new File.Info(gStatData);
     };

     /**
      * Flushes the file's buffers and causes all buffered data
      * to be written.
      *
      * @throws {OS.File.Error} In case of I/O error.
      */
     File.prototype.flush = function flush() {
       throw_on_negative("flush", UnixFile.fsync(this.fd));
     };


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
      * Checks if a file exists
      *
      * @param {string} path The path to the file.
      *
      * @return {bool} true if the file exists, false otherwise.
      */
     File.exists = function Unix_exists(path) {
       if (UnixFile.access(path, OS.Constants.libc.F_OK) == -1) {
         return false;
       } else {
         return true;
       }
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
      * Remove an empty directory.
      *
      * @param {string} path The name of the directory to remove.
      * @param {*=} options Additional options.
      *   - {bool} ignoreAbsent If |true|, do not fail if the
      *     directory does not exist yet.
      */
     File.removeEmptyDir = function removeEmptyDir(path, options) {
       options = options || noOptions;
       let result = UnixFile.rmdir(path);
       if (result == -1) {
         if (options.ignoreAbsent && ctypes.errno == Const.ENOENT) {
           return;
         }
         throw new File.Error("removeEmptyDir");
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
      * - {bool} ignoreExisting If |true|, do not fail if the
      * directory already exists.
      */
     File.makeDir = function makeDir(path, options) {
       options = options || noOptions;
       let omode = options.unixMode || DEFAULT_UNIX_MODE_DIR;
       let result = UnixFile.mkdir(path, omode);
       if (result != -1 ||
           options.ignoreExisting && ctypes.errno == OS.Constants.libc.EEXIST) {
        return;
       }
       throw new File.Error("makeDir");
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
         options = options || noOptions;
         let bufSize = options.bufSize || 4096;
         let nbytes = options.nbytes || Infinity;
         if (!pump_buffer || pump_buffer.length < bufSize) {
           pump_buffer = new (ctypes.ArrayType(ctypes.char))(bufSize);
         }
         let read = source._read.bind(source);
         let write = dest._write.bind(dest);
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
           options = options || noOptions;
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
             dest = File.open(destPath, {trunc:true});
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
     } // End of definition of copy

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
       // or if the error is EXDEV and we have passed an option
       // that prevents us from crossing devices, throw the
       // error.
       if (ctypes.errno != Const.EXDEV || options.noCopy) {
         throw new File.Error("move");
       }

       // Otherwise, copy and remove.
       File.copy(sourcePath, destPath, options);
       // FIXME: Clean-up in case of copy error?
       File.remove(sourcePath);
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
       let dir = throw_on_null("DirectoryIterator", UnixFile.opendir(path));
       this._dir = dir;
       this._path = path;
     };
     File.DirectoryIterator.prototype = Object.create(exports.OS.Shared.AbstractFile.AbstractIterator.prototype);

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
     File.DirectoryIterator.prototype.next = function next() {
       if (!this._dir) {
         throw StopIteration;
       }
       for (let entry = UnixFile.readdir(this._dir);
            entry != null && !entry.isNull();
            entry = UnixFile.readdir(this._dir)) {
         let contents = entry.contents;
         if (contents.d_type == OS.Constants.libc.DT_DIR) {
           let name = contents.d_name.readString();
           if (name == "." || name == "..") {
             continue;
           }
         }
         return new File.DirectoryIterator.Entry(contents, this._path);
       }
       this.close();
       throw StopIteration;
     };

     /**
      * Close the iterator and recover all resources.
      * You should call this once you have finished iterating on a directory.
      */
     File.DirectoryIterator.prototype.close = function close() {
       if (!this._dir) return;
       UnixFile.closedir(this._dir);
       this._dir = null;
     };

     /**
      * Return directory as |File|
      */
     File.DirectoryIterator.prototype.unixAsFile = function unixAsFile() {
       if (!this._dir) throw File.Error.closed();
       return error_or_file(UnixFile.dirfd(this._dir));
     };

     /**
      * An entry in a directory.
      */
     File.DirectoryIterator.Entry = function Entry(unix_entry, parent) {
       // Copy the relevant part of |unix_entry| to ensure that
       // our data is not overwritten prematurely.
       this._d_type = unix_entry.d_type;
       this._name = unix_entry.d_name.readString();
       this._parent = parent;
     };
     File.DirectoryIterator.Entry.prototype = {
       /**
        * |true| if the entry is a directory, |false| otherwise
        */
       get isDir() {
         return this._d_type == OS.Constants.libc.DT_DIR;
       },

       /**
        * |true| if the entry is a symbolic link, |false| otherwise
        */
       get isSymLink() {
         return this._d_type == OS.Constants.libc.DT_LNK;
       },

       /**
        * The name of the entry.
        * @type {string}
        */
       get name() {
         return this._name;
       },

       /**
        * The full path to the entry.
        */
       get path() {
         delete this.path;
         let path = OS.Unix.Path.join(this._parent, this.name);
         Object.defineProperty(this, "path", {value: path});
         return path;
       }
     };

     /**
      * Return a version of an instance of
      * File.DirectoryIterator.Entry that can be sent from a worker
      * thread to the main thread. Note that deserialization is
      * asymmetric and returns an object with a different
      * implementation.
      */
     File.DirectoryIterator.Entry.toMsg = function toMsg(value) {
       if (!value instanceof File.DirectoryIterator.Entry) {
         throw new TypeError("parameter of " +
           "File.DirectoryIterator.Entry.toMsg must be a " +
           "File.DirectoryIterator.Entry");
       }
       let serialized = {};
       for (let key in File.DirectoryIterator.Entry.prototype) {
         serialized[key] = value[key];
       }
       return serialized;
     };

     let gStatData = new OS.Shared.Type.stat.implementation();
     let gStatDataPtr = gStatData.address();
     let MODE_MASK = 4095 /*= 07777*/;
     File.Info = function Info(stat) {
       this._st_mode = stat.st_mode;
       this._st_uid = stat.st_uid;
       this._st_gid = stat.st_gid;
       this._st_atime = stat.st_atime;
       this._st_mtime = stat.st_mtime;
       this._st_ctime = stat.st_ctime;
       // Some platforms (e.g. MacOS X, some BSDs) store a file creation date
       if ("OSFILE_OFFSETOF_STAT_ST_BIRTHTIME" in OS.Constants.libc) {
         this._st_birthtime = stat.st_birthtime;
       }
       this._st_size = stat.st_size;
     };
     File.Info.prototype = {
       /**
        * |true| if this file is a directory, |false| otherwise
        */
       get isDir() {
         return (this._st_mode & OS.Constants.libc.S_IFMT) == OS.Constants.libc.S_IFDIR;
       },
       /**
        * |true| if this file is a symbolink link, |false| otherwise
        */
       get isSymLink() {
         return (this._st_mode & OS.Constants.libc.S_IFMT) == OS.Constants.libc.S_IFLNK;
       },
       /**
        * The size of the file, in bytes.
        *
        * Note that the result may be |NaN| if the size of the file cannot be
        * represented in JavaScript.
        *
        * @type {number}
        */
       get size() {
         return exports.OS.Shared.Type.size_t.importFromC(this._st_size);
       },
       // Deprecated, use macBirthDate/winBirthDate instead
       get creationDate() {
         // On the Macintosh, returns the birth date if available.
         // On other Unix, as the birth date is not available,
         // returns the epoch.
         return this.macBirthDate || new Date(0);
       },
       /**
        * The date of last access to this file.
        *
        * Note that the definition of last access may depend on the
        * underlying operating system and file system.
        *
        * @type {Date}
        */
       get lastAccessDate() {
         delete this.lastAccessDate;
         let date = new Date(this._st_atime * 1000);
         Object.defineProperty(this, "lastAccessDate", {value: date});
         return date;
       },
       /**
        * Return the date of last modification of this file.
        */
       get lastModificationDate() {
         delete this.lastModificationDate;
         let date = new Date(this._st_mtime * 1000);
         Object.defineProperty(this, "lastModificationDate", {value: date});
         return date;
       },
       /**
        * Return the date at which the status of this file was last modified
        * (this is the date of the latest write/renaming/mode change/...
        * of the file)
        */
       get unixLastStatusChangeDate() {
         delete this.unixLastStatusChangeDate;
         let date = new Date(this._st_ctime * 1000);
         Object.defineProperty(this, "unixLastStatusChangeDate", {value: date});
         return date;
       },
       /**
        * Return the Unix owner of this file.
        */
       get unixOwner() {
         return exports.OS.Shared.Type.uid_t.importFromC(this._st_uid);
       },
       /**
        * Return the Unix group of this file.
        */
       get unixGroup() {
         return exports.OS.Shared.Type.gid_t.importFromC(this._st_gid);
       },
       /**
        * Return the Unix mode of this file.
        */
       get unixMode() {
         return exports.OS.Shared.Type.mode_t.importFromC(this._st_mode & MODE_MASK);
       }
     };

    /**
     * The date of creation of this file.
     *
     * Note that the date returned by this method is not always
     * reliable. Not all file systems are able to provide this
     * information.
     *
     * @type {Date}
     */
     if ("OSFILE_OFFSETOF_STAT_ST_BIRTHTIME" in OS.Constants.libc) {
       Object.defineProperty(
         File.Info.prototype,
         "macBirthDate",
         {
           get: function macBirthDate() {
             delete this.macBirthDate;
             let time;
             time = this._st_birthtime;
             let date = new Date(time * 1000);
             Object.defineProperty(this, "macBirthDate", { value: date });
             return date;
           }
         }
       );
     }

     /**
      * Return a version of an instance of File.Info that can be sent
      * from a worker thread to the main thread. Note that deserialization
      * is asymmetric and returns an object with a different implementation.
      */
     File.Info.toMsg = function toMsg(stat) {
       if (!stat instanceof File.Info) {
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
     File.stat = function stat(path, options) {
       options = options || noOptions;
       if (options.unixNoFollowingLinks) {
         throw_on_negative("stat", UnixFile.lstat(path, gStatDataPtr));
       } else {
         throw_on_negative("stat", UnixFile.stat(path, gStatDataPtr));
       }
       return new File.Info(gStatData);
     };

     File.read = exports.OS.Shared.AbstractFile.read;
     File.writeAtomic = exports.OS.Shared.AbstractFile.writeAtomic;

     /**
      * Get the current directory by getCurrentDirectory.
      */
     File.getCurrentDirectory = function getCurrentDirectory() {
       let path = UnixFile.get_current_dir_name?UnixFile.get_current_dir_name():
         UnixFile.getwd_auto(null);
       throw_on_null("getCurrentDirectory",path);
       return path.readString();
     };

     /**
      * Set the current directory by setCurrentDirectory.
      */
     File.setCurrentDirectory = function setCurrentDirectory(path) {
       throw_on_negative("setCurrentDirectory",
         UnixFile.chdir(path)
       );
     };

     /**
      * Get/set the current directory.
      */
     Object.defineProperty(File, "curDir", {
         set: function(path) {
           this.setCurrentDirectory(path);
         },
         get: function() {
           return this.getCurrentDirectory();
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

     function throw_on_null(operation, result) {
       if (result == null || (result.isNull && result.isNull())) {
         throw new File.Error(operation);
       }
       return result;
     }

     File.Unix = exports.OS.Unix.File;
     File.Error = exports.OS.Shared.Unix.Error;
     exports.OS.File = File;

     Object.defineProperty(File, "POS_START", { value: OS.Shared.POS_START });
     Object.defineProperty(File, "POS_CURRENT", { value: OS.Shared.POS_CURRENT });
     Object.defineProperty(File, "POS_END", { value: OS.Shared.POS_END });
   })(this);
}
