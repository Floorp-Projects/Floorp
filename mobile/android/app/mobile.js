#filter dumbComments emptyLines substitution

// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Non-static prefs that are specific to Firefox on Android belong in this file
// (unless there is a compelling and documented reason for them to belong in
// another file).
//
// Please indent all prefs defined within #ifdef/#ifndef conditions. This
// improves readability, particular for conditional blocks that exceed a single
// screen.

pref("toolkit.defaultChromeURI", "chrome://geckoview/content/geckoview.xhtml");

pref("toolkit.zoomManager.zoomValues", ".2,.3,.5,.67,.8,.9,1,1.1,1.2,1.33,1.5,1.7,2,2.4,3,4");

// Show/Hide scrollbars when active/inactive
pref("ui.useOverlayScrollbars", 1);
pref("ui.scrollbarFadeBeginDelay", 450);
pref("ui.scrollbarFadeDuration", 0);

/* image cache prefs */
pref("image.cache.size", 1048576); // bytes

/* disable some protocol warnings */
pref("network.protocol-handler.warn-external.tel", false);
pref("network.protocol-handler.warn-external.sms", false);
pref("network.protocol-handler.warn-external.mailto", false);
pref("network.protocol-handler.warn-external.vnd.youtube", false);

/* http prefs */
pref("network.http.keep-alive.timeout", 109);
pref("network.http.max-persistent-connections-per-proxy", 20);

// spdy
pref("network.http.http2.push-allowance", 32768);
pref("network.http.http2.default-hpack-buffer", 4096); // 4k

// See bug 545869 for details on why these are set the way they are
pref("network.buffer.cache.size",  16384);

// CookieBehavior setting for the private browsing.
pref("network.cookie.cookieBehavior.pbmode", 4);

/* session history */
pref("browser.sessionhistory.max_entries", 50);
pref("browser.sessionhistory.contentViewerTimeout", 360);

/* session store */
pref("browser.sessionstore.resume_from_crash", true);
pref("browser.sessionstore.interval", 10000); // milliseconds
pref("browser.sessionstore.max_tabs_undo", 10);
pref("browser.sessionstore.max_resumed_crashes", 2);
pref("browser.sessionstore.privacy_level", 0); // saving data: 0 = all, 1 = unencrypted sites, 2 = never

// Download protection lists are not available on Fennec.
pref("urlclassifier.downloadAllowTable", "");
pref("urlclassifier.downloadBlockTable", "");

/* these should help performance */
pref("layout.css.report_errors", false);

/* download manager (don't show the window or alert) */
pref("browser.download.useDownloadDir", true);
pref("browser.download.folderList", 1); // Default to ~/Downloads
pref("browser.download.manager.addToRecentDocs", true);

/* password manager */
pref("signon.firefoxRelay.feature", "not available");

/* form helper (scroll to and optionally zoom into editable fields)  */
pref("formhelper.autozoom", true);

/* spellcheck */
pref("layout.spellcheckDefault", 0);

/* extension manager and xpinstall */
pref("xpinstall.whitelist.fileRequest", false);
pref("xpinstall.whitelist.add", "https://addons.mozilla.org");

pref("extensions.langpacks.signatures.required", true);
pref("xpinstall.signatures.required", true);

// Whether MV3 restrictions for actions popup urls should be extended to MV2 extensions
// (only allowing same extension urls to be used as action popup urls).
pref("extensions.manifestV2.actionsPopupURLRestricted", true);

// Disable add-ons that are not installed by the user in all scopes by default (See the SCOPE
// constants in AddonManager.jsm for values to use here, and Bug 1405528 for a rationale).
pref("extensions.autoDisableScopes", 15);

pref("extensions.enabledScopes", 5);
pref("extensions.update.enabled", true);
pref("extensions.update.interval", 86400);
pref("extensions.logging.enabled", false);
pref("extensions.strictCompatibility", false);
pref("extensions.minCompatibleAppVersion", "11.0");

