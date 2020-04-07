/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Non-static prefs that are specific to Firefox on Android belong in this file
// (unless there is a compelling and documented reason for them to belong in
// another file).
//
// Please indent all prefs defined within #ifdef/#ifndef conditions. This
// improves readability, particular for conditional blocks that exceed a single
// screen.

#filter substitution

// For browser.js element
//
// cacheRatio* is a ratio that determines the amount of pixels to cache. The
// ratio is multiplied by the viewport width or height to get the displayport's
// width or height, respectively.
//
// (divide integer value by 1000 to get the ratio)
//
// For instance: cachePercentageWidth is 1500
//               viewport height is 500
//               => display port height will be 500 * 1.5 = 750
//
pref("toolkit.browser.cacheRatioWidth", 2000);
pref("toolkit.browser.cacheRatioHeight", 3000);

// How long before a content view (a handle to a remote scrollable object)
// expires.
pref("toolkit.browser.contentViewExpire", 3000);

pref("toolkit.defaultChromeURI", "chrome://geckoview/content/geckoview.xhtml");
pref("browser.chromeURL", "chrome://browser/content/");

// If a tab has not been active for this long (seconds), then it may be
// turned into a zombie tab to preemptively free up memory. -1 disables time-based
// expiration (but low-memory conditions may still require the tab to be zombified).
pref("browser.tabs.expireTime", 900);

// Disables zombification of background tabs under memory pressure.
// Intended for use in testing, where we don't want the tab running the
// test harness code to be zombified.
pref("browser.tabs.disableBackgroundZombification", false);

// Control whether tab content should try to load from disk cache when network
// is offline.
// Controlled by Switchboard experiment "offline-cache".
pref("browser.tabs.useCache", false);

pref("toolkit.zoomManager.zoomValues", ".2,.3,.5,.67,.8,.9,1,1.1,1.2,1.33,1.5,1.7,2,2.4,3,4");

// Mobile will use faster, less durable mode.
pref("toolkit.storage.synchronous", 0);

// Android needs concurrent access to the same database from multiple processes,
// thus we can't use exclusive locking on it.
pref("storage.multiProcessAccess.enabled", true);

// The default fallback zoom level to render pages at. Set to -1 to fit page; otherwise
// the value is divided by 1000 and clamped to hard-coded min/max scale values.
pref("browser.viewport.defaultZoom", -1);

// Show/Hide scrollbars when active/inactive
pref("ui.showHideScrollbars", 1);
pref("ui.useOverlayScrollbars", 1);
pref("ui.scrollbarFadeBeginDelay", 450);
pref("ui.scrollbarFadeDuration", 0);

/* turn off the caret blink after 10 cycles */
pref("ui.caretBlinkCount", 10);

pref("browser.cache.memory_limit", 5120); // 5 MB

/* image cache prefs */
pref("image.cache.size", 1048576); // bytes

/* offline cache prefs */
pref("browser.offline-apps.notify", true);
pref("browser.cache.offline.capacity", 5120); // kilobytes

// Automatically shrink-to-fit image documents.
pref("browser.enable_automatic_image_resizing", true);

/* disable some protocol warnings */
pref("network.protocol-handler.warn-external.tel", false);
pref("network.protocol-handler.warn-external.sms", false);
pref("network.protocol-handler.warn-external.mailto", false);
pref("network.protocol-handler.warn-external.vnd.youtube", false);

/* http prefs */
pref("network.http.keep-alive.timeout", 109);
pref("network.http.max-persistent-connections-per-server", 6);
pref("network.http.max-persistent-connections-per-proxy", 20);

// spdy
pref("network.http.spdy.push-allowance", 32768);
pref("network.http.spdy.default-hpack-buffer", 4096); // 4k

// http3
pref("network.http.http3.default-qpack-table-size", 4096); // 4k

// See bug 545869 for details on why these are set the way they are
pref("network.buffer.cache.count", 24);
pref("network.buffer.cache.size",  16384);

