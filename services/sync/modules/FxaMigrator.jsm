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

// We send this notification whenever the "user" migration state changes.
const OBSERVER_STATE_CHANGE_TOPIC = "fxa-migration:state-changed";
// We also send the state notification when we *receive* this.  This allows
// consumers to avoid loading this module until it receives a notification
// from us (which may never happen if there's no migration to do)
const OBSERVER_STATE_REQUEST_TOPIC = "fxa-migration:state-request";

// We send this notification whenever the migration is paused waiting for
// something internal to complete.
const OBSERVER_INTERNAL_STATE_CHANGE_TOPIC = "fxa-migration:internal-state-changed";

// We use this notification so Sync's healthreport module can record telemetry
// (actually via "health report") for us.
const OBSERVER_INTERNAL_TELEMETRY_TOPIC = "fxa-migration:internal-telemetry";

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
  // Note that "identity.sync.tokenserver.uri" and "services.sync.tokenServerURI"
  // have special handing when writing/reading prefs.
];

function Migrator() {
  // Leave the log-level as Debug - Sync will setup log appenders such that
  // these messages generally will not be seen unless other log related
  // prefs are set.
  this.log.level = Log.Level.Debug;

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

  // What internal state are we at?  This is primarily used for FHR reporting so
  // we can determine why exactly we might be stalled.
  STATE_INTERNAL_WAITING_SYNC_COMPLETE: "waiting for sync to complete",
  STATE_INTERNAL_WAITING_WRITE_SENTINEL: "waiting for sentinel to be written",
  STATE_INTERNAL_WAITING_START_OVER: "waiting for sync to reset itself",
  STATE_INTERNAL_COMPLETE: "migration complete",

  // Flags for the telemetry we record.  The UI will call a helper to record
  // the fact some UI was interacted with.
  TELEMETRY_ACCEPTED: "accepted",
  TELEMETRY_DECLINED: "declined",
  TELEMETRY_UNLINKED: "unlinked",

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
    let update = (newState, email=null) => {
      this.log.info("Migration state: '${state}' => '${newState}'",
                    {state: this._state, newState: newState});
      if (forceObserver || newState !== this._state) {
        this._state = newState;
        let subject = Cc["@mozilla.org/supports-string;1"]
                      .createInstance(Ci.nsISupportsString);
        subject.data = email || "";
        Services.obs.notifyObservers(subject, OBSERVER_STATE_CHANGE_TOPIC, newState);
      }
      return newState;
    }

    // If we have no sync user, or are already using an FxA account we must
    // be done.
    if (WeaveService.fxAccountsEnabled) {
      // should not be necessary, but if we somehow ended up with FxA enabled
      // and sync blocked it would be bad - so better safe than sorry.
      this.log.debug("FxA enabled - there's nothing to do!")
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
      // See if there is a migration sentinel so we can send the email
      // address that was used on a different device for this account (ie, if
      // this is a "join the party" migration rather than the first)
      let sentinel = yield this._getSyncMigrationSentinel();
      return update(this.STATE_USER_FXA, sentinel && sentinel.email);
    }
    if (!fxauser.verified) {
      return update(this.STATE_USER_FXA_VERIFIED, fxauser.email);
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
      Services.obs.notifyObservers(null, OBSERVER_INTERNAL_STATE_CHANGE_TOPIC,
                                   this.STATE_INTERNAL_WAITING_SYNC_COMPLETE);
      return null;
    }

    // Write the migration sentinel if necessary.
    Services.obs.notifyObservers(null, OBSERVER_INTERNAL_STATE_CHANGE_TOPIC,
                                 this.STATE_INTERNAL_WAITING_WRITE_SENTINEL);
    yield this._setMigrationSentinelIfNecessary();

    // Get the list of enabled engines to we can restore that state.
    let enginePrefs = this._getEngineEnabledPrefs();

    // Must be ready to perform the actual migration.
    this.log.info("Performing final sync migration steps");
    // Do the actual migration.  We setup one observer for when the new identity
    // is about to be initialized so we can reset some key preferences - but
    // there's no promise associated with this.
    let observeStartOverIdentity;
    Services.obs.addObserver(observeStartOverIdentity = () => {
      this.log.info("observed that startOver is about to re-initialize the identity");
      Services.obs.removeObserver(observeStartOverIdentity, "weave:service:start-over:init-identity");
      // We've now reset all sync prefs - set the engine related prefs back to
      // what they were.
      for (let [prefName, prefType, prefVal] of enginePrefs) {
        this.log.debug("Restoring pref ${prefName} (type=${prefType}) to ${prefVal}",
                       {prefName, prefType, prefVal});
        switch (prefType) {
          case Services.prefs.PREF_BOOL:
            Services.prefs.setBoolPref(prefName, prefVal);
            break;
          case Services.prefs.PREF_STRING:
            Services.prefs.setCharPref(prefName, prefVal);
            break;
          default:
            // _getEngineEnabledPrefs doesn't return any other type...
            Cu.reportError("unknown engine pref type for " + prefName + ": " + prefType);
        }
      }
    }, "weave:service:start-over:init-identity", false);

    // And another observer for the startOver being fully complete - the only
    // reason for this is so we can wait until everything is fully reset.
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
    Services.obs.notifyObservers(null, OBSERVER_INTERNAL_STATE_CHANGE_TOPIC,
                                 this.STATE_INTERNAL_WAITING_START_OVER);
    yield startOverComplete;
    // observer fired, now kick things off with the FxA user.
    this.log.info("scheduling initial FxA sync.");
    // Note we technically don't need to unblockSync as by now all sync prefs
    // have been reset - but it doesn't hurt.
    this._unblockSync();
    Weave.Service.scheduler.scheduleNextSync(0);

    // Tell the front end that migration is now complete -- Sync is now
    // configured with an FxA user.
    forceObserver = true;
    this.log.info("Migration complete");
    update(null);

    Services.obs.notifyObservers(null, OBSERVER_INTERNAL_STATE_CHANGE_TOPIC,
                                 this.STATE_INTERNAL_COMPLETE);
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
    // We used to use services.sync.tokenServerURI as the tokenServer pref but
    // have since changed to identity.sync.tokenserver.uri. However, clients
    // using this pref may not have updated - so always write whatever value
    // is actually being used to the "old" name.
    let tokenServerValue;
    for (let pref of ["services.sync.tokenServerURI", "identity.sync.tokenserver.uri"]) {
      if (Services.prefs.prefHasUserValue(pref)) {
        tokenServerValue = Services.prefs.getCharPref(pref);
        break;
      }
    }
    if (tokenServerValue) {
      result["services.sync.tokenServerURI"] = tokenServerValue;
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
    // And special handling for the tokenserver prefs.
    let tokenServerValue = savedPrefs["services.sync.tokenServerURI"];
    if (tokenServerValue) {
      Services.prefs.setCharPref("identity.sync.tokenserver.uri", tokenServerValue);
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
    yield Weave.Service.setFxAMigrationSentinel(sentinel);
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
    let sentinel = yield Weave.Service.getFxAMigrationSentinel();
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
    Weave.Service.scheduler.blockSync();
  },

  _unblockSync() {
    Weave.Service.scheduler.unblockSync();
  },

  /* Return a list of [prefName, prefType, prefVal] for all engine related
     preferences.
  */
  _getEngineEnabledPrefs() {
    let result = [];
    for (let engine of Weave.Service.engineManager.getAll()) {
      let prefName = "services.sync.engine." + engine.prefName;
      let prefVal;
      try {
        prefVal = Services.prefs.getBoolPref(prefName);
        result.push([prefName, Services.prefs.PREF_BOOL, prefVal]);
      } catch (ex) {} /* just skip this pref */
    }
    // and the declined list.
    try {
      let prefName = "services.sync.declinedEngines";
      let prefVal = Services.prefs.getCharPref(prefName);
      result.push([prefName, Services.prefs.PREF_STRING, prefVal]);
    } catch (ex) {}
    return result;
  },

  /* return true if all engines are enabled, false otherwise. */
  _allEnginesEnabled() {
    return Weave.Service.engineManager.getAll().every(e => e.enabled);
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
    let {url, options} = yield this.getFxAccountCreationOptions();
    win.switchToTabHavingURI(url, true, options);
    // An FxA observer will fire when the user completes this, which will
    // cause us to move to the next "user blocked" state and notify via our
    // observer notification.
  }),

  // Returns an object with properties "url" and "options", suitable for
  // opening FxAccounts to create/signin to FxA suitable for the migration
  // state.  The caller of this is responsible for the actual opening of the
  // page.
  // This should only be called while we are in the STATE_USER_FXA state.  When
  // the user completes the creation we'll see an ONLOGIN_NOTIFICATION
  // notification from FxA and we'll move to either the STATE_USER_FXA_VERIFIED
  // state or we'll just complete the migration if they login as an already
  // verified user.
  getFxAccountCreationOptions: Task.async(function* (win) {
    // warn if we aren't in the expected state - but go ahead anyway!
    if (this._state != this.STATE_USER_FXA) {
      this.log.warn("getFxAccountCreationOptions called in an unexpected state: ${}", this._state);
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
    // A special flag so server-side metrics can tell this is part of migration.
    tail += "&migration=sync11";
    // We want to ask FxA to offer a "Customize Sync" checkbox iff any engines
    // are disabled.
    let customize = !this._allEnginesEnabled();
    tail += "&customizeSync=" + customize;

    // We assume the caller of this is going to actually use it, so record
    // telemetry now.
    this.recordTelemetry(this.TELEMETRY_ACCEPTED);
    return {
      url: "about:accounts?action=" + action + tail,
      options: {ignoreFragment: true, replaceQueryString: true}
    };
  }),

  // Ask the FxA servers to re-send a verification mail for the currently
  // logged in user. This should only be called while we are in the
  // STATE_USER_FXA_VERIFIED state.  When the user clicks on the link in
  // the mail we should see an ONVERIFIED_NOTIFICATION which will cause us
  // to complete the migration.
  resendVerificationMail: Task.async(function * (win) {
    // warn if we aren't in the expected state - but go ahead anyway!
    if (this._state != this.STATE_USER_FXA_VERIFIED) {
      this.log.warn("resendVerificationMail called in an unexpected state: ${}", this._state);
    }
    let ok = true;
    try {
      yield fxAccounts.resendVerificationEmail();
    } catch (ex) {
      this.log.error("Failed to resend verification mail: ${}", ex);
      ok = false;
    }
    this.recordTelemetry(this.TELEMETRY_ACCEPTED);
    let fxauser = yield fxAccounts.getSignedInUser();
    let sb = Services.strings.createBundle("chrome://browser/locale/accounts.properties");

    let heading = ok ?
                  sb.formatStringFromName("verificationSentHeading", [fxauser.email], 1) :
                  sb.GetStringFromName("verificationNotSentHeading");
    let title = sb.GetStringFromName(ok ? "verificationSentTitle" : "verificationNotSentTitle");
    let description = sb.GetStringFromName(ok ? "verificationSentDescription"
                                              : "verificationNotSentDescription");

    let factory = Cc["@mozilla.org/prompter;1"]
                    .getService(Ci.nsIPromptFactory);
    let prompt = factory.getPrompt(win, Ci.nsIPrompt);
    let bag = prompt.QueryInterface(Ci.nsIWritablePropertyBag2);
    bag.setPropertyAsBool("allowTabModal", true);

    prompt.alert(title, heading + "\n\n" + description);
  }),

  // "forget" about the current Firefox account. This should only be called
  // while we are in the STATE_USER_FXA_VERIFIED state.  After this we will
  // see an ONLOGOUT_NOTIFICATION, which will cause the migrator to return back
  // to the STATE_USER_FXA state, from where they can choose a different account.
  forgetFxAccount: Task.async(function * () {
    // warn if we aren't in the expected state - but go ahead anyway!
    if (this._state != this.STATE_USER_FXA_VERIFIED) {
      this.log.warn("forgetFxAccount called in an unexpected state: ${}", this._state);
    }
    return fxAccounts.signOut();
  }),

  recordTelemetry(flag) {
    // Note the value is the telemetry field name - but this is an
    // implementation detail which could be changed later.
    switch (flag) {
      case this.TELEMETRY_ACCEPTED:
      case this.TELEMETRY_UNLINKED:
      case this.TELEMETRY_DECLINED:
        Services.obs.notifyObservers(null, OBSERVER_INTERNAL_TELEMETRY_TOPIC, flag);
        break;
      default:
        throw new Error("Unexpected telemetry flag: " + flag);
    }
  },

  get learnMoreLink() {
    try {
      var url = Services.prefs.getCharPref("app.support.baseURL");
    } catch (err) {
      return null;
    }
    url += "sync-upgrade";
    let sb = Services.strings.createBundle("chrome://weave/locale/services/sync.properties");
    return {
      text: sb.GetStringFromName("sync.eol.learnMore.label"),
      href: Services.urlFormatter.formatURL(url),
    };
  },
};

// We expose a singleton
this.EXPORTED_SYMBOLS = ["fxaMigrator"];
let fxaMigrator = new Migrator();
