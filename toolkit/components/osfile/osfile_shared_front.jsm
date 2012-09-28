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

let LOG = exports.OS.Shared.LOG.bind(OS.Shared, "Shared front-end");

const noOptions = {};

/**
 * Code shared by implementations of File.
 *
 * @param {*} fd An OS-specific file handle.
 * @constructor
 */
let AbstractFile = function AbstractFile(fd) {
  this._fd = fd;
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
    throw OS.File.Error.closed();
  },
  /**
   * Read bytes from this file to a new buffer.
   *
   * @param {number=} bytes If unspecified, read all the remaining bytes from
   * this file. If specified, read |bytes| bytes, or less if the file does not
   * contain that many bytes.
   * @return {buffer: ArrayBuffer, bytes: bytes} A buffer containing the
   * bytes read and the number of bytes read. Note that |buffer| may be
   * larger than the number of bytes actually read.
   */
  read: function read(bytes) {
    if (bytes == null) {
      bytes = this.stat().size;
    }
    let buffer = new ArrayBuffer(bytes);
    let size = this.readTo(buffer, bytes);
    return {
      buffer: buffer,
      bytes: size
    };
  },

  /**
   * Read bytes from this file to an existing buffer.
   *
   * Note that, by default, this function may perform several I/O
   * operations to ensure that the buffer is as full as possible.
   *
   * @param {ArrayBuffer | C pointer} buffer The buffer in which to
   * store the bytes. If options.offset is not given, bytes are stored
   * from the start of the array. The buffer must be large enough to
   * accomodate |bytes| bytes.
   * @param {*=} options Optionally, an object that may contain the
   * following fields:
   * - {number} offset The offset in |buffer| at which to start placing
   * data
   * - {number} bytes The number of |bytes| to write from the buffer. If
   * unspecified, this is |buffer.byteLength|. Note that |bytes| is required
   * if |buffer| is a C pointer.
   *
   * @return {number} The number of bytes actually read, which may be
   * less than |bytes| if the file did not contain that many bytes left.
   */
  readTo: function readTo(buffer, options) {
    options = options || noOptions;

    let {ptr, bytes} = AbstractFile.normalizeToPointer(buffer, options.bytes,
      options.offset);
    let pos = 0;
    while (pos < bytes) {
      let chunkSize = this._read(ptr, bytes - pos, options);
      if (chunkSize == 0) {
        break;
      }
      pos += chunkSize;
      ptr = exports.OS.Shared.offsetBy(ptr, chunkSize);
    }

    return pos;
  },

  /**
   * Write bytes from a buffer to this file.
   *
   * Note that, by default, this function may perform several I/O
   * operations to ensure that the buffer is fully written.
   *
   * @param {ArrayBuffer | C pointer} buffer The buffer in which the
   * the bytes are stored. If options.offset is not given, bytes are stored
   * from the start of the array. The buffer must be large enough to
   * accomodate |bytes| bytes.
   * @param {*=} options Optionally, an object that may contain the
   * following fields:
   * - {number} offset The offset in |buffer| at which to start extracting
   * data
   * - {number} bytes The number of |bytes| to write from the buffer. If
   * unspecified, this is |buffer.byteLength|. Note that |bytes| is required
   * if |buffer| is a C pointer.
   *
   * @return {number} The number of bytes actually written.
   */
  write: function write(buffer, options) {
    options = options || noOptions;

    let {ptr, bytes} = AbstractFile.normalizeToPointer(buffer, options.bytes,
      options.offset);

    let pos = 0;
    while (pos < bytes) {
      let chunkSize = this._write(ptr, bytes - pos, options);
      pos += chunkSize;
      ptr = exports.OS.Shared.offsetBy(ptr, chunkSize);
    }
    return pos;
  }
};

