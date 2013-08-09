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

let loader = Cu.import("resource://gre/modules/commonjs/toolkit/loader.js", {}).Loader;
let promise = Cu.import("resource://gre/modules/commonjs/sdk/core/promise.js", {}).Promise;

this.EXPORTED_SYMBOLS = ["devtools"];

/**
 * Providers are different strategies for loading the devtools.
 */

let loaderGlobals = {
  btoa: btoa,
  console: console,
  _Iterator: Iterator,
  loader: {
    lazyGetter: XPCOMUtils.defineLazyGetter.bind(XPCOMUtils),
    lazyImporter: XPCOMUtils.defineLazyModuleGetter.bind(XPCOMUtils),
    lazyServiceGetter: XPCOMUtils.defineLazyServiceGetter.bind(XPCOMUtils)
  }
}

// Used when the tools should be loaded from the Firefox package itself (the default)
var BuiltinProvider = {
  load: function(done) {
    this.loader = new loader.Loader({
      modules: {
        "toolkit/loader": loader
      },
      paths: {
        "": "resource://gre/modules/commonjs/",
        "main": "resource:///modules/devtools/main.js",
        "devtools": "resource:///modules/devtools",
        "devtools/server": "resource://gre/modules/devtools/server",
        "devtools/toolkit/webconsole": "resource://gre/modules/devtools/toolkit/webconsole",
        "devtools/styleinspector/css-logic": "resource://gre/modules/devtools/styleinspector/css-logic",

        // Allow access to xpcshell test items from the loader.
        "xpcshell-test": "resource://test"
      },
      globals: loaderGlobals
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
var SrcdirProvider = {
  fileURI: function(path) {
    let file = new FileUtils.File(path);
    return Services.io.newFileURI(file).spec;
  },

  load: function(done) {
    let srcdir = Services.prefs.getComplexValue("devtools.loader.srcdir",
                                                Ci.nsISupportsString);
    srcdir = OS.Path.normalize(srcdir.data.trim());
    let devtoolsDir = OS.Path.join(srcdir, "browser", "devtools");
    let devtoolsURI = this.fileURI(devtoolsDir);
    let toolkitURI = this.fileURI(OS.Path.join(srcdir, "toolkit", "devtools"));
    let serverURI = this.fileURI(OS.Path.join(srcdir, "toolkit", "devtools", "server"));
    let webconsoleURI = this.fileURI(OS.Path.join(srcdir, "toolkit", "devtools", "webconsole"));
    let cssLogicURI = this.fileURI(OS.Path.join(toolkitURI, "styleinspector", "css-logic"));

    let mainURI = this.fileURI(OS.Path.join(srcdir, "browser", "devtools", "main.js"));
    this.loader = new loader.Loader({
      modules: {
        "toolkit/loader": loader
      },
      paths: {
        "": "resource://gre/modules/commonjs/",
        "devtools/server": serverURI,
        "devtools/toolkit/webconsole": webconsoleURI,
        "devtools": devtoolsURI,
        "devtools/styleinspector/css-logic": cssLogicURI,
        "main": mainURI
      },
      globals: loaderGlobals
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
 * exports from the main module.
 */
this.devtools = {
  _provider: null,

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
    return loader.load(this._provider.loader, module).exports;
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
    this._main = loader.main(this._provider.loader, id);

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
      this.setProvider(SrcdirProvider);
    } else {
      this.setProvider(BuiltinProvider);
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
};

// Now load the tools.
devtools._chooseProvider();
