/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * OS.File utilities used by all threads.
 *
 * This module defines:
 * - logging;
 * - the base constants;
 * - base types and primitives for declaring new types;
 * - primitives for importing C functions;
 * - primitives for dealing with integers, pointers, typed arrays;
 * - the base class OSError;
 * - a few additional utilities.
 */

// Boilerplate used to be able to import this module both from the main
// thread and from worker threads.
if (typeof Components != "undefined") {
  // Global definition of |exports|, to keep everybody happy.
  // In non-main thread, |exports| is provided by the module
  // loader.
  this.exports = {};

  const Cu = Components.utils;
  const Ci = Components.interfaces;
  const Cc = Components.classes;

  Cu.import("resource://gre/modules/Services.jsm", this);
}

let EXPORTED_SYMBOLS = [
  "LOG",
  "clone",
  "Config",
  "Constants",
  "Type",
  "HollowStructure",
  "OSError",
  "Library",
  "declareFFI",
  "declareLazy",
  "declareLazyFFI",
  "normalizeToPointer",
  "projectValue",
  "isTypedArray",
  "defineLazyGetter",
  "offsetBy",
  "OS" // Warning: this exported symbol will disappear
];

////////////////////// Configuration of OS.File

let Config = {
  /**
   * If |true|, calls to |LOG| are shown. Otherwise, they are hidden.
   *
   * This configuration option is controlled by preference "toolkit.osfile.log".
   */
  DEBUG: false,

  /**
   * TEST
   */
  TEST: false
};
exports.Config = Config;

////////////////////// OS Constants

if (typeof Components != "undefined") {
  // On the main thread, OS.Constants is defined by a xpcom
  // component. On other threads, it is available automatically
  Cu.import("resource://gre/modules/ctypes.jsm");
  Cc["@mozilla.org/net/osfileconstantsservice;1"].
    getService(Ci.nsIOSFileConstantsService).init();
}

exports.Constants = OS.Constants;

///////////////////// Utilities

// Define a lazy getter for a property
let defineLazyGetter = function defineLazyGetter(object, name, getter) {
  Object.defineProperty(object, name, {
    configurable: true,
    get: function lazy() {
      delete this[name];
      let value = getter.call(this);
      Object.defineProperty(object, name, {
        value: value
      });
      return value;
    }
  });
};
exports.defineLazyGetter = defineLazyGetter;


///////////////////// Logging

/**
 * The default implementation of the logger.
 *
 * The choice of logger can be overridden with Config.TEST.
 */
let gLogger;
if (typeof console != "undefined" && console.log) {
  gLogger = console.log.bind(console, "OS");
} else {
  gLogger = function(...args) {
    dump("OS " + args.join(" ") + "\n");
  };
}

/**
 * Attempt to stringify an argument into something useful for
 * debugging purposes, by using |.toString()| or |JSON.stringify|
 * if available.
 *
 * @param {*} arg An argument to be stringified if possible.
 * @return {string} A stringified version of |arg|.
 */
let stringifyArg = function stringifyArg(arg) {
  if (typeof arg === "string") {
    return arg;
  }
  if (arg && typeof arg === "object") {
    let argToString = "" + arg;

    /**
     * The only way to detect whether this object has a non-default
     * implementation of |toString| is to check whether it returns
     * '[object Object]'. Unfortunately, we cannot simply compare |arg.toString|
     * and |Object.prototype.toString| as |arg| typically comes from another
     * compartment.
     */
    if (argToString === "[object Object]") {
      return JSON.stringify(arg);
    } else {
      return argToString;
    }
  }
  return arg;
};

let LOG = function (...args) {
  if (!Config.DEBUG) {
    // If logging is deactivated, don't log
    return;
  }

  let logFunc = gLogger;
  if (Config.TEST && typeof Components != "undefined") {
    // If _TESTING_LOGGING is set, and if we are on the main thread,
    // redirect logs to Services.console, for testing purposes
    logFunc = function logFunc(...args) {
      let message = ["TEST", "OS"].concat(args).join(" ");
      Services.console.logStringMessage(message + "\n");
    };
  }
  logFunc.apply(null, [stringifyArg(arg) for (arg of args)]);
};

exports.LOG = LOG;