// predictive actions
pref("network.predictor.max-db-size", 2097152); // bytes
pref("network.predictor.preserve", 50); // percentage of predictor data to keep when cleaning up

// Use JS mDNS as a fallback
pref("network.mdns.use_js_fallback", false);

/* How many times should have passed before the remote tabs list is refreshed */
pref("browser.display.remotetabs.timeout", 10);

/* session history */
pref("browser.sessionhistory.max_total_viewers", -1);
pref("browser.sessionhistory.max_entries", 50);
pref("browser.sessionhistory.contentViewerTimeout", 360);
pref("browser.sessionhistory.bfcacheIgnoreMemoryPressure", false);

/* session store */
pref("browser.sessionstore.resume_from_crash", true);
pref("browser.sessionstore.interval", 10000); // milliseconds
pref("browser.sessionstore.backupInterval", 120000); // milliseconds -> 2 minutes
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

/* download helper */
pref("browser.helperApps.deleteTempFileOnExit", false);

/* password manager */
pref("signon.rememberSignons", true);
pref("signon.autofillForms.http", true);
pref("signon.expireMasterPassword", false);
pref("signon.debug", false);

/* form helper (scroll to and optionally zoom into editable fields)  */
pref("formhelper.autozoom", true);

/* find helper */
pref("findhelper.autozoom", true);

/* autocomplete */
pref("browser.formfill.enable", true);

/* spellcheck */
pref("layout.spellcheckDefault", 0);

/* new html5 forms */
pref("dom.forms.datetime.others", true);

/* extension manager and xpinstall */
pref("xpinstall.whitelist.fileRequest", false);
pref("xpinstall.whitelist.add", "https://addons.mozilla.org");

pref("extensions.langpacks.signatures.required", true);
pref("xpinstall.signatures.required", true);

// Disable add-ons that are not installed by the user in all scopes by default (See the SCOPE
// constants in AddonManager.jsm for values to use here, and Bug 1405528 for a rationale).
pref("extensions.autoDisableScopes", 15);

pref("extensions.enabledScopes", 5);
pref("extensions.autoupdate.enabled", true);
pref("extensions.autoupdate.interval", 86400);
pref("extensions.update.enabled", true);
pref("extensions.update.interval", 86400);
pref("extensions.dss.enabled", false);
pref("extensions.ignoreMTimeChanges", false);
pref("extensions.logging.enabled", false);
pref("extensions.hideInstallButton", true);
pref("extensions.hideUpdateButton", false);
pref("extensions.strictCompatibility", false);
pref("extensions.minCompatibleAppVersion", "11.0");

pref("extensions.update.url", "https://versioncheck.addons.mozilla.org/update/VersionCheck.php?reqVersion=%REQ_VERSION%&id=%ITEM_ID%&version=%ITEM_VERSION%&maxAppVersion=%ITEM_MAXAPPVERSION%&status=%ITEM_STATUS%&appID=%APP_ID%&appVersion=%APP_VERSION%&appOS=%APP_OS%&appABI=%APP_ABI%&locale=%APP_LOCALE%&currentAppVersion=%CURRENT_APP_VERSION%&updateType=%UPDATE_TYPE%&compatMode=%COMPATIBILITY_MODE%");
pref("extensions.update.background.url", "https://versioncheck-bg.addons.mozilla.org/update/VersionCheck.php?reqVersion=%REQ_VERSION%&id=%ITEM_ID%&version=%ITEM_VERSION%&maxAppVersion=%ITEM_MAXAPPVERSION%&status=%ITEM_STATUS%&appID=%APP_ID%&appVersion=%APP_VERSION%&appOS=%APP_OS%&appABI=%APP_ABI%&locale=%APP_LOCALE%&currentAppVersion=%CURRENT_APP_VERSION%&updateType=%UPDATE_TYPE%&compatMode=%COMPATIBILITY_MODE%");

