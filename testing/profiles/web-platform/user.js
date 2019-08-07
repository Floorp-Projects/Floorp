// Base preferences file for web-platform-tests.
/* globals user_pref */
// Don't restore the last open set of tabs if the browser has crashed
user_pref("browser.sessionstore.resume_from_crash", false);
// Only install add-ons from the profile and the application scope
// Also ensure that those are not getting disabled.
// see: https://developer.mozilla.org/en/Installing_extensions
user_pref("extensions.autoDisableScopes", 10);
// Don't open a dialog to show available add-on updates
user_pref("extensions.update.notifyUser", false);
// Enable test mode to run multiple tests in parallel
user_pref("focusmanager.testmode", true);
// Enable fake media streams for getUserMedia
user_pref("media.navigator.streams.fake", true);
// Enable pre-fetching of resources
user_pref("network.preload", true);
// Enable direct connection
user_pref("network.proxy.type", 0);
// Web-platform-tests load a lot of URLs very quickly. This puts avoidable and
// unnecessary I/O pressure on the Places DB (measured to be in the
// gigabytes).
user_pref("places.history.enabled", false);
// Suppress automatic safe mode after crashes
user_pref("toolkit.startup.max_resumed_crashes", -1);
// Disable antialiasing for the Ahem font.
user_pref("gfx.font_rendering.ahem_antialias_none", true);
// Disable antiphishing popup
user_pref("network.http.phishy-userpass-length", 255);
// Disable safebrowsing components
user_pref("browser.safebrowsing.blockedURIs.enabled", false);
user_pref("browser.safebrowsing.downloads.enabled", false);
user_pref("browser.safebrowsing.passwords.enabled", false);
user_pref("browser.safebrowsing.malware.enabled", false);
user_pref("browser.safebrowsing.phishing.enabled", false);
// Automatically unload beforeunload alerts
user_pref("dom.disable_beforeunload", true);
// sometime wpt runs test even before the document becomes visible, which would
// delay video.play() and cause play() running in wrong order.
user_pref("media.block-autoplay-until-in-foreground", false);