pref("extensions.update.url", "https://versioncheck.addons.mozilla.org/update/VersionCheck.php?reqVersion=%REQ_VERSION%&id=%ITEM_ID%&version=%ITEM_VERSION%&maxAppVersion=%ITEM_MAXAPPVERSION%&status=%ITEM_STATUS%&appID=%APP_ID%&appVersion=%APP_VERSION%&appOS=%APP_OS%&appABI=%APP_ABI%&locale=%APP_LOCALE%&currentAppVersion=%CURRENT_APP_VERSION%&updateType=%UPDATE_TYPE%&compatMode=%COMPATIBILITY_MODE%");
pref("extensions.update.background.url", "https://versioncheck-bg.addons.mozilla.org/update/VersionCheck.php?reqVersion=%REQ_VERSION%&id=%ITEM_ID%&version=%ITEM_VERSION%&maxAppVersion=%ITEM_MAXAPPVERSION%&status=%ITEM_STATUS%&appID=%APP_ID%&appVersion=%APP_VERSION%&appOS=%APP_OS%&appABI=%APP_ABI%&locale=%APP_LOCALE%&currentAppVersion=%CURRENT_APP_VERSION%&updateType=%UPDATE_TYPE%&compatMode=%COMPATIBILITY_MODE%");

/* preferences for the Get Add-ons pane */
pref("extensions.getAddons.cache.enabled", true);
pref("extensions.getAddons.search.browseURL", "https://addons.mozilla.org/%LOCALE%/android/search?q=%TERMS%&platform=%OS%&appver=%VERSION%");
pref("extensions.getAddons.browseAddons", "https://addons.mozilla.org/%LOCALE%/android/collections/4757633/mob/?page=1&collection_sort=-popularity");
pref("extensions.getAddons.get.url", "https://services.addons.mozilla.org/api/v4/addons/search/?guid=%IDS%&lang=%LOCALE%");
pref("extensions.getAddons.langpacks.url", "https://services.addons.mozilla.org/api/v4/addons/language-tools/?app=android&type=language&appversion=%VERSION%");

/* Don't let XPIProvider install distribution add-ons; we do our own thing on mobile. */
pref("extensions.installDistroAddons", false);

pref("extensions.webextOptionalPermissionPrompts", true);

pref("extensions.experiments.enabled", false);

/* The abuse report feature needs some UI that we do not have on mobile. */
pref("extensions.abuseReport.amWebAPI.enabled", false);

/* block popups by default, and notify the user about blocked popups */
pref("dom.disable_open_during_load", true);
pref("privacy.popups.showBrowserMessage", true);

/* disable opening windows with the dialog feature */
pref("dom.disable_window_open_dialog_feature", true);

pref("keyword.enabled", true);
pref("browser.fixup.domainwhitelist.localhost", true);

pref("accessibility.typeaheadfind", false);
pref("accessibility.typeaheadfind.timeout", 5000);
pref("accessibility.typeaheadfind.flashBar", 1);
pref("accessibility.typeaheadfind.linksonly", false);
pref("accessibility.browsewithcaret_shortcut.enabled", false);

// SSL error page behaviour
pref("browser.xul.error_pages.expert_bad_cert", false);

// disable updating
pref("browser.search.update", false);

// disable search suggestions by default
pref("browser.search.suggest.enabled", false);

// Enable sparse localization by setting a few package locale overrides
pref("chrome.override_package.global", "browser");
pref("chrome.override_package.mozapps", "browser");
pref("chrome.override_package.passwordmgr", "browser");

// don't allow JS to move and resize existing windows
pref("dom.disable_window_move_resize", true);

// open in tab preferences
pref("browser.link.open_newwindow", 3);
// 0=force all new windows to tabs, 1=don't force, 2=only force those with no features set
pref("browser.link.open_newwindow.restriction", 0);

// content sink control -- controls responsiveness during page load
// see https://bugzilla.mozilla.org/show_bug.cgi?id=481566#c9
//pref("content.sink.enable_perf_mode",  2); // 0 - switch, 1 - interactive, 2 - perf
//pref("content.sink.pending_event_mode", 0);
//pref("content.sink.perf_deflect_count", 1000000);
//pref("content.sink.perf_parse_time", 50000000);