/* preferences for the Get Add-ons pane */
pref("extensions.getAddons.cache.enabled", true);
pref("extensions.getAddons.search.browseURL", "https://addons.mozilla.org/%LOCALE%/android/search?q=%TERMS%&platform=%OS%&appver=%VERSION%");
pref("extensions.getAddons.browseAddons", "https://addons.mozilla.org/%LOCALE%/android/collections/4757633/mob/?page=1&collection_sort=-popularity");
pref("extensions.getAddons.get.url", "https://services.addons.mozilla.org/api/v3/addons/search/?guid=%IDS%&lang=%LOCALE%");
pref("extensions.getAddons.langpacks.url", "https://services.addons.mozilla.org/api/v3/addons/language-tools/?app=android&type=language&appversion=%VERSION%");

/* preference for the locale picker */
pref("extensions.getLocales.get.url", "");
pref("extensions.compatability.locales.buildid", "0");

/* Don't let XPIProvider install distribution add-ons; we do our own thing on mobile. */
pref("extensions.installDistroAddons", false);

pref("extensions.webextPermissionPrompts", true);
pref("extensions.webextOptionalPermissionPrompts", true);

// Add-on content security policies.
pref("extensions.webextensions.base-content-security-policy", "script-src 'self' https://* moz-extension: blob: filesystem: 'unsafe-eval' 'unsafe-inline'; object-src 'self' https://* moz-extension: blob: filesystem:;");
pref("extensions.webextensions.default-content-security-policy", "script-src 'self'; object-src 'self';");

pref("extensions.webextensions.background-delayed-startup", true);

pref("extensions.experiments.enabled", false);

/* block popups by default, and notify the user about blocked popups */
pref("dom.disable_open_during_load", true);
pref("privacy.popups.showBrowserMessage", true);

/* disable opening windows with the dialog feature */
pref("dom.disable_window_open_dialog_feature", true);
pref("dom.disable_window_find", true);

pref("keyword.enabled", true);
pref("browser.fixup.domainwhitelist.localhost", true);

pref("accessibility.typeaheadfind", false);
pref("accessibility.typeaheadfind.timeout", 5000);
pref("accessibility.typeaheadfind.flashBar", 1);
pref("accessibility.typeaheadfind.linksonly", false);
pref("accessibility.typeaheadfind.casesensitive", 0);
pref("accessibility.browsewithcaret_shortcut.enabled", false);
pref("findbar.matchdiacritics", 0);

// Whether the character encoding menu is under the main Firefox button. This
// preference is a string so that localizers can alter it.
pref("browser.menu.showCharacterEncoding", "chrome://browser/locale/browser.properties");

// SSL error page behaviour
pref("browser.ssl_override_behavior", 2);
pref("browser.xul.error_pages.expert_bad_cert", false);

// Market-specific search defaults
pref("browser.search.geoSpecificDefaults", true);
pref("browser.search.geoSpecificDefaults.url", "https://search.services.mozilla.com/1/%APP%/%VERSION%/%CHANNEL%/%LOCALE%/%REGION%/%DISTRIBUTION%/%DISTRIBUTION_VERSION%");

// disable updating
pref("browser.search.update", false);

// disable search suggestions by default
pref("browser.search.suggest.enabled", false);
pref("browser.search.suggest.prompted", false);

// tell the search service that we don't really expose the "current engine"
pref("browser.search.noCurrentEngine", true);

// Control media casting & mirroring features
pref("browser.casting.enabled", true);

// Enable sparse localization by setting a few package locale overrides
pref("chrome.override_package.global", "browser");
pref("chrome.override_package.mozapps", "browser");
pref("chrome.override_package.passwordmgr", "browser");

// disable color management
pref("gfx.color_management.mode", 0);

// 0=fixed margin, 1=velocity bias, 2=dynamic resolution, 3=no margins, 4=prediction bias
pref("gfx.displayport.strategy", 1);

// all of the following displayport strategy prefs will be divided by 1000
// to obtain some multiplier which is then used in the strategy.
// fixed margin strategy options
pref("gfx.displayport.strategy_fm.multiplier", -1); // displayport dimension multiplier
pref("gfx.displayport.strategy_fm.danger_x", -1); // danger zone on x-axis when multiplied by viewport width
pref("gfx.displayport.strategy_fm.danger_y", -1); // danger zone on y-axis when multiplied by viewport height

