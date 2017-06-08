use mozprofile::preferences::Pref;

lazy_static! {
    pub static ref DEFAULT: [(&'static str, Pref); 79] = [
        // Disable automatic downloading of new releases
        ("app.update.auto", Pref::new(false)),

        // Disable automatically upgrading Firefox
        ("app.update.enabled", Pref::new(false)),

        // Increase the APZ content response timeout in tests to 1
        // minute.  This is to accommodate the fact that test environments
        // tends to be slower than production environments (with the
        // b2g emulator being the slowest of them all), resulting in the
        // production timeout value sometimes being exceeded and causing
        // false-positive test failures.
        //
        // (bug 1176798, bug 1177018, bug 1210465)
        ("apz.content_response_timeout", Pref::new(60000)),

        // Enable the dump function, which sends messages to the system
        // console
        ("browser.dom.window.dump.enabled", Pref::new(true)),

        // Indicate that the download panel has been shown once so
        // that whichever download test runs first does not show the popup
        // inconsistently
        ("browser.download.panel.shown", Pref::new(true)),

        // Implicitly accept license
        ("browser.EULA.override", Pref::new(true)),

        // use about:blank as new tab page
        ("browser.newtabpage.enabled", Pref::new(false)),

        // Assume the about:newtab pages intro panels have been shown
        // to not depend on which test runs first and happens to open
        // about:newtab
        ("browser.newtabpage.introShown", Pref::new(true)),

        // Never start the browser in offline mode
        ("browser.offline", Pref::new(false)),

        // Background thumbnails in particular cause grief, and disabling
        // thumbnails in general cannot hurt
        ("browser.pagethumbnails.capturing_disabled", Pref::new(true)),

        // Avoid performing Reader Mode intros during tests
        ("browser.reader.detectedFirstArticle", Pref::new(true)),

        // Disable safebrowsing components
        ("browser.safebrowsing.blockedURIs.enabled", Pref::new(false)),
        ("browser.safebrowsing.downloads.enabled", Pref::new(false)),
        ("browser.safebrowsing.enabled", Pref::new(false)),
        ("browser.safebrowsing.forbiddenURIs.enabled", Pref::new(false)),
        ("browser.safebrowsing.malware.enabled", Pref::new(false)),
        ("browser.safebrowsing.phishing.enabled", Pref::new(false)),

        // Disable updates to search engines
        ("browser.search.update", Pref::new(false)),

        // Do not restore the last open set of tabs if the browser crashed
        ("browser.sessionstore.resume_from_crash", Pref::new(false)),

        // Skip check for default browser on startup
        ("browser.shell.checkDefaultBrowser", Pref::new(false)),

        // Do not warn when quitting with multiple tabs
        ("browser.showQuitWarning", Pref::new(false)),

        // Disable Android snippets
        ("browser.snippets.enabled", Pref::new(false)),
        ("browser.snippets.syncPromo.enabled", Pref::new(false)),
        ("browser.snippets.firstrunHomepage.enabled", Pref::new(false)),

        // Do not redirect user when a milestone upgrade of Firefox
        // is detected
        ("browser.startup.homepage_override.mstone", Pref::new("ignore")),

        // Start with a blank page (about:blank)
        ("browser.startup.page", Pref::new(0)),

        // Disable tab animation
        ("browser.tabs.animate", Pref::new(false)),

        // Do not warn when quitting a window with multiple tabs
        ("browser.tabs.closeWindowWithLastTab", Pref::new(false)),

        // Do not allow background tabs to be zombified, otherwise for
        // tests that open additional tabs, the test harness tab itself
        // might get unloaded
        ("browser.tabs.disableBackgroundZombification", Pref::new(false)),

        // Do not warn on exit when multiple tabs are open
        ("browser.tabs.warnOnClose", Pref::new(false)),

        // Do not warn when closing all other open tabs
        ("browser.tabs.warnOnCloseOtherTabs", Pref::new(false)),

        // Do not warn when multiple tabs will be opened
        ("browser.tabs.warnOnOpen", Pref::new(false)),

        // Disable first run splash page on Windows 10
        ("browser.usedOnWindows10.introURL", Pref::new("")),

        // Disable the UI tour
        ("browser.uitour.enabled", Pref::new(false)),

        // Do not warn on quitting Firefox
        ("browser.warnOnQuit", Pref::new(false)),

        // Do not show datareporting policy notifications which can
        // interfere with tests
        ("datareporting.healthreport.about.reportUrl", Pref::new("http://%(server)s/dummy/abouthealthreport/")),
        ("datareporting.healthreport.documentServerURI", Pref::new("http://%(server)s/dummy/healthreport/")),
        ("datareporting.healthreport.logging.consoleEnabled", Pref::new(false)),
        ("datareporting.healthreport.service.enabled", Pref::new(false)),
        ("datareporting.healthreport.service.firstRun", Pref::new(false)),
        ("datareporting.healthreport.uploadEnabled", Pref::new(false)),
        ("datareporting.policy.dataSubmissionEnabled", Pref::new(false)),
        ("datareporting.policy.dataSubmissionPolicyAccepted", Pref::new(false)),
        ("datareporting.policy.dataSubmissionPolicyBypassNotification", Pref::new(true)),

        // Disable popup-blocker
        ("dom.disable_open_during_load", Pref::new(false)),

        // Disable the ProcessHangMonitor
        ("dom.ipc.reportProcessHangs", Pref::new(false)),

        // Disable slow script dialogues
        ("dom.max_chrome_script_run_time", Pref::new(0)),
        ("dom.max_script_run_time", Pref::new(0)),

        // Only load extensions from the application and user profile
        // AddonManager.SCOPE_PROFILE + AddonManager.SCOPE_APPLICATION
        ("extensions.autoDisableScopes", Pref::new(0)),
        ("extensions.enabledScopes", Pref::new(5)),

        // don't block add-ons for e10s
        ("extensions.e10sBlocksEnabling", Pref::new(false)),

        // Disable metadata caching for installed add-ons by default
        ("extensions.getAddons.cache.enabled", Pref::new(false)),

        // Disable intalling any distribution extensions or add-ons
        ("extensions.installDistroAddons", Pref::new(false)),
        ("extensions.showMismatchUI", Pref::new(false)),

        // Turn off extension updates so they do not bother tests
        ("extensions.update.enabled", Pref::new(false)),
        ("extensions.update.notifyUser", Pref::new(false)),

        // Make sure opening about:addons will not hit the network
        ("extensions.webservice.discoverURL", Pref::new("http://%(server)s/dummy/discoveryURL")),

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

        // Show chrome errors and warnings in the error console
        ("javascript.options.showInConsole", Pref::new(true)),

        // Make sure the disk cache does not get auto disabled
        ("network.http.bypass-cachelock-threshold", Pref::new(200000)),

        // Do not prompt with long usernames or passwords in URLs
        ("network.http.phishy-userpass-length", Pref::new(255)),

        // Do not prompt for temporary redirects
        ("network.http.prompt-temp-redirect", Pref::new(false)),

        // Disable speculative connections so they are not reported as
        // leaking when they are hanging around
        ("network.http.speculative-parallel-limit", Pref::new(0)),

        // Do not automatically switch between offline and online
        ("network.manage-offline-status", Pref::new(false)),

        // Make sure SNTP requests do not hit the network
        ("network.sntp.pools", Pref::new("%(server)s")),

        // Disable Flash.  The plugin container it is run in is
        // causing problems when quitting Firefox from geckodriver,
        // c.f. https://github.com/mozilla/geckodriver/issues/225.
        ("plugin.state.flash", Pref::new(0)),

        // Local documents have access to all other local docments,
        // including directory listings.
        ("security.fileuri.strict_origin_policy", Pref::new(false)),

        // Tests don't wait for the notification button security delay
        ("security.notification_enable_delay", Pref::new(0)),

        // Ensure blocklist updates don't hit the network
        ("services.settings.server", Pref::new("http://%(server)s/dummy/blocklist/")),

        // Do not automatically fill sign-in forms with known usernames
        // and passwords
        ("signon.autofillForms", Pref::new(false)),

        // Disable password capture, so that tests that include forms
        // are not influenced by the presence of the persistent doorhanger
        // notification
        ("signon.rememberSignons", Pref::new(false)),

        // Disable first run pages
        ("startup.homepage_welcome_url", Pref::new("about:blank")),
        ("startup.homepage_welcome_url.additional", Pref::new("")),

        // Prevent starting into safe mode after application crashes
        ("toolkit.startup.max_resumed_crashes", Pref::new(-1)),

        // We want to collect telemetry, but we don't want to send in the results
        ("toolkit.telemetry.server", Pref::new("https://%(server)s/dummy/telemetry/")),
    ];
}
