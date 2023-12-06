/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use mozprofile::preferences::Pref;

// ALL CHANGES TO THIS FILE MUST HAVE REVIEW FROM A GECKODRIVER PEER!
//
// Please refer to INSTRUCTIONS TO ADD A NEW PREFERENCE in
// remote/shared/RecommendedPreferences.sys.mjs
//
// Note: geckodriver is used out-of-tree with various builds of Firefox.
// Removing a preference from this file will cause regressions,
// so please be careful and get review from a Testing :: geckodriver peer
// before you make any changes to this file.
lazy_static! {
    pub static ref DEFAULT: Vec<(&'static str, Pref)> = vec![
        // Make sure Shield doesn't hit the network.
        ("app.normandy.api_url", Pref::new("")),

        // Disable Firefox old build background check
        ("app.update.checkInstallTime", Pref::new(false)),

        // Disable automatically upgrading Firefox
        //
        // Note: Possible update tests could reset or flip the value to allow
        // updates to be downloaded and applied.
        ("app.update.disabledForTesting", Pref::new(true)),

        // Enable the dump function, which sends messages to the system
        // console
        ("browser.dom.window.dump.enabled", Pref::new(true)),
        ("devtools.console.stdout.chrome", Pref::new(true)),

        // Do not restore the last open set of tabs if the browser crashed
        ("browser.sessionstore.resume_from_crash", Pref::new(false)),

        // Skip check for default browser on startup
        ("browser.shell.checkDefaultBrowser", Pref::new(false)),

        // Do not redirect user when a milestone upgrade of Firefox
        // is detected
        ("browser.startup.homepage_override.mstone", Pref::new("ignore")),

        // Start with a blank page (about:blank)
        ("browser.startup.page", Pref::new(0)),

        // Disable the UI tour
        ("browser.uitour.enabled", Pref::new(false)),

        // Do not warn on quitting Firefox
        ("browser.warnOnQuit", Pref::new(false)),

        // Defensively disable data reporting systems
        ("datareporting.healthreport.documentServerURI", Pref::new("http://%(server)s/dummy/healthreport/")),
        ("datareporting.healthreport.logging.consoleEnabled", Pref::new(false)),
        ("datareporting.healthreport.service.enabled", Pref::new(false)),
        ("datareporting.healthreport.service.firstRun", Pref::new(false)),
        ("datareporting.healthreport.uploadEnabled", Pref::new(false)),

        // Do not show datareporting policy notifications which can
        // interfere with tests
        ("datareporting.policy.dataSubmissionEnabled", Pref::new(false)),
        ("datareporting.policy.dataSubmissionPolicyBypassNotification", Pref::new(true)),

        // Disable the ProcessHangMonitor
        ("dom.ipc.reportProcessHangs", Pref::new(false)),

        // Only load extensions from the application and user profile
        // AddonManager.SCOPE_PROFILE + AddonManager.SCOPE_APPLICATION
        ("extensions.autoDisableScopes", Pref::new(0)),
        ("extensions.enabledScopes", Pref::new(5)),

        // Disable intalling any distribution extensions or add-ons
        ("extensions.installDistroAddons", Pref::new(false)),

        // Turn off extension updates so they do not bother tests
        ("extensions.update.enabled", Pref::new(false)),
        ("extensions.update.notifyUser", Pref::new(false)),

        // Allow the application to have focus even it runs in the
        // background
        ("focusmanager.testmode", Pref::new(true)),

        // Disable useragent updates
        ("general.useragent.updates.enabled", Pref::new(false)),

        // Always use network provider for geolocation tests so we bypass
        // the macOS dialog raised by the corelocation provider
        ("geo.provider.testing", Pref::new(true)),

        // Do not scan wi-fi
        ("geo.wifi.scan", Pref::new(false)),

        // No hang monitor
        ("hangmonitor.timeout", Pref::new(0)),

        // Disable idle-daily notifications to avoid expensive operations
        // that may cause unexpected test timeouts.
        ("idle.lastDailyNotification", Pref::new(-1)),

        // Disable download and usage of OpenH264, and Widevine plugins
        ("media.gmp-manager.updateEnabled", Pref::new(false)),

        // Disable the GFX sanity window
        ("media.sanity-test.disabled", Pref::new(true)),

        // Do not automatically switch between offline and online
        ("network.manage-offline-status", Pref::new(false)),

        // Make sure SNTP requests do not hit the network
        ("network.sntp.pools", Pref::new("%(server)s")),

        // Don't do network connections for mitm priming
        ("security.certerrors.mitm.priming.enabled", Pref::new(false)),

        // Ensure remote settings do not hit the network
        ("services.settings.server", Pref::new("data:,#remote-settings-dummy/v1")),

        // Disable first run pages
        ("startup.homepage_welcome_url", Pref::new("about:blank")),
        ("startup.homepage_welcome_url.additional", Pref::new("")),

        // asrouter expects a plain object or null
        ("browser.newtabpage.activity-stream.asrouter.providers.cfr", Pref::new("null")),
        // TODO: Remove once minimum supported Firefox release is 93.
        ("browser.newtabpage.activity-stream.asrouter.providers.cfr-fxa", Pref::new("null")),

        // TODO: Remove once minimum supported Firefox release is 128.
        ("browser.newtabpage.activity-stream.asrouter.providers.snippets", Pref::new("null")),

        ("browser.newtabpage.activity-stream.asrouter.providers.message-groups", Pref::new("null")),
        ("browser.newtabpage.activity-stream.asrouter.providers.whats-new-panel", Pref::new("null")),
        ("browser.newtabpage.activity-stream.asrouter.providers.messaging-experiments", Pref::new("null")),
        ("browser.newtabpage.activity-stream.feeds.system.topstories", Pref::new(false)),

        // TODO: Remove once minimum supported Firefox release is 128.
        ("browser.newtabpage.activity-stream.feeds.snippets", Pref::new(false)),

        ("browser.newtabpage.activity-stream.tippyTop.service.endpoint", Pref::new("")),
        ("browser.newtabpage.activity-stream.discoverystream.config", Pref::new("[]")),

        // For Activity Stream firstrun page, use an empty string to avoid fetching.
        ("browser.newtabpage.activity-stream.fxaccounts.endpoint", Pref::new("")),

        // Prevent starting into safe mode after application crashes
        ("toolkit.startup.max_resumed_crashes", Pref::new(-1)),

        // Disable webapp updates.
        ("browser.webapps.checkForUpdates", Pref::new(0)),
    ];
}