/**
 * Return a shallow clone of the enumerable properties of an object.
 *
 * Utility used whenever normalizing options requires making (shallow)
 * changes to an option object. The copy ensures that we do not modify
 * a client-provided object by accident.
 *
 * Note: to reference and not copy specific fields, provide an optional
 * |refs| argument containing their names.
 *
 * @param {JSON} object Options to be cloned.
 * @param {Array} refs An optional array of field names to be passed by
 * reference instead of copying.
 */
let clone = function (object, refs = []) {
  let result = {};
  // Make a reference between result[key] and object[key].
  let refer = function refer(result, key, object) {
    Object.defineProperty(result, key, {
      enumerable: true,
      get: function() {
        return object[key];
      },
      set: function(value) {
        object[key] = value;
      }
    });
  };
  for (let k in object) {
    if (refs.indexOf(k) < 0) {
      result[k] = object[k];
    } else {
      refer(result, k, object);
    }
  }
  return result;
};

exports.clone = clone;

///////////////////// Abstractions above js-ctypes

/**
 * Abstraction above js-ctypes types.
 *
 * Use values of this type to register FFI functions. In addition to the
 * usual features of js-ctypes, values of this type perform the necessary
 * transformations to ensure that C errors are handled nicely, to connect
 * resources with their finalizer, etc.
 *
 * @param {string} name The name of the type. Must be unique.
 * @param {CType} implementation The js-ctypes implementation of the type.
 *
 * @constructor
 */
function Type(name, implementation) {
  if (!(typeof name == "string")) {
    throw new TypeError("Type expects as first argument a name, got: "
                        + name);
  }
  if (!(implementation instanceof ctypes.CType)) {
    throw new TypeError("Type expects as second argument a ctypes.CType"+
                        ", got: " + implementation);
  }
  Object.defineProperty(this, "name", { value: name });
  Object.defineProperty(this, "implementation", { value: implementation });
}
Type.prototype = {
  /**
   * Serialize a value of |this| |Type| into a format that can
   * be transmitted as a message (not necessarily a string).
   *
   * In the default implementation, the method returns the
   * value unchanged.
   */
  toMsg: function default_toMsg(value) {
    return value;
  },
  /**
   * Deserialize a message to a value of |this| |Type|.
   *
   * In the default implementation, the method returns the
   * message unchanged.
   */
  fromMsg: function default_fromMsg(msg) {
    return msg;
  },
  /**
   * Import a value from C.
   *
   * In this default implementation, return the value
   * unchanged.
   */
  importFromC: function default_importFromC(value) {
    return value;
  },

  /**
   * A pointer/array used to pass data to the foreign function.
   */
  get in_ptr() {
    delete this.in_ptr;
    let ptr_t = new PtrType(
      "[in] " + this.name + "*",
      this.implementation.ptr,
      this);
    Object.defineProperty(this, "in_ptr",
    {
      get: function() {
        return ptr_t;
      }
    });
    return ptr_t;
  },

  /**
   * A pointer/array used to receive data from the foreign function.
   */
  get out_ptr() {
    delete this.out_ptr;
    let ptr_t = new PtrType(
      "[out] " + this.name + "*",
      this.implementation.ptr,
      this);
    Object.defineProperty(this, "out_ptr",
    {
      get: function() {
        return ptr_t;
      }
    });
    return ptr_t;
  },

  /**
   * A pointer/array used to both pass data to the foreign function
   * and receive data from the foreign function.
   *
   * Whenever possible, prefer using |in_ptr| or |out_ptr|, which
   * are generally faster.
   */
  get inout_ptr() {
    delete this.inout_ptr;
    let ptr_t = new PtrType(
      "[inout] " + this.name + "*",
      this.implementation.ptr,
      this);
    Object.defineProperty(this, "inout_ptr",
    {
      get: function() {
        return ptr_t;
      }
    });
    return ptr_t;
  },

  /**
   * Attach a finalizer to a type.
   */
  releaseWith: function releaseWith(finalizer) {
    let parent = this;
    let type = this.withName("[auto " + this.name + ", " + finalizer + "] ");
    type.importFromC = function importFromC(value, operation) {
      return ctypes.CDataFinalizer(
        parent.importFromC(value, operation),
        finalizer);
    };
    return type;
  },

  /**
   * Return an alias to a type with a different name.
   */
  withName: function withName(name) {
    return Object.create(this, {name: {value: name}});
  },

  /**
   * Cast a C value to |this| type.
   *
   * Throw an error if the value cannot be casted.
   */
  cast: function cast(value) {
    return ctypes.cast(value, this.implementation);
  },

  /**
   * Return the number of bytes in a value of |this| type.
   *
   * This may not be defined, e.g. for |void_t|, array types
   * without length, etc.
   */
  get size() {
    return this.implementation.size;
  }
};