// velocity bias strategy options
pref("gfx.displayport.strategy_vb.multiplier", -1); // displayport dimension multiplier
pref("gfx.displayport.strategy_vb.threshold", -1); // velocity threshold in inches/frame
pref("gfx.displayport.strategy_vb.reverse_buffer", -1); // fraction of buffer to keep in reverse direction from scroll
pref("gfx.displayport.strategy_vb.danger_x_base", -1); // danger zone on x-axis when multiplied by viewport width
pref("gfx.displayport.strategy_vb.danger_y_base", -1); // danger zone on y-axis when multiplied by viewport height
pref("gfx.displayport.strategy_vb.danger_x_incr", -1); // additional danger zone on x-axis when multiplied by viewport width and velocity
pref("gfx.displayport.strategy_vb.danger_y_incr", -1); // additional danger zone on y-axis when multiplied by viewport height and velocity

// prediction bias strategy options
pref("gfx.displayport.strategy_pb.threshold", -1); // velocity threshold in inches/frame

// don't allow JS to move and resize existing windows
pref("dom.disable_window_move_resize", true);

// prevent click image resizing for nsImageDocument
pref("browser.enable_click_image_resizing", false);

// open in tab preferences
// 0=default window, 1=current window/tab, 2=new window, 3=new tab in most window
pref("browser.link.open_external", 3);
pref("browser.link.open_newwindow", 3);
// 0=force all new windows to tabs, 1=don't force, 2=only force those with no features set
pref("browser.link.open_newwindow.restriction", 0);

// show images option
// 0=never, 1=always, 2=cellular-only
pref("browser.image_blocking", 1);

// controls which bits of private data to clear. by default we clear them all.
pref("privacy.item.cache", true);
pref("privacy.item.cookies", true);
pref("privacy.item.offlineApps", true);
pref("privacy.item.history", true);
pref("privacy.item.searchHistory", true);
pref("privacy.item.formdata", true);
pref("privacy.item.downloads", true);
pref("privacy.item.passwords", true);
pref("privacy.item.sessions", true);
pref("privacy.item.geolocation", true);
pref("privacy.item.siteSettings", true);
pref("privacy.item.syncAccount", true);

// content sink control -- controls responsiveness during page load
// see https://bugzilla.mozilla.org/show_bug.cgi?id=481566#c9
//pref("content.sink.enable_perf_mode",  2); // 0 - switch, 1 - interactive, 2 - perf
//pref("content.sink.pending_event_mode", 0);
//pref("content.sink.perf_deflect_count", 1000000);
//pref("content.sink.perf_parse_time", 50000000);

pref("javascript.options.mem.high_water_mark", 32);

pref("dom.max_chrome_script_run_time", 0); // disable slow script dialog for chrome
pref("dom.max_script_run_time", 20);

// Absolute path to the devtools unix domain socket file used
// to communicate with a usb cable via adb forward.
pref("devtools.debugger.unix-domain-socket", "@ANDROID_PACKAGE_NAME@/firefox-debugger-socket");

pref("devtools.remote.usb.enabled", false);
pref("devtools.remote.wifi.enabled", false);

pref("ui.touch.radius.enabled", false);
pref("ui.touch.radius.leftmm", 3);
pref("ui.touch.radius.topmm", 5);
pref("ui.touch.radius.rightmm", 3);
pref("ui.touch.radius.bottommm", 2);
pref("ui.touch.radius.visitedWeight", 120);

pref("ui.mouse.radius.enabled", true);
pref("ui.mouse.radius.leftmm", 3);
pref("ui.mouse.radius.topmm", 5);
pref("ui.mouse.radius.rightmm", 3);
pref("ui.mouse.radius.bottommm", 2);
pref("ui.mouse.radius.visitedWeight", 120);
pref("ui.mouse.radius.reposition", true);

// Maximum distance from the point where the user pressed where we still
// look for text to select
pref("browser.ui.selection.distance", 250);

