/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* This code is loaded in every child process that is started by mochitest in
 * order to be used as a replacement for UniversalXPConnect
 */

/* globals bindDOMWindowUtils, SpecialPowersAPI */

ChromeUtils.import("resource://gre/modules/Services.jsm");

Services.scriptloader.loadSubScript("resource://specialpowers/MozillaLogger.js", this);

var EXPORTED_SYMBOLS = ["SpecialPowers", "attachSpecialPowersToWindow"];

ChromeUtils.import("resource://specialpowers/specialpowersAPI.js", this);

Cu.forcePermissiveCOWs();

function SpecialPowers(window, mm) {
  this.mm = mm;

  this.window = Cu.getWeakReference(window);
  this._windowID = window.windowUtils.currentInnerWindowID;
  this._encounteredCrashDumpFiles = [];
  this._unexpectedCrashDumpFiles = { };
  this._crashDumpDir = null;
  this.DOMWindowUtils = bindDOMWindowUtils(window);
  Object.defineProperty(this, "Components", {
      configurable: true, enumerable: true, value: this.getFullComponents(),
  });
  this._pongHandlers = [];
  this._messageListener = this._messageReceived.bind(this);
  this._grandChildFrameMM = null;
  this._createFilesOnError = null;
  this._createFilesOnSuccess = null;
  this.SP_SYNC_MESSAGES = ["SPChromeScriptMessage",
                           "SPLoadChromeScript",
                           "SPImportInMainProcess",
                           "SPObserverService",
                           "SPPermissionManager",
                           "SPPrefService",
                           "SPProcessCrashService",
                           "SPSetTestPluginEnabledState",
                           "SPCleanUpSTSData"];

  this.SP_ASYNC_MESSAGES = ["SpecialPowers.Focus",
                            "SpecialPowers.Quit",
                            "SpecialPowers.CreateFiles",
                            "SpecialPowers.RemoveFiles",
                            "SPPingService",
                            "SPLoadExtension",
                            "SPProcessCrashManagerWait",
                            "SPStartupExtension",
                            "SPUnloadExtension",
                            "SPExtensionMessage",
                            "SPRequestDumpCoverageCounters",
                            "SPRequestResetCoverageCounters"];
  mm.addMessageListener("SPPingService", this._messageListener);
  mm.addMessageListener("SpecialPowers.FilesCreated", this._messageListener);
  mm.addMessageListener("SpecialPowers.FilesError", this._messageListener);
  let self = this;
  Services.obs.addObserver(function onInnerWindowDestroyed(subject, topic, data) {
    var id = subject.QueryInterface(Ci.nsISupportsPRUint64).data;
    if (self._windowID === id) {
      Services.obs.removeObserver(onInnerWindowDestroyed, "inner-window-destroyed");
      try {
        mm.removeMessageListener("SPPingService", self._messageListener);
        mm.removeMessageListener("SpecialPowers.FilesCreated", self._messageListener);
        mm.removeMessageListener("SpecialPowers.FilesError", self._messageListener);
      } catch (e) {
        // Ignore the exception which the message manager has been destroyed.
        if (e.result != Cr.NS_ERROR_ILLEGAL_VALUE) {
          throw e;
        }
      }
    }
  }, "inner-window-destroyed");
}

SpecialPowers.prototype = new SpecialPowersAPI();

SpecialPowers.prototype.toString = function() { return "[SpecialPowers]"; };
SpecialPowers.prototype.sanityCheck = function() { return "foo"; };

// This gets filled in in the constructor.
SpecialPowers.prototype.DOMWindowUtils = undefined;
SpecialPowers.prototype.Components = undefined;
SpecialPowers.prototype.IsInNestedFrame = false;

SpecialPowers.prototype._sendSyncMessage = function(msgname, msg) {
  if (!this.SP_SYNC_MESSAGES.includes(msgname)) {
    dump("TEST-INFO | specialpowers.js |  Unexpected SP message: " + msgname + "\n");
  }
  let result = this.mm.sendSyncMessage(msgname, msg);
  return Cu.cloneInto(result, this);
};

SpecialPowers.prototype._sendAsyncMessage = function(msgname, msg) {
  if (!this.SP_ASYNC_MESSAGES.includes(msgname)) {
    dump("TEST-INFO | specialpowers.js |  Unexpected SP message: " + msgname + "\n");
  }
  this.mm.sendAsyncMessage(msgname, msg);
};

SpecialPowers.prototype._addMessageListener = function(msgname, listener) {
  this.mm.addMessageListener(msgname, listener);
  this.mm.sendAsyncMessage("SPPAddNestedMessageListener", { name: msgname });
};

SpecialPowers.prototype._removeMessageListener = function(msgname, listener) {
  this.mm.removeMessageListener(msgname, listener);
};

SpecialPowers.prototype.registerProcessCrashObservers = function() {
  this.mm.addMessageListener("SPProcessCrashService", this._messageListener);
  this.mm.sendSyncMessage("SPProcessCrashService", { op: "register-observer" });
};

SpecialPowers.prototype.unregisterProcessCrashObservers = function() {
  this.mm.removeMessageListener("SPProcessCrashService", this._messageListener);
  this.mm.sendSyncMessage("SPProcessCrashService", { op: "unregister-observer" });
};

