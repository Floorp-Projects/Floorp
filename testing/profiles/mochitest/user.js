// Base preferences file used by the mochitest
/* globals user_pref */
/* eslint quotes: 0 */

// XXX: Bug 1617611 - Fix all the tests broken by "cookies sameSite=lax by default"
user_pref("network.cookie.sameSite.laxByDefault", false);

// Enable blocking access to storage from tracking resources by default.
// We don't want to run mochitest using BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN (5 - aka Dynamic First Party Isolation) yet.
user_pref("network.cookie.cookieBehavior", 4);
