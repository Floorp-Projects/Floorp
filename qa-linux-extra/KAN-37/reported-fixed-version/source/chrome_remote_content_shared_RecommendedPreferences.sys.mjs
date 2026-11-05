/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  Log: "chrome://remote/content/shared/Log.sys.mjs",
});

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "useRecommendedPrefs",
  "remote.prefs.recommended",
  false
);

ChromeUtils.defineLazyGetter(lazy, "logger", () => lazy.Log.get());

// Ensure we are in the parent process.
if (Services.appinfo.processType != Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT) {
  throw new Error(
    "RecommendedPreferences should only be loaded in the parent process"
  );
}

// ALL CHANGES TO THIS LIST MUST HAVE REVIEW FROM A WEBDRIVER PEER!
//
// Preferences are set for automation on startup, unless
// remote.prefs.recommended has been set to false.
//
// Note: Clients do not always use the latest version of the application. As
// such backward compatibility has to be ensured at least for the last three
// releases.

// INSTRUCTIONS TO ADD A NEW PREFERENCE
//
// Preferences for remote control and automation can be set from several entry
// points:
// - remote/shared/RecommendedPreferences.sys.mjs
// - remote/test/puppeteer/packages/browsers/src/browser-data/firefox.ts
// - testing/geckodriver/src/prefs.rs
// - testing/marionette/client/marionette_driver/geckoinstance.py
// - testing/profiles/
//
// The preferences in `firefox.ts`, `prefs.rs` and `geckoinstance.py`
// will be applied before the application starts, and should typically be used
// for preferences which cannot be updated during the lifetime of the application.
//
// The preferences in `RecommendedPreferences.sys.mjs` are applied after
// the application has started, which means that the application must apply this
// change dynamically and behave correctly. Note that you can also define
// protocol specific preferences (WebDriver, ...) which are merged with the
// COMMON_PREFERENCES from `RecommendedPreferences.sys.mjs`.
//
// Additionally, users relying on the Marionette Python client (ie. using
// geckoinstance.py) set `remote.prefs.recommended = false`. This means that
// preferences from `RecommendedPreferences.sys.mjs` are not applied and have to
// be added to the list of preferences in that Python file. Note that there are
// several lists of preferences, either common or specific to a given application
// (Firefox Desktop, Fennec, Thunderbird).
//
// Some test types may disable recommended preferences. Search for
// `"remote.prefs.recommended", false` in `/testing/profiles` to find
// `user.js` files that do so.
//
// Depending on how users interact with the Remote Agent, they will use different
// combinations of preferences. So it's important to update the preferences files
// so that all users have the proper preferences.
//
// When adding a new preference, follow this guide to decide where to add it:
// - Add the preference to `geckoinstance.py`
// - If the preference has to be set before startup:
//   - Add the preference to `prefs.rs`
//   - Add the preference `browser-data/firefox.ts` in the puppeteer folder
//   - Create a PR to upstream the change on `browser-data/firefox.ts` to puppeteer
// - Otherwise, if the preference can be set after startup:
//   - Add the preference to `RecommendedPreferences.sys.mjs`
// - If a `user.js` file in `/testing/profiles` disables recommended preferences,
//   identify affected test suites and set the new preference where needed.
const COMMON_PREFERENCES = new Map([
  // Make sure Shield doesn't hit the network.
  ["app.normandy.api_url", ""],

  // Disable automatically upgrading Firefox
  //
  // Note: This preference should have already been set by the client when
  // creating the profile. But if not and to absolutely make sure that updates
  // of Firefox aren't downloaded and applied, enforce its presence.
  ["app.update.disabledForTesting", true],

  // Disable scroll axis lock, WebDriver should be able to scroll arbitrary
  // directions.
  ["apz.axis_lock.mode", 0],

  // Increase the APZ content response timeout in tests to 1 minute.
  // This is to accommodate the fact that test environments tends to be
  // slower than production environments (with the b2g emulator being
  // the slowest of them all), resulting in the production timeout value
  // sometimes being exceeded and causing false-positive test failures.
  //
  // (bug 1176798, bug 1177018, bug 1210465)
  ["apz.content_response_timeout", 60000],

  // Disable the profile backup service.
  ["browser.backup.enabled", false],

  // Don't show the content blocking introduction panel.
  // We use a larger number than the default 22 to have some buffer
  // This can be removed once Firefox 69 and 68 ESR and are no longer supported.
  ["browser.contentblocking.introCount", 99],

  // Disable extension discovery
  ["browser.discovery.enabled", false],

  // Set global `dump` function to log strings to `stdout` for release builds as well.
  ["browser.dom.window.dump.enabled", true],

  // Don't open the downloads panel every time a download begins.
  // The first download ever run in a new profile will still open the panel,
  // but because "browser.download.panel.shown" is set to true,
  // this preference is going to act as the first download already happened.
  ["browser.download.alwaysOpenPanel", false],

  // Indicate that the download panel has been shown once so that
  // whichever download test runs first doesn't show the popup
  // inconsistently.
  ["browser.download.panel.shown", true],

  // Make sure error page is not shown for blank pages with 4xx or 5xx response code
  ["browser.http.blank_page_with_error_response.enabled", true],

  // Disable all machine learning features by default
  ["browser.ml.enable", false],

  // Disable CFR features for automated tests.
  ["browser.newtabpage.activity-stream.asrouter.userprefs.cfr.features", false],

  // Do not initialize any activitystream features
  ["browser.newtabpage.activity-stream.testing.shouldInitializeFeeds", false],

  // Make sure Topsites doesn't hit the network to retrieve sponsored tiles.
  ["browser.newtabpage.activity-stream.showSponsoredTopSites", false],

  // Background thumbnails in particular cause grief, and disabling
  // thumbnails in general cannot hurt
  ["browser.pagethumbnails.capturing_disabled", true],

  // Do not show the preonboarding modal/splash which can interfere with tests
  ["browser.preonboarding.enabled", false],

  // Disable geolocation ping(#1)
  ["browser.region.network.url", ""],

  // Disable safebrowsing components.
  //
  // These should also be set in the profile prior to starting Firefox,
  // as it is picked up at runtime.
  ["browser.safebrowsing.blockedURIs.enabled", false],
  ["browser.safebrowsing.downloads.enabled", false],
  ["browser.safebrowsing.malware.enabled", false],
  ["browser.safebrowsing.phishing.enabled", false],

  // Disable updates to search engines.
  //
  // Should be set in profile.
  ["browser.search.update", false],

  // Do not restore the last open set of tabs if the browser has crashed
  ["browser.sessionstore.resume_from_crash", false],

  // Don't check for the default web browser during startup.
  //
  // These should also be set in the profile prior to starting Firefox,
  // as it is picked up at runtime.
  ["browser.shell.checkDefaultBrowser", false],

  // Disable session restore infobar
  ["browser.startup.couldRestoreSession.count", -1],

  // Do not redirect user when a milstone upgrade of Firefox is detected
  ["browser.startup.homepage_override.mstone", "ignore"],

  // Unload the previously selected tab immediately
  ["browser.tabs.remote.unloadDelayMs", 0],

  // Don't unload tabs when available memory is running low
  ["browser.tabs.unloadOnLowMemory", false],

  // Do not warn when closing all open tabs
  ["browser.tabs.warnOnClose", false],

  // Do not warn when closing all other open tabs
  ["browser.tabs.warnOnCloseOtherTabs", false],

  // Do not warn when multiple tabs will be opened
  ["browser.tabs.warnOnOpen", false],

  // Don't show the Bookmarks Toolbar on any tab (the above pref that
  // disables the New Tab Page ends up showing the toolbar on about:blank).
  ["browser.toolbars.bookmarks.visibility", "never"],

  // Make sure Topsites doesn't hit the network to retrieve tiles from Contile.
  ["browser.topsites.contile.enabled", false],

  // Disable translations
  ["browser.translations.enable", false],

  // Disable first run splash page on Windows 10
  ["browser.usedOnWindows10.introURL", ""],

  // Turn off Merino suggestions in the location bar so as not to trigger
  // network connections.
  ["browser.urlbar.merino.endpointURL", ""],
  ["browser.urlbar.merino.ohttpConfigURL", ""],
  ["browser.urlbar.merino.ohttpRelayURL", ""],

  // Turn off search suggestions in the location bar so as not to trigger
  // network connections.
  ["browser.urlbar.suggest.searches", false],

  // Do not warn on quitting Firefox
  ["browser.warnOnQuit", false],

  // Do not show datareporting policy notifications which can
  // interfere with tests
  [
    "datareporting.healthreport.documentServerURI",
    "http://%(server)s/dummy/healthreport/",
  ],
  ["datareporting.healthreport.logging.consoleEnabled", false],
  ["datareporting.healthreport.service.enabled", false],
  ["datareporting.healthreport.service.firstRun", false],
  ["datareporting.healthreport.uploadEnabled", false],
  ["datareporting.policy.dataSubmissionEnabled", false],
  ["datareporting.policy.dataSubmissionPolicyAccepted", false],
  ["datareporting.policy.dataSubmissionPolicyBypassNotification", true],
  ["datareporting.usage.uploadEnabled", false],
  ["telemetry.fog.test.localhost_port", -1],

  // Disable popup-blocker
  ["dom.disable_open_during_load", false],

  // Enabling the support for File object creation in the content process
  ["dom.file.createInChild", true],

  // Disable delayed user input event handling
  ["dom.input_events.security.minNumTicks", 0],
  ["dom.input_events.security.minTimeElapsedInMS", 0],

  // Disable the ProcessHangMonitor
  ["dom.ipc.reportProcessHangs", false],

  // Disable the QoS manager on MacOS and the priority manager on all other
  // platforms to not cause stalled processes in background tabs when the
  // overall CPU load on the machine is high.
  //
  // TODO: Should be considered to get removed once bug 1960741 is fixed.
  ["threads.lower_mainthread_priority_in_background.enabled", false],
  ["dom.ipc.processPriorityManager.enabled", false],

  // Disable slow script dialogues
  ["dom.max_chrome_script_run_time", 0],
  ["dom.max_script_run_time", 0],

  // Disable location change rate limitation
  ["dom.navigation.navigationRateLimit.count", 0],

  // Disable system permission checks for navigator.permissions.query
  ["dom.permissions.testing.enabled", true],

  // DOM Push
  ["dom.push.connection.enabled", false],

  // Screen Orientation API
  ["dom.screenorientation.allow-lock", true],

  // Disable dialog abuse if alerts are triggered too quickly.
  ["dom.successive_dialog_time_limit", 0],

  // Only load extensions from the application and user profile
  // AddonManager.SCOPE_PROFILE + AddonManager.SCOPE_APPLICATION
  //
  // Should be set in profile.
  ["extensions.autoDisableScopes", 0],
  ["extensions.enabledScopes", 5],

  // Disable form autofill for extensions and credit cards
  ["extensions.formautofill.addresses.enabled", false],
  ["extensions.formautofill.creditCards.enabled", false],

  // Disable metadata caching for installed add-ons by default
  ["extensions.getAddons.cache.enabled", false],

  // Disable installing any distribution extensions or add-ons.
  // Should be set in profile.
  ["extensions.installDistroAddons", false],

  // Turn off extension updates so they do not bother tests
  ["extensions.update.enabled", false],
  ["extensions.update.notifyUser", false],

  // Make sure opening about:addons will not hit the network
  ["extensions.getAddons.discovery.api_url", "data:, "],

  // Redirect various extension update URLs
  [
    "extensions.blocklist.detailsURL",
    "http://%(server)s/extensions-dummy/blocklistDetailsURL",
  ],
  [
    "extensions.blocklist.itemURL",
    "http://%(server)s/extensions-dummy/blocklistItemURL",
  ],
  ["extensions.hotfix.url", "http://%(server)s/extensions-dummy/hotfixURL"],
  ["extensions.systemAddon.update.enabled", false],
  [
    "extensions.update.background.url",
    "http://%(server)s/extensions-dummy/updateBackgroundURL",
  ],
  ["extensions.update.url", "http://%(server)s/extensions-dummy/updateURL"],

  // Make sure opening about: addons won't hit the network
  ["extensions.getAddons.discovery.api_url", "data:, "],
  [
    "extensions.getAddons.get.url",
    "http://%(server)s/extensions-dummy/repositoryGetURL",
  ],
  [
    "extensions.getAddons.search.browseURL",
    "http://%(server)s/extensions-dummy/repositoryBrowseURL",
  ],

  // Allow the application to have focus even it runs in the background
  ["focusmanager.testmode", true],

  // Disable useragent updates
  ["general.useragent.updates.enabled", false],

  // Do not open system settings when geolocation is requested without OS permission
  ["geo.prompt.open_system_prefs", false],

  // Disable geolocation ping(#2)
  ["geo.provider.network.url", ""],

  // Always use network provider for geolocation tests so we bypass the
  // macOS dialog raised by the corelocation provider
  ["geo.provider.testing", true],

  // Do not scan Wifi
  ["geo.wifi.scan", false],

  // Disable Firefox accounts ping
  ["identity.fxaccounts.auth.uri", "https://{server}/dummy/fxa"],

  // Allow scroll amount larger than one page on a single mouse wheel event.
  ["mousewheel.allow_scrolling_more_than_one_page", true],

  // Disable captive portal service
  ["network.captive-portal-service.enabled", false],

  // Disable connectivity service pings
  ["network.connectivity-service.enabled", false],

  // Do not prompt for temporary redirects
  ["network.http.prompt-temp-redirect", false],

  // Do not automatically switch between offline and online
  ["network.manage-offline-status", false],

  // Make sure SNTP requests do not hit the network
  ["network.sntp.pools", "%(server)s"],

  // Turn off semantic history search as it triggers network connections to
  // download ML models.
  ["places.semanticHistory.featureGate", false],

  // Privacy and Tracking Protection
  ["privacy.trackingprotection.enabled", false],
  ["privacy.trackingprotection.pbmode.enabled", false],

  // Used to check if recommended preferences are applied
  ["remote.prefs.recommended.applied", true],

  // Don't do network connections for mitm priming
  ["security.certerrors.mitm.priming.enabled", false],

  // Local documents have access to all other local documents,
  // including directory listings
  ["security.fileuri.strict_origin_policy", false],

  // Tests do not wait for the notification button security delay
  ["security.notification_enable_delay", 0],

  // Do not download intermediate certificates
  ["security.remote_settings.intermediates.enabled", false],

  // Disable the WebAuthn consents prompt
  ["security.webauthn.related_origin_requests_mode", 1],

  // Disable logging for remote settings
  ["services.settings.loglevel", "off"],

  // Ensure remote settings do not hit the network
  ["services.settings.server", "data:,#remote-settings-dummy/v1"],

  // Do not automatically fill sign-in forms with known usernames and
  // passwords
  ["signon.autofillForms", false],

  // Disable alerts for credential issues
  ["signon.management.page.breach-alerts.enabled", false],
  ["signon.management.page.vulnerable-passwords.enabled", false],

  // Disable password capture, so that tests that include forms are not
  // influenced by the presence of the persistent doorhanger notification
  ["signon.rememberSignons", false],

  // Disable first-run welcome page
  ["startup.homepage_welcome_url", "about:blank"],
  ["startup.homepage_welcome_url.additional", ""],

  // Do not show TOU new user modal which can interfere with tests
  ["termsofuse.bypassNotification", true],

  // Prevent starting into safe mode after application crashes
  ["toolkit.startup.max_resumed_crashes", -1],

  // Disable all telemetry pings
  ["toolkit.telemetry.server", "https://%(server)s/telemetry-dummy/"],

  // Disable window occlusion on Windows, which can prevent webdriver commands
  // such as WebDriver:FindElements from working properly (Bug 1802473).
  ["widget.windows.window_occlusion_tracking.enabled", false],
]);