/**
 * Utility function used to determine whether an object is a typed array
 */
let isTypedArray = function isTypedArray(obj) {
  return typeof obj == "object"
    && "byteOffset" in obj;
};
exports.isTypedArray = isTypedArray;

/**
 * A |Type| of pointers.
 *
 * @param {string} name The name of this type.
 * @param {CType} implementation The type of this pointer.
 * @param {Type} targetType The target type.
 */
function PtrType(name, implementation, targetType) {
  Type.call(this, name, implementation);
  if (targetType == null || !targetType instanceof Type) {
    throw new TypeError("targetType must be an instance of Type");
  }
  /**
   * The type of values targeted by this pointer type.
   */
  Object.defineProperty(this, "targetType", {
    value: targetType
  });
}
PtrType.prototype = Object.create(Type.prototype);

/**
 * Convert a value to a pointer.
 *
 * Protocol:
 * - |null| returns |null|
 * - a string returns |{string: value}|
 * - a typed array returns |{ptr: address_of_buffer}|
 * - a C array returns |{ptr: address_of_buffer}|
 * everything else raises an error
 */
PtrType.prototype.toMsg = function ptr_toMsg(value) {
  if (value == null) {
    return null;
  }
  if (typeof value == "string") {
    return { string: value };
  }
  let normalized;
  if (isTypedArray(value)) { // Typed array
    normalized = Type.uint8_t.in_ptr.implementation(value.buffer);
    if (value.byteOffset != 0) {
      normalized = offsetBy(normalized, value.byteOffset);
    }
  } else if ("addressOfElement" in value) { // C array
    normalized = value.addressOfElement(0);
  } else if ("isNull" in value) { // C pointer
    normalized = value;
  } else {
    throw new TypeError("Value " + value +
      " cannot be converted to a pointer");
  }
  let cast = Type.uintptr_t.cast(normalized);
  return {ptr: cast.value.toString()};
};

/**
 * Convert a message back to a pointer.
 */
PtrType.prototype.fromMsg = function ptr_fromMsg(msg) {
  if (msg == null) {
    return null;
  }
  if ("string" in msg) {
    return msg.string;
  }
  if ("ptr" in msg) {
    let address = ctypes.uintptr_t(msg.ptr);
    return this.cast(address);
  }
  throw new TypeError("Message " + msg.toSource() +
    " does not represent a pointer");
};

exports.Type = Type;


/*
 * Some values are large integers on 64 bit platforms. Unfortunately,
 * in practice, 64 bit integers cannot be manipulated in JS. We
 * therefore project them to regular numbers whenever possible.
 */

let projectLargeInt = function projectLargeInt(x) {
  return parseInt(x.toString(), 10);
};
let projectLargeUInt = function projectLargeUInt(x) {
  return parseInt(x.toString(), 10);
};
let projectValue = function projectValue(x) {
  if (!(x instanceof ctypes.CData)) {
    return x;
  }
  if (!("value" in x)) { // Sanity check
    throw new TypeError("Number " + x.toSource() + " has no field |value|");
  }
  return x.value;
};

function projector(type, signed) {
  LOG("Determining best projection for", type,
    "(size: ", type.size, ")", signed?"signed":"unsigned");
  if (type instanceof Type) {
    type = type.implementation;
  }
  if (!type.size) {
    throw new TypeError("Argument is not a proper C type");
  }
  // Determine if type is projected to Int64/Uint64
  if (type.size == 8           // Usual case
      // The following cases have special treatment in js-ctypes
      // Regardless of their size, the value getter returns
      // a Int64/Uint64
      || type == ctypes.size_t // Special cases
      || type == ctypes.ssize_t
      || type == ctypes.intptr_t
      || type == ctypes.uintptr_t
      || type == ctypes.off_t) {
    if (signed) {
      LOG("Projected as a large signed integer");
      return projectLargeInt;
    } else {
      LOG("Projected as a large unsigned integer");
      return projectLargeUInt;
    }
  }
  LOG("Projected as a regular number");
  return projectValue;
};
exports.projectValue = projectValue;

