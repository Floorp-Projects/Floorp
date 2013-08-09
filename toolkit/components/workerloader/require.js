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
     * Mapping from object urls to module paths.
     */
    let paths = {
      /**
       * @keys {string} The object url holding a module.
       * @values {string} The absolute path to that module.
       */
      _map: new Map(),
      /**
       * A regexp that may be used to search for all mapped paths.
       */
      get regexp() {
        if (this._regexp) {
          return this._regexp;
        }
        let objectURLs = [];
        for (let [objectURL, _] of this._map) {
          objectURLs.push(objectURL);
        }
        return this._regexp = new RegExp(objectURLs.join("|"), "g");
      },
      _regexp: null,
      /**
       * Add a mapping from an object url to a path.
       */
      set: function(url, path) {
        this._regexp = null; // invalidate regexp
        this._map.set(url, path);
      },
      /**
       * Get a mapping from an object url to a path.
       */
      get: function(url) {
        return this._map.get(url);
      },
      /**
       * Transform a string by replacing all the instances of objectURLs
       * appearing in that string with the corresponding module path.
       *
       * This is used typically to translate exception stacks.
       *
       * @param {string} source A source string.
       * @return {string} The same string as |source|, in which every occurrence
       * of an objectURL registered in this object has been replaced with the
       * corresponding module path.
       */
      substitute: function(source) {
        let map = this._map;
        return source.replace(this.regexp, function(url) {
          return map.get(url) || url;
        }, "g");
      }
    };

    /**
     * A human-readable version of |stack|.
     *
     * @type {string}
     */
    Object.defineProperty(Error.prototype, "moduleStack",
    {
      get: function() {
        return paths.substitute(this.stack);
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
        return paths.substitute(this.fileName);
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


      // Load source of module, synchronously
      let xhr = new XMLHttpRequest();
      xhr.open("GET", uri, false);
      xhr.responseType = "text";
      xhr.send();


      let source = xhr.responseText;
      let name = ":" + path;
      let objectURL;
      try {
        if (source == "") {
          // There doesn't seem to be a better way to detect that the file couldn't be found
          throw new Error("Could not find module " + path);
        }
        // From the source, build a function and an object URL. We
        // avoid any newline at the start of the file to ensure that
        // we do not mess up with line numbers. However, using object URLs
        // messes up with stack traces in instances of Error().
        source = "require._tmpModules[\"" + name + "\"] = " +
          "function(exports, require, module) {" +
          source +
        "\n}\n";
        let blob = new Blob([(new TextEncoder()).encode(source)]);
        objectURL = URL.createObjectURL(blob);
        paths.set(objectURL, path);
        importScripts(objectURL);
        require._tmpModules[name].call(null, exports, require, module);

      } catch (ex) {
        // Module loading has failed, exports should not be made available
        // after all.
        modules.delete(path);
        throw ex;
      } finally {
        if (objectURL) {
          // Clean up the object url as soon as possible. It will not be needed.
          URL.revokeObjectURL(objectURL);
        }
        delete require._tmpModules[name];
      }

      Object.freeze(module.exports);
      Object.freeze(module);
      return module.exports;
    };
  })();

  /**
   * An object used to hold temporarily the module constructors
   * while they are being loaded.
   *
   * @keys {string} The path to the module, prefixed with ":".
   * @values {function} A function wrapping the module.
   */
  require._tmpModules = Object.create(null);
  Object.freeze(require);

  Object.defineProperty(exports, "require", {
    value: require,
    enumerable: true,
    configurable: false
  });
})(this);
