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

var EXPORTED_SYMBOLS = ["ExtensionTestCommon", "MockExtension"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyGlobalGetters(this, ["TextEncoder"]);

ChromeUtils.defineModuleGetter(
  this,
  "AddonManager",
  "resource://gre/modules/AddonManager.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "Extension",
  "resource://gre/modules/Extension.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "ExtensionParent",
  "resource://gre/modules/ExtensionParent.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "ExtensionPermissions",
  "resource://gre/modules/ExtensionPermissions.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "FileUtils",
  "resource://gre/modules/FileUtils.jsm"
);
ChromeUtils.defineModuleGetter(this, "OS", "resource://gre/modules/osfile.jsm");

XPCOMUtils.defineLazyGetter(
  this,
  "apiManager",
  () => ExtensionParent.apiManager
);

const { ExtensionCommon } = ChromeUtils.import(
  "resource://gre/modules/ExtensionCommon.jsm"
);
const { ExtensionUtils } = ChromeUtils.import(
  "resource://gre/modules/ExtensionUtils.jsm"
);

XPCOMUtils.defineLazyServiceGetter(
  this,
  "uuidGen",
  "@mozilla.org/uuid-generator;1",
  "nsIUUIDGenerator"
);

const { flushJarCache } = ExtensionUtils;

const { instanceOf } = ExtensionCommon;

XPCOMUtils.defineLazyGetter(this, "console", () =>
  ExtensionCommon.getConsole()
);

var ExtensionTestCommon;

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
  constructor(file, rootURI, addonData) {
    this.id = null;
    this.file = file;
    this.rootURI = rootURI;
    this.installType = addonData.useAddonManager;
    this.addonData = addonData;
    this.addon = null;

    let promiseEvent = eventName =>
      new Promise(resolve => {
        let onstartup = async (msg, extension) => {
          this.maybeSetID(extension.rootURI, extension.id);
          if (!this.id && this.addonPromise) {
            await this.addonPromise;
          }

          if (extension.id == this.id) {
            apiManager.off(eventName, onstartup);
            this._extension = extension;
            resolve(extension);
          }
        };
        apiManager.on(eventName, onstartup);
      });

    this._extension = null;
    this._extensionPromise = promiseEvent("startup");
    this._readyPromise = promiseEvent("ready");
    this._uninstallPromise = promiseEvent("uninstall-complete");
  }

  maybeSetID(uri, id) {
    if (
      !this.id &&
      uri instanceof Ci.nsIJARURI &&
      uri.JARFile.QueryInterface(Ci.nsIFileURL).file.equals(this.file)
    ) {
      this.id = id;
    }
  }

  testMessage(...args) {
    return this._extension.testMessage(...args);
  }

  on(...args) {
    this._extensionPromise.then(extension => {
      extension.on(...args);
    });
    // Extension.jsm emits a "startup" event on |extension| before emitting the
    // "startup" event on |apiManager|. Trigger the "startup" event anyway, to
    // make sure that the MockExtension behaves like an Extension with regards
    // to the startup event.
    if (args[0] === "startup" && !this._extension) {
      this._extensionPromise.then(extension => {
        args[1]("startup", extension);
      });
    }
  }

  off(...args) {
    this._extensionPromise.then(extension => {
      extension.off(...args);
    });
  }

  _setIncognitoOverride() {
    let { addonData } = this;
    if (addonData && addonData.incognitoOverride) {
      try {
        let { id } = addonData.manifest.applications.gecko;
        if (id) {
          return ExtensionTestCommon.setIncognitoOverride({ id, addonData });
        }
      } catch (e) {}
      throw new Error(
        "Extension ID is required for setting incognito permission."
      );
    }
  }

  async startup() {
    await this._setIncognitoOverride();

    if (this.installType == "temporary") {
      return AddonManager.installTemporaryAddon(this.file).then(async addon => {
        this.addon = addon;
        this.id = addon.id;
        return this._readyPromise;
      });
    } else if (this.installType == "permanent") {
      this.addonPromise = new Promise(resolve => {
        this.resolveAddon = resolve;
      });
      let install = await AddonManager.getInstallForFile(this.file);
      return new Promise((resolve, reject) => {
        let listener = {
          onInstallFailed: reject,
          onInstallEnded: async (install, newAddon) => {
            this.addon = newAddon;
            this.id = newAddon.id;
            this.resolveAddon(newAddon);
            resolve(this._readyPromise);
          },
        };

        install.addListener(listener);
        install.install();
      });
    }
    throw new Error("installType must be one of: temporary, permanent");
  }

  shutdown() {
    this.addon.uninstall();
    return this.cleanupGeneratedFile();
  }

  cleanupGeneratedFile() {
    return this._extensionPromise
      .then(extension => {
        return extension.broadcast("Extension:FlushJarCache", {
          path: this.file.path,
        });
      })
      .then(() => {
        return OS.File.remove(this.file.path);
      });
  }
}

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

