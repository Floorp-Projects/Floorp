/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/FileUtils.jsm");
Cu.import("resource://services-sync/util.js");

const SYNC_PREFS_BRANCH = "services.sync.";


/**
 * Sync's XPCOM service.
 *
 * It is named "Weave" for historical reasons.
 *
 * It's worth noting how Sync is lazily loaded. We register a timer that
 * loads Sync a few seconds after app startup. This is so Sync does not
 * adversely affect application start time.
 *
 * If Sync is not configured, no extra Sync code is loaded. If an
 * external component (say the UI) needs to interact with Sync, it
 * should do something like the following:
 *
 * // 1. Grab a handle to the Sync XPCOM service.
 * let service = Cc["@mozilla.org/weave/service;1"]
 *                 .getService(Components.interfaces.nsISupports)
 *                 .wrappedJSObject;
 *
 * // 2. Check if the service has been initialized.
 * if (service.ready) {
 *   // You are free to interact with "Weave." objects.
 *   return;
 * }
 *
 * // 3. Install "ready" listener.
 * Services.obs.addObserver(function onReady() {
 *   Services.obs.removeObserver(onReady, "weave:service:ready");
 *
 *   // You are free to interact with "Weave." objects.
 * }, "weave:service:ready", false);
 *
 * // 4. Trigger loading of Sync.
 * service.ensureLoaded();
 */
