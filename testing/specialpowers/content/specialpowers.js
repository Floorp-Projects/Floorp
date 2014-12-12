/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* This code is loaded in every child process that is started by mochitest in
 * order to be used as a replacement for UniversalXPConnect
 */

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");
Components.utils.import("resource://gre/modules/Services.jsm");

const Cc = Components.classes;
const Ci = Components.interfaces;

const CHILD_SCRIPT = "chrome://specialpowers/content/specialpowers.js";
const CHILD_SCRIPT_API = "chrome://specialpowers/content/specialpowersAPI.js";
const CHILD_LOGGER_SCRIPT = "chrome://specialpowers/content/MozillaLogger.js";

// All messages sent to observer should be listed here.
const SP_SYNC_MESSAGES = ["SPChromeScriptMessage",
                          "SPLoadChromeScript",
                          "SPObserverService",
                          "SPPermissionManager",
                          "SPPrefService",
                          "SPProcessCrashService",
                          "SPSetTestPluginEnabledState",
                          "SPWebAppService"];

const SP_ASYNC_MESSAGES = ["SpecialPowers.Focus",
                           "SpecialPowers.Quit",
                           "SPPingService",
                           "SPQuotaManager"];

function SpecialPowers(window) {
  this.window = Components.utils.getWeakReference(window);
  this._windowID = window.QueryInterface(Components.interfaces.nsIInterfaceRequestor)
                         .getInterface(Components.interfaces.nsIDOMWindowUtils)
                         .currentInnerWindowID;
  this._encounteredCrashDumpFiles = [];
  this._unexpectedCrashDumpFiles = { };
  this._crashDumpDir = null;
  this.DOMWindowUtils = bindDOMWindowUtils(window);
  Object.defineProperty(this, 'Components', {
      configurable: true, enumerable: true, get: function() {
          var win = this.window.get();
          if (!win)
              return null;
          return getRawComponents(win);
      }});
  this._pongHandlers = [];
  this._messageListener = this._messageReceived.bind(this);
  this._nestedFrameInjected = false;
  this._grandChildFrameMM = null;
  addMessageListener("SPPingService", this._messageListener);
  let (self = this) {
    Services.obs.addObserver(function onInnerWindowDestroyed(subject, topic, data) {
      var id = subject.QueryInterface(Components.interfaces.nsISupportsPRUint64).data;
      if (self._windowID === id) {
        Services.obs.removeObserver(onInnerWindowDestroyed, "inner-window-destroyed");
        try {
          removeMessageListener("SPPingService", self._messageListener);
        } catch (e if e.result == Components.results.NS_ERROR_ILLEGAL_VALUE) {
          // Ignore the exception which the message manager has been destroyed.
          ;
        }
      }
    }, "inner-window-destroyed", false);
    Services.obs.addObserver(function onRemoteBrowserShown(subject, topic, data) {
      let frameLoader = subject;

      // get a ref to the app <iframe>
      frameLoader.QueryInterface(Ci.nsIFrameLoader);
      let frame = frameLoader.ownerElement;
      let mm = frame.QueryInterface(Ci.nsIFrameLoaderOwner).frameLoader.messageManager;
      var frameId = frame.getAttribute('id');

      if (frameId === "nested-parent-frame" && !self._nestedFrameInjected) {
        Services.obs.removeObserver(onRemoteBrowserShown, "remote-browser-shown");
        self._grandChildFrameMM = mm;
        mm.loadFrameScript(CHILD_LOGGER_SCRIPT, true);
        mm.loadFrameScript(CHILD_SCRIPT_API, true);
        mm.loadFrameScript(CHILD_SCRIPT, true);

        SP_SYNC_MESSAGES.forEach(function (msgname) {
          mm.addMessageListener(msgname, function (msg) {
            return self._sendSyncMessage(msgname, msg.data)[0];
          });
        });
        SP_ASYNC_MESSAGES.forEach(function (msgname) {
          mm.addMessageListener(msgname, function (msg) {
            self._sendAsyncMessage(msgname, msg.data);
          });
        });
        mm.addMessageListener("SPPAddNestedMessageListener", function(msg) {
          self._addMessageListener(msg.json.name, function(aMsg) {
            mm.sendAsyncMessage(aMsg.name, aMsg.data);
            });
          });
        self._nestedFrameInjected = true;
      }
    }, "remote-browser-shown", false);
  }
}

