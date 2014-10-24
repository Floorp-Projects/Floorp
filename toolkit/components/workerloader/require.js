/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
 * Implementation of a CommonJS module loader for workers.
 *
 * Use:
 * // in the .js file loaded by the constructor of the worker
 * importScripts("resource://gre/modules/workers/require.js");
 * let module = require("resource://gre/modules/worker/myModule.js");
 *
 * // in myModule.js
 * // Load dependencies
 * let SimpleTest = require("resource://gre/modules/workers/SimpleTest.js");
 * let Logger = require("resource://gre/modules/workers/Logger.js");
 *
 * // Define things that will not be exported
 * let someValue = // ...
 *
 * // Export symbols
 * exports.foo = // ...
 * exports.bar = // ...
 *
 *
 * Note #1:
 * Properties |fileName| and |stack| of errors triggered from a module
 * contain file names that do not correspond to human-readable module paths.
 * Human readers should rather use properties |moduleName| and |moduleStack|.
 *
 * Note #2:
 * The current version of |require()| only accepts absolute URIs.
 *
 * Note #3:
 * By opposition to some other module loader implementations, this module
 * loader does not enforce separation of global objects. Consequently, if
 * a module modifies a global object (e.g. |String.prototype|), all other
 * modules in the same worker may be affected.
 */


(function(exports) {
  "use strict";

  if (exports.require) {
    // Avoid double-imports
    return;
  }

  // Simple implementation of |require|
  let require = (function() {

    /**
     * Mapping from module paths to module exports.
     *
     * @keys {string} The absolute path to a module.
     * @values {object} The |exports| objects for that module.
     */
    let modules = new Map();

    /**
     * A human-readable version of |stack|.
     *
     * @type {string}
     */
    Object.defineProperty(Error.prototype, "moduleStack",
    {
      get: function() {
        return this.stack;
      }
    });
    /**
     * A human-readable version of |fileName|.
     *
     * @type {string}
     */
    Object.defineProperty(Error.prototype, "moduleName",
    {
      get: function() {
        let match = this.stack.match(/\@(.*):.*:/);
        if (match) {
          return match[1];
        }
        return "(unknown module)";
      }
    });

    /**
     * Import a module
     *
     * @param {string} path The path to the module.
     * @return {*} An object containing the properties exported by the module.
     */
    return function require(path) {
      if (typeof path != "string" || path.indexOf("://") == -1) {
        throw new TypeError("The argument to require() must be a string uri, got " + path);
      }
      // Automatically add ".js" if there is no extension
      let uri;
      if (path.lastIndexOf(".") <= path.lastIndexOf("/")) {
        uri = path + ".js";
      } else {
        uri = path;
      }

      // Exports provided by the module
      let exports = Object.create(null);

      // Identification of the module
      let module = {
        id: path,
        uri: uri,
        exports: exports
      };

      // Make module available immediately
      // (necessary in case of circular dependencies)
      if (modules.has(path)) {
        return modules.get(path).exports;
      }
      modules.set(path, module);

      let name = ":" + path;
      let objectURL;
      try {
        // Load source of module, synchronously
        let xhr = new XMLHttpRequest();
        xhr.open("GET", uri, false);
        xhr.responseType = "text";
        xhr.send();


        let source = xhr.responseText;
        if (source == "") {
          // There doesn't seem to be a better way to detect that the file couldn't be found
          throw new Error("Could not find module " + path);
        }
        let code = new Function("exports", "require", "module",
          source + "\n//# sourceURL=" + uri + "\n"
        );
        code(exports, require, module);
      } catch (ex) {
        // Module loading has failed, exports should not be made available
        // after all.
        modules.delete(path);
        throw ex;
      }

      Object.freeze(module.exports);
      Object.freeze(module);
      return module.exports;
    };
  })();

  Object.freeze(require);

  Object.defineProperty(exports, "require", {
    value: require,
    enumerable: true,
    configurable: false
  });
})(this);
