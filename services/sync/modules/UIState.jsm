/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

 /**
 * @typedef {Object} UIState
 * @property {string} status The Sync/FxA status, see STATUS_* constants.
 * @property {string} [email] The FxA email configured to log-in with Sync.
 * @property {string} [displayName] The user's FxA display name.
 * @property {string} [avatarURL] The user's FxA avatar URL.
 * @property {Date} [lastSync] The last sync time.
 * @property {boolean} [syncing] Whether or not we are currently syncing.
 */

this.EXPORTED_SYMBOLS = ["UIState"];

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Weave",
                                  "resource://services-sync/main.js");

const TOPICS = [
  "weave:service:login:change",
  "weave:service:login:error",
  "weave:service:ready",
  "weave:service:sync:start",
  "weave:service:sync:finish",
  "weave:service:sync:error",
  "fxaccounts:onlogin", // Defined in FxAccountsCommon, pulling it is expensive.
  "fxaccounts:onlogout",
  "fxaccounts:profilechange",
];

const ON_UPDATE = "sync-ui-state:update"

const STATUS_NOT_CONFIGURED = "not_configured";
const STATUS_LOGIN_FAILED = "login_failed";
const STATUS_NOT_VERIFIED = "not_verified";
const STATUS_SIGNED_IN = "signed_in";

const DEFAULT_STATE = {
  status: STATUS_NOT_CONFIGURED
};

const UIStateInternal = {
  _initialized: false,
  _state: null,

  // We keep _syncing out of the state object because we can only track it
  // using sync events and we can't determine it at any point in time.
  _syncing: false,

  get state() {
    if (!this._state) {
      return DEFAULT_STATE;
    }
    return Object.assign({}, this._state, { syncing: this._syncing });
  },

  isReady() {
    if (!this._initialized) {
      this.init();
      return false;
    }
    return true;
  },

  init() {
    this._initialized = true;
    // Refresh the state in the background.
    this.refreshState().catch(e => {
      Cu.reportError(e);
    });
  },

  // Used for testing.
  reset() {
    this._state = null;
    this._syncing = false;
    this._initialized = false;
  },

  observe(subject, topic, data) {
    switch (topic) {
      case "weave:service:sync:start":
        this.toggleSyncActivity(true);
        break;
      case "weave:service:sync:finish":
      case "weave:service:sync:error":
        this.toggleSyncActivity(false);
        break;
      default:
        this.refreshState().catch(e => {
          Cu.reportError(e);
        });
        break;
    }
  },

  // Builds a new state from scratch.
  async refreshState() {
    this._state = {};
    await this._refreshFxAState();
    this._setLastSyncTime(this._state); // We want this in case we change accounts.

    this.notifyStateUpdated();
    return this.state;
  },

  // Update the current state with the last sync time/currently syncing status.
  toggleSyncActivity(syncing) {
    this._syncing = syncing;
    this._setLastSyncTime(this._state);

    this.notifyStateUpdated();
  },

  notifyStateUpdated() {
    Services.obs.notifyObservers(null, ON_UPDATE);
  },

  async _refreshFxAState() {
    let userData = await this._getUserData();
    this._populateWithUserData(this._state, userData);
    if (this.state.status != STATUS_SIGNED_IN) {
      return;
    }
    let profile = await this._getProfile();
    if (!profile) {
      return;
    }
    this._populateWithProfile(this._state, profile);
  },

  _populateWithUserData(state, userData) {
    let status;
    if (!userData) {
      status = STATUS_NOT_CONFIGURED;
    } else {
      if (this._loginFailed()) {
        status = STATUS_LOGIN_FAILED;
      } else if (!userData.verified) {
        status = STATUS_NOT_VERIFIED;
      } else {
        status = STATUS_SIGNED_IN;
      }
      state.email = userData.email;
    }
    state.status = status;
  },

  _populateWithProfile(state, profile) {
    state.displayName = profile.displayName;
    state.avatarURL = profile.avatar;
  },

  async _getUserData() {
    try {
      return await this.fxAccounts.getSignedInUser();
    } catch (e) {
      // This is most likely in tests, where we quickly log users in and out.
      // The most likely scenario is a user logged out, so reflect that.
      // Bug 995134 calls for better errors so we could retry if we were
      // sure this was the failure reason.
      Cu.reportError("Error updating FxA account info: " + e);
      return null;
    }
  },

  async _getProfile() {
    try {
      return await this.fxAccounts.getSignedInUserProfile();
    } catch (e) {
      // Not fetching the profile is sad but the FxA logs will already have noise.
      return null;
    }
  },

  _setLastSyncTime(state) {
    if (state.status == UIState.STATUS_SIGNED_IN) {
      try {
        state.lastSync = new Date(Services.prefs.getCharPref("services.sync.lastSync", null));
      } catch (_) {
        state.lastSync = null;
      }
    }
  },

  _loginFailed() {
    // Referencing Weave.Service will implicitly initialize sync, and we don't
    // want to force that - so first check if it is ready.
    let service = Cc["@mozilla.org/weave/service;1"]
                  .getService(Ci.nsISupports)
                  .wrappedJSObject;
    if (!service.ready) {
      return false;
    }
    // LOGIN_FAILED_LOGIN_REJECTED explicitly means "you must log back in".
    // All other login failures are assumed to be transient and should go
    // away by themselves, so aren't reflected here.
    return Weave.Status.login == Weave.LOGIN_FAILED_LOGIN_REJECTED;
  },

  set fxAccounts(mockFxAccounts) {
    delete this.fxAccounts;
    this.fxAccounts = mockFxAccounts;
  }
};

XPCOMUtils.defineLazyModuleGetter(UIStateInternal, "fxAccounts",
                                  "resource://gre/modules/FxAccounts.jsm");

for (let topic of TOPICS) {
  Services.obs.addObserver(UIStateInternal, topic);
}

this.UIState = {
  _internal: UIStateInternal,

  ON_UPDATE,

  STATUS_NOT_CONFIGURED,
  STATUS_LOGIN_FAILED,
  STATUS_NOT_VERIFIED,
  STATUS_SIGNED_IN,

  /**
   * Returns true if the module has been initialized and the state set.
   * If not, return false and trigger an init in the background.
   */
  isReady() {
    return this._internal.isReady();
  },

  /**
   * @returns {UIState} The current Sync/FxA UI State.
   */
  get() {
    return this._internal.state;
  },

  /**
   * Refresh the state. Used for testing, don't call this directly since
   * UIState already listens to Sync/FxA notifications to determine if the state
   * needs to be refreshed. ON_UPDATE will be fired once the state is refreshed.
   *
   * @returns {Promise<UIState>} Resolved once the state is refreshed.
   */
  refresh() {
    return this._internal.refreshState();
  },

  /**
   * Reset the state of the whole module. Used for testing.
   */
  reset() {
    this._internal.reset();
  }
};