/**
 * Get the appropriate type for an unsigned int of the given size.
 *
 * This function is useful to define types such as |mode_t| whose
 * actual width depends on the OS/platform.
 *
 * @param {number} size The number of bytes requested.
 */
Type.uintn_t = function uintn_t(size) {
  switch (size) {
  case 1: return Type.uint8_t;
  case 2: return Type.uint16_t;
  case 4: return Type.uint32_t;
  case 8: return Type.uint64_t;
  default:
    throw new Error("Cannot represent unsigned integers of " + size + " bytes");
  }
};

/**
 * Get the appropriate type for an signed int of the given size.
 *
 * This function is useful to define types such as |mode_t| whose
 * actual width depends on the OS/platform.
 *
 * @param {number} size The number of bytes requested.
 */
Type.intn_t = function intn_t(size) {
  switch (size) {
  case 1: return Type.int8_t;
  case 2: return Type.int16_t;
  case 4: return Type.int32_t;
  case 8: return Type.int64_t;
  default:
    throw new Error("Cannot represent integers of " + size + " bytes");
  }
};

/**
 * Actual implementation of common C types.
 */

/**
 * The void value.
 */
Type.void_t =
  new Type("void",
           ctypes.void_t);

/**
 * Shortcut for |void*|.
 */
Type.voidptr_t =
  new PtrType("void*",
              ctypes.voidptr_t,
              Type.void_t);

// void* is a special case as we can cast any pointer to/from it
// so we have to shortcut |in_ptr|/|out_ptr|/|inout_ptr| and
// ensure that js-ctypes' casting mechanism is invoked directly
["in_ptr", "out_ptr", "inout_ptr"].forEach(function(key) {
  Object.defineProperty(Type.void_t, key,
  {
    value: Type.voidptr_t
  });
});

/**
 * A Type of integers.
 *
 * @param {string} name The name of this type.
 * @param {CType} implementation The underlying js-ctypes implementation.
 * @param {bool} signed |true| if this is a type of signed integers,
 * |false| otherwise.
 *
 * @constructor
 */
function IntType(name, implementation, signed) {
  Type.call(this, name, implementation);
  this.importFromC = projector(implementation, signed);
  this.project = this.importFromC;
};
IntType.prototype = Object.create(Type.prototype);
IntType.prototype.toMsg = function toMsg(value) {
  if (typeof value == "number") {
    return value;
  }
  return this.project(value);
};

/**
 * A C char (one byte)
 */
Type.char =
  new Type("char",
           ctypes.char);

/**
 * A C wide char (two bytes)
 */
Type.jschar =
  new Type("jschar",
           ctypes.jschar);

 /**
  * Base string types.
  */
Type.cstring = Type.char.in_ptr.withName("[in] C string");
Type.wstring = Type.jschar.in_ptr.withName("[in] wide string");
Type.out_cstring = Type.char.out_ptr.withName("[out] C string");
Type.out_wstring = Type.jschar.out_ptr.withName("[out] wide string");

/**
 * A C integer (8-bits).
 */
Type.int8_t =
  new IntType("int8_t", ctypes.int8_t, true);

Type.uint8_t =
  new IntType("uint8_t", ctypes.uint8_t, false);

/**
 * A C integer (16-bits).
 *
 * Also known as WORD under Windows.
 */
Type.int16_t =
  new IntType("int16_t", ctypes.int16_t, true);

Type.uint16_t =
  new IntType("uint16_t", ctypes.uint16_t, false);

/**
 * A C integer (32-bits).
 *
 * Also known as DWORD under Windows.
 */
Type.int32_t =
  new IntType("int32_t", ctypes.int32_t, true);

Type.uint32_t =
  new IntType("uint32_t", ctypes.uint32_t, false);

/**
 * A C integer (64-bits).
 */
Type.int64_t =
  new IntType("int64_t", ctypes.int64_t, true);

