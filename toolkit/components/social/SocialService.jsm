/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const EXPORTED_SYMBOLS = ["SocialService"];

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "SocialProvider", "resource://gre/modules/SocialProvider.jsm");

// Internal helper methods and state
let SocialServiceInternal = {
  enabled: Services.prefs.getBoolPref("social.enabled"),
  get providerArray() {
    return [p for ([, p] of Iterator(this.providers))];
  }
};

XPCOMUtils.defineLazyGetter(SocialServiceInternal, "providers", function () {
  // Initialize the service (add a pref observer)
  function prefObserver(subject, topic, data) {
    SocialService._setEnabled(Services.prefs.getBoolPref(data));
  }
  Services.prefs.addObserver("social.enabled", prefObserver, false);
  Services.obs.addObserver(function xpcomShutdown() {
    Services.obs.removeObserver(xpcomShutdown, "xpcom-shutdown");
    Services.prefs.removeObserver("social.enabled", prefObserver);
  }, "xpcom-shutdown", false);

  // Now retrieve the providers
  let providers = {};
  let MANIFEST_PREFS = Services.prefs.getBranch("social.manifest.");
  let prefs = MANIFEST_PREFS.getChildList("", {});
  prefs.forEach(function (pref) {
    try {
      var manifest = JSON.parse(MANIFEST_PREFS.getCharPref(pref));
      if (manifest && typeof(manifest) == "object") {
        let provider = new SocialProvider(manifest, SocialServiceInternal.enabled);
        providers[provider.origin] = provider;
      }
    } catch (err) {
      Cu.reportError("SocialService: failed to load provider: " + pref +
                     ", exception: " + err);
    }
  });

  return providers;
});

function schedule(callback) {
  Services.tm.mainThread.dispatch(callback, Ci.nsIThread.DISPATCH_NORMAL);
}

// Public API
const SocialService = {
  get enabled() {
    return SocialServiceInternal.enabled;
  },
  set enabled(val) {
    let enable = !!val;
    if (enable == SocialServiceInternal.enabled)
      return;

    Services.prefs.setBoolPref("social.enabled", enable);
    this._setEnabled(enable);
  },
  _setEnabled: function _setEnabled(enable) {
    SocialServiceInternal.providerArray.forEach(function (p) p.enabled = enable);
    SocialServiceInternal.enabled = enable;
    Services.obs.notifyObservers(null, "social:pref-changed", enable ? "enabled" : "disabled");
  },

  // Adds a provider given a manifest, and returns the added provider.
  addProvider: function addProvider(manifest, onDone) {
    if (SocialServiceInternal.providers[manifest.origin])
      throw new Error("SocialService.addProvider: provider with this origin already exists");

    let provider = new SocialProvider(manifest, SocialServiceInternal.enabled);
    SocialServiceInternal.providers[provider.origin] = provider;

    schedule(function () {
      onDone(provider);
    });
  },

  // Removes a provider with the given origin, and notifies when the removal is
  // complete.
  removeProvider: function removeProvider(origin, onDone) {
    if (!(origin in SocialServiceInternal.providers))
      throw new Error("SocialService.removeProvider: no provider with this origin exists!");

    let provider = SocialServiceInternal.providers[origin];
    provider.enabled = false;

    delete SocialServiceInternal.providers[origin];

    if (onDone)
      schedule(onDone);
  },

  // Returns a single provider object with the specified origin.
  getProvider: function getProvider(origin, onDone) {
    schedule((function () {
      onDone(SocialServiceInternal.providers[origin] || null);
    }).bind(this));
  },

  // Returns an array of installed provider origins.
  getProviderList: function getProviderList(onDone) {
    schedule(function () {
      onDone(SocialServiceInternal.providerArray);
    });
  }
};
