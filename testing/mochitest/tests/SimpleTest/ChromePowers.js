/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function ChromePowers(window) {
  this.window = Components.utils.getWeakReference(window);

  // In the case of browser-chrome tests, we are running as a [ChromeWindow]
  // and we have no window.QueryInterface available, content.window is what we need
  if (typeof(window) == "ChromeWindow" && typeof(content.window) == "Window") {
    this.DOMWindowUtils = bindDOMWindowUtils(content.window);
    this.window = Components.utils.getWeakReference(content.window);
  } else {
    this.DOMWindowUtils = bindDOMWindowUtils(window);
  }

  this.spObserver = new SpecialPowersObserverAPI();
}

ChromePowers.prototype = new SpecialPowersAPI();

ChromePowers.prototype.toString = function() { return "[ChromePowers]"; };
ChromePowers.prototype.sanityCheck = function() { return "foo"; };

// This gets filled in in the constructor.
ChromePowers.prototype.DOMWindowUtils = undefined;

ChromePowers.prototype._sendSyncMessage = function(type, msg) {
  var aMessage = {'name':type, 'json': msg};
  return [this._receiveMessage(aMessage)];
};

ChromePowers.prototype._sendAsyncMessage = function(type, msg) {
  var aMessage = {'name':type, 'json': msg};
  this._receiveMessage(aMessage);
};

ChromePowers.prototype.registerProcessCrashObservers = function() {
  this._sendSyncMessage("SPProcessCrashService", { op: "register-observer" });
};

ChromePowers.prototype.unregisterProcessCrashObservers = function() {
  this._sendSyncMessage("SPProcessCrashService", { op: "unregister-observer" });
};

ChromePowers.prototype._receiveMessage = function(aMessage) {
  switch (aMessage.name) {
    case "SpecialPowers.Quit":
      let appStartup = Cc["@mozilla.org/toolkit/app-startup;1"].getService(Ci.nsIAppStartup);
      appStartup.quit(Ci.nsIAppStartup.eForceQuit);
      break;
    case "SPProcessCrashService":
      if (aMessage.json.op == "register-observer" || aMessage.json.op == "unregister-observer") {
        // Hack out register/unregister specifically for browser-chrome leaks
        break;
      } else if (aMessage.type == "crash-observed") {
        for (let e of msg.dumpIDs) {
          this._encounteredCrashDumpFiles.push(e.id + "." + e.extension);
        }
      }
    default:
      // All calls go here, because we need to handle SPProcessCrashService calls as well
      return this.spObserver._receiveMessageAPI(aMessage);
      break;
  }
};

ChromePowers.prototype.quit = function() {
  // We come in here as SpecialPowers.quit, but SpecialPowers is really ChromePowers.
  // For some reason this.<func> resolves to TestRunner, so using SpecialPowers
  // allows us to use the ChromePowers object which we defined below.
  SpecialPowers._sendSyncMessage("SpecialPowers.Quit", {});
};

ChromePowers.prototype.focus = function(aWindow) {
  // We come in here as SpecialPowers.focus, but SpecialPowers is really ChromePowers.
  // For some reason this.<func> resolves to TestRunner, so using SpecialPowers
  // allows us to use the ChromePowers object which we defined below.
  if (aWindow)
    aWindow.focus();
};

ChromePowers.prototype.executeAfterFlushingMessageQueue = function(aCallback) {
  aCallback();
};

// Expose everything but internal APIs (starting with underscores) to
// web content.  We cannot use Object.keys to view SpecialPowers.prototype since
// we are using the functions from SpecialPowersAPI.prototype
ChromePowers.prototype.__exposedProps__ = {};
for (var i in ChromePowers.prototype) {
  if (i.charAt(0) != "_")
    ChromePowers.prototype.__exposedProps__[i] = "r";
}

if ((window.parent !== null) &&
    (window.parent !== undefined) &&
    (window.parent.wrappedJSObject.SpecialPowers) &&
    !(window.wrappedJSObject.SpecialPowers)) {
  window.wrappedJSObject.SpecialPowers = window.parent.SpecialPowers;
} else {
  window.wrappedJSObject.SpecialPowers = new ChromePowers(window);
}

