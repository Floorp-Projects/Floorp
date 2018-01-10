/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/**
 * This module contains extension testing helper logic which is common
 * between all test suites.
 */

/* exported ExtensionTestCommon, MockExtension */

this.EXPORTED_SYMBOLS = ["ExtensionTestCommon", "MockExtension"];

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.importGlobalProperties(["TextEncoder"]);

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "AddonManager",
                                  "resource://gre/modules/AddonManager.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Extension",
                                  "resource://gre/modules/Extension.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "ExtensionParent",
                                  "resource://gre/modules/ExtensionParent.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "FileUtils",
                                  "resource://gre/modules/FileUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "OS",
                                  "resource://gre/modules/osfile.jsm");

XPCOMUtils.defineLazyGetter(this, "apiManager",
                            () => ExtensionParent.apiManager);

Cu.import("resource://gre/modules/ExtensionUtils.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "uuidGen",
                                   "@mozilla.org/uuid-generator;1",
                                   "nsIUUIDGenerator");

const {
  flushJarCache,
  instanceOf,
} = ExtensionUtils;

XPCOMUtils.defineLazyGetter(this, "console", ExtensionUtils.getConsole);


/**
 * A skeleton Extension-like object, used for testing, which installs an
 * add-on via the add-on manager when startup() is called, and
 * uninstalles it on shutdown().
 *
 * @param {string} id
 * @param {nsIFile} file
 * @param {nsIURI} rootURI
 * @param {string} installType
 */
class MockExtension {
  constructor(file, rootURI, installType) {
    this.id = null;
    this.file = file;
    this.rootURI = rootURI;
    this.installType = installType;
    this.addon = null;

    let promiseEvent = eventName => new Promise(resolve => {
      let onstartup = (msg, extension) => {
        if (this.addon && extension.id == this.addon.id) {
          apiManager.off(eventName, onstartup);

          this.id = extension.id;
          this._extension = extension;
          resolve(extension);
        }
      };
      apiManager.on(eventName, onstartup);
    });

    this._extension = null;
    this._extensionPromise = promiseEvent("startup");
    this._readyPromise = promiseEvent("ready");
  }

  testMessage(...args) {
    return this._extension.testMessage(...args);
  }

  on(...args) {
    this._extensionPromise.then(extension => {
      extension.on(...args);
    });
  }

  off(...args) {
    this._extensionPromise.then(extension => {
      extension.off(...args);
    });
  }

  startup() {
    if (this.installType == "temporary") {
      return AddonManager.installTemporaryAddon(this.file).then(addon => {
        this.addon = addon;
        return this._readyPromise;
      });
    } else if (this.installType == "permanent") {
      return new Promise((resolve, reject) => {
        AddonManager.getInstallForFile(this.file, install => {
          let listener = {
            onInstallFailed: reject,
            onInstallEnded: (install, newAddon) => {
              this.addon = newAddon;
              resolve(this._readyPromise);
            },
          };

          install.addListener(listener);
          install.install();
        });
      });
    }
    throw new Error("installType must be one of: temporary, permanent");
  }

  shutdown() {
    this.addon.uninstall();
    return this.cleanupGeneratedFile();
  }

  cleanupGeneratedFile() {
    return this._extensionPromise.then(extension => {
      return extension.broadcast("Extension:FlushJarCache", {path: this.file.path});
    }).then(() => {
      return OS.File.remove(this.file.path);
    });
  }
}