ExtensionTestCommon = class ExtensionTestCommon {
  /**
   * This code is designed to make it easy to test a WebExtension
   * without creating a bunch of files. Everything is contained in a
   * single JS object.
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
   * @param {object} data
   * @returns {object}
   */
  static generateFiles(data) {
    let files = {};

    Object.assign(files, data.files);

    let manifest = data.manifest;
    if (!manifest) {
      manifest = {};
    }

    provide(manifest, ["name"], "Generated extension");
    provide(manifest, ["manifest_version"], 2);
    provide(manifest, ["version"], "1.0");

    if (data.background) {
      let bgScript = uuidGen.generateUUID().number + ".js";

      provide(manifest, ["background", "scripts"], [bgScript], true);
      files[bgScript] = data.background;
    }

    provide(files, ["manifest.json"], JSON.stringify(manifest));

    for (let filename in files) {
      let contents = files[filename];
      if (typeof contents == "function") {
        files[filename] = this.serializeScript(contents);
      } else if (
        typeof contents != "string" &&
        !instanceOf(contents, "ArrayBuffer")
      ) {
        files[filename] = JSON.stringify(contents);
      }
    }

    return files;
  }

  /**
   * Write an xpi file to disk for a webextension.
   * The generated extension is stored in the system temporary directory,
   * and an nsIFile object pointing to it is returned.
   *
   * @param {object} data In the format handled by generateFiles.
   * @returns {nsIFile}
   */
  static generateXPI(data) {
    let files = this.generateFiles(data);
    return this.generateZipFile(files);
  }

  static generateZipFile(files, baseName = "generated-extension.xpi") {
    let ZipWriter = Components.Constructor(
      "@mozilla.org/zipwriter;1",
      "nsIZipWriter"
    );
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
      if (!instanceOf(script, "ArrayBuffer")) {
        script = new TextEncoder("utf-8").encode(script).buffer;
      }

      let stream = Cc[
        "@mozilla.org/io/arraybuffer-input-stream;1"
      ].createInstance(Ci.nsIArrayBufferInputStream);
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
    const method = /^(async )?(?:(\w+)|"(\w+)\.js")\(/;

    let code = script.toString();
    let match = code.match(method);
    if (match && match[2] !== "function") {
      code = code.replace(method, "$1function $2$3(");
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

  static setIncognitoOverride(extension) {
    let { id, addonData } = extension;
    if (!addonData || !addonData.incognitoOverride) {
      return;
    }
    if (addonData.incognitoOverride == "not_allowed") {
      return ExtensionPermissions.remove(id, {
        permissions: ["internal:privateBrowsingAllowed"],
        origins: [],
      });
    }
    return ExtensionPermissions.add(id, {
      permissions: ["internal:privateBrowsingAllowed"],
      origins: [],
    });
  }

  static setExtensionID(data) {
    try {
      if (data.manifest.applications.gecko.id) {
        return;
      }
    } catch (e) {
      // No ID is set.
    }
    provide(
      data,
      ["manifest", "applications", "gecko", "id"],
      uuidGen.generateUUID().number
    );
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
    Services.ppmm.broadcastAsyncMessage("Extension:FlushJarCache", {
      path: file.path,
    });

    let fileURI = Services.io.newFileURI(file);
    let jarURI = Services.io.newURI("jar:" + fileURI.spec + "!/");

    // This may be "temporary" or "permanent".
    if (data.useAddonManager) {
      return new MockExtension(file, jarURI, data);
    }

    let id;
    if (data.manifest) {
      if (data.manifest.applications && data.manifest.applications.gecko) {
        id = data.manifest.applications.gecko.id;
      } else if (
        data.manifest.browser_specific_settings &&
        data.manifest.browser_specific_settings.gecko
      ) {
        id = data.manifest.browser_specific_settings.gecko.id;
      }
    }
    if (!id) {
      id = uuidGen.generateUUID().number;
    }

    let signedState = AddonManager.SIGNEDSTATE_SIGNED;
    if (data.isPrivileged) {
      signedState = AddonManager.SIGNEDSTATE_PRIVILEGED;
    }
    if (data.isSystem) {
      signedState = AddonManager.SIGNEDSTATE_SYSTEM;
    }

    return new Extension({
      id,
      resourceURI: jarURI,
      cleanupFile: file,
      signedState,
      incognitoOverride: data.incognitoOverride,
      temporarilyInstalled: !!data.temporarilyInstalled,
      TEST_NO_ADDON_MANAGER: true,
    });
  }
};
