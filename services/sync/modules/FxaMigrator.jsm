/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict;"

const {classes: Cc, interfaces: Ci, results: Cr, utils: Cu} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Log.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Task.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "fxAccounts",
  "resource://gre/modules/FxAccounts.jsm");

XPCOMUtils.defineLazyGetter(this, "WeaveService", function() {
  return Cc["@mozilla.org/weave/service;1"]
         .getService(Components.interfaces.nsISupports)
         .wrappedJSObject;
});

XPCOMUtils.defineLazyModuleGetter(this, "Weave",
  "resource://services-sync/main.js");

// FxAccountsCommon.js doesn't use a "namespace", so create one here.
let fxAccountsCommon = {};
Cu.import("resource://gre/modules/FxAccountsCommon.js", fxAccountsCommon);

// We send this notification whenever the migration state changes.
const OBSERVER_STATE_CHANGE_TOPIC = "fxa-migration:state-changed";
// We also send the state notification when we *receive* this.  This allows
// consumers to avoid loading this module until it receives a notification
// from us (which may never happen if there's no migration to do)
const OBSERVER_STATE_REQUEST_TOPIC = "fxa-migration:state-request";

const OBSERVER_TOPICS = [
  "xpcom-shutdown",
  "weave:service:sync:start",
  "weave:service:sync:finish",
  "weave:service:sync:error",
  "weave:eol",
  OBSERVER_STATE_REQUEST_TOPIC,
  fxAccountsCommon.ONLOGIN_NOTIFICATION,
  fxAccountsCommon.ONLOGOUT_NOTIFICATION,
  fxAccountsCommon.ONVERIFIED_NOTIFICATION,
];

// A list of preference names we write to the migration sentinel.  We only
// write ones that have a user-set value.
const FXA_SENTINEL_PREFS = [
  "identity.fxaccounts.auth.uri",
  "identity.fxaccounts.remote.force_auth.uri",
  "identity.fxaccounts.remote.signup.uri",
  "identity.fxaccounts.remote.signin.uri",
  "identity.fxaccounts.settings.uri",
  "services.sync.tokenServerURI",
];

function Migrator() {
  // Leave the log-level as Debug - Sync will setup log appenders such that
  // these messages generally will not be seen unless other log related
  // prefs are set.
  this.level = Log.Level.Debug;

  this._nextUserStatePromise = Promise.resolve();

  for (let topic of OBSERVER_TOPICS) {
    Services.obs.addObserver(this, topic, false);
  }
  // ._state is an optimization so we avoid sending redundant observer
  // notifications when the state hasn't actually changed.
  this._state = null;
}

