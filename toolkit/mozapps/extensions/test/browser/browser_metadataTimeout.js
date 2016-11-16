/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Test how update window behaves when metadata ping times out
// bug 965788

const URI_EXTENSION_UPDATE_DIALOG = "chrome://mozapps/content/extensions/update.xul";

const PREF_METADATA_LASTUPDATE        = "extensions.getAddons.cache.lastUpdate";

Components.utils.import("resource://gre/modules/Promise.jsm");

var repo = {};
var ARContext = Components.utils.import("resource://gre/modules/addons/AddonRepository.jsm", repo);

// Mock out the XMLHttpRequest factory for AddonRepository so
// we can reply with a timeout
var pXHRStarted = Promise.defer();
var oldXHRConstructor = ARContext.XHRequest;
ARContext.XHRequest = function() {
  this._handlers = new Map();
  this.mozBackgroundRequest = false;
  this.timeout = undefined;
  this.open = function(aMethod, aURI, aAsync) {
      this.method = aMethod;
      this.uri = aURI;
      this.async = aAsync;
      info("Opened XHR for " + aMethod + " " + aURI);
    };
  this.overrideMimeType = function(aMimeType) {
      this.mimeType = aMimeType;
    };
  this.addEventListener = function(aEvent, aHandler, aCapture) {
      this._handlers.set(aEvent, aHandler);
    };
  this.send = function(aBody) {
      info("Send XHR for " + this.method + " " + this.uri + " handlers: " + [this._handlers.keys()].join(", "));
      pXHRStarted.resolve(this);
    }
};


// Returns promise{window}, resolves with a handle to the compatibility
// check window
function promise_open_compatibility_window(aInactiveAddonIds) {
  let deferred = Promise.defer();
  // This will reset the longer timeout multiplier to 2 which will give each
  // test that calls open_compatibility_window a minimum of 60 seconds to
  // complete.
  requestLongerTimeout(2);

  var variant = Cc["@mozilla.org/variant;1"].
                createInstance(Ci.nsIWritableVariant);
  variant.setFromVariant(aInactiveAddonIds);

  // Cannot be modal as we want to interract with it, shouldn't cause problems
  // with testing though.
  var features = "chrome,centerscreen,dialog,titlebar";
  var ww = Cc["@mozilla.org/embedcomp/window-watcher;1"].
           getService(Ci.nsIWindowWatcher);
  var win = ww.openWindow(null, URI_EXTENSION_UPDATE_DIALOG, "", features, variant);

  win.addEventListener("load", function() {
    function page_shown(aEvent) {
      if (aEvent.target.pageid)
        info("Page " + aEvent.target.pageid + " shown");
    }

    win.removeEventListener("load", arguments.callee, false);

    info("Compatibility dialog opened");

    win.addEventListener("pageshow", page_shown, false);
    win.addEventListener("unload", function() {
      win.removeEventListener("unload", arguments.callee, false);
      win.removeEventListener("pageshow", page_shown, false);
      dump("Compatibility dialog closed\n");
    }, false);

    deferred.resolve(win);
  }, false);
  return deferred.promise;
}

function promise_window_close(aWindow) {
  let deferred = Promise.defer();
  aWindow.addEventListener("unload", function() {
    aWindow.removeEventListener("unload", arguments.callee, false);
    deferred.resolve(aWindow);
  }, false);
  return deferred.promise;
}

// Start the compatibility update dialog, but use the mock XHR to respond with
// a timeout
add_task(function* amo_ping_timeout() {
  Services.prefs.setBoolPref(PREF_GETADDONS_CACHE_ENABLED, true);
  Services.prefs.clearUserPref(PREF_METADATA_LASTUPDATE);
  let compatWindow = yield promise_open_compatibility_window([]);

  let xhr = yield pXHRStarted.promise;
  is(xhr.timeout, 30000, "XHR request should have 30 second timeout");
  ok(xhr._handlers.has("timeout"), "Timeout handler set on XHR");
  // call back the timeout handler
  xhr._handlers.get("timeout")();

  // Put the old XHR constructor back
  ARContext.XHRequest = oldXHRConstructor;
  // The window should close without further interaction
  yield promise_window_close(compatWindow);
});
