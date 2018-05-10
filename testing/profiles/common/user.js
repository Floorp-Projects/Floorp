// Base preferences file used by both unittest and perf harnesses.
/* globals user_pref */
user_pref("app.update.enabled", false);
user_pref("browser.EULA.override", true);
// Disable Bookmark backups by default.
user_pref("browser.bookmarks.max_backups", 0);
user_pref("browser.dom.window.dump.enabled", true);
// Use an empty list of sites to avoid fetching
user_pref("browser.newtabpage.activity-stream.default.sites", "");
user_pref("browser.newtabpage.activity-stream.feeds.section.topstories", false);
user_pref("browser.newtabpage.activity-stream.feeds.snippets", false);
user_pref("browser.newtabpage.activity-stream.telemetry", false);
user_pref("browser.newtabpage.activity-stream.tippyTop.service.endpoint", "");
user_pref("browser.search.countryCode", "US");
// This will prevent HTTP requests for region defaults.
user_pref("browser.search.geoSpecificDefaults", false);
// Tell the search service we are running in the US.  This also has the desired
// side-effect of preventing our geoip lookup.
user_pref("browser.search.isUS", true);
user_pref("browser.shell.checkDefaultBrowser", false);
// Disable android snippets
user_pref("browser.snippets.enabled", false);
user_pref("browser.snippets.syncPromo.enabled", false);
// Turn off the location bar search suggestions opt-in.  It interferes with
// tests that don't expect it to be there.
user_pref("browser.urlbar.userMadeSearchSuggestionsChoice", true);
user_pref("browser.warnOnQuit", false);
// Disable webapp updates.  Yes, it is supposed to be an integer.
user_pref("browser.webapps.checkForUpdates", 0);
// We do not wish to display datareporting policy notifications as it might
// cause other tests to fail. Tests that wish to test the notification functionality
// should explicitly disable this pref.
user_pref("datareporting.policy.dataSubmissionPolicyBypassNotification", true);
user_pref("devtools.chrome.enabled", false);
user_pref("devtools.debugger.remote-enabled", false);
user_pref("dom.allow_scripts_to_close_windows", true);
user_pref("dom.disable_open_during_load", false);
user_pref("dom.max_chrome_script_run_time", 0);
user_pref("dom.max_script_run_time", 0); // no slow script dialogs
user_pref("dom.send_after_paint_to_content", true);
// Only load extensions from the application and user profile
// AddonManager.SCOPE_PROFILE + AddonManager.SCOPE_APPLICATION
user_pref("extensions.enabledScopes", 5);
user_pref("extensions.legacy.enabled", true);
// Turn off extension updates so they don't bother tests
user_pref("extensions.update.enabled", false);
// Disable useragent updates.
user_pref("general.useragent.updates.enabled", false);
user_pref("hangmonitor.timeout", 0); // no hang monitor
// Make tests run consistently on DevEdition (which has a lightweight theme
// selected by default).
user_pref("lightweightThemes.selectedThemeID", "");
user_pref("media.gmp-manager.updateEnabled", false);
// Don't block old libavcodec libraries when testing, because our test systems
// cannot easily be upgraded.
user_pref("media.libavcodec.allow-obsolete", true);
// Disable speculative connections so they aren't reported as leaking when they're hanging around.
user_pref("network.http.speculative-parallel-limit", 0);
// Set places maintenance far in the future (the maximum time possible in an
// int32_t) to avoid it kicking in during tests. The maintenance can take a
// relatively long time which may cause unnecessary intermittents and slow down
// tests. This, like many things, will stop working correctly in 2038.
user_pref("places.database.lastMaintenance", 2147483647);
// Make enablePrivilege continue to work for test code. :-(
user_pref("security.turn_off_all_security_so_that_viruses_can_take_over_this_computer", true);
user_pref("xpinstall.signatures.required", false);