pref("dom.max_script_run_time", 20);

// Absolute path to the devtools unix domain socket file used
// to communicate with a usb cable via adb forward.
pref("devtools.debugger.unix-domain-socket", "@ANDROID_PACKAGE_NAME@/firefox-debugger-socket");

// product URLs
// The breakpad report server to link to in about:crashes
pref("breakpad.reportURL", "https://crash-stats.mozilla.org/report/index/");

pref("app.support.baseURL", "https://support.mozilla.org/1/mobile/%VERSION%/%OS%/%LOCALE%/");
#if MOZ_UPDATE_CHANNEL == beta
  pref("app.releaseNotesURL", "https://www.mozilla.com/%LOCALE%/mobile/%VERSION%beta/releasenotes/");
#else
  pref("app.releaseNotesURL", "https://www.mozilla.com/%LOCALE%/mobile/%VERSION%/releasenotes/");
#endif

// Enable pinning
pref("security.cert_pinning.enforcement_level", 1);

/* prefs used by the update timer system (including blocklist pings) */
pref("app.update.timerFirstInterval", 30000); // milliseconds
pref("app.update.timerMinimumDelay", 30); // seconds

#ifdef MOZ_UPDATER
  /* prefs used specifically for updating the app */
  pref("app.update.channel", "@MOZ_UPDATE_CHANNEL@");
#endif

// APZ physics settings (fling acceleration, fling curving and axis lock) have
// been reviewed by UX
pref("apz.axis_lock.breakout_angle", "0.7853982");    // PI / 4 (45 degrees)
pref("apz.axis_lock.mode", 1); // Use "strict" axis locking
pref("apz.content_response_timeout", 600);
pref("apz.drag.enabled", false);
pref("apz.fling_curve_function_x1", "0.59");
pref("apz.fling_curve_function_y1", "0.46");
pref("apz.fling_curve_function_x2", "0.05");
pref("apz.fling_curve_function_y2", "1.00");
pref("apz.fling_curve_threshold_inches_per_ms", "0.01");
// apz.fling_friction and apz.fling_stopped_threshold are currently ignored by Fennec.
pref("apz.fling_friction", "0.004");
pref("apz.fling_stopped_threshold", "0.0");
pref("apz.max_velocity_inches_per_ms", "0.07");
pref("apz.overscroll.enabled", true);
pref("apz.second_tap_tolerance", "0.3");
pref("apz.touch_move_tolerance", "0.03");
pref("apz.touch_start_tolerance", "0.06");

// prevent tooltips from showing up
pref("browser.chrome.toolbar_tips", false);

// don't allow meta-refresh when backgrounded
pref("browser.meta_refresh_when_inactive.disabled", true);

// On mobile we throttle the download once the readahead_limit is hit
// if we're using a cellular connection, even if the download is slow.
// This is to preserve battery and data.
pref("media.throttle-cellular-regardless-of-download-rate", true);

// Number of video frames we buffer while decoding video.
// On Android this is decided by a similar value which varies for
// each OMX decoder |OMX_PARAM_PORTDEFINITIONTYPE::nBufferCountMin|. This
// number must be less than the OMX equivalent or gecko will think it is
// chronically starved of video frames. All decoders seen so far have a value
// of at least 4.
pref("media.video-queue.default-size", 3);
// The maximum number of queued frames to send to the compositor.
// On Android, it needs to be throttled because SurfaceTexture contains only one
// (the most recent) image data.
pref("media.video-queue.send-to-compositor-size", 1);

pref("media.mediadrm-widevinecdm.visible", true);

// OpenH264 is visible in about:plugins, and enabled, by default.
pref("media.gmp-gmpopenh264.visible", true);
pref("media.gmp-gmpopenh264.enabled", true);

// Disable future downloads of OpenH264 on Android
pref("media.gmp-gmpopenh264.autoupdate", false);

