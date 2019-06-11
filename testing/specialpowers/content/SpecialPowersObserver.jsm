/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// Based on:
// https://bugzilla.mozilla.org/show_bug.cgi?id=549539
// https://bug549539.bugzilla.mozilla.org/attachment.cgi?id=429661
// https://developer.mozilla.org/en/XPCOM/XPCOM_changes_in_Gecko_1.9.3
// https://developer.mozilla.org/en/how_to_build_an_xpcom_component_in_javascript

/* import-globals-from SpecialPowersObserverAPI.js */

var EXPORTED_SYMBOLS = ["SpecialPowersObserver"];

var {Services} = ChromeUtils.import("resource://gre/modules/Services.jsm");

const MESSAGE_NAMES = [
  "SPCheckServiceWorkers",
  "SPChromeScriptMessage",
  "SPCleanUpSTSData",
  "SPExtensionMessage",
  "SPImportInMainProcess",
  "SPLoadChromeScript",
  "SPLoadExtension",
  "SPObserverService",
  "SPPermissionManager",
  "SPPingService",
  "SPPrefService",
  "SPProcessCrashManagerWait",
  "SPProcessCrashService",
  "SPQuotaManager",
  "SPRemoveAllServiceWorkers",
  "SPRemoveServiceWorkerDataForExampleDomain",
  "SPRequestDumpCoverageCounters",
  "SPRequestResetCoverageCounters",
  "SPSetTestPluginEnabledState",
  "SPStartupExtension",
  "SPUnloadExtension",
  "SpecialPowers.CreateFiles",
  "SpecialPowers.Focus",
  "SpecialPowers.Quit",
  "SpecialPowers.RemoveFiles",
];

const FRAME_SCRIPTS = [
  "resource://specialpowers/specialpowersFrameScript.js",
];


// Glue to add in the observer API to this object.  This allows us to share code with chrome tests
Services.scriptloader.loadSubScript("resource://specialpowers/SpecialPowersObserverAPI.js");

class SpecialPowersObserver extends SpecialPowersObserverAPI {
  constructor() {
    super();
    this._initialized = false;
    this._messageManager = Services.mm;
    this._serviceWorkerListener = null;
  }

  observe(aSubject, aTopic, aData) {
    if (aTopic == "http-on-modify-request") {
      if (aSubject instanceof Ci.nsIChannel) {
        let uri = aSubject.URI.spec;
        this._sendAsyncMessage("specialpowers-http-notify-request", { uri });
      }
    } else {
      this._observe(aSubject, aTopic, aData);
    }
  }

  _loadFrameScript() {
    // Register for any messages our API needs us to handle
    for (let name of MESSAGE_NAMES) {
      this._messageManager.addMessageListener(name, this);
    }

    for (let script of FRAME_SCRIPTS) {
      this._messageManager.loadFrameScript(script, true);
    }
    this._createdFiles = null;
  }

  _unloadFrameScript() {
    for (let name of MESSAGE_NAMES) {
      this._messageManager.removeMessageListener(name, this);
    }

    for (let script of FRAME_SCRIPTS) {
      this._messageManager.removeDelayedFrameScript(script);
    }
  }

  _sendAsyncMessage(msgname, msg) {
    this._messageManager.broadcastAsyncMessage(msgname, msg);
  }

  _receiveMessage(aMessage) {
    return this._receiveMessageAPI(aMessage);
  }

  init() {
    if (this._initialized) {
      throw new Error("Already initialized");
    }

    // Register special testing modules.
    var testsURI = Services.dirsvc.get("ProfD", Ci.nsIFile);
    testsURI.append("tests.manifest");
    var manifestFile = Services.io.newFileURI(testsURI).
                         QueryInterface(Ci.nsIFileURL).file;

    Components.manager.QueryInterface(Ci.nsIComponentRegistrar).
                   autoRegister(manifestFile);

    Services.obs.addObserver(this, "http-on-modify-request");

    // We would like to check that tests don't leave service workers around
    // after they finish, but that information lives in the parent process.
    // Ideally, we'd be able to tell the child processes whenever service
    // workers are registered or unregistered so they would know at all times,
    // but service worker lifetimes are complicated enough to make that
    // difficult. For the time being, let the child process know when a test
    // registers a service worker so it can ask, synchronously, at the end if
    // the service worker had unregister called on it.
    let swm = Cc["@mozilla.org/serviceworkers/manager;1"]
                .getService(Ci.nsIServiceWorkerManager);
    let self = this;
    this._serviceWorkerListener = {
      onRegister() {
        self.onRegister();
      },

      onUnregister() {
        // no-op
      },
    };
    swm.addListener(this._serviceWorkerListener);

    this._loadFrameScript();

    this._initialized = true;
  }

