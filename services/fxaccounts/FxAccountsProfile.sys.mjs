/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Firefox Accounts Profile helper.
 *
 * This class abstracts interaction with the profile server for an account.
 * It will handle things like fetching profile data, listening for updates to
 * the user's profile in open browser tabs, and cacheing/invalidating profile data.
 */

import {
  ON_PROFILE_CHANGE_NOTIFICATION,
  log,
} from "resource://gre/modules/FxAccountsCommon.sys.mjs";

import { getFxAccountsSingleton } from "resource://gre/modules/FxAccounts.sys.mjs";

const fxAccounts = getFxAccountsSingleton();

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  FxAccountsProfileClient:
    "resource://gre/modules/FxAccountsProfileClient.sys.mjs",
});

export var FxAccountsProfile = function (options = {}) {
  this._currentFetchPromise = null;
  this._cachedAt = 0; // when we saved the cached version.
  this._isNotifying = false; // are we sending a notification?
  this.fxai = options.fxai || fxAccounts._internal;
  this.client =
    options.profileClient ||
    new lazy.FxAccountsProfileClient({
      fxai: this.fxai,
      serverURL: options.profileServerUrl,
    });

  // An observer to invalidate our _cachedAt optimization. We use a weak-ref
  // just incase this.tearDown isn't called in some cases.
  Services.obs.addObserver(this, ON_PROFILE_CHANGE_NOTIFICATION, true);
  // for testing
  if (options.channel) {
    this.channel = options.channel;
  }
};

FxAccountsProfile.prototype = {
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
    this.fxai = null;
    this.client = null;
    Services.obs.removeObserver(this, ON_PROFILE_CHANGE_NOTIFICATION);
  },

  _notifyProfileChange(uid) {
    this._isNotifying = true;
    Services.obs.notifyObservers(null, ON_PROFILE_CHANGE_NOTIFICATION, uid);
    this._isNotifying = false;
  },

  // Cache fetched data and send out a notification so that UI can update.
  _cacheProfile(response) {
    return this.fxai.withCurrentAccountState(async state => {
      const profile = response.body;
      const userData = await state.getUserAccountData();
      if (profile.uid != userData.uid) {
        throw new Error(
          "The fetched profile does not correspond with the current account."
        );
      }
      let profileCache = {
        profile,
        etag: response.etag,
      };
      await state.updateUserAccountData({ profileCache });
      if (profile.email != userData.email) {
        await this.fxai._handleEmailUpdated(profile.email);
      }
      log.debug("notifying profile changed for user ${uid}", userData);
      this._notifyProfileChange(userData.uid);
      return profile;
    });
  },

  async _getProfileCache() {
    let data = await this.fxai.currentAccountState.getUserAccountData([
      "profileCache",
    ]);
    return data ? data.profileCache : null;
  },

  async _fetchAndCacheProfileInternal() {
    try {
      const profileCache = await this._getProfileCache();
      const etag = profileCache ? profileCache.etag : null;
      let response;
      try {
        response = await this.client.fetchProfile(etag);
      } catch (err) {
        await this.fxai._handleTokenError(err);
        // _handleTokenError always re-throws.
        throw new Error("not reached!");
      }

      // response may be null if the profile was not modified (same ETag).
      if (!response) {
        return null;
      }
      return await this._cacheProfile(response);
    } finally {
      this._cachedAt = Date.now();
      this._currentFetchPromise = null;
    }
  },

  _fetchAndCacheProfile() {
    if (!this._currentFetchPromise) {
      this._currentFetchPromise = this._fetchAndCacheProfileInternal();
    }
    return this._currentFetchPromise;
  },

  // Returns cached data right away if available, otherwise returns null - if
  // it returns null, or if the profile is possibly stale, it attempts to
  // fetch the latest profile data in the background. After data is fetched a
  // notification will be sent out if the profile has changed.
  async getProfile() {
    const profileCache = await this._getProfileCache();
    if (!profileCache) {
      // fetch and cache it in the background.
      this._fetchAndCacheProfile().catch(err => {
        log.error("Background refresh of initial profile failed", err);
      });
      return null;
    }
    if (Date.now() > this._cachedAt + this.PROFILE_FRESHNESS_THRESHOLD) {
      // Note that _fetchAndCacheProfile isn't returned, so continues
      // in the background.
      this._fetchAndCacheProfile().catch(err => {
        log.error("Background refresh of profile failed", err);
      });
    } else {
      log.trace("not checking freshness of profile as it remains recent");
    }
    return profileCache.profile;
  },

  // Get the user's profile data, fetching from the network if necessary.
  // Most callers should instead use `getProfile()`; this methods exists to support
  // callers who need to await the underlying network request.
  async ensureProfile({ staleOk = false, forceFresh = false } = {}) {
    if (staleOk && forceFresh) {
      throw new Error("contradictory options specified");
    }
    const profileCache = await this._getProfileCache();
    if (
      forceFresh ||
      !profileCache ||
      (Date.now() > this._cachedAt + this.PROFILE_FRESHNESS_THRESHOLD &&
        !staleOk)
    ) {
      const profile = await this._fetchAndCacheProfile().catch(err => {
        log.error("Background refresh of profile failed", err);
      });
      if (profile) {
        return profile;
      }
    }
    log.trace("not checking freshness of profile as it remains recent");
    return profileCache ? profileCache.profile : null;
  },

  QueryInterface: ChromeUtils.generateQI([
    "nsIObserver",
    "nsISupportsWeakReference",
  ]),
};