Type.uint64_t =
  new IntType("uint64_t", ctypes.uint64_t, false);

 /**
 * A C integer
 *
 * Size depends on the platform.
 */
Type.int = Type.intn_t(ctypes.int.size).
  withName("int");

Type.unsigned_int = Type.intn_t(ctypes.unsigned_int.size).
  withName("unsigned int");

/**
 * A C long integer.
 *
 * Size depends on the platform.
 */
Type.long =
  Type.intn_t(ctypes.long.size).withName("long");

Type.unsigned_long =
  Type.intn_t(ctypes.unsigned_long.size).withName("unsigned long");

/**
 * An unsigned integer with the same size as a pointer.
 *
 * Used to cast a pointer to an integer, whenever necessary.
 */
Type.uintptr_t =
  Type.uintn_t(ctypes.uintptr_t.size).withName("uintptr_t");

/**
 * A boolean.
 * Implemented as a C integer.
 */
Type.bool = Type.int.withName("bool");
Type.bool.importFromC = function projectBool(x) {
  return !!(x.value);
};

/**
 * A user identifier.
 *
 * Implemented as a C integer.
 */
Type.uid_t =
  Type.int.withName("uid_t");

/**
 * A group identifier.
 *
 * Implemented as a C integer.
 */
Type.gid_t =
  Type.int.withName("gid_t");

/**
 * An offset (positive or negative).
 *
 * Implemented as a C integer.
 */
Type.off_t =
  new IntType("off_t", ctypes.off_t, true);

/**
 * A size (positive).
 *
 * Implemented as a C size_t.
 */
Type.size_t =
  new IntType("size_t", ctypes.size_t, false);

/**
 * An offset (positive or negative).
 * Implemented as a C integer.
 */
Type.ssize_t =
  new IntType("ssize_t", ctypes.ssize_t, true);

/**
 * Encoding/decoding strings
 */
Type.uencoder =
  new Type("uencoder", ctypes.StructType("uencoder"));
Type.udecoder =
  new Type("udecoder", ctypes.StructType("udecoder"));

/**
 * Utility class, used to build a |struct| type
 * from a set of field names, types and offsets.
 *
 * @param {string} name The name of the |struct| type.
 * @param {number} size The total size of the |struct| type in bytes.
 */
function HollowStructure(name, size) {
  if (!name) {
    throw new TypeError("HollowStructure expects a name");
  }
  if (!size || size < 0) {
    throw new TypeError("HollowStructure expects a (positive) size");
  }

  // A mapping from offsets in the struct to name/type pairs
  // (or nothing if no field starts at that offset).
  this.offset_to_field_info = [];

  // The name of the struct
  this.name = name;

  // The size of the struct, in bytes
  this.size = size;

  // The number of paddings inserted so far.
  // Used to give distinct names to padding fields.
  this._paddings = 0;
}
HollowStructure.prototype = {
  /**
   * Add a field at a given offset.
   *
   * @param {number} offset The offset at which to insert the field.
   * @param {string} name The name of the field.
   * @param {CType|Type} type The type of the field.
   */
  add_field_at: function add_field_at(offset, name, type) {
    if (offset == null) {
      throw new TypeError("add_field_at requires a non-null offset");
    }
    if (!name) {
      throw new TypeError("add_field_at requires a non-null name");
    }
    if (!type) {
      throw new TypeError("add_field_at requires a non-null type");
    }
    if (type instanceof Type) {
      type = type.implementation;
    }
    if (this.offset_to_field_info[offset]) {
      throw new Error("HollowStructure " + this.name +
                      " already has a field at offset " + offset);
    }
    if (offset + type.size > this.size) {
      throw new Error("HollowStructure " + this.name +
                      " cannot place a value of type " + type +
                      " at offset " + offset +
                      " without exceeding its size of " + this.size);
    }
    let field = {name: name, type:type};
    this.offset_to_field_info[offset] = field;
  },

  /**
   * Create a pseudo-field that will only serve as padding.
   *
   * @param {number} size The number of bytes in the field.
   * @return {Object} An association field-name => field-type,
   * as expected by |ctypes.StructType|.
   */
  _makePaddingField: function makePaddingField(size) {
    let field = ({});
    field["padding_" + this._paddings] =
      ctypes.ArrayType(ctypes.uint8_t, size);
    this._paddings++;
    return field;
  },

  /**
   * Convert this |HollowStructure| into a |Type|.
   */
  getType: function getType() {
    // Contents of the structure, in the format expected
    // by ctypes.StructType.
    let struct = [];

    let i = 0;
    while (i < this.size) {
      let currentField = this.offset_to_field_info[i];
      if (!currentField) {
        // No field was specified at this offset, we need to
        // introduce some padding.

        // Firstly, determine how many bytes of padding
        let padding_length = 1;
        while (i + padding_length < this.size
            && !this.offset_to_field_info[i + padding_length]) {
          ++padding_length;
        }

        // Then add the padding
        struct.push(this._makePaddingField(padding_length));

        // And proceed
        i += padding_length;
      } else {
        // We have a field at this offset.

        // Firstly, ensure that we do not have two overlapping fields
        for (let j = 1; j < currentField.type.size; ++j) {
          let candidateField = this.offset_to_field_info[i + j];
          if (candidateField) {
            throw new Error("Fields " + currentField.name +
              " and " + candidateField.name +
              " overlap at position " + (i + j));
          }
        }

        // Then add the field
        let field = ({});
        field[currentField.name] = currentField.type;
        struct.push(field);

        // And proceed
        i += currentField.type.size;
      }
    }
    let result = new Type(this.name, ctypes.StructType(this.name, struct));
    if (result.implementation.size != this.size) {
      throw new Error("Wrong size for type " + this.name +
          ": expected " + this.size +
          ", found " + result.implementation.size +
          " (" + result.implementation.toSource() + ")");
    }
    return result;
  }
};
exports.HollowStructure = HollowStructure;

