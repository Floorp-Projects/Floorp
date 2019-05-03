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

const CHILD_SCRIPT_API = "resource://specialpowers/specialpowersFrameScript.js";
const CHILD_LOGGER_SCRIPT = "resource://specialpowers/MozillaLogger.js";


// Glue to add in the observer API to this object.  This allows us to share code with chrome tests
Services.scriptloader.loadSubScript("resource://specialpowers/SpecialPowersObserverAPI.js");

/* XPCOM gunk */
function SpecialPowersObserver() {
  this._isFrameScriptLoaded = false;
  this._messageManager = Services.mm;
  this._serviceWorkerListener = null;
}


SpecialPowersObserver.prototype = new SpecialPowersObserverAPI();

SpecialPowersObserver.prototype.QueryInterface = ChromeUtils.generateQI([Ci.nsIObserver]);

SpecialPowersObserver.prototype.observe = function(aSubject, aTopic, aData) {
  switch (aTopic) {
    case "chrome-document-global-created":
      this._loadFrameScript();
      break;

    case "http-on-modify-request":
      if (aSubject instanceof Ci.nsIChannel) {
        let uri = aSubject.URI.spec;
        this._sendAsyncMessage("specialpowers-http-notify-request", { uri });
      }
      break;

    default:
      this._observe(aSubject, aTopic, aData);
      break;
  }
};

SpecialPowersObserver.prototype._loadFrameScript = function() {
  if (!this._isFrameScriptLoaded) {
    // Register for any messages our API needs us to handle
    this._messageManager.addMessageListener("SPPrefService", this);
    this._messageManager.addMessageListener("SPProcessCrashManagerWait", this);
    this._messageManager.addMessageListener("SPProcessCrashService", this);
    this._messageManager.addMessageListener("SPPingService", this);
    this._messageManager.addMessageListener("SpecialPowers.Quit", this);
    this._messageManager.addMessageListener("SpecialPowers.Focus", this);
    this._messageManager.addMessageListener("SpecialPowers.CreateFiles", this);
    this._messageManager.addMessageListener("SpecialPowers.RemoveFiles", this);
    this._messageManager.addMessageListener("SPPermissionManager", this);
    this._messageManager.addMessageListener("SPObserverService", this);
    this._messageManager.addMessageListener("SPLoadChromeScript", this);
    this._messageManager.addMessageListener("SPImportInMainProcess", this);
    this._messageManager.addMessageListener("SPChromeScriptMessage", this);
    this._messageManager.addMessageListener("SPQuotaManager", this);
    this._messageManager.addMessageListener("SPSetTestPluginEnabledState", this);
    this._messageManager.addMessageListener("SPLoadExtension", this);
    this._messageManager.addMessageListener("SPStartupExtension", this);
    this._messageManager.addMessageListener("SPUnloadExtension", this);
    this._messageManager.addMessageListener("SPExtensionMessage", this);
    this._messageManager.addMessageListener("SPCleanUpSTSData", this);
    this._messageManager.addMessageListener("SPRequestDumpCoverageCounters", this);
    this._messageManager.addMessageListener("SPRequestResetCoverageCounters", this);
    this._messageManager.addMessageListener("SPCheckServiceWorkers", this);
    this._messageManager.addMessageListener("SPRemoveAllServiceWorkers", this);
    this._messageManager.addMessageListener("SPRemoveServiceWorkerDataForExampleDomain", this);

    this._messageManager.loadFrameScript(CHILD_LOGGER_SCRIPT, true);
    this._messageManager.loadFrameScript(CHILD_SCRIPT_API, true);
    this._isFrameScriptLoaded = true;
    this._createdFiles = null;
  }
};

SpecialPowersObserver.prototype._sendAsyncMessage = function(msgname, msg) {
  this._messageManager.broadcastAsyncMessage(msgname, msg);
};

SpecialPowersObserver.prototype._receiveMessage = function(aMessage) {
  return this._receiveMessageAPI(aMessage);
};

