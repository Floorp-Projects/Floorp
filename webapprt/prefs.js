/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

pref("browser.chromeURL", "chrome://webapprt/content/webapp.xul");
pref("browser.download.folderList", 1);

// Disable all add-on locations other than the profile (which can't be disabled this way)
pref("extensions.enabledScopes", 1);
// Auto-disable any add-ons that are "dropped in" to the profile
pref("extensions.autoDisableScopes", 1);
// Disable add-on installation via the web-exposed APIs
pref("xpinstall.enabled", false);
// Disable installation of distribution add-ons
pref("extensions.installDistroAddons", false);
// Disable the add-on compatibility dialog
pref("extensions.showMismatchUI", false);

// Whether or not we've ever run.  We use this to set permissions on firstrun.
pref("webapprt.firstrun", false);

// Set reportURL for crashes
pref("breakpad.reportURL", "https://crash-stats.mozilla.com/report/index/");

// Blocklist preferences
pref("extensions.blocklist.enabled", true);
pref("extensions.blocklist.interval", 86400);
// Controls what level the blocklist switches from warning about items to forcibly
// blocking them.
pref("extensions.blocklist.level", 2);
pref("extensions.blocklist.url", "https://addons.mozilla.org/blocklist/3/%APP_ID%/%APP_VERSION%/%PRODUCT%/%BUILD_ID%/%BUILD_TARGET%/%LOCALE%/%CHANNEL%/%OS_VERSION%/%DISTRIBUTION%/%DISTRIBUTION_VERSION%/%PING_COUNT%/%TOTAL_PING_COUNT%/%DAYS_SINCE_LAST_PING%/");
pref("extensions.blocklist.detailsURL", "https://www.mozilla.com/%LOCALE%/blocklist/");
pref("extensions.blocklist.itemURL", "https://addons.mozilla.org/%LOCALE%/%APP%/blocked/%blockID%");

pref("full-screen-api.enabled", true);

// IndexedDB
pref("dom.indexedDB.enabled", true);
pref("indexedDB.feature.enabled", true);
pref("dom.indexedDB.warningQuota", 50);

// Offline cache prefs
pref("browser.offline-apps.notify", false);
pref("browser.cache.offline.enable", true);
pref("offline-apps.allow_by_default", true);

// TCPSocket
pref("dom.mozTCPSocket.enabled", true);

// Enable smooth scrolling
pref("general.smoothScroll", true);

// Enable window resize and move
pref("dom.always_allow_move_resize_window", true);

pref("plugin.allowed_types", "application/x-shockwave-flash,application/futuresplash");

// The default for this pref reflects whether the build is capable of IPC.
// (Turning it on in a no-IPC build will have no effect.)
#ifdef XP_MACOSX
// i386 ipc preferences
pref("dom.ipc.plugins.enabled.i386", false);
pref("dom.ipc.plugins.enabled.i386.flash player.plugin", true);
// x86_64 ipc preferences
pref("dom.ipc.plugins.enabled.x86_64", true);
#else
pref("dom.ipc.plugins.enabled", true);
#endif

