/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Manages the addon-sdk loader instance used to load the developer tools.
 */

let { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "NetUtil", "resource://gre/modules/NetUtil.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "FileUtils", "resource://gre/modules/FileUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "OS", "resource://gre/modules/osfile.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "console", "resource://gre/modules/devtools/Console.jsm");

let SourceMap = {};
Cu.import("resource://gre/modules/devtools/SourceMap.jsm", SourceMap);

let loader = Cu.import("resource://gre/modules/commonjs/toolkit/loader.js", {}).Loader;
let promise = Cu.import("resource://gre/modules/commonjs/sdk/core/promise.js", {}).Promise;

this.EXPORTED_SYMBOLS = ["DevToolsLoader", "devtools", "BuiltinProvider",
                         "SrcdirProvider"];

/**
 * Providers are different strategies for loading the devtools.
 */

let loaderGlobals = {
  btoa: btoa,
  console: console,
  _Iterator: Iterator,
  ChromeWorker: ChromeWorker,
  loader: {
    lazyGetter: XPCOMUtils.defineLazyGetter.bind(XPCOMUtils),
    lazyImporter: XPCOMUtils.defineLazyModuleGetter.bind(XPCOMUtils),
    lazyServiceGetter: XPCOMUtils.defineLazyServiceGetter.bind(XPCOMUtils)
  }
};

// Used when the tools should be loaded from the Firefox package itself (the default)
function BuiltinProvider() {}
BuiltinProvider.prototype = {
  load: function() {
    this.loader = new loader.Loader({
      modules: {
        "toolkit/loader": loader,
        "source-map": SourceMap,
      },
      paths: {
        // When you add a line to this mapping, don't forget to make a
        // corresponding addition to the SrcdirProvider mapping below as well.
        "": "resource://gre/modules/commonjs/",
        "main": "resource:///modules/devtools/main.js",
        "devtools": "resource:///modules/devtools",
        "devtools/server": "resource://gre/modules/devtools/server",
        "devtools/toolkit/webconsole": "resource://gre/modules/devtools/toolkit/webconsole",
        "devtools/app-actor-front": "resource://gre/modules/devtools/app-actor-front.js",
        "devtools/styleinspector/css-logic": "resource://gre/modules/devtools/styleinspector/css-logic",
        "devtools/css-color": "resource://gre/modules/devtools/css-color",
        "devtools/output-parser": "resource://gre/modules/devtools/output-parser",
        "devtools/touch-events": "resource://gre/modules/devtools/touch-events",
        "devtools/client": "resource://gre/modules/devtools/client",
        "devtools/pretty-fast": "resource://gre/modules/devtools/pretty-fast.js",

        "acorn": "resource://gre/modules/devtools/acorn.js",
        "acorn_loose": "resource://gre/modules/devtools/acorn_loose.js",

        // Allow access to xpcshell test items from the loader.
        "xpcshell-test": "resource://test"
      },
      globals: loaderGlobals,
      invisibleToDebugger: this.invisibleToDebugger
    });

    return promise.resolve(undefined);
  },

  unload: function(reason) {
    loader.unload(this.loader, reason);
    delete this.loader;
  },
};

// Used when the tools should be loaded from a mozilla-central checkout.  In addition
// to different paths, it needs to write chrome.manifest files to override chrome urls
// from the builtin tools.
function SrcdirProvider() {}
SrcdirProvider.prototype = {
  fileURI: function(path) {
    let file = new FileUtils.File(path);
    return Services.io.newFileURI(file).spec;
  },

  load: function() {
    let srcdir = Services.prefs.getComplexValue("devtools.loader.srcdir",
                                                Ci.nsISupportsString);
    srcdir = OS.Path.normalize(srcdir.data.trim());
    let devtoolsDir = OS.Path.join(srcdir, "browser", "devtools");
    let toolkitDir = OS.Path.join(srcdir, "toolkit", "devtools");
    let mainURI = this.fileURI(OS.Path.join(devtoolsDir, "main.js"));
    let devtoolsURI = this.fileURI(devtoolsDir);
    let serverURI = this.fileURI(OS.Path.join(toolkitDir, "server"));
    let webconsoleURI = this.fileURI(OS.Path.join(toolkitDir, "webconsole"));
    let appActorURI = this.fileURI(OS.Path.join(toolkitDir, "apps", "app-actor-front.js"));
    let cssLogicURI = this.fileURI(OS.Path.join(toolkitDir, "styleinspector", "css-logic"));
    let cssColorURI = this.fileURI(OS.Path.join(toolkitDir, "css-color"));
    let outputParserURI = this.fileURI(OS.Path.join(toolkitDir, "output-parser"));
    let touchEventsURI = this.fileURI(OS.Path.join(toolkitDir, "touch-events"));
    let clientURI = this.fileURI(OS.Path.join(toolkitDir, "client"));
    let prettyFastURI = this.fileURI(OS.Path.join(toolkitDir), "pretty-fast.js");
    let acornURI = this.fileURI(OS.Path.join(toolkitDir, "acorn"));
    let acornLoosseURI = this.fileURI(OS.Path.join(toolkitDir, "acorn_loose.js"));
    this.loader = new loader.Loader({
      modules: {
        "toolkit/loader": loader,
        "source-map": SourceMap,
      },
      paths: {
        "": "resource://gre/modules/commonjs/",
        "main": mainURI,
        "devtools": devtoolsURI,
        "devtools/server": serverURI,
        "devtools/toolkit/webconsole": webconsoleURI,
        "devtools/app-actor-front": appActorURI,
        "devtools/styleinspector/css-logic": cssLogicURI,
        "devtools/css-color": cssColorURI,
        "devtools/output-parser": outputParserURI,
        "devtools/touch-events": touchEventsURI,
        "devtools/client": clientURI,
        "devtools/pretty-fast": prettyFastURI,

        "acorn": acornURI,
        "acorn_loose": acornLoosseURI
      },
      globals: loaderGlobals,
      invisibleToDebugger: this.invisibleToDebugger
    });

    return this._writeManifest(devtoolsDir).then(null, Cu.reportError);
  },

  unload: function(reason) {
    loader.unload(this.loader, reason);
    delete this.loader;
  },

  _readFile: function(filename) {
    let deferred = promise.defer();
    let file = new FileUtils.File(filename);
    NetUtil.asyncFetch(file, (inputStream, status) => {
      if (!Components.isSuccessCode(status)) {
        deferred.reject(new Error("Couldn't load manifest: " + filename + "\n"));
        return;
      }
      var data = NetUtil.readInputStreamToString(inputStream, inputStream.available());
      deferred.resolve(data);
    });
    return deferred.promise;
  },

  _writeFile: function(filename, data) {
    let deferred = promise.defer();
    let file = new FileUtils.File(filename);

    var ostream = FileUtils.openSafeFileOutputStream(file)

    var converter = Cc["@mozilla.org/intl/scriptableunicodeconverter"].
                    createInstance(Ci.nsIScriptableUnicodeConverter);
    converter.charset = "UTF-8";
    var istream = converter.convertToInputStream(data);
    NetUtil.asyncCopy(istream, ostream, (status) => {
      if (!Components.isSuccessCode(status)) {
        deferred.reject(new Error("Couldn't write manifest: " + filename + "\n"));
        return;
      }

      deferred.resolve(null);
    });
    return deferred.promise;
  },

  _writeManifest: function(dir) {
    return this._readFile(OS.Path.join(dir, "jar.mn")).then((data) => {
      // The file data is contained within inputStream.
      // You can read it into a string with
      let entries = [];
      let lines = data.split(/\n/);
      let preprocessed = /^\s*\*/;
      let contentEntry = new RegExp("^\\s+content/(\\w+)/(\\S+)\\s+\\((\\S+)\\)");
      for (let line of lines) {
        if (preprocessed.test(line)) {
          dump("Unable to override preprocessed file: " + line + "\n");
          continue;
        }
        let match = contentEntry.exec(line);
        if (match) {
          let pathComponents = match[3].split("/");
          pathComponents.unshift(dir);
          let path = OS.Path.join.apply(OS.Path, pathComponents);
          let uri = this.fileURI(path);
          let entry = "override chrome://" + match[1] + "/content/" + match[2] + "\t" + uri;
          entries.push(entry);
        }
      }
      return this._writeFile(OS.Path.join(dir, "chrome.manifest"), entries.join("\n"));
    }).then(() => {
      Components.manager.addBootstrappedManifestLocation(new FileUtils.File(dir));
    });
  }
};

/**
 * The main devtools API.
 * In addition to a few loader-related details, this object will also include all
 * exports from the main module.  The standard instance of this loader is
 * exported as |devtools| below, but if a fresh copy of the loader is needed,
 * then a new one can also be created.
 */
this.DevToolsLoader = function DevToolsLoader() {
  this.require = this.require.bind(this);
};

DevToolsLoader.prototype = {
  get provider() {
    if (!this._provider) {
      this._chooseProvider();
    }
    return this._provider;
  },

  _provider: null,

  /**
   * A dummy version of require, in case a provider hasn't been chosen yet when
   * this is first called.  This will then be replaced by the real version.
   * @see setProvider
   */
  require: function() {
    this._chooseProvider();
    return this.require.apply(this, arguments);
  },

  /**
   * Add a URI to the loader.
   * @param string id
   *    The module id that can be used within the loader to refer to this module.
   * @param string uri
   *    The URI to load as a module.
   * @returns The module's exports.
   */
  loadURI: function(id, uri) {
    let module = loader.Module(id, uri);
    return loader.load(this.provider.loader, module).exports;
  },

  /**
   * Let the loader know the ID of the main module to load.
   *
   * The loader doesn't need a main module, but it's nice to have.  This
   * will be called by the browser devtools to load the devtools/main module.
   *
   * When only using the server, there's no main module, and this method
   * can be ignored.
   */
  main: function(id) {
    this._mainid = id;
    this._main = loader.main(this.provider.loader, id);

    // Mirror the main module's exports on this object.
    Object.getOwnPropertyNames(this._main).forEach(key => {
      XPCOMUtils.defineLazyGetter(this, key, () => this._main[key]);
    });
  },

  /**
   * Override the provider used to load the tools.
   */
  setProvider: function(provider) {
    if (provider === this._provider) {
      return;
    }

    if (this._provider) {
      var events = this.require("sdk/system/events");
      events.emit("devtools-unloaded", {});
      delete this.require;
      this._provider.unload("newprovider");
    }
    this._provider = provider;
    this._provider.invisibleToDebugger = this.invisibleToDebugger;
    this._provider.load();
    this.require = loader.Require(this._provider.loader, { id: "devtools" });

    if (this._mainid) {
      this.main(this._mainid);
    }
  },

  /**
   * Choose a default tools provider based on the preferences.
   */
  _chooseProvider: function() {
    if (Services.prefs.prefHasUserValue("devtools.loader.srcdir")) {
      this.setProvider(new SrcdirProvider());
    } else {
      this.setProvider(new BuiltinProvider());
    }
  },

  /**
   * Reload the current provider.
   */
  reload: function() {
    var events = this.require("sdk/system/events");
    events.emit("startupcache-invalidate", {});
    events.emit("devtools-unloaded", {});

    this._provider.unload("reload");
    delete this._provider;
    this._chooseProvider();
  },

  /**
   * Sets whether the compartments loaded by this instance should be invisible
   * to the debugger.  Invisibility is needed for loaders that support debugging
   * of chrome code.  This is true of remote target environments, like Fennec or
   * B2G.  It is not the default case for desktop Firefox because we offer the
   * Browser Toolbox for chrome debugging there, which uses its own, separate
   * loader instance.
   * @see browser/devtools/framework/ToolboxProcess.jsm
   */
  invisibleToDebugger: Services.appinfo.name !== "Firefox"
};

// Export the standard instance of DevToolsLoader used by the tools.
this.devtools = new DevToolsLoader();