SpecialPowersObserver.prototype.init = function() {
  var obs = Services.obs;
  obs.addObserver(this, "chrome-document-global-created");

  // Register special testing modules.
  var testsURI = Services.dirsvc.get("ProfD", Ci.nsIFile);
  testsURI.append("tests.manifest");
  var manifestFile = Services.io.newFileURI(testsURI).
                       QueryInterface(Ci.nsIFileURL).file;

  Components.manager.QueryInterface(Ci.nsIComponentRegistrar).
                 autoRegister(manifestFile);

  obs.addObserver(this, "http-on-modify-request");

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
};

SpecialPowersObserver.prototype.uninit = function() {
  var obs = Services.obs;
  obs.removeObserver(this, "chrome-document-global-created");
  obs.removeObserver(this, "http-on-modify-request");
  this._registerObservers._topics.forEach((element) => {
    obs.removeObserver(this._registerObservers, element);
  });
  this._removeProcessCrashObservers();

  let swm = Cc["@mozilla.org/serviceworkers/manager;1"]
              .getService(Ci.nsIServiceWorkerManager);
  swm.removeListener(this._serviceWorkerListener);

  if (this._isFrameScriptLoaded) {
    this._messageManager.removeMessageListener("SPPrefService", this);
    this._messageManager.removeMessageListener("SPProcessCrashManagerWait", this);
    this._messageManager.removeMessageListener("SPProcessCrashService", this);
    this._messageManager.removeMessageListener("SPPingService", this);
    this._messageManager.removeMessageListener("SpecialPowers.Quit", this);
    this._messageManager.removeMessageListener("SpecialPowers.Focus", this);
    this._messageManager.removeMessageListener("SpecialPowers.CreateFiles", this);
    this._messageManager.removeMessageListener("SpecialPowers.RemoveFiles", this);
    this._messageManager.removeMessageListener("SPPermissionManager", this);
    this._messageManager.removeMessageListener("SPObserverService", this);
    this._messageManager.removeMessageListener("SPLoadChromeScript", this);
    this._messageManager.removeMessageListener("SPImportInMainProcess", this);
    this._messageManager.removeMessageListener("SPChromeScriptMessage", this);
    this._messageManager.removeMessageListener("SPQuotaManager", this);
    this._messageManager.removeMessageListener("SPSetTestPluginEnabledState", this);
    this._messageManager.removeMessageListener("SPLoadExtension", this);
    this._messageManager.removeMessageListener("SPStartupExtension", this);
    this._messageManager.removeMessageListener("SPUnloadExtension", this);
    this._messageManager.removeMessageListener("SPExtensionMessage", this);
    this._messageManager.removeMessageListener("SPCleanUpSTSData", this);
    this._messageManager.removeMessageListener("SPRequestDumpCoverageCounters", this);
    this._messageManager.removeMessageListener("SPRequestResetCoverageCounters", this);
    this._messageManager.removeMessageListener("SPCheckServiceWorkers", this);
    this._messageManager.removeMessageListener("SPRemoveAllServiceWorkers", this);
    this._messageManager.removeMessageListener("SPRemoveServiceWorkerDataForExampleDomain", this);

    this._messageManager.removeDelayedFrameScript(CHILD_LOGGER_SCRIPT);
    this._messageManager.removeDelayedFrameScript(CHILD_SCRIPT_API);
    this._isFrameScriptLoaded = false;
  }
};

SpecialPowersObserver.prototype._addProcessCrashObservers = function() {
  if (this._processCrashObserversRegistered) {
    return;
  }

  Services.obs.addObserver(this, "plugin-crashed");
  Services.obs.addObserver(this, "ipc:content-shutdown");
  this._processCrashObserversRegistered = true;
};

SpecialPowersObserver.prototype._removeProcessCrashObservers = function() {
  if (!this._processCrashObserversRegistered) {
    return;
  }

  Services.obs.removeObserver(this, "plugin-crashed");
  Services.obs.removeObserver(this, "ipc:content-shutdown");
  this._processCrashObserversRegistered = false;
};

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

/**
 * messageManager callback function
 * This will get requests from our API in the window and process them in chrome for it
 **/
SpecialPowersObserver.prototype.receiveMessage = function(aMessage) {
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
};

SpecialPowersObserver.prototype.onRegister = function() {
  this._sendAsyncMessage("SPServiceWorkerRegistered",
    { registered: true });
};
