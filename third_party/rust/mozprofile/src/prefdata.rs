#![allow(dead_code)]
use preferences::Pref;

lazy_static! {
    pub static ref FIREFOX_PREFERENCES: [(&'static str, Pref); 17] = [
        // Don't automatically update the application
        ("app.update.enabled", Pref::new(false)),
        // Don't restore the last open set of tabs if the browser has crashed
        ("browser.sessionstore.resume_from_crash", Pref::new(false)),
        // Don't check for the default web browser during startup
        ("browser.shell.checkDefaultBrowser", Pref::new(false)),
        // Don't warn on exit when multiple tabs are open
        ("browser.tabs.warnOnClose", Pref::new(false)),
        // Don't warn when exiting the browser
        ("browser.warnOnQuit", Pref::new(false)),
        // Don't send Firefox health reports to the production server
        //"datareporting.healthreport.documentServerURI", "http://%(server)s/healthreport/",
        // Only install add-ons from the profile and the application scope
        // Also ensure that those are not getting disabled.
        // see: https://developer.mozilla.org/en/Installing_extensions
        ("extensions.enabledScopes", Pref::new(5)),
        ("extensions.autoDisableScopes", Pref::new(10)),
        // Don't send the list of installed addons to AMO
        ("extensions.getAddons.cache.enabled", Pref::new(false)),
        // Don't install distribution add-ons from the app folder
        ("extensions.installDistroAddons", Pref::new(false)),
        // Dont' run the add-on compatibility check during start-up
        ("extensions.showMismatchUI", Pref::new(false)),
        // Don't automatically update add-ons
        ("extensions.update.enabled", Pref::new(false)),
        // Don't open a dialog to show available add-on updates
        ("extensions.update.notifyUser", Pref::new(false)),
        // Enable test mode to run multiple tests in parallel
        ("focusmanager.testmode", Pref::new(true)),
        // Enable test mode to not raise an OS level dialog for location sharing
        ("geo.provider.testing", Pref::new(true)),
        // Suppress delay for main action in popup notifications
        ("security.notification_enable_delay", Pref::new(0)),
        // Suppress automatic safe mode after crashes
        ("toolkit.startup.max_resumed_crashes", Pref::new(-1)),
        // Don't report telemetry information
        ("toolkit.telemetry.enabled", Pref::new(false)),
    ];
}
