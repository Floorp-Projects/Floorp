/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

{
  if (typeof Components != "undefined") {
    // We do not wish osfile_shared.jsm to be used directly as a main thread
    // module yet. When time comes, it will be loaded by a combination of
    // a main thread front-end/worker thread implementation that makes sure
    // that we are not executing synchronous IO code in the main thread.

    throw new Error("osfile_shared.jsm cannot be used from the main thread yet");
  }

  (function(exports) {
     "use strict";
     /*
      * This block defines |OS.Shared.Type|. However, |OS| can exist already
      * (in particular, if this code is executed in a worker thread, it is
      * defined).
      */
     if (!exports.OS) {
       exports.OS = {};
     }
     if (!exports.OS.Shared) {
       exports.OS.Shared = {};
     }
     if (exports.OS.Shared.Type) {
       return; // Avoid double-initialization
     }

     let LOG;
     if (typeof console != "undefined" && console.log) {
       LOG = console.log.bind(console, "OS");
     } else {
       LOG = function() {
         let text = "OS";
         for (let i = 0; i < arguments.length; ++i) {
           text += (" " + arguments[i]);
         }
         dump(text + "\n");
       };
     }
     exports.OS.Shared.LOG = LOG;

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
     exports.OS.Shared.Error = OSError;

     /**
      * Abstraction above js-ctypes types.
      *
      * Use values of this type to register FFI functions. In addition to the
      * usual features of js-ctypes, values of this type perform the necessary
      * transformations to ensure that C errors are handled nicely, to connect
      * resources with their finalizer, etc.
      *
      * @param {string} name The name of the type. Must be unique.
      * @param {function=} convert_from_c A function to call to construct a
      * value of this type from a C return value.
      *
      * @constructor
      */
     function Type(name, implementation, convert_from_c) {
       if (!(typeof name == "string")) {
         throw new TypeError("Type expects as first argument a name, got: "
                             + name);
       }
       if (!(implementation instanceof ctypes.CType)) {
         throw new TypeError("Type expects as second argument a ctypes.CType"+
                             ", got: " + implementation);
       }
       this.name = name;
       this.implementation = implementation;
       if (convert_from_c) {
         this.convert_from_c = convert_from_c;
       } else {// Optimization: Ensure harmony of shapes
         this.convert_from_c = Type.prototype.convert_from_c;
       }
     }
     Type.prototype = {
       convert_from_c: function(value) {
         return value;
       },

       /**
        * A pointer/array used to pass data to the foreign function.
        */
       get in_ptr() {
         delete this.in_ptr;
         let ptr_t = new Type("[int] " + this.name + "*",
           this.implementation.ptr);
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
         let ptr_t = new Type("[out] " + this.name + "*",
           this.implementation.ptr);
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
         let ptr_t = new Type("[inout] " + this.name + "*",
           this.implementation.ptr);
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
       releaseWith: function(finalizer) {
         let parent = this;
         let type = new Type("[auto " + finalizer +"] " + this.name,
           this.implementation,
           function (value, operation) {
             return ctypes.CDataFinalizer(
               parent.convert_from_c(value, operation),
               finalizer);
           });
         return type;
       }
     };

     exports.OS.Shared.Type = Type;
     let Types = Type;

     /*
      * Some values are large integers on 64 bit platforms. Unfortunately,
      * in practice, 64 bit integers cannot be manipulated in JS. We
      * therefore project them to regular numbers whenever possible.
      */

     let projectLargeInt = function projectLargeInt(x) {
       return parseInt(x.toString(), 10);
     };
     let projectLargeUInt = function projectLargeUInt(x) {
       if (ctypes.UInt64.hi(x)) {
         throw new Error("Number too large " + x +
             "(unsigned, hi: " + ctypes.UInt64.hi(x) +
                 ", lo:" + ctypes.UInt64.lo(x) + ")");
       }
       return ctypes.UInt64.lo(x);
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
       if (!type.size) {
         throw new TypeError("Argument is not a proper C type");
       }
       LOG("Determining best projection for", type,
             "(size: ", type.size, ")", signed?"signed":"unsigned");
       // Determine if type is projected to Int64/Uint64
       if (type.size == 8           // Usual case
           || type == ctypes.size_t // Special cases
           || type == ctypes.ssize_t
           || type == ctypes.intptr_t
           || type == ctypes.uintptr_t
           || type == ctypes.off_t){
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
     exports.OS.Shared.projectValue = projectValue;



     /**
      * Get the appropriate type for an unsigned int of the given size.
      *
      * This function is useful to define types such as |mode_t| whose
      * actual width depends on the OS/platform.
      *
      * @param {number} size The number of bytes requested.
      */
     Types.uintn_t = function uintn_t(size) {
       switch (size) {
       case 1: return Types.uint8_t;
       case 2: return Types.uint16_t;
       case 4: return Types.uint32_t;
       case 8: return Types.uint64_t;
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
     Types.intn_t = function intn_t(size) {
       switch (size) {
       case 1: return Types.int8_t;
       case 2: return Types.int16_t;
       case 4: return Types.int32_t;
       case 8: return Types.int64_t;
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
     Types.void_t =
       new Type("void",
                ctypes.void_t);

     /**
      * Shortcut for |void*|.
      */
     Types.voidptr_t =
       new Type("void*",
                ctypes.voidptr_t);

     // void* is a special case as we can cast any pointer to/from it
     // so we have to shortcut |in_ptr|/|out_ptr|/|inout_ptr| and
     // ensure that js-ctypes' casting mechanism is invoked directly
     ["in_ptr", "out_ptr", "inout_ptr"].forEach(function(key) {
       Object.defineProperty(Types.void_t, key,
       {
         get: function() {
           return Types.voidptr_t;
         }
       });
     });

     /**
      * A C char (one byte)
      */
     Types.char =
       new Type("char",
                ctypes.char);

     /**
      * A C wide char (two bytes)
      */
     Types.jschar =
       new Type("jschar",
                ctypes.jschar);

     /**
      * A C integer
      *
      * Size depends on the platform.
      */
     Types.int =
       new Type("int",
                ctypes.int,
                projector(ctypes.int, true));

     Types.unsigned_int =
       new Type("unsigned int",
                ctypes.unsigned_int,
                projector(ctypes.unsigned_int, false));

     /**
      * A C integer (8-bits).
      */
     Types.int8_t =
       new Type("int8_t",
                ctypes.int8_t,
                projectValue);

     Types.uint8_t =
       new Type("uint8_t",
                ctypes.uint8_t,
                projectValue);

     /**
      * A C integer (16-bits).
      *
      * Also known as WORD under Windows.
      */
     Types.int16_t =
       new Type("int16_t",
                ctypes.int16_t,
                projectValue);

     Types.uint16_t =
       new Type("uint16_t",
                ctypes.uint16_t,
                projectValue);

     /**
      * A C integer (32-bits).
      *
      * Also known as DWORD under Windows.
      */
     Types.int32_t =
       new Type("int32_t",
                ctypes.int32_t,
                projectValue);

     Types.uint32_t =
       new Type("uint32_t",
                ctypes.uint32_t,
                projectValue);

     /**
      * A C integer (64-bits).
      */
     Types.int64_t =
       new Type("int64_t",
                ctypes.int64_t,
                projectLargeInt);

     Types.uint64_t =
       new Type("uint64_t",
                ctypes.uint64_t,
                projectLargeUInt);

     /**
      * A C integer.
      * Size depends on the platform.
      */
     Types.long =
       new Type("long",
                ctypes.long,
                projector(ctypes.long, true));

     /**
      * A boolean.
      * Implemented as a C integer.
      */
     Types.bool =
       new Type("bool",
                ctypes.int,
                function projectBool(x) {
                  return !!(x.value);
                });

     /**
      * A user identifier.
      * Implemented as a C integer.
      */
     Types.uid_t =
       new Type("uid_t",
                ctypes.int,
                projector(ctypes.int, true));

     /**
      * A group identifier.
      * Implemented as a C integer.
      */
     Types.gid_t =
       new Type("gid_t",
                ctypes.int,
                projector(ctypes.int, true));

     /**
      * An offset (positive or negative).
      * Implemented as a C integer.
      */
     Types.off_t =
       new Type("off_t",
                ctypes.off_t,
                projector(ctypes.off_t, true));

     /**
      * A size (positive).
      * Implemented as a C size_t.
      */
     Types.size_t =
       new Type("size_t",
                ctypes.size_t,
                projector(ctypes.size_t, false));

     /**
      * An offset (positive or negative).
      * Implemented as a C integer.
      */
     Types.ssize_t =
       new Type("ssize_t",
                ctypes.ssize_t,
                projector(ctypes.ssize_t, true));

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
      */// Note: Future versions will use a different implementation of this
        // function on the main thread, osfile worker thread and regular worker
        // thread
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
       let signature = [symbol, abi];
       let argtypes  = [];
       for (let i = 3; i < arguments.length; ++i) {
         let current = arguments[i];
         if (!current.implementation) {
           throw new TypeError("Missing implementation for argument " + (i - 3)
                               + " of symbol " + symbol
                               + " ( " + current.name + " )" );
         }
         signature.push(current.implementation);
       }
       try {
         let fun = lib.declare.apply(lib, signature);
         let result = function ffi(/*arguments*/) {
           let result = fun.apply(fun, arguments);
           return returnType.convert_from_c(result, symbol);
         };
         if (exports.OS.Shared.DEBUG) {
           result.fun = fun; // Also return the raw FFI function.
         }
         LOG("Function", symbol, "declared");
         return result;
       } catch (x) {
         // Note: Not being able to declare a function is normal.
         // Some functions are OS (or OS version)-specific.
         LOG("Could not declare function " + symbol, x);
         return null;
       }
     };
     exports.OS.Shared.declareFFI = declareFFI;


     /**
      * Specific tools that don't really fit anywhere.
      */
     let _aux = {};
     exports.OS.Shared._aux = _aux;

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
     _aux.normalizeOpenMode = function normalizeOpenMode(mode) {
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
   })(this);
}
