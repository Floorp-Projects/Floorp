# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is Google Safe Browsing.
#
# The Initial Developer of the Original Code is Google Inc.
# Portions created by the Initial Developer are Copyright (C) 2006
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Fritz Schneider <fritz@google.com> (original author)
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****


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
// alert(p.getPref("some-true-pref"));     // shows true
// alert(p.getPref("no-such-pref", true)); // shows true   
// alert(p.getPref("no-such-pref", null)); // shows null
//
// function observe(prefThatChanged) {
//   alert("Pref changed: " + prefThatChanged);
// };
//
// p.addObserver("somepref", observe);
// p.setPref("somepref", true);            // alerts
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
 * Set a boolean preference
 *
 * @param which Name of preference to set
 * @param value Boolean indicating value to set
 *
 * @deprecated  Just use setPref.
 */
G_Preferences.prototype.setBoolPref = function(which, value) {
  return this.setPref(which, value);
}

/**
 * Get a boolean preference. WILL THROW IF PREFERENCE DOES NOT EXIST.
 * If you don't want this behavior, use getBoolPrefOrDefault.
 *
 * @param which Name of preference to get.
 *
 * @deprecated  Just use getPref.
 */
G_Preferences.prototype.getBoolPref = function(which) {
  return this.prefs_.getBoolPref(which);
}

/**
 * Get a boolean preference or return some default value if it doesn't
 * exist. Note that the default doesn't have to be bool -- it could be
 * anything (e.g., you could pass in null and check if the return
 * value is === null to determine if the pref doesn't exist).
 *
 * @param which Name of preference to get.
 * @param def Value to return if the preference doesn't exist
 * @returns Boolean value of the pref if it exists, else def
 *
 * @deprecated  Just use getPref.
 */
G_Preferences.prototype.getBoolPrefOrDefault = function(which, def) {
  return this.getPref(which, def);
}

/**
 * Get a boolean preference if it exists. If it doesn't, set its value
 * to a default and return the default. Note that the default will be
 * coherced to a bool if it is set, but not in the return value.
 *
 * @param which Name of preference to get.
 * @param def Value to set and return if the preference doesn't exist
 * @returns Boolean value of the pref if it exists, else def
 *
 * @deprecated  Just use getPref.
 */
G_Preferences.prototype.getBoolPrefOrDefaultAndSet = function(which, def) {
  try {
    return this.prefs_.getBoolPref(which);
  } catch(e) {
    this.prefs_.setBoolPref(which, !!def);  // The !! forces boolean conversion
    return def;
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
    // non-existent pref is cleared
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
  var observer = new G_PreferenceObserver(callback);
  // Need to store the observer we create so we can eventually unregister it
  if (!this.observers_[which])
    this.observers_[which] = new G_ObjectSafeMap();
  this.observers_[which].insert(callback, observer);
  this.prefs_.addObserver(which, observer, false /* strong reference */);
}

/**
 * Remove an observer for a given pref.
 *
 * @param which String containing the pref to stop listening to
 * @param callback Function to remove as an observer
 */
G_Preferences.prototype.removeObserver = function(which, callback) {
  var observer = this.observers_[which].find(callback);
  G_Assert(this, !!observer, "Tried to unregister a nonexistant observer"); 
  this.prefs_.removeObserver(which, observer);
  this.observers_[which].erase(callback);
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
  var Ci = Ci;
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
    function observe(prefChanged) {
      G_Assert(z, prefChanged == testPref, "observer broken");
      observeCount++;
    };

    // Test setting, getting, and observing
    p.addObserver(testPref, observe);
    p.setBoolPref(testPref, true);
    G_Assert(z, p.getBoolPref(testPref), "get or set broken");
    G_Assert(z, observeCount == 1, "observer adding not working");

    p.removeObserver(testPref, observe);

    p.setBoolPref(testPref, false);
    G_Assert(z, observeCount == 1, "observer removal not working");
    G_Assert(z, !p.getBoolPref(testPref), "get broken");
    try {
      p.getBoolPref(noSuchPref);
      G_Assert(z, false, "getting non-existent pref didn't throw");
    } catch (e) {
    }
    
    // Try the default varieties
    G_Assert(z, 
             p.getBoolPrefOrDefault(noSuchPref, true), "default borken (t)");
    G_Assert(z, !p.getBoolPrefOrDefault(noSuchPref, false), "default borken");
    
    // And the default-and-set variety
    G_Assert(z, p.getBoolPrefOrDefaultAndSet(noSuchPref, true), 
             "default and set broken (didnt default");
    G_Assert(z, 
             p.getBoolPref(noSuchPref), "default and set broken (didnt set)");
    
    // Remember to clean up the prefs we've set, and test removing prefs 
    // while we're at it
    p.clearPref(noSuchPref);
    G_Assert(z, !p.getBoolPrefOrDefault(noSuchPref, false), "clear broken");
    
    p.clearPref(testPref);
    
    G_Debug(z, "PASSED");
  }
}
#endif