// plugins
pref("plugin.disable", true);
pref("dom.ipc.plugins.enabled", false);

// product URLs
// The breakpad report server to link to in about:crashes
pref("breakpad.reportURL", "https://crash-stats.mozilla.org/report/index/");

pref("app.support.baseURL", "https://support.mozilla.org/1/mobile/%VERSION%/%OS%/%LOCALE%/");
pref("app.supportURL", "https://support.mozilla.org/1/mobile/%VERSION%/%OS%/%LOCALE%/mobile-help");
pref("app.faqURL", "https://support.mozilla.org/1/mobile/%VERSION%/%OS%/%LOCALE%/faq");

// URL for feedback page
// This should be kept in sync with the "feedback_link" string defined in strings.xml.in
pref("app.feedbackURL", "https://input.mozilla.org/feedback/android/%VERSION%/%CHANNEL%/?utm_source=feedback-prompt");

pref("app.privacyURL", "https://www.mozilla.org/privacy/firefox/");
pref("app.creditsURL", "https://www.mozilla.org/credits/");
pref("app.channelURL", "https://www.mozilla.org/%LOCALE%/firefox/channel/");
#if MOZ_UPDATE_CHANNEL == aurora
  pref("app.releaseNotesURL", "https://www.mozilla.com/%LOCALE%/mobile/%VERSION%/auroranotes/");
#elif MOZ_UPDATE_CHANNEL == beta
  pref("app.releaseNotesURL", "https://www.mozilla.com/%LOCALE%/mobile/%VERSION%beta/releasenotes/");
#else
  pref("app.releaseNotesURL", "https://www.mozilla.com/%LOCALE%/mobile/%VERSION%/releasenotes/");
#endif

// Name of alternate about: page for certificate errors (when undefined, defaults to about:neterror)
pref("security.alternate_certificate_error_page", "certerror");

pref("security.warn_viewing_mixed", false); // Warning is disabled.  See Bug 616712.

// Enable pinning
pref("security.cert_pinning.enforcement_level", 1);

// Only fetch OCSP for EV certificates
pref("security.OCSP.enabled", 2);

/* prefs used by the update timer system (including blocklist pings) */
pref("app.update.timerFirstInterval", 30000); // milliseconds
pref("app.update.timerMinimumDelay", 30); // seconds

// used by update service to decide whether or not to
// automatically download an update
pref("app.update.autodownload", "wifi");
pref("app.update.url.android", "https://aus5.mozilla.org/update/4/%PRODUCT%/%VERSION%/%BUILD_ID%/%BUILD_TARGET%/%LOCALE%/%CHANNEL%/%OS_VERSION%/%DISTRIBUTION%/%DISTRIBUTION_VERSION%/%MOZ_VERSION%/update.xml");

#ifdef MOZ_UPDATER
  /* prefs used specifically for updating the app */
  pref("app.update.channel", "@MOZ_UPDATE_CHANNEL@");
#endif

// replace newlines with spaces on paste into single-line text boxes
pref("editor.singleLine.pasteNewlines", 2);

// threshold where a tap becomes a drag, in 1/240" reference pixels
// The names of the preferences are to be in sync with EventStateManager.cpp
pref("ui.dragThresholdX", 25);
pref("ui.dragThresholdY", 25);

pref("layers.async-video.enabled", true);

// APZ physics settings (fling acceleration, fling curving and axis lock) have
// been reviewed by UX
pref("apz.axis_lock.breakout_angle", "0.7853982");    // PI / 4 (45 degrees)
pref("apz.axis_lock.mode", 1); // Use "strict" axis locking
pref("apz.content_response_timeout", 600);
pref("apz.drag.enabled", false);
pref("apz.fling_accel_interval_ms", 750);
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

// Enable the Visual Viewport API
pref("dom.visualviewport.enabled", true);

pref("layers.progressive-paint", true);
pref("layers.low-precision-buffer", true);
// We want to limit layers for two reasons:
// 1) We can't scroll smoothly if we have to many draw calls
// 2) Pages that have too many layers consume too much memory and crash.
// By limiting the number of layers on mobile we're making the main thread
// work harder keep scrolling smooth and memory low.
pref("layers.max-active", 20);