// The download protection UI is not implemented yet (bug 1239094).
pref("browser.safebrowsing.downloads.enabled", false);

// The application reputation lists are not available on Android.
pref("urlclassifier.downloadAllowTable", "");
pref("urlclassifier.downloadBlockTable", "");

// The Potentially Harmful Apps list replaces the malware one on Android.
pref("urlclassifier.malwareTable", "goog-harmful-proto,goog-unwanted-proto,moztest-harmful-simple,moztest-malware-simple,moztest-unwanted-simple");

// True if you always want dump() to work
//
// On Android, you also need to do the following for the output
// to show up in logcat:
//
// $ adb shell stop
// $ adb shell setprop log.redirect-stdio true
// $ adb shell start
pref("browser.dom.window.dump.enabled", true);
pref("devtools.console.stdout.chrome", true);

// Transmit UDP busy-work to the LAN when anticipating low latency
// network reads and on wifi to mitigate 802.11 Power Save Polling delays
pref("network.tickle-wifi.enabled", true);

// The mode of home provider syncing.
// 0: Sync always
// 1: Sync only when on wifi
pref("home.sync.updateMode", 0);

// How frequently to check if we should sync home provider data.
pref("home.sync.checkIntervalSecs", 3600);

// Enable meta-viewport support for font inflation code
pref("dom.meta-viewport.enabled", true);

// Enable GMP support in the addon manager.
pref("media.gmp-provider.enabled", true);

// The default color scheme in reader mode (light, dark, auto)
// auto = color automatically adjusts according to ambient light level
// (auto only works on platforms where the 'devicelight' event is enabled)
// auto doesn't work: https://bugzilla.mozilla.org/show_bug.cgi?id=1472957
// pref("reader.color_scheme", "auto");
pref("reader.color_scheme", "light");

// Color scheme values available in reader mode UI.
// pref("reader.color_scheme.values", "[\"dark\",\"auto\",\"light\"]");
pref("reader.color_scheme.values", "[\"dark\",\"sepia\",\"light\"]");

// Whether to use a vertical or horizontal toolbar.
pref("reader.toolbar.vertical", false);

// Telemetry settings.
// Whether to use the unified telemetry behavior, requires a restart.
pref("toolkit.telemetry.unified", false);

// AccessibleCaret CSS for the Android L style assets.
pref("layout.accessiblecaret.width", "22.0");
pref("layout.accessiblecaret.height", "22.0");
pref("layout.accessiblecaret.margin-left", "-11.5");

// Android needs to show the caret when long tapping on an empty content.
pref("layout.accessiblecaret.caret_shown_when_long_tapping_on_empty_content", true);

// Androids carets are always tilt to match the text selection guideline.
pref("layout.accessiblecaret.always_tilt", true);

// Update any visible carets for selection changes due to JS calls,
// but don't show carets if carets are hidden.
pref("layout.accessiblecaret.script_change_update_mode", 1);

// Optionally provide haptic feedback on longPress selection events.
pref("layout.accessiblecaret.hapticfeedback", true);

// Initial text selection on long-press is enhanced to provide
// a smarter phone-number selection for direct-dial ActionBar action.
pref("layout.accessiblecaret.extend_selection_for_phone_number", true);

// Allow service workers to open windows for a longer period after a notification
// click on mobile.  This is to account for some devices being quite slow.
pref("dom.serviceWorkers.disable_open_click_delay", 5000);

pref("dom.push.maxRecentMessageIDsPerSubscription", 0);

// Ask for permission when enumerating WebRTC devices.
pref("media.navigator.permission.device", true);

// Allow system add-on updates
pref("extensions.systemAddon.update.url", "https://aus5.mozilla.org/update/3/SystemAddons/%VERSION%/%BUILD_ID%/%BUILD_TARGET%/%LOCALE%/%CHANNEL%/%OS_VERSION%/%DISTRIBUTION%/%DISTRIBUTION_VERSION%/update.xml");
pref("extensions.systemAddon.update.enabled", true);
