use mozprofile::preferences::Pref;

// ALL CHANGES TO THIS FILE MUST HAVE REVIEW FROM A GECKODRIVER PEER!
//
// All preferences in this file are not immediately effective, and
// require a restart of Firefox, or have to be set in the profile before
// Firefox gets started the first time. If a preference has to be added,
// which is immediately effective, it needs to be done in Marionette
// (marionette.js).
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
        // !!! For backward compatibility up to Firefox 64. Only remove
        // when this Firefox version is no longer supported by geckodriver !!!
        ("app.update.auto", Pref::new(false)),

        // Enable the dump function, which sends messages to the system
        // console
        ("browser.dom.window.dump.enabled", Pref::new(true)),
        ("devtools.console.stdout.chrome", Pref::new(true)),

        // Disable safebrowsing components
        ("browser.safebrowsing.blockedURIs.enabled", Pref::new(false)),
        ("browser.safebrowsing.downloads.enabled", Pref::new(false)),
        ("browser.safebrowsing.passwords.enabled", Pref::new(false)),
        ("browser.safebrowsing.malware.enabled", Pref::new(false)),
        ("browser.safebrowsing.phishing.enabled", Pref::new(false)),

        // Do not restore the last open set of tabs if the browser crashed
        ("browser.sessionstore.resume_from_crash", Pref::new(false)),

        // Skip check for default browser on startup
        ("browser.shell.checkDefaultBrowser", Pref::new(false)),

        // Do not redirect user when a milestone upgrade of Firefox
        // is detected
        ("browser.startup.homepage_override.mstone", Pref::new("ignore")),

        // Start with a blank page (about:blank)
        ("browser.startup.page", Pref::new(0)),

        // Do not close the window when the last tab gets closed
        // TODO: Remove once minimum supported Firefox release is 61.
        ("browser.tabs.closeWindowWithLastTab", Pref::new(false)),

        // Do not warn when closing all open tabs
        // TODO: Remove once minimum supported Firefox release is 61.
        ("browser.tabs.warnOnClose", Pref::new(false)),

        // Disable the UI tour
        ("browser.uitour.enabled", Pref::new(false)),

        // Do not warn on quitting Firefox
        ("browser.warnOnQuit", Pref::new(false)),

        // Do not show datareporting policy notifications which can
        // interfere with tests
        ("datareporting.healthreport.about.reportUrl", Pref::new("http://%(server)s/dummy/abouthealthreport/")),  // removed in Firefox 59
        ("datareporting.healthreport.documentServerURI", Pref::new("http://%(server)s/dummy/healthreport/")),
        ("datareporting.healthreport.logging.consoleEnabled", Pref::new(false)),
        ("datareporting.healthreport.service.enabled", Pref::new(false)),
        ("datareporting.healthreport.service.firstRun", Pref::new(false)),
        ("datareporting.healthreport.uploadEnabled", Pref::new(false)),
        ("datareporting.policy.dataSubmissionEnabled", Pref::new(false)),
        ("datareporting.policy.dataSubmissionPolicyAccepted", Pref::new(false)),
        ("datareporting.policy.dataSubmissionPolicyBypassNotification", Pref::new(true)),

        // Disable the ProcessHangMonitor
        ("dom.ipc.reportProcessHangs", Pref::new(false)),

        // Only load extensions from the application and user profile
        // AddonManager.SCOPE_PROFILE + AddonManager.SCOPE_APPLICATION
        ("extensions.autoDisableScopes", Pref::new(0)),
        ("extensions.enabledScopes", Pref::new(5)),

        // Disable intalling any distribution extensions or add-ons
        ("extensions.installDistroAddons", Pref::new(false)),

        // Disable extensions compatibility dialogue.
        // TODO: Remove once minimum supported Firefox release is 61.
        ("extensions.showMismatchUI", Pref::new(false)),

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

        // Show chrome errors and warnings in the error console
        ("javascript.options.showInConsole", Pref::new(true)),

        // Disable download and usage of OpenH264, and Widevine plugins
        ("media.gmp-manager.updateEnabled", Pref::new(false)),

        // Do not prompt with long usernames or passwords in URLs
        // TODO: Remove once minimum supported Firefox release is 61.
        ("network.http.phishy-userpass-length", Pref::new(255)),

        // Do not automatically switch between offline and online
        ("network.manage-offline-status", Pref::new(false)),

        // Make sure SNTP requests do not hit the network
        ("network.sntp.pools", Pref::new("%(server)s")),

        // Disable Flash.  The plugin container it is run in is
        // causing problems when quitting Firefox from geckodriver,
        // c.f. https://github.com/mozilla/geckodriver/issues/225.
        ("plugin.state.flash", Pref::new(0)),

        // Don't do network connections for mitm priming
        ("security.certerrors.mitm.priming.enabled", Pref::new(false)),

        // Ensure blocklist updates don't hit the network
        ("services.settings.server", Pref::new("http://%(server)s/dummy/blocklist/")),

        // Disable first run pages
        ("startup.homepage_welcome_url", Pref::new("about:blank")),
        ("startup.homepage_welcome_url.additional", Pref::new("")),

        // Prevent starting into safe mode after application crashes
        ("toolkit.startup.max_resumed_crashes", Pref::new(-1)),

        // We want to collect telemetry, but we don't want to send in the results
        ("toolkit.telemetry.server", Pref::new("https://%(server)s/dummy/telemetry/")),
    ];
}