pref("notification.feature.enabled", true);

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

// Switch block autoplay logic to v2.
pref("media.autoplay.enabled.user-gestures-needed", true);
// Set Fennec to block autoplay by default.
pref("media.autoplay.default", 1); // 0=Allowed, 1=Blocked

// Enable WebSpeech speech synthesis
pref("media.webspeech.synth.enabled", true);

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

// controls if we want camera support
pref("device.camera.enabled", true);
pref("media.realtime_decoder.enabled", true);

pref("javascript.options.showInConsole", true);

pref("full-screen-api.enabled", true);

pref("direct-texture.force.enabled", false);
pref("direct-texture.force.disabled", false);

// This fraction in 1000ths of velocity remains after every animation frame when the velocity is low.
pref("ui.scrolling.friction_slow", -1);
// This fraction in 1000ths of velocity remains after every animation frame when the velocity is high.
pref("ui.scrolling.friction_fast", -1);
// The maximum velocity change factor between events, per ms, in 1000ths.
// Direction changes are excluded.
pref("ui.scrolling.max_event_acceleration", -1);
// The rate of deceleration when the surface has overscrolled, in 1000ths.
pref("ui.scrolling.overscroll_decel_rate", -1);
// The fraction of the surface which can be overscrolled before it must snap back, in 1000ths.
pref("ui.scrolling.overscroll_snap_limit", -1);
// The minimum amount of space that must be present for an axis to be considered scrollable,
// in 1/1000ths of pixels.
pref("ui.scrolling.min_scrollable_distance", -1);
// The axis lock mode for panning behaviour - set between standard, free and sticky
pref("ui.scrolling.axis_lock_mode", "standard");
// Determine the dead zone for gamepad joysticks. Higher values result in larger dead zones; use a negative value to
// auto-detect based on reported hardware values
pref("ui.scrolling.gamepad_dead_zone", 115);

// Prefs for fling acceleration
pref("ui.scrolling.fling_accel_interval", -1);
pref("ui.scrolling.fling_accel_base_multiplier", -1);
pref("ui.scrolling.fling_accel_supplemental_multiplier", -1);

// Prefs for fling curving
pref("ui.scrolling.fling_curve_function_x1", -1);
pref("ui.scrolling.fling_curve_function_y1", -1);
pref("ui.scrolling.fling_curve_function_x2", -1);
pref("ui.scrolling.fling_curve_function_y2", -1);
pref("ui.scrolling.fling_curve_threshold_velocity", -1);
pref("ui.scrolling.fling_curve_max_velocity", -1);
pref("ui.scrolling.fling_curve_newton_iterations", -1);

// Enable accessibility mode if platform accessibility is enabled.
pref("accessibility.accessfu.activate", 2);
pref("accessibility.accessfu.quicknav_modes", "Link,Heading,FormElement,Landmark,ListItem");
// Active quicknav mode, index value of list from quicknav_modes
pref("accessibility.accessfu.quicknav_index", 0);
// Setting for an utterance order (0 - description first, 1 - description last).
pref("accessibility.accessfu.utterance", 1);
// Whether to skip images with empty alt text
pref("accessibility.accessfu.skip_empty_images", true);

// Transmit UDP busy-work to the LAN when anticipating low latency
// network reads and on wifi to mitigate 802.11 Power Save Polling delays
pref("network.tickle-wifi.enabled", true);

// Mobile manages state by autodetection
pref("network.manage-offline-status", true);

// Media plugins for libstagefright playback on android
pref("media.plugins.enabled", true);

// Stagefright's OMXCodec::CreationFlags. The interesting flag values are:
//  0 = Let Stagefright choose hardware or software decoding (default)
//  8 = Force software decoding
// 16 = Force hardware decoding
pref("media.stagefright.omxcodec.flags", 0);

// Coalesce touch events to prevent them from flooding the event queue
pref("dom.event.touch.coalescing.enabled", false);