/**
 * Representation of a native library.
 *
 * The native library is opened lazily, during the first call to its
 * field |library| or whenever accessing one of the methods imported
 * with declareLazyFFI.
 *
 * @param {string} name A human-readable name for the library. Used
 * for debugging and error reporting.
 * @param {string...} candidates A list of system libraries that may
 * represent this library. Used e.g. to try different library names
 * on distinct operating systems ("libxul", "XUL", etc.).
 *
 * @constructor
 */
function Library(name, ...candidates) {
  this.name = name;
  this._candidates = candidates;
};
Library.prototype = Object.freeze({
  /**
   * The native library as a js-ctypes object.
   *
   * @throws {Error} If none of the candidate libraries could be opened.
   */
  get library() {
    let library;
    delete this.library;
    for (let candidate of this._candidates) {
      try {
        library = ctypes.open(candidate);
      } catch (ex) {
        LOG("Could not open library", candidate, ex);
      }
    }
    this._candidates = null;
    if (library) {
      Object.defineProperty(this, "library", {
        value: library
      });
      Object.freeze(this);
      return library;
    }
    let error = new Error("Could not open library " + this.name);
    Object.defineProperty(this, "library", {
      get: function() {
        throw error;
      }
    });
    Object.freeze(this);
    throw error;
  },

  /**
   * Declare a function, lazily.
   *
   * @param {object} The object containing the function as a field.
   * @param {string} The name of the field containing the function.
   * @param {string} symbol The name of the function, as defined in the
   * library.
   * @param {ctypes.abi} abi The abi to use, or |null| for default.
   * @param {Type} returnType The type of values returned by the function.
   * @param {...Type} argTypes The type of arguments to the function.
   */
  declareLazyFFI: function(object, field, ...args) {
    let lib = this;
    Object.defineProperty(object, field, {
      get: function() {
        delete this[field];
        let ffi = declareFFI(lib.library, ...args);
        if (ffi) {
          return this[field] = ffi;
        }
        return undefined;
      },
      configurable: true,
      enumerable: true
    });
  },

  toString: function() {
    return "[Library " + this.name + "]";
  }
});
exports.Library = Library;

/**
 * Declare a function through js-ctypes
 *
 * @param {ctypes.library} lib The ctypes library holding the function.
 * @param {string} symbol The name of the function, as defined in the
 * library.
 * @param {ctypes.abi} abi The abi to use, or |null| for default.
 * @param {Type} returnType The type of values returned by the function.
 * @param {...Type} argTypes The type of arguments to the function.
 *
 * @return null if the function could not be defined (generally because
 * it does not exist), or a JavaScript wrapper performing the call to C
 * and any type conversion required.
 */
