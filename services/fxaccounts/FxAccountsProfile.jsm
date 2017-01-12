/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Firefox Accounts Profile helper.
 *
 * This class abstracts interaction with the profile server for an account.
 * It will handle things like fetching profile data, listening for updates to
 * the user's profile in open browser tabs, and cacheing/invalidating profile data.
 */

this.EXPORTED_SYMBOLS = ["FxAccountsProfile"];

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/FxAccountsCommon.js");
Cu.import("resource://gre/modules/FxAccounts.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "FxAccountsProfileClient",
  "resource://gre/modules/FxAccountsProfileClient.jsm");

// Based off of deepEqual from Assert.jsm
function deepEqual(actual, expected) {
  if (actual === expected) {
    return true;
  } else if (typeof actual != "object" && typeof expected != "object") {
    return actual == expected;
  }
  return objEquiv(actual, expected);
}

function isUndefinedOrNull(value) {
  return value === null || value === undefined;
}

function objEquiv(a, b) {
  if (isUndefinedOrNull(a) || isUndefinedOrNull(b)) {
    return false;
  }
  if (a.prototype !== b.prototype) {
    return false;
  }
  let ka, kb, key, i;
  try {
    ka = Object.keys(a);
    kb = Object.keys(b);
  } catch (e) {
    return false;
  }
  if (ka.length != kb.length) {
    return false;
  }
  ka.sort();
  kb.sort();
  for (i = ka.length - 1; i >= 0; i--) {
    key = ka[i];
    if (!deepEqual(a[key], b[key])) {
      return false;
    }
  }
  return true;
}

function hasChanged(oldData, newData) {
  return !deepEqual(oldData, newData);
}

this.FxAccountsProfile = function(options = {}) {
  this._cachedProfile = null;
  this._cachedAt = 0; // when we saved the cached version.
  this._currentFetchPromise = null;
  this._isNotifying = false; // are we sending a notification?
  this.fxa = options.fxa || fxAccounts;
  this.client = options.profileClient || new FxAccountsProfileClient({
    fxa: this.fxa,
    serverURL: options.profileServerUrl,
  });

  // An observer to invalidate our _cachedAt optimization. We use a weak-ref
  // just incase this.tearDown isn't called in some cases.
  Services.obs.addObserver(this, ON_PROFILE_CHANGE_NOTIFICATION, true);
  // for testing
  if (options.channel) {
    this.channel = options.channel;
  }
}

this.FxAccountsProfile.prototype = {
  // If we get subsequent requests for a profile within this period, don't bother
  // making another request to determine if it is fresh or not.
  PROFILE_FRESHNESS_THRESHOLD: 120000, // 2 minutes

  observe(subject, topic, data) {
    // If we get a profile change notification from our webchannel it means
    // the user has just changed their profile via the web, so we want to
    // ignore our "freshness threshold"
    if (topic == ON_PROFILE_CHANGE_NOTIFICATION && !this._isNotifying) {
      log.debug("FxAccountsProfile observed profile change");
      this._cachedAt = 0;
    }
  },

  tearDown() {
    this.fxa = null;
    this.client = null;
    this._cachedProfile = null;
    Services.obs.removeObserver(this, ON_PROFILE_CHANGE_NOTIFICATION);
  },

  _getCachedProfile() {
    // The cached profile will end up back in the generic accountData
    // once bug 1157529 is fixed.
    return Promise.resolve(this._cachedProfile);
  },

  _notifyProfileChange(uid) {
    this._isNotifying = true;
    Services.obs.notifyObservers(null, ON_PROFILE_CHANGE_NOTIFICATION, uid);
    this._isNotifying = false;
  },

  // Cache fetched data if it is different from what's in the cache.
  // Send out a notification if it has changed so that UI can update.
  _cacheProfile(profileData) {
    if (!hasChanged(this._cachedProfile, profileData)) {
      log.debug("fetched profile matches cached copy");
      return Promise.resolve(null); // indicates no change (but only tests care)
    }
    this._cachedProfile = profileData;
    this._cachedAt = Date.now();
    return this.fxa.getSignedInUser()
      .then(userData => {
        log.debug("notifying profile changed for user ${uid}", userData);
        this._notifyProfileChange(userData.uid);
        return profileData;
      });
  },

  _fetchAndCacheProfile() {
    if (!this._currentFetchPromise) {
      this._currentFetchPromise = this.client.fetchProfile().then(profile => {
        return this._cacheProfile(profile).then(() => {
          return profile;
        });
      }).then(profile => {
        this._currentFetchPromise = null;
        return profile;
      }, err => {
        this._currentFetchPromise = null;
        throw err;
      });
    }
    return this._currentFetchPromise
  },

  // Returns cached data right away if available, then fetches the latest profile
  // data in the background. After data is fetched a notification will be sent
  // out if the profile has changed.
  getProfile() {
    return this._getCachedProfile()
      .then(cachedProfile => {
        if (cachedProfile) {
          if (Date.now() > this._cachedAt + this.PROFILE_FRESHNESS_THRESHOLD) {
            // Note that _fetchAndCacheProfile isn't returned, so continues
            // in the background.
            this._fetchAndCacheProfile().catch(err => {
              log.error("Background refresh of profile failed", err);
            });
          } else {
            log.trace("not checking freshness of profile as it remains recent");
          }
          return cachedProfile;
        }
        return this._fetchAndCacheProfile();
      })
      .then(profile => {
        return profile;
      });
  },

  QueryInterface: XPCOMUtils.generateQI([
      Ci.nsIObserver,
      Ci.nsISupportsWeakReference,
  ]),
};
