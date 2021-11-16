// Base preferences file used by the mochitest
/* globals user_pref */
/* eslint quotes: 0 */

// XXX: Bug 1617611 - Fix all the tests broken by "cookies SameSite=lax by default"
user_pref("network.cookie.sameSite.laxByDefault", false);

// Enable blocking access to storage from tracking resources by default.
// We don't want to run mochitest using BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN (5 - aka Dynamic First Party Isolation) yet.
user_pref("network.cookie.cookieBehavior", 4);

// Don't restore the last open set of tabs if the browser has crashed
// or if the profile folder is re-used after an exit(0) fast shutdown.
user_pref("browser.sessionstore.resume_from_crash", false);

// Don't enable paint suppression when the background is unknown. While paint
// is suppressed, synthetic click events and co. go to the old page, which can
// be confusing for tests that send click events before the first paint.
user_pref("nglayout.initialpaint.unsuppress_with_no_background", true);