let declareFFI = function declareFFI(lib, symbol, abi,
                                     returnType /*, argTypes ...*/) {
  LOG("Attempting to declare FFI ", symbol);
  // We guard agressively, to avoid any late surprise
  if (typeof symbol != "string") {
    throw new TypeError("declareFFI expects as first argument a string");
  }
  abi = abi || ctypes.default_abi;
  if (Object.prototype.toString.call(abi) != "[object CABI]") {
    // Note: This is the only known manner of checking whether an object
    // is an abi.
    throw new TypeError("declareFFI expects as second argument an abi or null");
  }
  if (!returnType.importFromC) {
    throw new TypeError("declareFFI expects as third argument an instance of Type");
  }
  let signature = [symbol, abi];
  let argtypes  = [];
  for (let i = 3; i < arguments.length; ++i) {
    let current = arguments[i];
    if (!current) {
      throw new TypeError("Missing type for argument " + ( i - 3 ) +
                          " of symbol " + symbol);
    }
    if (!current.implementation) {
      throw new TypeError("Missing implementation for argument " + (i - 3)
                          + " of symbol " + symbol
                          + " ( " + current.name + " )" );
    }
    signature.push(current.implementation);
  }
  try {
    let fun = lib.declare.apply(lib, signature);
    let result = function ffi(...args) {
      for (let i = 0; i < args.length; i++) {
        if (typeof args[i] == "undefined") {
          throw new TypeError("Argument " + i + " of " + symbol + " is undefined");
        }
      }
      let result = fun.apply(fun, args);
      return returnType.importFromC(result, symbol);
    };
    LOG("Function", symbol, "declared");
    return result;
  } catch (x) {
    // Note: Not being able to declare a function is normal.
    // Some functions are OS (or OS version)-specific.
    LOG("Could not declare function ", symbol, x);
    return null;
  }
};
exports.declareFFI = declareFFI;

/**
 * Define a lazy getter to a js-ctypes function using declareFFI.
 *
 * @param {object} The object containing the function as a field.
 * @param {string} The name of the field containing the function.
 * @param {ctypes.library} lib The ctypes library holding the function.
 * @param {string} symbol The name of the function, as defined in the
 * library.
 * @param {ctypes.abi} abi The abi to use, or |null| for default.
 * @param {Type} returnType The type of values returned by the function.
 * @param {...Type} argTypes The type of arguments to the function.
 */
function declareLazyFFI(object, field, ...declareFFIArgs) {
  Object.defineProperty(object, field, {
    get: function() {
      delete this[field];
      let ffi = declareFFI(...declareFFIArgs);
      if (ffi) {
        return this[field] = ffi;
      }
      return undefined;
    },
    configurable: true,
    enumerable: true
  });
}
exports.declareLazyFFI = declareLazyFFI;

/**
 * Define a lazy getter to a js-ctypes function using ctypes method declare.
 *
 * @param {object} The object containing the function as a field.
 * @param {string} The name of the field containing the function.
 * @param {ctypes.library} lib The ctypes library holding the function.
 * @param {string} symbol The name of the function, as defined in the
 * library.
 * @param {ctypes.abi} abi The abi to use, or |null| for default.
 * @param {ctypes.CType} returnType The type of values returned by the function.
 * @param {...ctypes.CType} argTypes The type of arguments to the function.
 */
function declareLazy(object, field, lib, ...declareArgs) {
  Object.defineProperty(object, field, {
    get: function() {
      delete this[field];
      try {
        let ffi = lib.declare(...declareArgs);
        return this[field] = ffi;
      } catch (ex) {
        // The symbol doesn't exist
        return undefined;
      }
    },
    configurable: true
  });
}
exports.declareLazy = declareLazy;

// A bogus array type used to perform pointer arithmetics
let gOffsetByType;

/**
 * Advance a pointer by a number of items.
 *
 * This method implements adding an integer to a pointer in C.
 *
 * Example:
 *   // ptr is a uint16_t*,
 *   offsetBy(ptr, 3)
 *  // returns a uint16_t* with the address ptr + 3 * 2 bytes
 *
 * @param {C pointer} pointer The start pointer.
 * @param {number} length The number of items to advance. Must not be
 * negative.
 *
 * @return {C pointer} |pointer| advanced by |length| items
 */
