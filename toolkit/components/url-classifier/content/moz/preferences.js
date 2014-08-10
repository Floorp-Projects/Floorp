# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


// Class for manipulating preferences. Aside from wrapping the pref
// service, useful functionality includes:
//
// - abstracting prefobserving so that you can observe preferences
//   without implementing nsIObserver 
// 
// - getters that return a default value when the pref doesn't exist 
//   (instead of throwing)
// 
// - get-and-set getters
//
// Example:
// 
// var p = new PROT_Preferences();
// dump(p.getPref("some-true-pref"));     // shows true
// dump(p.getPref("no-such-pref", true)); // shows true   
// dump(p.getPref("no-such-pref", null)); // shows null
//
// function observe(prefThatChanged) {
//   dump("Pref changed: " + prefThatChanged);
// };
//
// p.addObserver("somepref", observe);
// p.setPref("somepref", true);            // dumps
// p.removeObserver("somepref", observe);
//
// TODO: should probably have the prefobserver pass in the new and old
//       values

// TODO(tc): Maybe remove this class and just call natively since we're no
//           longer an extension.

/**
 * A class that wraps the preferences service.
 *
 * @param opt_startPoint        A starting point on the prefs tree to resolve 
 *                              names passed to setPref and getPref.
 *
 * @param opt_useDefaultPranch  Set to true to work against the default 
 *                              preferences tree instead of the profile one.
 *
 * @constructor
 */
function G_Preferences(opt_startPoint, opt_getDefaultBranch) {
  this.debugZone = "prefs";
  this.observers_ = {};
  this.getDefaultBranch_ = !!opt_getDefaultBranch;

  this.startPoint_ = opt_startPoint || null;
}

G_Preferences.setterMap_ = { "string": "setCharPref",
                             "boolean": "setBoolPref",
                             "number": "setIntPref" };

G_Preferences.getterMap_ = {};
G_Preferences.getterMap_[Ci.nsIPrefBranch.PREF_STRING] = "getCharPref";
G_Preferences.getterMap_[Ci.nsIPrefBranch.PREF_BOOL] = "getBoolPref";
G_Preferences.getterMap_[Ci.nsIPrefBranch.PREF_INT] = "getIntPref";

G_Preferences.prototype.__defineGetter__('prefs_', function() {
  var prefs;
  var prefSvc = Cc["@mozilla.org/preferences-service;1"]
                  .getService(Ci.nsIPrefService);

  if (this.getDefaultBranch_) {
    prefs = prefSvc.getDefaultBranch(this.startPoint_);
  } else {
    prefs = prefSvc.getBranch(this.startPoint_);
  }

  // QI to prefs in case we want to add observers
  prefs.QueryInterface(Ci.nsIPrefBranchInternal);
  return prefs;
});

/**
 * Stores a key/value in a user preference. Valid types for val are string,
 * boolean, and number. Complex values are not yet supported (but feel free to
 * add them!).
 */
G_Preferences.prototype.setPref = function(key, val) {
  var datatype = typeof(val);

  if (datatype == "number" && (val % 1 != 0)) {
    throw new Error("Cannot store non-integer numbers in preferences.");
  }

  var meth = G_Preferences.setterMap_[datatype];

  if (!meth) {
    throw new Error("Pref datatype {" + datatype + "} not supported.");
  }

  return this.prefs_[meth](key, val);
}

/**
 * Retrieves a user preference. Valid types for the value are the same as for
 * setPref. If the preference is not found, opt_default will be returned 
 * instead.
 */
G_Preferences.prototype.getPref = function(key, opt_default) {
  var type = this.prefs_.getPrefType(key);

  // zero means that the specified pref didn't exist
  if (type == Ci.nsIPrefBranch.PREF_INVALID) {
    return opt_default;
  }

  var meth = G_Preferences.getterMap_[type];

  if (!meth) {
    throw new Error("Pref datatype {" + type + "} not supported.");
  }

  // If a pref has been cleared, it will have a valid type but won't
  // be gettable, so this will throw.
  try {
    return this.prefs_[meth](key);
  } catch(e) {
    return opt_default;
  }
}

/**
 * Delete a preference. 
 *
 * @param which Name of preference to obliterate
 */
G_Preferences.prototype.clearPref = function(which) {
  try {
    // This throws if the pref doesn't exist, which is fine because a 
    // nonexistent pref is cleared
    this.prefs_.clearUserPref(which);
  } catch(e) {}
}