export const RecommendedPreferences = {
  alteredPrefs: new Set(),

  isInitialized: false,

  /**
   * Apply the provided map of preferences.
   *
   * Note, that they will be automatically reset on application shutdown.
   *
   * @param {Map<string, object>=} preferences
   *     Map of preference name to preference value.
   */
  applyPreferences(preferences = new Map()) {
    if (!lazy.useRecommendedPrefs) {
      // If remote.prefs.recommended is set to false, do not set any preference
      // here. Needed for our Firefox CI.
      return;
    }

    // Only apply common recommended preferences on first call to
    // applyPreferences.
    if (!this.isInitialized) {
      // Merge common preferences and optionally provided preferences in a
      // single map. Hereby the extra preferences have higher priority.
      preferences = new Map([...COMMON_PREFERENCES, ...preferences]);

      Services.obs.addObserver(this, "xpcom-shutdown");
      this.isInitialized = true;
    }

    for (const [k, v] of preferences) {
      if (!Services.prefs.prefHasUserValue(k)) {
        lazy.logger.debug(`Setting recommended pref ${k} to ${v}`);

        switch (typeof v) {
          case "string":
            Services.prefs.setStringPref(k, v);
            break;
          case "boolean":
            Services.prefs.setBoolPref(k, v);
            break;
          case "number":
            Services.prefs.setIntPref(k, v);
            break;
          default:
            throw new TypeError(`Invalid preference type: ${typeof v}`);
        }

        // Keep track all the altered preferences to restore them on
        // xpcom-shutdown.
        this.alteredPrefs.add(k);
      }
    }
  },

  observe(subject, topic) {
    if (topic === "xpcom-shutdown") {
      this.restoreAllPreferences();
    }
  },

  /**
   * Restore all the altered preferences.
   */
  restoreAllPreferences() {
    this.restorePreferences(this.alteredPrefs);
    if (this.isInitialized) {
      Services.obs.removeObserver(this, "xpcom-shutdown");
    }
    this.isInitialized = false;
  },

  /**
   * Restore provided preferences.
   *
   * @param {Map} preferences
   *     Map of preferences that should be restored.
   */
  restorePreferences(preferences) {
    for (const k of preferences.keys()) {
      lazy.logger.debug(`Resetting recommended pref ${k}`);
      Services.prefs.clearUserPref(k);
      this.alteredPrefs.delete(k);
    }
  },
};
