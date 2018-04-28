/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.defineModuleGetter(this, "FileUtils",
                               "resource://gre/modules/FileUtils.jsm");
XPCOMUtils.defineLazyGetter(this, "Utils", () => {
  return ChromeUtils.import("resource://services-sync/util.js", {}).Utils;
});

XPCOMUtils.defineLazyPreferenceGetter(this, "syncUsername", "services.sync.username");

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
 * should use the promise-base function whenLoaded() - something like the
 * following:
 *
 * // 1. Grab a handle to the Sync XPCOM service.
 * let service = Cc["@mozilla.org/weave/service;1"]
 *                 .getService(Components.interfaces.nsISupports)
 *                 .wrappedJSObject;
 *
 * // 2. Use the .then method of the promise.
 * service.whenLoaded().then(() => {
 *   // You are free to interact with "Weave." objects.
 *   return;
 * });
 *
 * And that's it!  However, if you really want to avoid promises and do it
 * old-school, then
 *
 * // 1. Get a reference to the service as done in (1) above.
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

  QueryInterface: ChromeUtils.generateQI([Ci.nsIObserver,
                                          Ci.nsISupportsWeakReference]),

  ensureLoaded() {
    ChromeUtils.import("resource://services-sync/main.js");

    // Side-effect of accessing the service is that it is instantiated.
    Weave.Service;
  },

  whenLoaded() {
    if (this.ready) {
      return Promise.resolve();
    }
    let onReadyPromise = new Promise(resolve => {
      Services.obs.addObserver(function onReady() {
        Services.obs.removeObserver(onReady, "weave:service:ready");
        resolve();
      }, "weave:service:ready");
    });
    this.ensureLoaded();
    return onReadyPromise;
  },

  init() {
    // Force Weave service to load if it hasn't triggered from overlays
    this.timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
    this.timer.initWithCallback({
      notify: () => {
        let isConfigured = false;
        // We only load more if it looks like Sync is configured.
        if (this.enabled) {
          // We have an associated FxAccount. So, do a more thorough check.
          // This will import a number of modules and thus increase memory
          // accordingly. We could potentially copy code performed by
          // this check into this file if our above code is yielding too
          // many false positives.
          ChromeUtils.import("resource://services-sync/main.js");
          isConfigured = Weave.Status.checkSetup() != Weave.CLIENT_NOT_CONFIGURED;
        }
        let getHistogramById = Services.telemetry.getHistogramById;
        getHistogramById("WEAVE_CONFIGURED").add(isConfigured);
        if (isConfigured) {
          this.ensureLoaded();
        }
      }
    }, 10000, Ci.nsITimer.TYPE_ONE_SHOT);
  },

  /**
   * Whether Sync appears to be enabled.
   *
   * This returns true if we have an associated FxA account and Sync is enabled.
   *
   * It does *not* perform a robust check to see if the client is working.
   * For that, you'll want to check Weave.Status.checkSetup().
   */
  get enabled() {
    return !!syncUsername && Services.prefs.getBoolPref("identity.fxaccounts.enabled");
  }
};

function AboutWeaveLog() {}
AboutWeaveLog.prototype = {
  classID: Components.ID("{d28f8a0b-95da-48f4-b712-caf37097be41}"),

  QueryInterface: ChromeUtils.generateQI([Ci.nsIAboutModule,
                                          Ci.nsISupportsWeakReference]),

  getURIFlags(aURI) {
    return 0;
  },

  newChannel(aURI, aLoadInfo) {
    let dir = FileUtils.getDir("ProfD", ["weave", "logs"], true);
    let uri = Services.io.newFileURI(dir);
    let channel = Services.io.newChannelFromURIWithLoadInfo(uri, aLoadInfo);

    channel.originalURI = aURI;

    // Ensure that the about page has the same privileges as a regular directory
    // view. That way links to files can be opened. make sure we use the correct
    // origin attributes when creating the principal for accessing the
    // about:sync-log data.
    let principal = Services.scriptSecurityManager.createCodebasePrincipal(uri,
      aLoadInfo.originAttributes);

    channel.owner = principal;
    return channel;
  }
};

const components = [WeaveService, AboutWeaveLog];
this.NSGetFactory = XPCOMUtils.generateNSGetFactory(components);
