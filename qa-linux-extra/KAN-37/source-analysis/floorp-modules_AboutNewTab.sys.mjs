/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { AppConstants } from "resource://gre/modules/AppConstants.sys.mjs";
import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  AboutNewTabParent: "resource:///actors/AboutNewTabParent.sys.mjs",
  AboutNewTabResourceMapping:
    "resource:///modules/AboutNewTabResourceMapping.sys.mjs",
  ActivityStream: "resource://newtab/lib/ActivityStream.sys.mjs",
  NimbusFeatures: "resource://nimbus/ExperimentAPI.sys.mjs",
  ObjectUtils: "resource://gre/modules/ObjectUtils.sys.mjs",
  ProfileAge: "resource://gre/modules/ProfileAge.sys.mjs",
  TelemetryReportingPolicy:
    "resource://gre/modules/TelemetryReportingPolicy.sys.mjs",
});

const ABOUT_URL = "about:newtab";
const PREF_ACTIVITY_STREAM_DEBUG = "browser.newtabpage.activity-stream.debug";
// AboutHomeStartupCache needs us in "quit-application", so stay alive longer.
// TODO: We could better have a shared async shutdown blocker?
const TOPIC_APP_QUIT = "profile-before-change";
const TRAINHOP_NIMBUS_FEATURE = "newtabTrainhop";

