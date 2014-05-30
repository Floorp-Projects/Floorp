/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// A CommonJS module loader that is designed to run inside a worker debugger.
// We can't simply use the SDK module loader, because it relies heavily on
// Components, which isn't available in workers.
//
// In principle, the standard instance of the worker loader should provide the
// same built-in modules as its devtools counterpart, so that both loaders are
// interchangable on the main thread, making them easier to test.
//
// On the worker thread, some of these modules, in particular those that rely on
// the use of Components and for which the worker debugger doesn't provide an
// alternative API, will be replaced by vacuous objects. Consequently, they can
// still be required, but any attempts to use them will lead to an exception.

this.EXPORTED_SYMBOLS = ["WorkerDebuggerLoader", "worker"];

// Some notes on module ids and URLs:
//
// An id is either a relative id or an absolute id. An id is relative if and
// only if it starts with a dot. An absolute id is a normalized id if and only if
// it contains no redundant components. Every normalized id is a URL.
//
// A URL is either an absolute URL or a relative URL. A URL is absolute if and
// only if it starts with a scheme name followed by a colon and 2 slashes.

// Resolve the given relative id to an absolute id.
function resolveId(id, baseId) {
  return baseId + "/../" + id;
};

// Convert the given absolute id to a normalized id.
function normalizeId(id) {
  // An id consists of an optional root and a path. A root consists of either
  // a scheme name followed by 2 or 3 slashes, or a single slash. Slashes in the
  // root are not used as separators, so only normalize the path.
  let [_, root, path] = id.match(/^(\w+:\/\/\/?|\/)?(.*)/);

  let stack = [];
  path.split("/").forEach(function (component) {
    switch (component) {
    case "":
    case ".":
      break;
    case "..":
      if (stack.length === 0) {
        if (root !== undefined) {
          throw new Error("can't convert absolute id " + id + " to " +
                          "normalized id because it's going past root!");
        } else {
          stack.push("..");
        }
      } else {
        if (stack[stack.length] == "..") {
          stack.push("..");
        } else {
          stack.pop();
        }
      }
      break;
    default:
      stack.push(component);
      break;
    }
  });

  return (root ? root : "") + stack.join("/");
}

// Create a module object with the given id.
function createModule(id) {
  return Object.create(null, {
    // CommonJS specifies the id property to be non-configurable and
    // non-writable.
    id: {
      configurable: false,
      enumerable: true,
      value: id,
      writable: false
    },

    // CommonJS does not specify an exports property, so follow the NodeJS
    // convention, which is to make it non-configurable and writable.
    exports: {
      configurable: false,
      enumerable: true,
      value: Object.create(null),
      writable: true
    }
  });
};

// A whitelist of modules from which the built-in chrome module may be
// required. The idea is add all modules that depend on chrome to the whitelist
// initially, and then remove them one by one, fixing any errors as we go along.
// Once the whitelist is empty, we can remove the built-in chrome module from
// the loader entirely.
//
// TODO: Remove this when the whitelist becomes empty
let chromeWhitelist = [
  "devtools/toolkit/DevToolsUtils",
  "devtools/toolkit/event-emitter",
];

