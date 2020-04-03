// Base preferences file used by the mochitest
/* globals user_pref */
/* eslint quotes: 0 */

// XXX: Bug 1617611 - Fix all the tests broken by "cookies sameSite=lax by default"
user_pref("network.cookie.sameSite.laxByDefault", false);

// Always run in e10s
user_pref("browser.tabs.remote.autostart", true);
