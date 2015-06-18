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
  } else {
    return objEquiv(actual, expected);
  }
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

this.FxAccountsProfile = function (options = {}) {
  this._cachedProfile = null;
  this.fxa = options.fxa || fxAccounts;
  this.client = options.profileClient || new FxAccountsProfileClient({
    fxa: this.fxa,
    serverURL: options.profileServerUrl,
  });

  // for testing
  if (options.channel) {
    this.channel = options.channel;
  }
}

this.FxAccountsProfile.prototype = {

  tearDown: function () {
    this.fxa = null;
    this.client = null;
    this._cachedProfile = null;
  },

  _getCachedProfile: function () {
    // The cached profile will end up back in the generic accountData
    // once bug 1157529 is fixed.
    return Promise.resolve(this._cachedProfile);
  },

  _notifyProfileChange: function (uid) {
    Services.obs.notifyObservers(null, ON_PROFILE_CHANGE_NOTIFICATION, uid);
  },

  // Cache fetched data if it is different from what's in the cache.
  // Send out a notification if it has changed so that UI can update.
  _cacheProfile: function (profileData) {
    if (!hasChanged(this._cachedProfile, profileData)) {
      log.debug("fetched profile matches cached copy");
      return Promise.resolve(null); // indicates no change (but only tests care)
    }
    this._cachedProfile = profileData;
    return this.fxa.getSignedInUser()
      .then(userData => {
        log.debug("notifying profile changed for user ${uid}", userData);
        this._notifyProfileChange(userData.uid);
        return profileData;
      });
  },

  _fetchAndCacheProfile: function () {
    return this.client.fetchProfile()
      .then(profile => {
        return this._cacheProfile(profile).then(() => profile);
      });
  },

  // Returns cached data right away if available, then fetches the latest profile
  // data in the background. After data is fetched a notification will be sent
  // out if the profile has changed.
  getProfile: function () {
    return this._getCachedProfile()
      .then(cachedProfile => {
        if (cachedProfile) {
          // Note that _fetchAndCacheProfile isn't returned, so continues
          // in the background.
          this._fetchAndCacheProfile().catch(err => {
            log.error("Background refresh of profile failed", err);
          });
          return cachedProfile;
        }
        return this._fetchAndCacheProfile();
      })
      .then(profile => {
        return profile;
      });
  },
};