SpecialPowers.prototype._messageReceived = function(aMessage) {
  switch (aMessage.name) {
    case "SPProcessCrashService":
      if (aMessage.json.type == "crash-observed") {
        for (let e of aMessage.json.dumpIDs) {
          this._encounteredCrashDumpFiles.push(e.id + "." + e.extension);
        }
      }
      break;

    case "SPPingService":
      if (aMessage.json.op == "pong") {
        var handler = this._pongHandlers.shift();
        if (handler) {
          handler();
        }
        if (this._grandChildFrameMM) {
          this._grandChildFrameMM.sendAsyncMessage("SPPingService", { op: "pong" });
        }
      }
      break;

    case "SpecialPowers.FilesCreated":
      var createdHandler = this._createFilesOnSuccess;
      this._createFilesOnSuccess = null;
      this._createFilesOnError = null;
      if (createdHandler) {
        createdHandler(Cu.cloneInto(aMessage.data, this.mm.content));
      }
      break;

    case "SpecialPowers.FilesError":
      var errorHandler = this._createFilesOnError;
      this._createFilesOnSuccess = null;
      this._createFilesOnError = null;
      if (errorHandler) {
        errorHandler(aMessage.data);
      }
      break;
  }

  return true;
};

SpecialPowers.prototype.quit = function() {
  this.mm.sendAsyncMessage("SpecialPowers.Quit", {});
};

// fileRequests is an array of file requests. Each file request is an object.
// A request must have a field |name|, which gives the base of the name of the
// file to be created in the profile directory. If the request has a |data| field
// then that data will be written to the file.
SpecialPowers.prototype.createFiles = function(fileRequests, onCreation, onError) {
  if (this._createFilesOnSuccess || this._createFilesOnError) {
    onError("Already waiting for SpecialPowers.createFiles() to finish.");
    return;
  }

  this._createFilesOnSuccess = onCreation;
  this._createFilesOnError = onError;
  this.mm.sendAsyncMessage("SpecialPowers.CreateFiles", fileRequests);
};

// Remove the files that were created using |SpecialPowers.createFiles()|.
// This will be automatically called by |SimpleTest.finish()|.
SpecialPowers.prototype.removeFiles = function() {
  this.mm.sendAsyncMessage("SpecialPowers.RemoveFiles", {});
};

SpecialPowers.prototype.executeAfterFlushingMessageQueue = function(aCallback) {
  this._pongHandlers.push(aCallback);
  this.mm.sendAsyncMessage("SPPingService", { op: "ping" });
};

SpecialPowers.prototype.nestedFrameSetup = function() {
  let self = this;
  Services.obs.addObserver(function onRemoteBrowserShown(subject, topic, data) {
    let frameLoader = subject;
    // get a ref to the app <iframe>
    let frame = frameLoader.ownerElement;
    let frameId = frame.getAttribute("id");
    if (frameId === "nested-parent-frame") {
      Services.obs.removeObserver(onRemoteBrowserShown, "remote-browser-shown");

      let mm = frame.frameLoader.messageManager;
      self._grandChildFrameMM = mm;

      self.SP_SYNC_MESSAGES.forEach(function(msgname) {
        mm.addMessageListener(msgname, function(msg) {
          return self._sendSyncMessage(msgname, msg.data)[0];
        });
      });
      self.SP_ASYNC_MESSAGES.forEach(function(msgname) {
        mm.addMessageListener(msgname, function(msg) {
          self._sendAsyncMessage(msgname, msg.data);
        });
      });
      mm.addMessageListener("SPPAddNestedMessageListener", function(msg) {
        self._addMessageListener(msg.json.name, function(aMsg) {
          mm.sendAsyncMessage(aMsg.name, aMsg.data);
          });
      });

      mm.loadFrameScript("resource://specialpowers/specialpowersFrameScript.js", false);

      let frameScript = "SpecialPowers.prototype.IsInNestedFrame=true;";
      mm.loadFrameScript("data:," + frameScript, false);
    }
  }, "remote-browser-shown");
};

SpecialPowers.prototype.isServiceWorkerRegistered = function() {
  var swm = Cc["@mozilla.org/serviceworkers/manager;1"]
              .getService(Ci.nsIServiceWorkerManager);
  return swm.getAllRegistrations().length != 0;
};

// Attach our API to the window.
function attachSpecialPowersToWindow(aWindow, mm) {
  try {
    if ((aWindow !== null) &&
        (aWindow !== undefined) &&
        (aWindow.wrappedJSObject) &&
        !(aWindow.wrappedJSObject.SpecialPowers)) {
      let sp = new SpecialPowers(aWindow, mm);
      aWindow.wrappedJSObject.SpecialPowers = sp;
      if (sp.IsInNestedFrame) {
        sp.addPermission("allowXULXBL", true, aWindow.document);
      }
    }
  } catch (ex) {
    dump("TEST-INFO | specialpowers.js |  Failed to attach specialpowers to window exception: " + ex + "\n");
  }
}

this.SpecialPowers = SpecialPowers;
this.attachSpecialPowersToWindow = attachSpecialPowersToWindow;

// In the case of Chrome mochitests that inject specialpowers.js as
// a regular content script
if (typeof window != "undefined") {
  window.addMessageListener = function() {};
  window.removeMessageListener = function() {};
  window.wrappedJSObject.SpecialPowers = new SpecialPowers(window, window);
}