/**
 * Add an observer for a given pref.
 *
 * @param which String containing the pref to listen to
 * @param callback Function to be called when the pref changes. This
 *                 function will receive a single argument, a string 
 *                 holding the preference name that changed
 */
G_Preferences.prototype.addObserver = function(which, callback) {
  // Need to store the observer we create so we can eventually unregister it
  if (!this.observers_[which])
    this.observers_[which] = { callbacks: [], observers: [] };

  /* only add an observer if the callback hasn't been registered yet */
  if (this.observers_[which].callbacks.indexOf(callback) == -1) {
    var observer = new G_PreferenceObserver(callback);
    this.observers_[which].callbacks.push(callback);
    this.observers_[which].observers.push(observer);
    this.prefs_.addObserver(which, observer, false /* strong reference */);
  }
}

/**
 * Remove an observer for a given pref.
 *
 * @param which String containing the pref to stop listening to
 * @param callback Function to remove as an observer
 */
G_Preferences.prototype.removeObserver = function(which, callback) {
  var ix = this.observers_[which].callbacks.indexOf(callback);
  G_Assert(this, ix != -1, "Tried to unregister a nonexistent observer"); 
  this.observers_[which].callbacks.splice(ix, 1);
  var observer = this.observers_[which].observers.splice(ix, 1)[0];
  this.prefs_.removeObserver(which, observer);
}

/**
 * Remove all preference observers registered through this object.
 */
G_Preferences.prototype.removeAllObservers = function() {
  for (var which in this.observers_) {
    for each (var observer in this.observers_[which].observers) {
      this.prefs_.removeObserver(which, observer);
    }
  }
  this.observers_ = {};
}

/**
 * Helper class that knows how to observe preference changes and
 * invoke a callback when they do
 *
 * @constructor
 * @param callback Function to call when the preference changes
 */
function G_PreferenceObserver(callback) {
  this.debugZone = "prefobserver";
  this.callback_ = callback;
}

/**
 * Invoked by the pref system when a preference changes. Passes the
 * message along to the callback.
 *
 * @param subject The nsIPrefBranch that changed
 * @param topic String "nsPref:changed" (aka 
 *              NS_PREFBRANCH_PREFCHANGE_OBSERVER_ID -- but where does it
 *              live???)
 * @param data Name of the pref that changed
 */
G_PreferenceObserver.prototype.observe = function(subject, topic, data) {
  G_Debug(this, "Observed pref change: " + data);
  this.callback_(data);
}

/**
 * XPCOM cruft
 *
 * @param iid Interface id of the interface the caller wants
 */
G_PreferenceObserver.prototype.QueryInterface = function(iid) {
  if (iid.equals(Ci.nsISupports) || 
      iid.equals(Ci.nsIObserver) ||
      iid.equals(Ci.nsISupportsWeakReference))
    return this;
  throw Components.results.NS_ERROR_NO_INTERFACE;
}

#ifdef DEBUG
// UNITTESTS
function TEST_G_Preferences() {
  if (G_GDEBUG) {
    var z = "preferences UNITTEST";
    G_debugService.enableZone(z);
    G_Debug(z, "Starting");

    var p = new G_Preferences();
    
    var testPref = "test-preferences-unittest";
    var noSuchPref = "test-preferences-unittest-aypabtu";
    
    // Used to test observing
    var observeCount = 0;
    let observe = function (prefChanged) {
      G_Assert(z, prefChanged == testPref, "observer broken");
      observeCount++;
    };

    // Test setting, getting, and observing
    p.addObserver(testPref, observe);
    p.setPref(testPref, true);
    G_Assert(z, p.getPref(testPref), "get or set broken");
    G_Assert(z, observeCount == 1, "observer adding not working");

    p.removeObserver(testPref, observe);

    p.setPref(testPref, false);
    G_Assert(z, observeCount == 1, "observer removal not working");
    G_Assert(z, !p.getPref(testPref), "get broken");
    
    // Remember to clean up the prefs we've set, and test removing prefs 
    // while we're at it
    p.clearPref(noSuchPref);
    G_Assert(z, !p.getPref(noSuchPref, false), "clear broken");
    
    p.clearPref(testPref);
    
    G_Debug(z, "PASSED");
  }
}
#endif