export const AboutNewTab = {
  QueryInterface: ChromeUtils.generateQI([
    "nsIObserver",
    "nsISupportsWeakReference",
  ]),

  // AboutNewTab
  initialized: false,

  willNotifyUser: false,

  _activityStreamEnabled: false,
  activityStream: null,
  activityStreamDebug: false,
  activityStreamPromise: null,
  _activityStreamResolver: null,

  _cachedTopSites: null,

  _newTabURL: ABOUT_URL,
  _newTabURLOverridden: false,

  /**
   * init - Initializes an instance of Activity Stream if one doesn't exist already.
   */
  init() {
    if (this.initialized) {
      return;
    }
    let { promise, resolve } = Promise.withResolvers();
    this.activityStreamPromise = promise;
    this._activityStreamResolver = resolve;

    Services.obs.addObserver(this, TOPIC_APP_QUIT);
    if (!AppConstants.RELEASE_OR_BETA) {
      XPCOMUtils.defineLazyPreferenceGetter(
        this,
        "activityStreamDebug",
        PREF_ACTIVITY_STREAM_DEBUG,
        false,
        () => {
          this.notifyChange();
        }
      );
    }

    XPCOMUtils.defineLazyPreferenceGetter(
      this,
      "privilegedAboutProcessEnabled",
      "browser.tabs.remote.separatePrivilegedContentProcess",
      false,
      () => {
        this.notifyChange();
      }
    );

    // Make sure to register newtab resource mapping as early as possible
    // on startup.
    lazy.AboutNewTabResourceMapping.init();

    // ActivityStream.init() also sets this default,
    // but it is gated behind TOU acceptance, which is too late for
    // about:welcome's first-screen impression.
    // Setting it here lets AboutWelcomeTelemetry and ASRouter
    // telemetry submit pings before TOU is accepted while Glean gates the
    // upload on datareporting.healthreport.uploadEnabled.
    const AStelemetryPref = "browser.newtabpage.activity-stream.telemetry";
    if (
      Services.prefs.getPrefType(AStelemetryPref) ===
      Services.prefs.PREF_INVALID
    ) {
      Services.prefs
        .getDefaultBranch("")
        .setBoolPref(AStelemetryPref, AppConstants.MOZILLA_OFFICIAL);
    }

    // More initialization happens here
    this.toggleActivityStream(true);
    this.initialized = true;

    Services.obs.addObserver(
      this,
      lazy.TelemetryReportingPolicy.TELEMETRY_TOU_ACCEPTED_OR_INELIGIBLE
    );
  },

  /**
   * React to changes to the activity stream being enabled or not.
   *
   * This will only act if there is a change of state and if not overridden.
   *
   * @returns {boolean} Returns if there has been a state change
   *
   * @param {boolean}   stateEnabled    activity stream enabled state to set to
   * @param {boolean}   forceState      force state change
   */
  toggleActivityStream(stateEnabled, forceState = false) {
    if (
      !forceState &&
      (this._newTabURLOverridden ||
        stateEnabled === this._activityStreamEnabled)
    ) {
      // exit there is no change of state
      return false;
    }
    if (stateEnabled) {
      this._activityStreamEnabled = true;
    } else {
      this._activityStreamEnabled = false;
    }

    this._newTabURL = ABOUT_URL;
    return true;
  },

  get newTabURL() {
    return this._newTabURL;
  },

  set newTabURL(aNewTabURL) {
    let newTabURL = aNewTabURL.trim();
    if (newTabURL === ABOUT_URL) {
      // avoid infinite redirects in case one sets the URL to about:newtab
      this.resetNewTabURL();
      return;
    } else if (newTabURL === "") {
      newTabURL = "about:blank";
    }

    this.toggleActivityStream(false);
    this._newTabURL = newTabURL;
    this._newTabURLOverridden = true;
    this.notifyChange();
  },

  get newTabURLOverridden() {
    return this._newTabURLOverridden;
  },

  get activityStreamEnabled() {
    return this._activityStreamEnabled;
  },

  resetNewTabURL() {
    this._newTabURLOverridden = false;
    this._newTabURL = ABOUT_URL;
    this.toggleActivityStream(true, true);
    this.notifyChange();
  },

  notifyChange() {
    Services.obs.notifyObservers(null, "newtab-url-changed", this._newTabURL);
  },

  /**
   * onBrowserReady - Continues the initialization of Activity Stream after browser is ready.
   */
  async onBrowserReady() {
    if (this.activityStream && this.activityStream.initialized) {
      return;
    }

    // We want to block newtab startup on two things:
    //  - The built-in addon has reported that it has finished
    //    initializing
    //  - The TRAINHOP_NIMBUS_FEATURE feature is up-to-date and ready to
    //    read.
    //
    // That way, when the various feeds initialize, they can be certain that
    // the addon has finished registering its resources, and that the
    // trainhopConfig value computed in PrefsFeed is ready to be read.

    const nimbusFeature = lazy.NimbusFeatures[TRAINHOP_NIMBUS_FEATURE];
    const trainhopFeatureReady = nimbusFeature.ready();

    let redirector = Cc[
      "@mozilla.org/network/protocol/about;1?what=newtab"
    ].getService(Ci.nsIAboutModule).wrappedJSObject;

    const addonInitted = redirector.promiseBuiltInAddonInitialized;

    // The ProfileAge function itself returns a Promise, which resolves to the
    // underlying accessor, and the created getter on that accessor also returns
    // a Promise. This construct gives us a Promise that ultimately resolves
    // to the created timestamp.
    const profileCreatedAccessorReady = lazy
      .ProfileAge()
      .then(accessor => accessor.created);

    const [createdTimestamp] = await Promise.all([
      profileCreatedAccessorReady,
      trainhopFeatureReady,
      addonInitted,
    ]);
    const createdInstant = createdTimestamp
      ? Temporal.Instant.fromEpochMilliseconds(createdTimestamp)
      : null;

    lazy.AboutNewTabResourceMapping.scheduleUpdateTrainhopAddonState();

    try {
      this.activityStream = new lazy.ActivityStream(createdInstant);
      Glean.newtab.activityStreamCtorSuccess.set(true);
      this._activityStreamResolver();
    } catch (error) {
      // Send Activity Stream loading failure telemetry
      // This probe will help to monitor if ActivityStream failure has crossed
      // a threshold and send alert. See Bug 1965278
      Glean.newtab.activityStreamCtorSuccess.set(false);
      console.error(error);
      throw error;
    }

    try {
      this.activityStream.init();
      this._subscribeToActivityStream();
    } catch (e) {
      console.error(e);
    }
  },

  _subscribeToActivityStream() {
    const store = this.activityStream.store;
    let unsubscribe = store.subscribe(() => {
      // If the top sites changed, broadcast "newtab-top-sites-changed". We
      // ignore changes to the `screenshot` property in each site because
      // screenshots are generated at times that are hard to predict and it ends
      // up interfering with tests that rely on "newtab-top-sites-changed".
      // Observers likely don't care about screenshots anyway.
      let topSites = store.getState().TopSites.rows.map(site => {
        site = { ...site };
        delete site.screenshot;
        return site;
      });
      if (!lazy.ObjectUtils.deepEqual(topSites, this._cachedTopSites)) {
        this._cachedTopSites = topSites;
        Services.obs.notifyObservers(null, "newtab-top-sites-changed");
      }
    });
    this._unsubscribeFromActivityStream = () => {
      try {
        unsubscribe();
      } catch (e) {
        console.error(e);
      }
    };
  },

  /**
   * uninit - Uninitializes Activity Stream if it exists.
   */
  uninit() {
    if (this.activityStream) {
      this._unsubscribeFromActivityStream?.();
      this.activityStream.uninit();
      this.activityStream = null;
    }
    try {
      Services.obs.removeObserver(this, TOPIC_APP_QUIT);
      Services.obs.removeObserver(
        this,
        lazy.TelemetryReportingPolicy.TELEMETRY_TOU_ACCEPTED_OR_INELIGIBLE
      );
    } catch (e) {
      // If init failed before registering these observers, removeObserver may throw.
      // Safe to ignore during shutdown.
    }

    this.initialized = false;
  },

  getTopSites() {
    return this.activityStream
      ? this.activityStream.store.getState().TopSites.rows
      : [];
  },

  /**
   * The id of the newtab visit a browser is showing, which the `newtab` ping
   * reports as `newtab_visit_id`.
   *
   * @param {MozBrowser} browser
   *   The browser to look up.
   * @returns {?string}
   *   The visit id, or null if the browser isn't showing a newtab page or the
   *   visit isn't being measured.
   */
  getVisitId(browser) {
    let portID = lazy.AboutNewTabParent.loadedTabs.get(browser)?.portID;
    if (!portID) {
      return null;
    }
    let telemetryFeed = this.activityStream?.store.feeds.get("feeds.telemetry");
    return telemetryFeed?.sessions.get(portID)?.session_id ?? null;
  },

  _alreadyRecordedTopsitesPainted: false,
  _nonDefaultStartup: false,

  noteNonDefaultStartup() {
    this._nonDefaultStartup = true;
  },

  maybeRecordTopsitesPainted(timestamp) {
    if (this._alreadyRecordedTopsitesPainted || this._nonDefaultStartup) {
      return;
    }

    let startupInfo = Services.startup.getStartupInfo();
    let processStartTs = startupInfo.process.getTime();
    let delta = Math.round(timestamp - processStartTs);
    Glean.timestamps.aboutHomeTopsitesFirstPaint.set(delta);
    ChromeUtils.addProfilerMarker("aboutHomeTopsitesFirstPaint");
    this._alreadyRecordedTopsitesPainted = true;
  },

  // nsIObserver implementation

  observe(subject, topic) {
    switch (topic) {
      case TOPIC_APP_QUIT: {
        this.uninit();
        break;
      }
      case lazy.TelemetryReportingPolicy.TELEMETRY_TOU_ACCEPTED_OR_INELIGIBLE: {
        Services.obs.removeObserver(
          this,
          lazy.TelemetryReportingPolicy.TELEMETRY_TOU_ACCEPTED_OR_INELIGIBLE
        );

        // Avoid running synchronously during this event that's used for timing
        Services.tm.dispatchToMainThread(() => this.onBrowserReady());
        break;
      }
    }
  },
};