// Create a CommonJS loader with the following options:
// - createSandbox:
//     A function that will be used to create sandboxes. It takes the name and
//     prototype of the sandbox to be created, and should return the newly
//     created sandbox as result. This option is mandatory.
// - globals:
//     A map of built-in globals that will be exposed to every module. Defaults
//     to the empty map.
// - loadInSandbox:
//     A function that will be used to load scripts in sandboxes. It takes the
//     URL from which and the sandbox in which the script is to be loaded, and
//     should not return a result. This option is mandatory.
// - modules:
//     A map of built-in modules that will be added to the module cache.
//     Defaults to the empty map.
// - paths:
//     A map of paths to base URLs that will be used to resolve relative URLs to
//     absolute URLS. Defaults to the empty map.
// - resolve:
//     A function that will be used to resolve relative ids to absolute ids. It
//     takes the relative id of the module to be required and the normalized id
//     of the requiring module as arguments, and should return the absolute id
//     of the module to be required as result. Defaults to resolveId above.
function WorkerDebuggerLoader(options) {
  // Resolve the given relative URL to an absolute URL.
  function resolveURL(url) {
    let found = false;
    for (let [path, baseURL] of paths) {
      if (url.startsWith(path)) {
        found = true;
        url = url.replace(path, baseURL);
        break;
      }
    }
    if (!found) {
      throw new Error("can't resolve relative URL " + url + " to absolute " +
                      "URL!");
    }

    // If the url has no extension, use ".js" by default.
    return url.endsWith(".js") ? url : url + ".js";
  }

  // Load the given module with the given url.
  function loadModule(module, url) {
    // CommonJS specifies 3 free variables named require, exports, and module,
    // that must be exposed to every module, so define these as properties on
    // the sandbox prototype. Additional built-in globals are exposed by making
    // the map of built-in globals the prototype of the sandbox prototype.
    let prototype = Object.create(globals);
    prototype.Components = {};
    prototype.require = createRequire(module);
    prototype.exports = module.exports;
    prototype.module = module;

    let sandbox = createSandbox(url, prototype);
    try {
      loadInSandbox(url, sandbox);
    } catch (error) {
      if (String(error) === "Error opening input stream (invalid filename?)") {
        throw new Error("can't load module " + module.id + " with url " + url +
                        "!");
      }
      throw error;
    }

    // The value of exports may have been changed by the module script, so only
    // freeze it if it is still an object.
    if (typeof module.exports === "object" && module.exports !== null) {
      Object.freeze(module.exports);
    }
  };

  // Create a require function for the given requirer. If no requirer is given,
  // create a require function for top-level modules instead.
  function createRequire(requirer) {
    return function require(id) {
      // Make sure an id was passed.
      if (id === undefined) {
        throw new Error("can't require module without id!");
      }

      // If the module to be required is the built-in chrome module, and the
      // requirer is not in the whitelist, return a vacuous object as if the
      // module was unavailable.
      //
      // TODO: Remove this when the whitelist becomes empty
      if (id === "chrome" && chromeWhitelist.indexOf(requirer.id) < 0) {
        return { CC: undefined, Cc: undefined,
                 ChromeWorker: undefined, Cm: undefined, Ci: undefined, Cu: undefined,
                 Cr: undefined, components: undefined };
      }

      // Built-in modules are cached by id rather than URL, so try to find the
      // module to be required by id first.
      let module = modules[id];
      if (module === undefined) {
        // Failed to find the module to be required by id, so convert the id to
        // a URL and try again.

        // If the id is relative, resolve it to an absolute id.
        if (id.startsWith(".")) {
          if (requirer === undefined) {
            throw new Error("can't require top-level module with relative id " +
                            id + "!");
          }
          id = resolve(id, requirer.id);
        }

        // Convert the absolute id to a normalized id.
        id = normalizeId(id);

        // Convert the normalized id to a URL.
        let url = id;

        // If the URL is relative, resolve it to an absolute URL.
        if (url.match(/^\w+:\/\//) === null) {
          url = resolveURL(id);
        }

        // Try to find the module to be required by URL.
        module = modules[url];
        if (module === undefined) {
          // Failed to find the module to be required in the cache, so create
          // a new module, load it with the given URL, and add it to the cache.

          // Add modules to the cache early so that any recursive calls to
          // require for the same module will return the partially-loaded module
          // from the cache instead of triggering a new load.
          module = modules[url] = createModule(id);

          try {
            loadModule(module, url);
          } catch (error) {
            // If the module failed to load, remove it from the cache so that
            // subsequent calls to require for the same module will trigger a
            // new load instead of returning a partially-loaded module from
            // the cache.
            delete modules[url];
            throw error;
          }

          Object.freeze(module);
        }
      }

      return module.exports;
    };
  }

  let createSandbox = options.createSandbox;
  let globals = options.globals || Object.create(null);
  let loadInSandbox = options.loadInSandbox;

  // Create the module cache by converting each entry in the map of built-in
  // modules to a module object with its exports property set to a frozen
  // version of the original entry.
  let modules = options.modules || {};
  for (let id in modules) {
    let module = createModule(id);
    module.exports = Object.freeze(modules[id]);
    modules[id] = module;
  }

  // Convert the map of paths to baseURLs into an array for use by resolveURL.
  // The array is sorted from longest to shortest path so the longest path will
  // always be the first to be found.
  let paths = options.paths || {};
  paths = Object.keys(paths)
                .sort((a, b) => b.length - a.length)
                .map(path => [path, paths[path]]);

  let resolve = options.resolve || resolveId;

  this.require = createRequire();
}

this.WorkerDebuggerLoader = WorkerDebuggerLoader;

// The default instance of the worker debugger loader is defined differently
// depending on whether it is loaded from the main thread or a worker thread.
if (typeof Components === "object") {
  (function () {
    const { Constructor: CC, classes: Cc, manager: Cm, interfaces: Ci,
            results: Cr, utils: Cu } = Components;

    const principal = CC('@mozilla.org/systemprincipal;1', 'nsIPrincipal')();

    // Create a sandbox with the given name and prototype.
    const createSandbox = function (name, prototype) {
      return Cu.Sandbox(principal, {
        invisibleToDebugger: true,
        sandboxName: name,
        sandboxPrototype: prototype,
        wantComponents: false,
        wantXrays: false
      });
    };

    const loadSubScript = Cc['@mozilla.org/moz/jssubscript-loader;1'].
                          getService(Ci.mozIJSSubScriptLoader).loadSubScript;

    // Load a script from the given URL in the given sandbox.
    const loadInSandbox = function (url, sandbox) {
      loadSubScript(url, sandbox, "UTF-8");
    };

    // Define the Debugger object in a sandbox to ensure that the this passed to
    // addDebuggerToGlobal is a global.
    const Debugger = (function () {
      let sandbox = Cu.Sandbox(principal, {});
      Cu.evalInSandbox(
        "Components.utils.import('resource://gre/modules/jsdebugger.jsm');" +
        "addDebuggerToGlobal(this);",
        sandbox
      );
      return sandbox.Debugger;
    })();

    // TODO: Either replace these built-in modules with vacuous objects, or
    // provide them in a way that does not depend on the use of Components.
    const { Promise } = Cu.import("resource://gre/modules/Promise.jsm", {});;
    const { Services } = Cu.import("resource://gre/modules/Services.jsm", {});;
    let SourceMap = {};
    Cu.import("resource://gre/modules/devtools/SourceMap.jsm", SourceMap);
    const Timer = Cu.import("resource://gre/modules/Timer.jsm", {});
    const chrome = { CC: Function.bind.call(CC, Components), Cc: Cc,
                     ChromeWorker: ChromeWorker, Cm: Cm, Ci: Ci, Cu: Cu,
                     Cr: Cr, components: Components };
    const xpcInspector = Cc["@mozilla.org/jsinspector;1"].
                         getService(Ci.nsIJSInspector);

    this.worker = new WorkerDebuggerLoader({
      createSandbox: createSandbox,
      globals: {
        "promise": Promise,
        "reportError": Cu.reportError,
      },
      loadInSandbox: loadInSandbox,
      modules: {
        "Debugger": Debugger,
        "Services": Object.create(Services),
        "Timer": Object.create(Timer),
        "chrome": chrome,
        "source-map": SourceMap,
        "xpcInspector": xpcInspector,
      },
      paths: {
        "": "resource://gre/modules/commonjs/",
        "devtools": "resource:///modules/devtools",
        "devtools/server": "resource://gre/modules/devtools/server",
        "devtools/toolkit": "resource://gre/modules/devtools",
        "xpcshell-test": "resource://test",
      }
    });
  }).call(this);
}
