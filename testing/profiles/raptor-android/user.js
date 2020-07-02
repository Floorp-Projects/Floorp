// Preferences file used by the raptor harness exclusively on android
/* globals user_pref */

// disk cache smart size is enabled in shipped apps
user_pref("browser.cache.disk.smart_size.enabled", true);

// Raptor's manifest.json contains the "geckoProfiler" permission, which is
// not supported on Android. Ignore the warning about the unknown permission.
user_pref("extensions.webextensions.warnings-as-errors", false);
// disable telemetry bug 1639148
user_pref("toolkit.telemetry.server", "");