SpecialPowers.prototype = new SpecialPowersAPI();

SpecialPowers.prototype.toString = function() { return "[SpecialPowers]"; };
SpecialPowers.prototype.sanityCheck = function() { return "foo"; };

// This gets filled in in the constructor.
SpecialPowers.prototype.DOMWindowUtils = undefined;
SpecialPowers.prototype.Components = undefined;

SpecialPowers.prototype._sendSyncMessage = function(msgname, msg) {
  if (SP_SYNC_MESSAGES.indexOf(msgname) == -1) {
    throw new Error("Unexpected SP message: " + msgname);
  }
  return sendSyncMessage(msgname, msg);
};

SpecialPowers.prototype._sendAsyncMessage = function(msgname, msg) {
  if (SP_ASYNC_MESSAGES.indexOf(msgname) == -1) {
    throw new Error("Unexpected SP message: " + msgname);
  }
  sendAsyncMessage(msgname, msg);
};

SpecialPowers.prototype._addMessageListener = function(msgname, listener) {
  addMessageListener(msgname, listener);
  sendAsyncMessage("SPPAddNestedMessageListener", { name: msgname });
};

SpecialPowers.prototype._removeMessageListener = function(msgname, listener) {
  removeMessageListener(msgname, listener);
};

SpecialPowers.prototype.registerProcessCrashObservers = function() {
  addMessageListener("SPProcessCrashService", this._messageListener);
  sendSyncMessage("SPProcessCrashService", { op: "register-observer" });
};

SpecialPowers.prototype.unregisterProcessCrashObservers = function() {
  removeMessageListener("SPProcessCrashService", this._messageListener);
  sendSyncMessage("SPProcessCrashService", { op: "unregister-observer" });
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
  }
  return true;
};

SpecialPowers.prototype.quit = function() {
  sendAsyncMessage("SpecialPowers.Quit", {});
};

SpecialPowers.prototype.executeAfterFlushingMessageQueue = function(aCallback) {
  this._pongHandlers.push(aCallback);
  sendAsyncMessage("SPPingService", { op: "ping" });
};

// Attach our API to the window.
function attachSpecialPowersToWindow(aWindow) {
  try {
    if ((aWindow !== null) &&
        (aWindow !== undefined) &&
        (aWindow.wrappedJSObject) &&
        !(aWindow.wrappedJSObject.SpecialPowers)) {
      aWindow.wrappedJSObject.SpecialPowers = new SpecialPowers(aWindow);
    }
  } catch(ex) {
    dump("TEST-INFO | specialpowers.js |  Failed to attach specialpowers to window exception: " + ex + "\n");
  }
}

// This is a frame script, so it may be running in a content process.
// In any event, it is targeted at a specific "tab", so we listen for
// the DOMWindowCreated event to be notified about content windows
// being created in this context.

function SpecialPowersManager() {
  addEventListener("DOMWindowCreated", this, false);
}

SpecialPowersManager.prototype = {
  handleEvent: function handleEvent(aEvent) {
    var window = aEvent.target.defaultView;
    attachSpecialPowersToWindow(window);
  }
};

var specialpowersmanager = new SpecialPowersManager();

this.SpecialPowers = SpecialPowers;
this.attachSpecialPowersToWindow = attachSpecialPowersToWindow;

// In the case of Chrome mochitests that inject specialpowers.js as
// a regular content script
if (typeof window != 'undefined') {
  window.addMessageListener = function() {}
  window.removeMessageListener = function() {}
  window.wrappedJSObject.SpecialPowers = new SpecialPowers(window);
}