pref("layout.framevisibility.numscrollportwidths", 1);
pref("layout.framevisibility.numscrollportheights", 1);

pref("layers.enable-tiles", true);

// Enable the dynamic toolbar
pref("browser.chrome.dynamictoolbar", true);

// Location Bar AutoComplete.
pref("browser.urlbar.autocomplete.enabled", true);

// Hide common parts of URLs like "www." or "http://"
pref("browser.urlbar.trimURLs", true);

// initial web feed readers list
pref("browser.contentHandlers.types.0.title", "chrome://browser/locale/region.properties");
pref("browser.contentHandlers.types.0.uri", "chrome://browser/locale/region.properties");
pref("browser.contentHandlers.types.0.type", "application/vnd.mozilla.maybe.feed");
pref("browser.contentHandlers.types.1.title", "chrome://browser/locale/region.properties");
pref("browser.contentHandlers.types.1.uri", "chrome://browser/locale/region.properties");
pref("browser.contentHandlers.types.1.type", "application/vnd.mozilla.maybe.feed");
pref("browser.contentHandlers.types.2.title", "chrome://browser/locale/region.properties");
pref("browser.contentHandlers.types.2.uri", "chrome://browser/locale/region.properties");
pref("browser.contentHandlers.types.2.type", "application/vnd.mozilla.maybe.feed");
pref("browser.contentHandlers.types.3.title", "chrome://browser/locale/region.properties");
pref("browser.contentHandlers.types.3.uri", "chrome://browser/locale/region.properties");
pref("browser.contentHandlers.types.3.type", "application/vnd.mozilla.maybe.feed");

// Shortnumber matching needed for e.g. Brazil:
// 01187654321 can be found with 87654321
pref("dom.phonenumber.substringmatching.BR", 8);
pref("dom.phonenumber.substringmatching.CO", 10);
pref("dom.phonenumber.substringmatching.VE", 7);

pref("gfx.canvas.azure.backends", "skia");

// When true, phone number linkification is enabled.
pref("browser.ui.linkify.phone", false);

// Enables/disables Spatial Navigation
pref("snav.enabled", true);

// The mode of home provider syncing.
// 0: Sync always
// 1: Sync only when on wifi
pref("home.sync.updateMode", 0);

// How frequently to check if we should sync home provider data.
pref("home.sync.checkIntervalSecs", 3600);

// Enable device storage API
pref("device.storage.enabled", true);

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

pref("browser.tabs.showAudioPlayingIcon", true);

pref("dom.serviceWorkers.enabled", true);

// Allow service workers to open windows for a longer period after a notification
// click on mobile.  This is to account for some devices being quite slow.
pref("dom.serviceWorkers.disable_open_click_delay", 5000);

pref("dom.push.debug", false);
pref("dom.push.maxRecentMessageIDsPerSubscription", 0);

pref("dom.audiochannel.mediaControl", true);

pref("media.openUnsupportedTypeWithExternalApp", true);

// Ask for permission when enumerating WebRTC devices.
pref("media.navigator.permission.device", true);

// Allow system add-on updates
pref("extensions.systemAddon.update.url", "https://aus5.mozilla.org/update/3/SystemAddons/%VERSION%/%BUILD_ID%/%BUILD_TARGET%/%LOCALE%/%CHANNEL%/%OS_VERSION%/%DISTRIBUTION%/%DISTRIBUTION_VERSION%/update.xml");
pref("extensions.systemAddon.update.enabled", true);

// E10s stuff. We don't support 'file' or 'priveleged' process types.
pref("browser.tabs.remote.separateFileUriProcess", false);
pref("browser.tabs.remote.allowLinkedWebInFileUriProcess", true);
pref("browser.tabs.remote.separatePrivilegedContentProcess", false);
pref("browser.tabs.remote.enforceRemoteTypeRestrictions", false);

// Allow Web Authentication
pref("security.webauth.webauthn_enable_android_fido2", true);
pref("browser.tabs.remote.separatePrivilegedMozillaWebContentProcess", false);