/**
 * Utility function used to normalize a ArrayBuffer or C pointer into a uint8_t
 * C pointer.
 *
 * Future versions might extend this to other data structures.
 *
 * @param {ArrayBuffer|C pointer} candidate Either an ArrayBuffer or a
 * non-null C pointer.
 * @param {number} bytes The number of bytes that |candidate| should contain.
 * Used for sanity checking if the size of |candidate| can be determined.
 * @param {number=} offset Optionally, a number of bytes by which to shift
 * |candidate|.
 *
 * @return {ptr:{C pointer}, bytes:number} A C pointer of type uint8_t,
 * corresponding to the start of |candidate| + |offset| bytes.
 */
AbstractFile.normalizeToPointer = function normalizeToPointer(candidate, bytes, offset) {
  if (!candidate) {
    throw new TypeError("Expecting a C pointer or an ArrayBuffer");
  }
  if (offset == null) {
    offset = 0;
  }
  let ptr;
  if ("isNull" in candidate) {
    if (candidate.isNull()) {
      throw new TypeError("Expecting a non-null pointer");
    }
    ptr = exports.OS.Shared.Type.uint8_t.out_ptr.cast(candidate);
    if (bytes == null) {
      throw new TypeError("C pointer missing bytes indication.");
    }
  } else if ("byteLength" in candidate) {
    ptr = exports.OS.Shared.Type.uint8_t.out_ptr.implementation(candidate);
    if (bytes == null) {
      bytes = candidate.byteLength - offset;
    }
    if (candidate.byteLength < offset + bytes) {
      throw new TypeError("Buffer is too short. I need at least " +
                         (offset + bytes) +
                         " bytes but I have only " +
                         buffer.byteLength +
                          "bytes");
    }
  } else {
    throw new TypeError("Expecting a C pointer or an ArrayBuffer");
  }
  if (offset != 0) {
    ptr = exports.OS.Shared.offsetBy(ptr, offset);
  }
  return {ptr: ptr, bytes: bytes};
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
    existing: false
  };
  for (let key in mode) {
    if (!mode[key]) continue; // Only interpret true-ish keys
    switch (key) {
    case "read":
      result.read = true;
      break;
    case "write":
      result.write = true;
      break;
    case "truncate": // fallthrough
    case "trunc":
      result.trunc = true;
      result.write = true;
      break;
    case "create":
      result.create = true;
      result.write = true;
      break;
    case "existing": // fallthrough
    case "exist":
      result.existing = true;
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
 * to read.
 *
 * @return {{buffer: ArrayBuffer, bytes: number}} A buffer holding the bytes
 * and the number of bytes read from the file.
 */
AbstractFile.read = function read(path, bytes) {
  let file = exports.OS.File.open(path);
  try {
    return file.read(bytes);
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
 * Important note: In the current implementation, option |tmpPath|
 * is required. This requirement should disappear as part of bug 793660.
 *
 * @param {string} path The path of the file to modify.
 * @param {ArrayByffer} buffer A buffer containing the bytes to write.
 * @param {*=} options Optionally, an object determining the behavior
 * of this function. This object may contain the following fields:
 * - {number} offset The offset in |buffer| at which to start extracting
 * data. If unspecified, 0.
 * - {number} bytes The number of bytes to write. If unspecified, all
 * the bytes in |buffer|.
 * - {string} tmpPath The path at which to write the temporary file.
 *
 * @return {number} The number of bytes actually written.
 */
AbstractFile.writeAtomic =
     function writeAtomic(path, buffer, options) {
  options = options || noOptions;

  let tmpPath = options.tmpPath;

  if (!tmpPath) {
    throw new TypeError("Expected option tmpPath");
  }
  let tmpFile = OS.File.open(tmpPath, {write: true, truncate: true});
  let bytesWritten;
  try {
    bytesWritten = tmpFile.write(buffer, options);
    tmpFile.flush();
  } catch (x) {
    OS.File.remove(tmpPath);
    throw x;
  } finally {
    tmpFile.close();
  }

  OS.File.move(tmpPath, path, {noCopy: true});
  return bytesWritten;
};

   exports.OS.Shared.AbstractFile = AbstractFile;
})(this);
