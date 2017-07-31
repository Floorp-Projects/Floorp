/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Code shared by OS.File front-ends.
 *
 * This code is meant to be included by another library. It is also meant to
 * be executed only on a worker thread.
 */

if (typeof Components != "undefined") {
  throw new Error("osfile_shared_front.jsm cannot be used from the main thread");
}
(function(exports) {

var SharedAll =
  require("resource://gre/modules/osfile/osfile_shared_allthreads.jsm");
var Path = require("resource://gre/modules/osfile/ospath.jsm");
var Lz4 =
  require("resource://gre/modules/lz4.js");
var LOG = SharedAll.LOG.bind(SharedAll, "Shared front-end");
var clone = SharedAll.clone;

/**
 * Code shared by implementations of File.
 *
 * @param {*} fd An OS-specific file handle.
 * @param {string} path File path of the file handle, used for error-reporting.
 * @constructor
 */
var AbstractFile = function AbstractFile(fd, path) {
  this._fd = fd;
  if (!path) {
    throw new TypeError("path is expected");
  }
  this._path = path;
};

AbstractFile.prototype = {
  /**
   * Return the file handle.
   *
   * @throw OS.File.Error if the file has been closed.
   */
  get fd() {
    if (this._fd) {
      return this._fd;
    }
    throw OS.File.Error.closed("accessing file", this._path);
  },
  /**
   * Read bytes from this file to a new buffer.
   *
   * @param {number=} maybeBytes (deprecated, please use options.bytes)
   * @param {JSON} options
   * @return {Uint8Array} An array containing the bytes read.
   */
  read: function read(maybeBytes, options = {}) {
    if (typeof maybeBytes === "object") {
    // Caller has skipped `maybeBytes` and provided an options object.
      options = clone(maybeBytes);
      maybeBytes = null;
    } else {
      options = options || {};
    }
    let bytes = options.bytes || undefined;
    if (bytes === undefined) {
      bytes = maybeBytes == null ? this.stat().size : maybeBytes;
    }
    let buffer = new Uint8Array(bytes);
    let pos = 0;
    while (pos < bytes) {
      let length = bytes - pos;
      let view = new DataView(buffer.buffer, pos, length);
      let chunkSize = this._read(view, length, options);
      if (chunkSize == 0) {
        break;
      }
      pos += chunkSize;
    }
    if (pos == bytes) {
      return buffer;
    } else {
      return buffer.subarray(0, pos);
    }
  },

  /**
   * Write bytes from a buffer to this file.
   *
   * Note that, by default, this function may perform several I/O
   * operations to ensure that the buffer is fully written.
   *
   * @param {Typed array} buffer The buffer in which the the bytes are
   * stored. The buffer must be large enough to accomodate |bytes| bytes.
   * @param {*=} options Optionally, an object that may contain the
   * following fields:
   * - {number} bytes The number of |bytes| to write from the buffer. If
   * unspecified, this is |buffer.byteLength|.
   *
   * @return {number} The number of bytes actually written.
   */
  write: function write(buffer, options = {}) {
    let bytes =
      SharedAll.normalizeBufferArgs(buffer, ("bytes" in options) ? options.bytes : undefined);
    let pos = 0;
    while (pos < bytes) {
      let length = bytes - pos;
      let view = new DataView(buffer.buffer, buffer.byteOffset + pos, length);
      let chunkSize = this._write(view, length, options);
      pos += chunkSize;
    }
    return pos;
  }
};

/**
 * Creates and opens a file with a unique name. By default, generate a random HEX number and use it to create a unique new file name.
 *
 * @param {string} path The path to the file.
 * @param {*=} options Additional options for file opening. This
 * implementation interprets the following fields:
 *
 * - {number} humanReadable If |true|, create a new filename appending a decimal number. ie: filename-1.ext, filename-2.ext.
 *  If |false| use HEX numbers ie: filename-A65BC0.ext
 * - {number} maxReadableNumber Used to limit the amount of tries after a failed
 *  file creation. Default is 20.
 *
 * @return {Object} contains A file object{file} and the path{path}.
 * @throws {OS.File.Error} If the file could not be opened.
 */
AbstractFile.openUnique = function openUnique(path, options = {}) {
  let mode = {
    create : true
  };

  let dirName = Path.dirname(path);
  let leafName = Path.basename(path);
  let lastDotCharacter = leafName.lastIndexOf('.');
  let fileName = leafName.substring(0, lastDotCharacter != -1 ? lastDotCharacter : leafName.length);
  let suffix = (lastDotCharacter != -1 ? leafName.substring(lastDotCharacter) : "");
  let uniquePath = "";
  let maxAttempts = options.maxAttempts || 99;
  let humanReadable = !!options.humanReadable;
  const HEX_RADIX = 16;
  // We produce HEX numbers between 0 and 2^24 - 1.
  const MAX_HEX_NUMBER = 16777215;

  try {
    return {
      path: path,
      file: OS.File.open(path, mode)
    };
  } catch (ex) {
    if (ex instanceof OS.File.Error && ex.becauseExists) {
      for (let i = 0; i < maxAttempts; ++i) {
        try {
          if (humanReadable) {
            uniquePath = Path.join(dirName, fileName + "-" + (i + 1) + suffix);
          } else {
            let hexNumber = Math.floor(Math.random() * MAX_HEX_NUMBER).toString(HEX_RADIX);
            uniquePath = Path.join(dirName, fileName + "-" + hexNumber + suffix);
          }
          return {
            path: uniquePath,
            file: OS.File.open(uniquePath, mode)
          };
        } catch (ex) {
          if (ex instanceof OS.File.Error && ex.becauseExists) {
            // keep trying ...
          } else {
            throw ex;
          }
        }
      }
      throw OS.File.Error.exists("could not find an unused file name.", path);
    }
  }
};

/**
 * Code shared by iterators.
 */
AbstractFile.AbstractIterator = function AbstractIterator() {
};
AbstractFile.AbstractIterator.prototype = {
  /**
   * Allow iterating with |for|
   */
  __iterator__: function __iterator__() {
    return this;
  },
  /**
   * Apply a function to all elements of the directory sequentially.
   *
   * @param {Function} cb This function will be applied to all entries
   * of the directory. It receives as arguments
   *  - the OS.File.Entry corresponding to the entry;
   *  - the index of the entry in the enumeration;
   *  - the iterator itself - calling |close| on the iterator stops
   *   the loop.
   */
  forEach: function forEach(cb) {
    let index = 0;
    for (let entry in this) {
      cb(entry, index++, this);
    }
  },
  /**
   * Return several entries at once.
   *
   * Entries are returned in the same order as a walk with |forEach| or
   * |for(...)|.
   *
   * @param {number=} length If specified, the number of entries
   * to return. If unspecified, return all remaining entries.
   * @return {Array} An array containing the next |length| entries, or
   * less if the iteration contains less than |length| entries left.
   */
  nextBatch: function nextBatch(length) {
    let array = [];
    let i = 0;
    for (let entry in this) {
      array.push(entry);
      if (++i >= length) {
        return array;
      }
    }
    return array;
  }
};

/**
 * Utility function shared by implementations of |OS.File.open|:
 * extract read/write/trunc/create/existing flags from a |mode|
 * object.
 *
 * @param {*=} mode An object that may contain fields |read|,
 * |write|, |truncate|, |create|, |existing|. These fields
 * are interpreted only if true-ish.
 * @return {{read:bool, write:bool, trunc:bool, create:bool,
 * existing:bool}} an object recapitulating the options set
 * by |mode|.
 * @throws {TypeError} If |mode| contains other fields, or
 * if it contains both |create| and |truncate|, or |create|
 * and |existing|.
 */
AbstractFile.normalizeOpenMode = function normalizeOpenMode(mode) {
  let result = {
    read: false,
    write: false,
    trunc: false,
    create: false,
    existing: false,
    append: true
  };
  for (let key in mode) {
    let val = !!mode[key]; // bool cast.
    switch (key) {
    case "read":
      result.read = val;
      break;
    case "write":
      result.write = val;
      break;
    case "truncate": // fallthrough
    case "trunc":
      result.trunc = val;
      result.write |= val;
      break;
    case "create":
      result.create = val;
      result.write |= val;
      break;
    case "existing": // fallthrough
    case "exist":
      result.existing = val;
      break;
    case "append":
      result.append = val;
      break;
    default:
      throw new TypeError("Mode " + key + " not understood");
    }
  }
  // Reject opposite modes
  if (result.existing && result.create) {
    throw new TypeError("Cannot specify both existing:true and create:true");
  }
  if (result.trunc && result.create) {
    throw new TypeError("Cannot specify both trunc:true and create:true");
  }
  // Handle read/write
  if (!result.write) {
    result.read = true;
  }
  return result;
};

/**
 * Return the contents of a file.
 *
 * @param {string} path The path to the file.
 * @param {number=} bytes Optionally, an upper bound to the number of bytes
 * to read. DEPRECATED - please use options.bytes instead.
 * @param {object=} options Optionally, an object with some of the following
 * fields:
 * - {number} bytes An upper bound to the number of bytes to read.
 * - {string} compression If "lz4" and if the file is compressed using the lz4
 *   compression algorithm, decompress the file contents on the fly.
 *
 * @return {Uint8Array} A buffer holding the bytes
 * and the number of bytes read from the file.
 */
AbstractFile.read = function read(path, bytes, options = {}) {
  if (bytes && typeof bytes == "object") {
    options = bytes;
    bytes = options.bytes || null;
  }
  if ("encoding" in options && typeof options.encoding != "string") {
    throw new TypeError("Invalid type for option encoding");
  }
  if ("compression" in options && typeof options.compression != "string") {
    throw new TypeError("Invalid type for option compression: " + options.compression);
  }
  if ("bytes" in options && typeof options.bytes != "number") {
    throw new TypeError("Invalid type for option bytes");
  }
  let file = exports.OS.File.open(path);
  try {
    let buffer = file.read(bytes, options);
    if ("compression" in options) {
      if (options.compression == "lz4") {
        options = Object.create(options);
        options.path = path;
        buffer = Lz4.decompressFileContent(buffer, options);
      } else {
        throw OS.File.Error.invalidArgument("Compression");
      }
    }
    if (!("encoding" in options)) {
      return buffer;
    }
    let decoder;
    try {
      decoder = new TextDecoder(options.encoding);
    } catch (ex) {
      if (ex instanceof RangeError) {
        throw OS.File.Error.invalidArgument("Decode");
      } else {
        throw ex;
      }
    }
    return decoder.decode(buffer);
  } finally {
    file.close();
  }
};

/**
 * Write a file, atomically.
 *
 * By opposition to a regular |write|, this operation ensures that,
 * until the contents are fully written, the destination file is
 * not modified.
 *
 * Limitation: In a few extreme cases (hardware failure during the
 * write, user unplugging disk during the write, etc.), data may be
 * corrupted. If your data is user-critical (e.g. preferences,
 * application data, etc.), you may wish to consider adding options
 * |tmpPath| and/or |flush| to reduce the likelihood of corruption, as
 * detailed below. Note that no combination of options can be
 * guaranteed to totally eliminate the risk of corruption.
 *
 * @param {string} path The path of the file to modify.
 * @param {Typed Array | C pointer} buffer A buffer containing the bytes to write.
 * @param {*=} options Optionally, an object determining the behavior
 * of this function. This object may contain the following fields:
 * - {number} bytes The number of bytes to write. If unspecified,
 * |buffer.byteLength|. Required if |buffer| is a C pointer.
 * - {string} tmpPath If |null| or unspecified, write all data directly
 * to |path|. If specified, write all data to a temporary file called
 * |tmpPath| and, once this write is complete, rename the file to
 * replace |path|. Performing this additional operation is a little
 * slower but also a little safer.
 * - {bool} noOverwrite - If set, this function will fail if a file already
 * exists at |path|.
 * - {bool} flush - If |false| or unspecified, return immediately once the
 * write is complete. If |true|, before writing, force the operating system
 * to write its internal disk buffers to the disk. This is considerably slower
 * (not just for the application but for the whole system) but also safer:
 * if the system shuts down improperly (typically due to a kernel freeze
 * or a power failure) or if the device is disconnected before the buffer
 * is flushed, the file has more chances of not being corrupted.
 * - {string} compression - If empty or unspecified, do not compress the file.
 * If "lz4", compress the contents of the file atomically using lz4. For the
 * time being, the container format is specific to Mozilla and cannot be read
 * by means other than OS.File.read(..., { compression: "lz4"})
 * - {string} backupTo - If specified, backup the destination file as |backupTo|.
 * Note that this function renames the destination file before overwriting it.
 * If the process or the operating system freezes or crashes
 * during the short window between these operations,
 * the destination file will have been moved to its backup.
 *
 * @return {number} The number of bytes actually written.
 */
AbstractFile.writeAtomic =
     function writeAtomic(path, buffer, options = {}) {

  // Verify that path is defined and of the correct type
  if (typeof path != "string" || path == "") {
    throw new TypeError("File path should be a (non-empty) string");
  }
  let noOverwrite = options.noOverwrite;
  if (noOverwrite && OS.File.exists(path)) {
    throw OS.File.Error.exists("writeAtomic", path);
  }

  if (typeof buffer == "string") {
    // Normalize buffer to a C buffer by encoding it
    let encoding = options.encoding || "utf-8";
    buffer = new TextEncoder(encoding).encode(buffer);
  }

  if ("compression" in options && options.compression == "lz4") {
    buffer = Lz4.compressFileContent(buffer, options);
    options = Object.create(options);
    options.bytes = buffer.byteLength;
  }

  let bytesWritten = 0;

  if (!options.tmpPath) {
    if (options.backupTo) {
      try {
        OS.File.move(path, options.backupTo, {noCopy: true});
      } catch (ex) {
        if (ex.becauseNoSuchFile) {
          // The file doesn't exist, nothing to backup.
        } else {
          throw ex;
        }
      }
    }
    // Just write, without any renaming trick
    let dest = OS.File.open(path, {write: true, truncate: true});
    try {
      bytesWritten = dest.write(buffer, options);
      if (options.flush) {
        dest.flush();
      }
    } finally {
      dest.close();
    }
    return bytesWritten;
  }

  let tmpFile = OS.File.open(options.tmpPath, {write: true, truncate: true});
  try {
    bytesWritten = tmpFile.write(buffer, options);
    if (options.flush) {
      tmpFile.flush();
    }
  } catch (x) {
    OS.File.remove(options.tmpPath);
    throw x;
  } finally {
    tmpFile.close();
  }

  if (options.backupTo) {
    try {
      OS.File.move(path, options.backupTo, {noCopy: true});
    } catch (ex) {
     if (ex.becauseNoSuchFile) {
        // The file doesn't exist, nothing to backup.
      } else {
        throw ex;
      }
    }
  }

  OS.File.move(options.tmpPath, path, {noCopy: true});
  return bytesWritten;
};

/**
 * This function is used by removeDir to avoid calling lstat for each
 * files under the specified directory. External callers should not call
 * this function directly.
 */
AbstractFile.removeRecursive = function(path, options = {}) {
  let iterator = new OS.File.DirectoryIterator(path);
  if (!iterator.exists()) {
    if (!("ignoreAbsent" in options) || options.ignoreAbsent) {
      return;
    }
  }

  try {
    for (let entry in iterator) {
      if (entry.isDir) {
        if (entry.isLink) {
          // Unlike Unix symlinks, NTFS junctions or NTFS symlinks to
          // directories are directories themselves. OS.File.remove()
          // will not work for them.
          OS.File.removeEmptyDir(entry.path, options);
        } else {
          // Normal directories.
          AbstractFile.removeRecursive(entry.path, options);
        }
      } else {
        // NTFS symlinks to files, Unix symlinks, or regular files.
        OS.File.remove(entry.path, options);
      }
    }
  } finally {
    iterator.close();
  }

  OS.File.removeEmptyDir(path);
};

/**
 * Create a directory and, optionally, its parent directories.
 *
 * @param {string} path The name of the directory.
 * @param {*=} options Additional options.
 *
 * - {string} from If specified, the call to |makeDir| creates all the
 * ancestors of |path| that are descendants of |from|. Note that |path|
 * must be a descendant of |from|, and that |from| and its existing
 * subdirectories present in |path|  must be user-writeable.
 * Example:
 *   makeDir(Path.join(profileDir, "foo", "bar"), { from: profileDir });
 *  creates directories profileDir/foo, profileDir/foo/bar
 * - {bool} ignoreExisting If |false|, throw an error if the directory
 * already exists. |true| by default. Ignored if |from| is specified.
 * - {number} unixMode Under Unix, if specified, a file creation mode,
 * as per libc function |mkdir|. If unspecified, dirs are
 * created with a default mode of 0700 (dir is private to
 * the user, the user can read, write and execute). Ignored under Windows
 * or if the file system does not support file creation modes.
 * - {C pointer} winSecurity Under Windows, if specified, security
 * attributes as per winapi function |CreateDirectory|. If
 * unspecified, use the default security descriptor, inherited from
 * the parent directory. Ignored under Unix or if the file system
 * does not support security descriptors.
 */
AbstractFile.makeDir = function(path, options = {}) {
  let from = options.from;
  if (!from) {
    OS.File._makeDir(path, options);
    return;
  }
  if (!path.startsWith(from)) {
    // Apparently, `from` is not a parent of `path`. However, we may
    // have false negatives due to non-normalized paths, e.g.
    // "foo//bar" is a parent of "foo/bar/sna".
    path = Path.normalize(path);
    from = Path.normalize(from);
    if (!path.startsWith(from)) {
      throw new Error("Incorrect use of option |from|: " + path + " is not a descendant of " + from);
    }
  }
  let innerOptions = Object.create(options, {
    ignoreExisting: {
      value: true
    }
  });
  // Compute the elements that appear in |path| but not in |from|.
  let items = Path.split(path).components.slice(Path.split(from).components.length);
  let current = from;
  for (let item of items) {
    current = Path.join(current, item);
    OS.File._makeDir(current, innerOptions);
  }
};

if (!exports.OS.Shared) {
  exports.OS.Shared = {};
}
exports.OS.Shared.AbstractFile = AbstractFile;
})(this);