let offsetBy =
  function offsetBy(pointer, length) {
    if (length === undefined || length < 0) {
      throw new TypeError("offsetBy expects a positive number");
    }
   if (!("isNull" in pointer)) {
      throw new TypeError("offsetBy expects a pointer");
    }
    if (length == 0) {
      return pointer;
    }
    let type = pointer.constructor;
    let size = type.targetType.size;
    if (size == 0 || size == null) {
      throw new TypeError("offsetBy cannot be applied to a pointer without size");
    }
    let bytes = length * size;
    if (!gOffsetByType || gOffsetByType.size <= bytes) {
      gOffsetByType = ctypes.uint8_t.array(bytes * 2);
    }
    let addr = ctypes.cast(pointer, gOffsetByType.ptr).
      contents.addressOfElement(bytes);
    return ctypes.cast(addr, type);
};
exports.offsetBy = offsetBy;

/**
 * Utility function used to normalize a Typed Array or C
 * pointer into a uint8_t C pointer.
 *
 * Future versions might extend this to other data structures.
 *
 * @param {Typed array | C pointer} candidate The buffer. If
 * a C pointer, it must be non-null.
 * @param {number} bytes The number of bytes that |candidate| should contain.
 * Used for sanity checking if the size of |candidate| can be determined.
 *
 * @return {ptr:{C pointer}, bytes:number} A C pointer of type uint8_t,
 * corresponding to the start of |candidate|.
 */
function normalizeToPointer(candidate, bytes) {
  if (!candidate) {
    throw new TypeError("Expecting  a Typed Array or a C pointer");
  }
  let ptr;
  if ("isNull" in candidate) {
    if (candidate.isNull()) {
      throw new TypeError("Expecting a non-null pointer");
    }
    ptr = Type.uint8_t.out_ptr.cast(candidate);
    if (bytes == null) {
      throw new TypeError("C pointer missing bytes indication.");
    }
  } else if (isTypedArray(candidate)) {
    // Typed Array
    ptr = Type.uint8_t.out_ptr.implementation(candidate.buffer);
    if (bytes == null) {
      bytes = candidate.byteLength;
    } else if (candidate.byteLength < bytes) {
      throw new TypeError("Buffer is too short. I need at least " +
                         bytes +
                         " bytes but I have only " +
                         candidate.byteLength +
                          "bytes");
    }
  } else {
    throw new TypeError("Expecting  a Typed Array or a C pointer");
  }
  return {ptr: ptr, bytes: bytes};
};
exports.normalizeToPointer = normalizeToPointer;

///////////////////// OS interactions

/**
 * An OS error.
 *
 * This class is provided mostly for type-matching. If you need more
 * details about an error, you should use the platform-specific error
 * codes provided by subclasses of |OS.Shared.Error|.
 *
 * @param {string} operation The operation that failed.
 *
 * @constructor
 */
function OSError(operation) {
  Error.call(this);
  this.operation = operation;
}
exports.OSError = OSError;


///////////////////// Temporary boilerplate
// Boilerplate, to simplify the transition to require()
// Do not rely upon this symbol, it will disappear with
// bug 883050.
exports.OS = {
  Constants: exports.Constants,
  Shared: {
    LOG: LOG,
    clone: clone,
    Type: Type,
    HollowStructure: HollowStructure,
    Error: OSError,
    declareFFI: declareFFI,
    projectValue: projectValue,
    isTypedArray: isTypedArray,
    defineLazyGetter: defineLazyGetter,
    offsetBy: offsetBy
  }
};

Object.defineProperty(exports.OS.Shared, "DEBUG", {
  get: function() {
    return Config.DEBUG;
  },
  set: function(x) {
    return Config.DEBUG = x;
  }
});
Object.defineProperty(exports.OS.Shared, "TEST", {
  get: function() {
    return Config.TEST;
  },
  set: function(x) {
    return Config.TEST = x;
  }
});


///////////////////// Permanent boilerplate
if (typeof Components != "undefined") {
  this.EXPORTED_SYMBOLS = EXPORTED_SYMBOLS;
  for (let symbol of EXPORTED_SYMBOLS) {
    this[symbol] = exports[symbol];
  }
}