function WeaveService() {
  this.wrappedJSObject = this;
  this.ready = false;
}
WeaveService.prototype = {
  classID: Components.ID("{74b89fb0-f200-4ae8-a3ec-dd164117f6de}"),

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver,
                                         Ci.nsISupportsWeakReference]),

  ensureLoaded: function () {
    Components.utils.import("resource://services-sync/main.js");

    // Side-effect of accessing the service is that it is instantiated.
    Weave.Service;
  },

  get fxAccountsEnabled() {
    // first check if Firefox accounts is available at all.  This is so we can
    // get this landed without forcing Fxa to be used (and require nightly
    // testers to manually set this pref)
    // Once we decide we want Fxa to be available, we just remove this block.
    let fxAccountsAvailable;
    try {
      fxAccountsAvailable = Services.prefs.getBoolPref("identity.fxaccounts.enabled");
    } catch (_) {
    }
    if (!fxAccountsAvailable) {
      // Currently we don't support toggling this pref after initialization, so
      // inject the pref value as a regular boolean.
      delete this.fxAccountsEnabled;
      this.fxAccountsEnabled = false;
      return false;
    }
    // work out what identity manager to use.  This is stored in a preference;
    // if the preference exists, we trust it.
    let fxAccountsEnabled;
    try {
      fxAccountsEnabled = Services.prefs.getBoolPref("services.sync.fxaccounts.enabled");
    } catch (_) {
      // That pref doesn't exist - so let's assume this is a first-run.
      // If sync already appears configured, we assume it's for the legacy
      // provider.
      let prefs = Services.prefs.getBranch(SYNC_PREFS_BRANCH);
      fxAccountsEnabled = !prefs.prefHasUserValue("username");
      Services.prefs.setBoolPref("services.sync.fxaccounts.enabled", fxAccountsEnabled);
    }
    // Currently we don't support toggling this pref after initialization, so
    // inject the pref value as a regular boolean.
    delete this.fxAccountsEnabled;
    return this.fxAccountsEnabled = fxAccountsEnabled;
  },

  maybeInitWithFxAccountsAndEnsureLoaded: function() {
    Components.utils.import("resource://services-sync/main.js");
    // FxAccounts imports lots of stuff, so only do this as we need it
    Cu.import("resource://gre/modules/FxAccounts.jsm");

    // This isn't quite sufficient here to handle all the cases. Cases
    // we need to handle:
    //  - User is signed in to FxAccounts, btu hasn't set up sync.
    return fxAccounts.getSignedInUser().then(
      (accountData) => {
        if (accountData) {
          Cu.import("resource://services-sync/browserid_identity.js");
          // The Sync Identity module needs to be set in both these places if
          // it's swapped out as we are doing here. When Weave.Service initializes
          // it grabs a reference to Weave.Status._authManager, and for references
          // to Weave.Service.identity to resolve correctly, we also need to reset
          // Weave.Service.identity as well.
          Weave.Service.identity = Weave.Status._authManager = new BrowserIDManager(),
          // Init the identity module with any account data from
          // firefox accounts. The Identity module will fetch the signed in
          // user from fxAccounts directly.
          Weave.Service.identity.initWithLoggedInUser().then(function () {
            // Set the cluster data that we got from the token
            Weave.Service.clusterURL = Weave.Service.identity.clusterURL;
            // checkSetup() will check the auth state of the identity module
            // and records that status in Weave.Status
            if (Weave.Status.checkSetup() != Weave.CLIENT_NOT_CONFIGURED) {
              // This makes sure that Weave.Service is loaded
              Svc.Obs.notify("weave:service:setup-complete");
              // TODO: this shouldn't be here. It should be at the end
              // of the promise chain of the 'fxaccounts:onverified' handler.
              Weave.Utils.nextTick(Weave.Service.sync, Weave.Service);
              this.ensureLoaded();
            }
          }.bind(this));
        } else if (Weave.Status.checkSetup() != Weave.CLIENT_NOT_CONFIGURED) {
          // This makes sure that Weave.Service is loaded
          this.ensureLoaded();
        }
      },
      (err) => {dump("err in getting logged in account "+err.message)}
    ).then(null, (err) => {dump("err in processing logged in account "+err.message)});
  },

  observe: function (subject, topic, data) {
    switch (topic) {
    case "app-startup":
      let os = Cc["@mozilla.org/observer-service;1"].
               getService(Ci.nsIObserverService);
      os.addObserver(this, "final-ui-startup", true);
      os.addObserver(this, "fxaccounts:onverified", true);
      os.addObserver(this, "fxaccounts:onlogout", true);
      break;

    case "final-ui-startup":
      // Force Weave service to load if it hasn't triggered from overlays
      this.timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
      this.timer.initWithCallback({
        notify: function() {
          if (this.fxAccountsEnabled) {
            // init  the fxAccounts identity manager.
            this.maybeInitWithFxAccountsAndEnsureLoaded();
          } else {
            // init the "old" style, sync-specific identity manager.
            // We only load more if it looks like Sync is configured.
            let prefs = Services.prefs.getBranch(SYNC_PREFS_BRANCH);
            if (!prefs.prefHasUserValue("username")) {
              return;
            }

            // We have a username. So, do a more thorough check. This will
            // import a number of modules and thus increase memory
            // accordingly. We could potentially copy code performed by
            // this check into this file if our above code is yielding too
            // many false positives.
            Components.utils.import("resource://services-sync/main.js");
            if (Weave.Status.checkSetup() != Weave.CLIENT_NOT_CONFIGURED) {
              this.ensureLoaded();
            }
          }
        }.bind(this)
      }, 10000, Ci.nsITimer.TYPE_ONE_SHOT);
      break;

    case 'fxaccounts:onverified':
        // Tell sync that if this is a first sync, it should try and sync the
        // server data with what is on the client - despite the name implying
        // otherwise, this is what "resetClient" does.
        // TOOD: This implicitly assumes we're in the CLIENT_NOT_CONFIGURED state, and
        // if we're not, we should handle it here.
        Components.utils.import("resource://services-sync/main.js"); // ensure 'Weave' exists
        Weave.Svc.Prefs.set("firstSync", "resetClient");
        this.maybeInitWithFxAccountsAndEnsureLoaded().then(() => {
          // and off we go...
          // TODO: I have this being done in maybeInitWithFxAccountsAndEnsureLoaded
          // because I had a bug in the promise chains that was triggering this
          // too early. This should be fixed.
          //Weave.Utils.nextTick(Weave.Service.sync, Weave.Service);
        });
      break;
    case 'fxaccounts:onlogout':
      Components.utils.import("resource://services-sync/main.js"); // ensure 'Weave' exists
      // startOver is throwing some errors and we can't re-log in in this
      // session - so for now, we don't do this!
      //Weave.Service.startOver();
      break;
    }
  }
};

function AboutWeaveLog() {}
AboutWeaveLog.prototype = {
  classID: Components.ID("{d28f8a0b-95da-48f4-b712-caf37097be41}"),

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIAboutModule,
                                         Ci.nsISupportsWeakReference]),

  getURIFlags: function(aURI) {
    return 0;
  },

  newChannel: function(aURI) {
    let dir = FileUtils.getDir("ProfD", ["weave", "logs"], true);
    let uri = Services.io.newFileURI(dir);
    let channel = Services.io.newChannelFromURI(uri);
    channel.originalURI = aURI;

    // Ensure that the about page has the same privileges as a regular directory
    // view. That way links to files can be opened.
    let ssm = Cc["@mozilla.org/scriptsecuritymanager;1"]
                .getService(Ci.nsIScriptSecurityManager);
    let principal = ssm.getNoAppCodebasePrincipal(uri);
    channel.owner = principal;
    return channel;
  }
};

const components = [WeaveService, AboutWeaveLog];
this.NSGetFactory = XPCOMUtils.generateNSGetFactory(components);
