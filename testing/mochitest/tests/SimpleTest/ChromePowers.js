/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Special Powers code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Joel Maher <joel.maher@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL. 
 *
 * ***** END LICENSE BLOCK ***** */

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
        var self = this;
        msg.dumpIDs.forEach(function(id) {
          self._encounteredCrashDumpFiles.push(id + ".dmp");
          self._encounteredCrashDumpFiles.push(id + ".extra");
        });
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

