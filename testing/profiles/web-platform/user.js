/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Base preferences file for web-platform-tests.
/* globals user_pref */
// Don't use the new tab page but about:blank for opened tabs
user_pref("browser.newtabpage.enabled", false);
// Don't restore the last open set of tabs if the browser has crashed
user_pref("browser.sessionstore.resume_from_crash", false);
// Don't show the Bookmarks Toolbar on any tab (the above pref that
// disables the New Tab Page ends up showing the toolbar on about:blank).
user_pref("browser.toolbars.bookmarks.visibility", "never");
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
// Run the font loader task eagerly for more predictable behavior
user_pref("gfx.font_loader.delay", 0);
user_pref("gfx.font_loader.interval", 0);
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
// Enable implicit keyframes since the common animation interpolation test
// function assumes this is available.
user_pref("dom.animations-api.implicit-keyframes.enabled", true);
// Disable high DPI
user_pref("layout.css.devPixelsPerPx", "1.0")
// sometime wpt runs test even before the document becomes visible, which would
// delay video.play() and cause play() running in wrong order.
user_pref("media.block-autoplay-until-in-foreground", false);
// Disable dark scrollbars as it can be semi-transparent that many reftests
// don't expect.
user_pref("widget.disable-dark-scrollbar", true);
// Don't enable paint suppression when the background is unknown. While paint
// is suppressed, synthetic click events and co. go to the old page, which can
// be confusing for tests that send click events before the first paint.
user_pref("nglayout.initialpaint.unsuppress_with_no_background", true);
user_pref("media.block-autoplay-until-in-foreground", false);
// Enable AppCache globally for now whilst it's being removed in Bug 1584984
user_pref("browser.cache.offline.enable", true);
// Enable blocking access to storage from tracking resources by default.
// We don't want to run WPT using BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN (5 - aka Dynamic First Party Isolation) yet.
user_pref("network.cookie.cookieBehavior", 4);