Migrator.prototype = {
  log: Log.repository.getLogger("Sync.SyncMigration"),

  // What user action is necessary to push the migration forward?
  // A |null| state means there is nothing to do.  Note that a null state implies
  // either. (a) no migration is necessary or (b) that the migrator module is
  // waiting for something outside of the user's control - eg, sync to complete,
  // the migration sentinel to be uploaded, etc.  In most cases the wait will be
  // short, but edge cases (eg, no network, sync bugs that prevent it stopping
  // until shutdown) may require a significantly longer wait.
  STATE_USER_FXA: "waiting for user to be signed in to FxA",
  STATE_USER_FXA_VERIFIED: "waiting for a verified FxA user",

  finalize() {
    for (let topic of OBSERVER_TOPICS) {
      Services.obs.removeObserver(this, topic);
    }
  },

  observe(subject, topic, data) {
    this.log.debug("observed " + topic);
    switch (topic) {
      case "xpcom-shutdown":
        this.finalize();
        break;

      case OBSERVER_STATE_REQUEST_TOPIC:
        // someone has requested the state - send it.
        this._queueCurrentUserState(true);
        break;

      default:
        // some other observer that may affect our state has fired, so update.
        this._queueCurrentUserState().then(
          () => this.log.debug("update state from observer " + topic + " complete")
        ).catch(err => {
          let msg = "Failed to handle topic " + topic + ": " + err;
          Cu.reportError(msg);
          this.log.error(msg);
        });
    }
  },

  // Try and move to a state where we are blocked on a user action.
  // This needs to be restartable, and the states may, in edge-cases, end
  // up going backwards (eg, user logs out while we are waiting to be told
  // about verification)
  // This is called by our observer notifications - so if there is already
  // a promise in-flight, it's possible we will miss something important - so
  // we wait for the in-flight one to complete then fire another (ie, this
  // is effectively a queue of promises)
  _queueCurrentUserState(forceObserver = false) {
    return this._nextUserStatePromise = this._nextUserStatePromise.then(
      () => this._promiseCurrentUserState(forceObserver),
      err => {
        let msg = "Failed to determine the current user state: " + err;
        Cu.reportError(msg);
        this.log.error(msg);
        return this._promiseCurrentUserState(forceObserver)
      }
    );
  },

  _promiseCurrentUserState: Task.async(function* (forceObserver) {
    this.log.trace("starting _promiseCurrentUserState");
    let update = newState => {
      this.log.info("Migration state: '${state}' => '${newState}'",
                    {state: this._state, newState: newState});
      if (forceObserver || newState !== this._state) {
        this._state = newState;
        Services.obs.notifyObservers(null, OBSERVER_STATE_CHANGE_TOPIC, newState);
      }
      return newState;
    }

    // If we have no sync user, or are already using an FxA account we must
    // be done.
    if (WeaveService.fxAccountsEnabled) {
      // should not be necessary, but if we somehow ended up with FxA enabled
      // and sync blocked it would be bad - so better safe than sorry.
      this._unblockSync();
      return update(null);
    }

    // so we need to migrate - let's see how far along we are.
    // If sync isn't in EOL mode, then we are still waiting for the server
    // to offer the migration process - so no user action necessary.
    let isEOL = false;
    try {
      isEOL = !!Services.prefs.getCharPref("services.sync.errorhandler.alert.mode");
    } catch (e) {}

    if (!isEOL) {
      return update(null);
    }

    // So we are in EOL mode - have we a user?
    let fxauser = yield fxAccounts.getSignedInUser();
    if (!fxauser) {
      return update(this.STATE_USER_FXA);
    }
    if (!fxauser.verified) {
      return update(this.STATE_USER_FXA_VERIFIED);
    }

    // So we just have housekeeping to do - we aren't blocked on a user, so
    // reflect that.
    this.log.info("No next user state - doing some housekeeping");
    update(null);

    // We need to disable sync from automatically starting,
    // and if we are currently syncing wait for it to complete.
    this._blockSync();

    // Are we currently syncing?
    if (Weave.Service._locked) {
      // our observers will kick us further along when complete.
      this.log.info("waiting for sync to complete")
      return null;
    }

    // Write the migration sentinel if necessary.
    yield this._setMigrationSentinelIfNecessary();

    // Must be ready to perform the actual migration.
    this.log.info("Performing final sync migration steps");
    // Do the actual migration.
    let startOverComplete = new Promise((resolve, reject) => {
      let observe;
      Services.obs.addObserver(observe = () => {
        this.log.info("observed that startOver is complete");
        Services.obs.removeObserver(observe, "weave:service:start-over:finish");
        resolve();
      }, "weave:service:start-over:finish", false);
    });

    Weave.Service.startOver();
    // need to wait for an observer.
    yield startOverComplete;
    // observer fired, now kick things off with the FxA user.
    this.log.info("scheduling initial FxA sync.");
    this._unblockSync();
    Weave.Service.scheduler.scheduleNextSync(0);
    return null;
  }),

  /* Return an object with the preferences we care about */
  _getSentinelPrefs() {
    let result = {};
    for (let pref of FXA_SENTINEL_PREFS) {
      if (Services.prefs.prefHasUserValue(pref)) {
        result[pref] = Services.prefs.getCharPref(pref);
      }
    }
    return result;
  },

  /* Apply any preferences we've obtained from the sentinel */
  _applySentinelPrefs(savedPrefs) {
    for (let pref of FXA_SENTINEL_PREFS) {
      if (savedPrefs[pref]) {
        Services.prefs.setCharPref(pref, savedPrefs[pref]);
      }
    }
  },

  /* Ask sync to upload the migration sentinel */
  _setSyncMigrationSentinel: Task.async(function* () {
    yield WeaveService.whenLoaded();
    let signedInUser = yield fxAccounts.getSignedInUser();
    let sentinel = {
      email: signedInUser.email,
      uid: signedInUser.uid,
      verified: signedInUser.verified,
      prefs: this._getSentinelPrefs(),
    };
    if (Weave.Service.setFxaMigrationSentinel) {
      yield Weave.Service.setFxaMigrationSentinel(sentinel);
    } else {
      this.log.warn("Waiting on bug 1017433; no sync sentinel");
    }
  }),

  /* Ask sync to upload the migration sentinal if we (or any other linked device)
     haven't previously written one.
   */
  _setMigrationSentinelIfNecessary: Task.async(function* () {
    if (!(yield this._getSyncMigrationSentinel())) {
      this.log.info("writing the migration sentinel");
      yield this._setSyncMigrationSentinel();
    }
  }),

  /* Ask sync to return a migration sentinel if one exists, otherwise return null */
  _getSyncMigrationSentinel: Task.async(function* () {
    yield WeaveService.whenLoaded();
    if (!Weave.Service.getFxaMigrationSentinel) {
      this.log.warn("Waiting on bug 1017433; no sync sentinel");
      return null;
    }
    let sentinel = yield Weave.Service.getFxaMigrationSentinel();
    this.log.debug("got migration sentinel ${}", sentinel);
    return sentinel;
  }),

  _getDefaultAccountName: Task.async(function* (sentinel) {
    // Requires looking to see if other devices have written a migration
    // sentinel (eg, see _haveSynchedMigrationSentinel), and if not, see if
    // the legacy account name appears to be a valid email address (via the
    // services.sync.account pref), otherwise return null.
    // NOTE: Sync does all this synchronously via nested event loops, but we
    // expose a promise to make future migration to an async-sync easier.
    if (sentinel && sentinel.email) {
      this.log.info("defaultAccountName found via sentinel: ${}", sentinel.email);
      return sentinel.email;
    }
    // No previous migrations, so check the existing account name.
    let account = Weave.Service.identity.account;
    if (account && account.contains("@")) {
      this.log.info("defaultAccountName found via legacy account name: {}", account);
      return account;
    }
    this.log.info("defaultAccountName could not find an account");
    return null;
  }),

  // Prevent sync from automatically starting
  _blockSync() {
    if (Weave.Service.scheduler.blockSync) {
      Weave.Service.scheduler.blockSync();
    } else {
      this.log.warn("Waiting on bug 1019408; sync not blocked");
    }
  },

  _unblockSync() {
    if (Weave.Service.scheduler.unblockSync) {
      Weave.Service.scheduler.unblockSync();
    } else {
      this.log.warn("Waiting on bug 1019408; sync not unblocked");
    }
  },

  /*
   * Some helpers for the UI to try and move to the next state.
   */

  // Open a UI for the user to create a Firefox Account.  This should only be
  // called while we are in the STATE_USER_FXA state.  When the user completes
  // the creation we'll see an ONLOGIN_NOTIFICATION notification from FxA and
  // we'll move to either the STATE_USER_FXA_VERIFIED state or we'll just
  // complete the migration if they login as an already verified user.
  createFxAccount: Task.async(function* (win) {
    // warn if we aren't in the expected state - but go ahead anyway!
    if (this._state != this.STATE_USER_FXA) {
      this.log.warn("createFxAccount called in an unexpected state: ${}", this._state);
    }
    // We need to obtain the sentinel and apply any prefs that might be
    // specified *before* attempting to setup FxA as the prefs might
    // specify custom servers etc.
    let sentinel = yield this._getSyncMigrationSentinel();
    if (sentinel && sentinel.prefs) {
      this._applySentinelPrefs(sentinel.prefs);
    }
    // If we already have a sentinel then we assume the user has previously
    // created the specified account, so just ask to sign-in.
    let action = sentinel ? "signin" : "signup";
    // See if we can find a default account name to use.
    let email = yield this._getDefaultAccountName(sentinel);
    let tail = email ? "&email=" + encodeURIComponent(email) : "";
    win.switchToTabHavingURI("about:accounts?" + action + tail, true,
                             {ignoreFragment: true, replaceQueryString: true});
    // An FxA observer will fire when the user completes this, which will
    // cause us to move to the next "user blocked" state and notify via our
    // observer notification.
  }),

  // Ask the FxA servers to re-send a verification mail for the currently
  // logged in user. This should only be called while we are in the
  // STATE_USER_FXA_VERIFIED state.  When the user clicks on the link in
  // the mail we should see an ONVERIFIED_NOTIFICATION which will cause us
  // to complete the migration.
  resendVerificationMail: Task.async(function * () {
    // warn if we aren't in the expected state - but go ahead anyway!
    if (this._state != this.STATE_USER_FXA_VERIFIED) {
      this.log.warn("createFxAccount called in an unexpected state: ${}", this._state);
    }
    return fxAccounts.resendVerificationEmail();
  }),

  // "forget" about the current Firefox account. This should only be called
  // while we are in the STATE_USER_FXA_VERIFIED state.  After this we will
  // see an ONLOGOUT_NOTIFICATION, which will cause the migrator to return back
  // to the STATE_USER_FXA state, from where they can choose a different account.
  forgetFxAccount: Task.async(function * () {
    // warn if we aren't in the expected state - but go ahead anyway!
    if (this._state != this.STATE_USER_FXA_VERIFIED) {
      this.log.warn("createFxAccount called in an unexpected state: ${}", this._state);
    }
    return fxAccounts.signOut();
  }),

}

// We expose a singleton
this.EXPORTED_SYMBOLS = ["fxaMigrator"];
let fxaMigrator = new Migrator();