  uninit() {
    if (!this._initialized) {
      throw new Error("Not initialized");
    }
    this._initialized = false;

    var obs = Services.obs;
    obs.removeObserver(this, "http-on-modify-request");
    this._registerObservers._topics.forEach((element) => {
      obs.removeObserver(this._registerObservers, element);
    });
    this._removeProcessCrashObservers();

    let swm = Cc["@mozilla.org/serviceworkers/manager;1"]
                .getService(Ci.nsIServiceWorkerManager);
    swm.removeListener(this._serviceWorkerListener);

    this._unloadFrameScript();
  }

  _addProcessCrashObservers() {
    if (this._processCrashObserversRegistered) {
      return;
    }

    Services.obs.addObserver(this, "plugin-crashed");
    Services.obs.addObserver(this, "ipc:content-shutdown");
    this._processCrashObserversRegistered = true;
  }

  _removeProcessCrashObservers() {
    if (!this._processCrashObserversRegistered) {
      return;
    }

    Services.obs.removeObserver(this, "plugin-crashed");
    Services.obs.removeObserver(this, "ipc:content-shutdown");
    this._processCrashObserversRegistered = false;
  }

  /**
   * messageManager callback function
   * This will get requests from our API in the window and process them in chrome for it
   **/
  receiveMessage(aMessage) {
    switch (aMessage.name) {
      case "SPPingService":
        if (aMessage.json.op == "ping") {
          aMessage.target.frameLoader
                  .messageManager
                  .sendAsyncMessage("SPPingService", { op: "pong" });
        }
        break;
      case "SpecialPowers.Quit":
        Services.startup.quit(Ci.nsIAppStartup.eForceQuit);
        break;
      case "SpecialPowers.Focus":
        aMessage.target.focus();
        break;
      case "SpecialPowers.CreateFiles":
        let filePaths = [];
        if (!this._createdFiles) {
          this._createdFiles = [];
        }
        let createdFiles = this._createdFiles;
        try {
          let promises = [];
          aMessage.data.forEach(function(request) {
            const filePerms = 0o666;
            let testFile = Services.dirsvc.get("ProfD", Ci.nsIFile);
            if (request.name) {
              testFile.appendRelativePath(request.name);
            } else {
              testFile.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, filePerms);
            }
            let outStream = Cc["@mozilla.org/network/file-output-stream;1"].createInstance(Ci.nsIFileOutputStream);
            outStream.init(testFile, 0x02 | 0x08 | 0x20, // PR_WRONLY | PR_CREATE_FILE | PR_TRUNCATE
                           filePerms, 0);
            if (request.data) {
              outStream.write(request.data, request.data.length);
            }
            outStream.close();
            promises.push(File.createFromFileName(testFile.path, request.options).then(function(file) {
              filePaths.push(file);
            }));
            createdFiles.push(testFile);
          });

          Promise.all(promises).then(function() {
            aMessage.target.frameLoader
                    .messageManager
                    .sendAsyncMessage("SpecialPowers.FilesCreated", filePaths);
          }, function(e) {
            aMessage.target.frameLoader
                    .messageManager
                    .sendAsyncMessage("SpecialPowers.FilesError", e.toString());
          });
        } catch (e) {
            aMessage.target.frameLoader
                    .messageManager
                    .sendAsyncMessage("SpecialPowers.FilesError", e.toString());
        }

        break;
      case "SpecialPowers.RemoveFiles":
        if (this._createdFiles) {
          this._createdFiles.forEach(function(testFile) {
            try {
              testFile.remove(false);
            } catch (e) {}
          });
          this._createdFiles = null;
        }
        break;
      default:
        return this._receiveMessage(aMessage);
    }
    return undefined;
  }

  onRegister() {
    this._sendAsyncMessage("SPServiceWorkerRegistered",
      { registered: true });
  }
}

SpecialPowersObserver.prototype._registerObservers = {
  _self: null,
  _topics: [],
  _add(topic) {
    if (!this._topics.includes(topic)) {
      this._topics.push(topic);
      Services.obs.addObserver(this, topic);
    }
  },
  observe(aSubject, aTopic, aData) {
    var msg = { aData };
    switch (aTopic) {
      case "perm-changed":
        var permission = aSubject.QueryInterface(Ci.nsIPermission);

        // specialPowersAPI will consume this value, and it is used as a
        // fake permission, but only type will be used.
        //
        // We need to ensure that it looks the same as a real permission,
        // so we fake these properties.
        msg.permission = {
          principal: {
            originAttributes: {},
          },
          type: permission.type,
        };
      default:
        this._self._sendAsyncMessage("specialpowers-" + aTopic, msg);
    }
  },
};