this.ExtensionTestCommon = class ExtensionTestCommon {
  /**
   * This code is designed to make it easy to test a WebExtension
   * without creating a bunch of files. Everything is contained in a
   * single JSON blob.
   *
   * Properties:
   *   "background": "<JS code>"
   *     A script to be loaded as the background script.
   *     The "background" section of the "manifest" property is overwritten
   *     if this is provided.
   *   "manifest": {...}
   *     Contents of manifest.json
   *   "files": {"filename1": "contents1", ...}
   *     Data to be included as files. Can be referenced from the manifest.
   *     If a manifest file is provided here, it takes precedence over
   *     a generated one. Always use "/" as a directory separator.
   *     Directories should appear here only implicitly (as a prefix
   *     to file names)
   *
   * To make things easier, the value of "background" and "files"[] can
   * be a function, which is converted to source that is run.
   *
   * The generated extension is stored in the system temporary directory,
   * and an nsIFile object pointing to it is returned.
   *
   * @param {object} data
   * @returns {nsIFile}
   */
  static generateXPI(data) {
    let manifest = data.manifest;
    if (!manifest) {
      manifest = {};
    }

    let files = Object.assign({}, data.files);

    function provide(obj, keys, value, override = false) {
      if (keys.length == 1) {
        if (!(keys[0] in obj) || override) {
          obj[keys[0]] = value;
        }
      } else {
        if (!(keys[0] in obj)) {
          obj[keys[0]] = {};
        }
        provide(obj[keys[0]], keys.slice(1), value, override);
      }
    }

    provide(manifest, ["name"], "Generated extension");
    provide(manifest, ["manifest_version"], 2);
    provide(manifest, ["version"], "1.0");

    if (data.background) {
      let bgScript = uuidGen.generateUUID().number + ".js";

      provide(manifest, ["background", "scripts"], [bgScript], true);
      files[bgScript] = data.background;
    }

    provide(files, ["manifest.json"], manifest);

    if (data.embedded) {
      // Package this as a webextension embedded inside a legacy
      // extension.

      let xpiFiles = {
        "install.rdf": `<?xml version="1.0" encoding="UTF-8"?>
          <RDF xmlns="http://www.w3.org/1999/02/22-rdf-syntax-ns#"
               xmlns:em="http://www.mozilla.org/2004/em-rdf#">
              <Description about="urn:mozilla:install-manifest"
                  em:id="${manifest.applications.gecko.id}"
                  em:name="${manifest.name}"
                  em:type="2"
                  em:version="${manifest.version}"
                  em:description=""
                  em:multiprocessCompatible="true"
                  em:hasEmbeddedWebExtension="true"
                  em:bootstrap="true">

                  <!-- Firefox -->
                  <em:targetApplication>
                      <Description
                          em:id="{ec8030f7-c20a-464f-9b0e-13a3a9e97384}"
                          em:minVersion="51.0a1"
                          em:maxVersion="*"/>
                  </em:targetApplication>
                  <em:targetApplication>
                    <Description>
                      <em:id>toolkit@mozilla.org</em:id>
                      <em:minVersion>0</em:minVersion>
                      <em:maxVersion>*</em:maxVersion>
                    </Description>
                  </em:targetApplication>
              </Description>
          </RDF>
        `,

        "bootstrap.js": `
          function install() {}
          function uninstall() {}
          function shutdown() {}

          function startup(data) {
            data.webExtension.startup();
          }
        `,
      };

      for (let [path, data] of Object.entries(files)) {
        xpiFiles[`webextension/${path}`] = data;
      }

      files = xpiFiles;
    }

    return this.generateZipFile(files);
  }

  static generateZipFile(files, baseName = "generated-extension.xpi") {
    let ZipWriter = Components.Constructor("@mozilla.org/zipwriter;1", "nsIZipWriter");
    let zipW = new ZipWriter();

    let file = FileUtils.getFile("TmpD", [baseName]);
    file.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, FileUtils.PERMS_FILE);

    const MODE_WRONLY = 0x02;
    const MODE_TRUNCATE = 0x20;
    zipW.open(file, MODE_WRONLY | MODE_TRUNCATE);

    // Needs to be in microseconds for some reason.
    let time = Date.now() * 1000;

    function generateFile(filename) {
      let components = filename.split("/");
      let path = "";
      for (let component of components.slice(0, -1)) {
        path += component + "/";
        if (!zipW.hasEntry(path)) {
          zipW.addEntryDirectory(path, time, false);
        }
      }
    }

    for (let filename in files) {
      let script = files[filename];
      if (typeof(script) == "function") {
        script = this.serializeScript(script);
      } else if (instanceOf(script, "Object") || instanceOf(script, "Array")) {
        script = JSON.stringify(script);
      }

      if (!instanceOf(script, "ArrayBuffer")) {
        script = new TextEncoder("utf-8").encode(script).buffer;
      }

      let stream = Cc["@mozilla.org/io/arraybuffer-input-stream;1"].createInstance(Ci.nsIArrayBufferInputStream);
      stream.setData(script, 0, script.byteLength);

      generateFile(filename);
      zipW.addEntryStream(filename, time, 0, stream, false);
    }

    zipW.close();

    return file;
  }

  /**
   * Properly serialize a function into eval-able code string.
   *
   * @param {function} script
   * @returns {string}
   */
  static serializeFunction(script) {
    // Serialization of object methods doesn't include `function` anymore.
    const method = /^(async )?(\w+)\(/;

    let code = script.toString();
    let match = code.match(method);
    if (match && match[2] !== "function") {
      code = code.replace(method, "$1function $2(");
    }
    return code;
  }

  /**
   * Properly serialize a script into eval-able code string.
   *
   * @param {string|function|Array} script
   * @returns {string}
   */
  static serializeScript(script) {
    if (Array.isArray(script)) {
      return Array.from(script, this.serializeScript, this).join(";");
    }
    if (typeof script !== "function") {
      return script;
    }
    return `(${this.serializeFunction(script)})();`;
  }

  /**
   * Generates a new extension using |Extension.generateXPI|, and initializes a
   * new |Extension| instance which will execute it.
   *
   * @param {object} data
   * @returns {Extension}
   */
  static generate(data) {
    let file = this.generateXPI(data);

    flushJarCache(file.path);
    Services.ppmm.broadcastAsyncMessage("Extension:FlushJarCache", {path: file.path});

    let fileURI = Services.io.newFileURI(file);
    let jarURI = Services.io.newURI("jar:" + fileURI.spec + "!/");

    // This may be "temporary" or "permanent".
    if (data.useAddonManager) {
      return new MockExtension(file, jarURI, data.useAddonManager);
    }

    let id;
    if (data.manifest) {
      if (data.manifest.applications && data.manifest.applications.gecko) {
        id = data.manifest.applications.gecko.id;
      } else if (data.manifest.browser_specific_settings && data.manifest.browser_specific_settings.gecko) {
        id = data.manifest.browser_specific_settings.gecko.id;
      }
    }
    if (!id) {
      id = uuidGen.generateUUID().number;
    }

    return new Extension({
      id,
      resourceURI: jarURI,
      cleanupFile: file,
      signedState: data.isPrivileged ? AddonManager.SIGNEDSTATE_PRIVILEGED
                                     : AddonManager.SIGNEDSTATE_SIGNED,
      temporarilyInstalled: !!data.temporarilyInstalled,
    });
  }
};
