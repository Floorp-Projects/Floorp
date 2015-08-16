/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#filter substitution

// For browser.xml binding
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

pref("toolkit.defaultChromeURI", "chrome://browser/content/browser.xul");
pref("browser.chromeURL", "chrome://browser/content/");

// If a tab has not been active for this long (seconds), then it may be
// turned into a zombie tab to preemptively free up memory. -1 disables time-based
// expiration (but low-memory conditions may still require the tab to be zombified).
pref("browser.tabs.expireTime", 900);

// From libpref/src/init/all.js, extended to allow a slightly wider zoom range.
pref("zoom.minPercent", 20);
pref("zoom.maxPercent", 400);
pref("toolkit.zoomManager.zoomValues", ".2,.3,.5,.67,.8,.9,1,1.1,1.2,1.33,1.5,1.7,2,2.4,3,4");

// Mobile will use faster, less durable mode.
pref("toolkit.storage.synchronous", 0);

pref("browser.viewport.desktopWidth", 980);
// The default fallback zoom level to render pages at. Set to -1 to fit page; otherwise
// the value is divided by 1000 and clamped to hard-coded min/max scale values.
pref("browser.viewport.defaultZoom", -1);

/* allow scrollbars to float above chrome ui */
pref("ui.scrollbarsCanOverlapContent", 1);

/* turn off the caret blink after 10 cycles */
pref("ui.caretBlinkCount", 10);

/* cache prefs */
pref("browser.cache.disk.enable", true);
pref("browser.cache.disk.capacity", 20480); // kilobytes
pref("browser.cache.disk.max_entry_size", 4096); // kilobytes
pref("browser.cache.disk.smart_size.enabled", true);
pref("browser.cache.disk.smart_size.first_run", true);

#ifdef MOZ_PKG_SPECIAL
// low memory devices
pref("browser.cache.memory.enable", false);
#else
pref("browser.cache.memory.enable", true);
#endif
pref("browser.cache.memory.capacity", 1024); // kilobytes

pref("browser.cache.memory_limit", 5120); // 5 MB

/* image cache prefs */
pref("image.cache.size", 1048576); // bytes

/* offline cache prefs */
pref("browser.offline-apps.notify", true);
pref("browser.cache.offline.enable", true);
pref("browser.cache.offline.capacity", 5120); // kilobytes
pref("offline-apps.quota.warn", 1024); // kilobytes

// cache compression turned off for now - see bug #715198
pref("browser.cache.compression_level", 0);

/* disable some protocol warnings */
pref("network.protocol-handler.warn-external.tel", false);
pref("network.protocol-handler.warn-external.sms", false);
pref("network.protocol-handler.warn-external.mailto", false);
pref("network.protocol-handler.warn-external.vnd.youtube", false);

/* http prefs */
pref("network.http.pipelining", true);
pref("network.http.pipelining.ssl", true);
pref("network.http.proxy.pipelining", true);
pref("network.http.pipelining.maxrequests" , 6);
pref("network.http.keep-alive.timeout", 109);
pref("network.http.max-connections", 20);
pref("network.http.max-persistent-connections-per-server", 6);
pref("network.http.max-persistent-connections-per-proxy", 20);

// spdy
pref("network.http.spdy.push-allowance", 32768);

// See bug 545869 for details on why these are set the way they are
pref("network.buffer.cache.count", 24);
pref("network.buffer.cache.size",  16384);

// predictive actions
pref("network.predictor.enabled", true);
pref("network.predictor.max-db-size", 2097152); // bytes
pref("network.predictor.preserve", 50); // percentage of predictor data to keep when cleaning up

/* history max results display */
pref("browser.display.history.maxresults", 100);

/* How many times should have passed before the remote tabs list is refreshed */
pref("browser.display.remotetabs.timeout", 10);

/* session history */
pref("browser.sessionhistory.max_total_viewers", 1);
pref("browser.sessionhistory.max_entries", 50);
pref("browser.sessionhistory.contentViewerTimeout", 360);

/* session store */
pref("browser.sessionstore.resume_session_once", false);
pref("browser.sessionstore.resume_from_crash", true);
pref("browser.sessionstore.interval", 10000); // milliseconds
pref("browser.sessionstore.max_tabs_undo", 5);
pref("browser.sessionstore.max_resumed_crashes", 1);
pref("browser.sessionstore.recent_crashes", 0);
pref("browser.sessionstore.privacy_level", 0); // saving data: 0 = all, 1 = unencrypted sites, 2 = never

/* these should help performance */
pref("mozilla.widget.force-24bpp", true);
pref("mozilla.widget.use-buffer-pixmap", true);
pref("mozilla.widget.disable-native-theme", true);
pref("layout.reflow.synthMouseMove", false);
pref("layout.css.report_errors", false);

/* download manager (don't show the window or alert) */
pref("browser.download.useDownloadDir", true);
pref("browser.download.folderList", 1); // Default to ~/Downloads
pref("browser.download.manager.showAlertOnComplete", false);
pref("browser.download.manager.showAlertInterval", 2000);
pref("browser.download.manager.retention", 2);
pref("browser.download.manager.showWhenStarting", false);
pref("browser.download.manager.closeWhenDone", true);
pref("browser.download.manager.openDelay", 0);
pref("browser.download.manager.focusWhenStarting", false);
pref("browser.download.manager.flashCount", 2);
pref("browser.download.manager.displayedHistoryDays", 7);
pref("browser.download.manager.addToRecentDocs", true);

/* download helper */
pref("browser.helperApps.deleteTempFileOnExit", false);

/* password manager */
pref("signon.rememberSignons", true);
pref("signon.expireMasterPassword", false);
pref("signon.debug", false);

/* form helper (scroll to and optionally zoom into editable fields)  */
pref("formhelper.mode", 2);  // 0 = disabled, 1 = enabled, 2 = dynamic depending on screen size
pref("formhelper.autozoom", true);

/* find helper */
pref("findhelper.autozoom", true);

/* autocomplete */
pref("browser.formfill.enable", true);

/* spellcheck */
pref("layout.spellcheckDefault", 0);

/* new html5 forms */
pref("dom.experimental_forms", true);
pref("dom.forms.number", true);

/* extension manager and xpinstall */
pref("xpinstall.whitelist.directRequest", false);
pref("xpinstall.whitelist.fileRequest", false);
pref("xpinstall.whitelist.add", "https://addons.mozilla.org");
pref("xpinstall.whitelist.add.180", "https://marketplace.firefox.com");

pref("xpinstall.signatures.required", false);

pref("extensions.enabledScopes", 1);
pref("extensions.autoupdate.enabled", true);
pref("extensions.autoupdate.interval", 86400);
pref("extensions.update.enabled", false);
pref("extensions.update.interval", 86400);
pref("extensions.dss.enabled", false);
pref("extensions.dss.switchPending", false);
pref("extensions.ignoreMTimeChanges", false);
pref("extensions.logging.enabled", false);
pref("extensions.hideInstallButton", true);
pref("extensions.showMismatchUI", false);
pref("extensions.hideUpdateButton", false);
pref("extensions.strictCompatibility", false);
pref("extensions.minCompatibleAppVersion", "11.0");

pref("extensions.update.url", "https://versioncheck.addons.mozilla.org/update/VersionCheck.php?reqVersion=%REQ_VERSION%&id=%ITEM_ID%&version=%ITEM_VERSION%&maxAppVersion=%ITEM_MAXAPPVERSION%&status=%ITEM_STATUS%&appID=%APP_ID%&appVersion=%APP_VERSION%&appOS=%APP_OS%&appABI=%APP_ABI%&locale=%APP_LOCALE%&currentAppVersion=%CURRENT_APP_VERSION%&updateType=%UPDATE_TYPE%&compatMode=%COMPATIBILITY_MODE%");
pref("extensions.update.background.url", "https://versioncheck-bg.addons.mozilla.org/update/VersionCheck.php?reqVersion=%REQ_VERSION%&id=%ITEM_ID%&version=%ITEM_VERSION%&maxAppVersion=%ITEM_MAXAPPVERSION%&status=%ITEM_STATUS%&appID=%APP_ID%&appVersion=%APP_VERSION%&appOS=%APP_OS%&appABI=%APP_ABI%&locale=%APP_LOCALE%&currentAppVersion=%CURRENT_APP_VERSION%&updateType=%UPDATE_TYPE%&compatMode=%COMPATIBILITY_MODE%");

pref("extensions.hotfix.id", "firefox-android-hotfix@mozilla.org");
pref("extensions.hotfix.cert.checkAttributes", true);
pref("extensions.hotfix.certs.1.sha1Fingerprint", "91:53:98:0C:C1:86:DF:47:8F:35:22:9E:11:C9:A7:31:04:49:A1:AA");

/* preferences for the Get Add-ons pane */
pref("extensions.getAddons.cache.enabled", true);
pref("extensions.getAddons.maxResults", 15);
pref("extensions.getAddons.recommended.browseURL", "https://addons.mozilla.org/%LOCALE%/android/recommended/");
pref("extensions.getAddons.recommended.url", "https://services.addons.mozilla.org/%LOCALE%/android/api/%API_VERSION%/list/featured/all/%MAX_RESULTS%/%OS%/%VERSION%");
pref("extensions.getAddons.search.browseURL", "https://addons.mozilla.org/%LOCALE%/android/search?q=%TERMS%&platform=%OS%&appver=%VERSION%");
pref("extensions.getAddons.search.url", "https://services.addons.mozilla.org/%LOCALE%/android/api/%API_VERSION%/search/%TERMS%/all/%MAX_RESULTS%/%OS%/%VERSION%/%COMPATIBILITY_MODE%");
pref("extensions.getAddons.browseAddons", "https://addons.mozilla.org/%LOCALE%/android/");
pref("extensions.getAddons.get.url", "https://services.addons.mozilla.org/%LOCALE%/android/api/%API_VERSION%/search/guid:%IDS%?src=mobile&appOS=%OS%&appVersion=%VERSION%");
pref("extensions.getAddons.getWithPerformance.url", "https://services.addons.mozilla.org/%LOCALE%/android/api/%API_VERSION%/search/guid:%IDS%?src=mobile&appOS=%OS%&appVersion=%VERSION%&tMain=%TIME_MAIN%&tFirstPaint=%TIME_FIRST_PAINT%&tSessionRestored=%TIME_SESSION_RESTORED%");

/* preference for the locale picker */
pref("extensions.getLocales.get.url", "");
pref("extensions.compatability.locales.buildid", "0");

/* blocklist preferences */
pref("extensions.blocklist.enabled", true);
// OneCRL freshness checking depends on this value, so if you change it,
// please also update security.onecrl.maximum_staleness_in_seconds.
pref("extensions.blocklist.interval", 86400);
pref("extensions.blocklist.url", "https://blocklist.addons.mozilla.org/blocklist/3/%APP_ID%/%APP_VERSION%/%PRODUCT%/%BUILD_ID%/%BUILD_TARGET%/%LOCALE%/%CHANNEL%/%OS_VERSION%/%DISTRIBUTION%/%DISTRIBUTION_VERSION%/%PING_COUNT%/%TOTAL_PING_COUNT%/%DAYS_SINCE_LAST_PING%/");
pref("extensions.blocklist.detailsURL", "https://www.mozilla.com/%LOCALE%/blocklist/");

/* Don't let XPIProvider install distribution add-ons; we do our own thing on mobile. */
pref("extensions.installDistroAddons", false);

/* block popups by default, and notify the user about blocked popups */
pref("dom.disable_open_during_load", true);
pref("privacy.popups.showBrowserMessage", true);

/* disable opening windows with the dialog feature */
pref("dom.disable_window_open_dialog_feature", true);
pref("dom.disable_window_showModalDialog", true);
pref("dom.disable_window_print", true);
pref("dom.disable_window_find", true);

pref("keyword.enabled", true);
pref("browser.fixup.domainwhitelist.localhost", true);

pref("accessibility.typeaheadfind", false);
pref("accessibility.typeaheadfind.timeout", 5000);
pref("accessibility.typeaheadfind.flashBar", 1);
pref("accessibility.typeaheadfind.linksonly", false);
pref("accessibility.typeaheadfind.casesensitive", 0);
pref("accessibility.browsewithcaret_shortcut.enabled", false);

// Whether the character encoding menu is under the main Firefox button. This
// preference is a string so that localizers can alter it.
pref("browser.menu.showCharacterEncoding", "chrome://browser/locale/browser.properties");

// pointer to the default engine name
pref("browser.search.defaultenginename", "chrome://browser/locale/region.properties");
// maximum number of search suggestions, as a string because the search service expects a string pref
pref("browser.search.param.maxSuggestions", "4");
// SSL error page behaviour
pref("browser.ssl_override_behavior", 2);
pref("browser.xul.error_pages.expert_bad_cert", false);

// ordering of search engines in the engine list.
pref("browser.search.order.1", "chrome://browser/locale/region.properties");
pref("browser.search.order.2", "chrome://browser/locale/region.properties");
pref("browser.search.order.3", "chrome://browser/locale/region.properties");

// Market-specific search defaults
pref("browser.search.geoSpecificDefaults", true);
pref("browser.search.geoSpecificDefaults.url", "https://search.services.mozilla.com/1/%APP%/%VERSION%/%CHANNEL%/%LOCALE%/%REGION%/%DISTRIBUTION%/%DISTRIBUTION_VERSION%");

// US specific default (used as a fallback if the geoSpecificDefaults request fails).
pref("browser.search.defaultenginename.US", "chrome://browser/locale/region.properties");
pref("browser.search.order.US.1", "chrome://browser/locale/region.properties");
pref("browser.search.order.US.2", "chrome://browser/locale/region.properties");
pref("browser.search.order.US.3", "chrome://browser/locale/region.properties");

// disable updating
pref("browser.search.update", false);

// enable tracking protection for private browsing
pref("privacy.trackingprotection.pbmode.enabled", true);

// disable search suggestions by default
pref("browser.search.suggest.enabled", false);
pref("browser.search.suggest.prompted", false);

// tell the search service that we don't really expose the "current engine"
pref("browser.search.noCurrentEngine", true);

// Control media casting & mirroring features
pref("browser.casting.enabled", true);
#ifdef RELEASE_BUILD
// Chromecast mirroring is broken (bug 1131084)
pref("browser.mirroring.enabled", false);
#else
pref("browser.mirroring.enabled", true);
#endif

// Enable sparse localization by setting a few package locale overrides
pref("chrome.override_package.global", "browser");
pref("chrome.override_package.mozapps", "browser");
pref("chrome.override_package.passwordmgr", "browser");

// enable xul error pages
pref("browser.xul.error_pages.enabled", true);

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

// Allow 24-bit colour when the hardware supports it
pref("gfx.android.rgb16.force", false);

// Allow GLContexts to be attached/detached from SurfaceTextures
pref("gfx.SurfaceTexture.detach.enabled", true);

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

// Image blocking policy
pref("browser.image_blocking.enabled", false);

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

// enable geo
pref("geo.enabled", true);

// content sink control -- controls responsiveness during page load
// see https://bugzilla.mozilla.org/show_bug.cgi?id=481566#c9
//pref("content.sink.enable_perf_mode",  2); // 0 - switch, 1 - interactive, 2 - perf
//pref("content.sink.pending_event_mode", 0);
//pref("content.sink.perf_deflect_count", 1000000);
//pref("content.sink.perf_parse_time", 50000000);

// Disable the JS engine's gc on memory pressure, since we do one in the mobile
// browser (bug 669346).
pref("javascript.options.gc_on_memory_pressure", false);

#ifdef MOZ_PKG_SPECIAL
// low memory devices
pref("javascript.options.mem.gc_high_frequency_heap_growth_max", 120);
pref("javascript.options.mem.gc_high_frequency_heap_growth_min", 120);
pref("javascript.options.mem.gc_high_frequency_high_limit_mb", 40);
pref("javascript.options.mem.gc_high_frequency_low_limit_mb", 10);
pref("javascript.options.mem.gc_low_frequency_heap_growth", 120);
pref("javascript.options.mem.high_water_mark", 16);
pref("javascript.options.mem.gc_allocation_threshold_mb", 3);
pref("javascript.options.mem.gc_decommit_threshold_mb", 1);
pref("javascript.options.mem.gc_min_empty_chunk_count", 1);
pref("javascript.options.mem.gc_max_empty_chunk_count", 2);
#else
pref("javascript.options.mem.high_water_mark", 32);
#endif

pref("dom.max_chrome_script_run_time", 0); // disable slow script dialog for chrome
pref("dom.max_script_run_time", 20);

// JS error console
pref("devtools.errorconsole.enabled", false);
// Absolute path to the devtools unix domain socket file used
// to communicate with a usb cable via adb forward.
pref("devtools.debugger.unix-domain-socket", "/data/data/@ANDROID_PACKAGE_NAME@/firefox-debugger-socket");

pref("devtools.remote.usb.enabled", false);
pref("devtools.remote.wifi.enabled", false);

pref("font.size.inflation.minTwips", 0);

// When true, zooming will be enabled on all sites, even ones that declare user-scalable=no.
pref("browser.ui.zoom.force-user-scalable", false);

pref("ui.zoomedview.enabled", true);
pref("ui.zoomedview.keepLimitSize", 16); // value in layer pixels, used to not keep the large elements in the cluster list (Bug 1191041)
pref("ui.zoomedview.limitReadableSize", 8); // value in layer pixels
pref("ui.zoomedview.defaultZoomFactor", 2);
pref("ui.zoomedview.simplified", true); // Do not display all the zoomed view controls

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

// The percentage of the screen that needs to be scrolled before toolbar
// manipulation is allowed.
pref("browser.ui.scroll-toolbar-threshold", 10);

// Maximum distance from the point where the user pressed where we still
// look for text to select
pref("browser.ui.selection.distance", 250);

// plugins
pref("plugin.disable", false);
pref("dom.ipc.plugins.enabled", false);

// This pref isn't actually used anymore, but we're leaving this here to avoid changing
// the default so that we can migrate a user-set pref. See bug 885357.
pref("plugins.click_to_play", true);
// The default value for nsIPluginTag.enabledState (STATE_CLICKTOPLAY = 1)
pref("plugin.default.state", 1);

// product URLs
// The breakpad report server to link to in about:crashes
pref("breakpad.reportURL", "https://crash-stats.mozilla.com/report/index/");
pref("app.support.baseURL", "http://support.mozilla.org/1/mobile/%VERSION%/%OS%/%LOCALE%/");
// Used to submit data to input from about:feedback
pref("app.feedback.postURL", "https://input.mozilla.org/api/v1/feedback/");
pref("app.privacyURL", "https://www.mozilla.org/privacy/firefox/");
pref("app.creditsURL", "http://www.mozilla.org/credits/");
pref("app.channelURL", "http://www.mozilla.org/%LOCALE%/firefox/channel/");
#if MOZ_UPDATE_CHANNEL == aurora
pref("app.releaseNotesURL", "http://www.mozilla.com/%LOCALE%/mobile/%VERSION%/auroranotes/");
#elif MOZ_UPDATE_CHANNEL == beta
pref("app.releaseNotesURL", "http://www.mozilla.com/%LOCALE%/mobile/%VERSION%beta/releasenotes/");
#else
pref("app.releaseNotesURL", "http://www.mozilla.com/%LOCALE%/mobile/%VERSION%/releasenotes/");
#endif
#if MOZ_UPDATE_CHANNEL == beta
pref("app.faqURL", "http://www.mozilla.com/%LOCALE%/mobile/beta/faq/");
#else
pref("app.faqURL", "http://www.mozilla.com/%LOCALE%/mobile/faq/");
#endif

// Name of alternate about: page for certificate errors (when undefined, defaults to about:neterror)
pref("security.alternate_certificate_error_page", "certerror");

pref("security.warn_viewing_mixed", false); // Warning is disabled.  See Bug 616712.

// Block insecure active content on https pages
pref("security.mixed_content.block_active_content", true);

// Enable pinning
pref("security.cert_pinning.enforcement_level", 1);

// Allow SHA-1 certificates only before 2016-01-01
pref("security.pki.sha1_enforcement_level", 2);

// Required blocklist freshness for OneCRL OCSP bypass
// (default is 1.25x extensions.blocklist.interval, or 30 hours)
pref("security.onecrl.maximum_staleness_in_seconds", 108000);

// Only fetch OCSP for EV certificates
pref("security.OCSP.enabled", 2);

// Override some named colors to avoid inverse OS themes
pref("ui.-moz-dialog", "#efebe7");
pref("ui.-moz-dialogtext", "#101010");
pref("ui.-moz-field", "#fff");
pref("ui.-moz-fieldtext", "#1a1a1a");
pref("ui.-moz-buttonhoverface", "#f3f0ed");
pref("ui.-moz-buttonhovertext", "#101010");
pref("ui.-moz-combobox", "#fff");
pref("ui.-moz-comboboxtext", "#101010");
pref("ui.buttonface", "#ece7e2");
pref("ui.buttonhighlight", "#fff");
pref("ui.buttonshadow", "#aea194");
pref("ui.buttontext", "#101010");
pref("ui.captiontext", "#101010");
pref("ui.graytext", "#b1a598");
pref("ui.highlight", "#fad184");
pref("ui.highlighttext", "#1a1a1a");
pref("ui.infobackground", "#f5f5b5");
pref("ui.infotext", "#000");
pref("ui.menu", "#f7f5f3");
pref("ui.menutext", "#101010");
pref("ui.threeddarkshadow", "#000");
pref("ui.threedface", "#ece7e2");
pref("ui.threedhighlight", "#fff");
pref("ui.threedlightshadow", "#ece7e2");
pref("ui.threedshadow", "#aea194");
pref("ui.window", "#efebe7");
pref("ui.windowtext", "#101010");
pref("ui.windowframe", "#efebe7");

/* prefs used by the update timer system (including blocklist pings) */
pref("app.update.timerFirstInterval", 30000); // milliseconds
pref("app.update.timerMinimumDelay", 30); // seconds

// used by update service to decide whether or not to
// automatically download an update
pref("app.update.autodownload", "wifi");
pref("app.update.url.android", "https://aus5.mozilla.org/update/4/%PRODUCT%/%VERSION%/%BUILD_ID%/%BUILD_TARGET%/%LOCALE%/%CHANNEL%/%OS_VERSION%/%DISTRIBUTION%/%DISTRIBUTION_VERSION%/%MOZ_VERSION%/update.xml");

#ifdef MOZ_UPDATER
/* prefs used specifically for updating the app */
pref("app.update.enabled", false);
pref("app.update.channel", "@MOZ_UPDATE_CHANNEL@");

#endif

// replace newlines with spaces on paste into single-line text boxes
pref("editor.singleLine.pasteNewlines", 2);

// threshold where a tap becomes a drag, in 1/240" reference pixels
// The names of the preferences are to be in sync with EventStateManager.cpp
pref("ui.dragThresholdX", 25);
pref("ui.dragThresholdY", 25);

pref("layers.acceleration.disabled", false);
pref("layers.offmainthreadcomposition.enabled", true);
pref("layers.async-video.enabled", true);
#ifdef MOZ_ANDROID_APZ
pref("layers.async-pan-zoom.enabled", true);
#endif
pref("apz.allow_zooming", true);
pref("layers.progressive-paint", true);
pref("layers.low-precision-buffer", true);
pref("layers.low-precision-resolution", "0.25");
pref("layers.low-precision-opacity", "1.0");
// We want to limit layers for two reasons:
// 1) We can't scroll smoothly if we have to many draw calls
// 2) Pages that have too many layers consume too much memory and crash.
// By limiting the number of layers on mobile we're making the main thread
// work harder keep scrolling smooth and memory low.
pref("layers.max-active", 20);

pref("notification.feature.enabled", true);
pref("dom.webnotifications.enabled", true);

// prevent tooltips from showing up
pref("browser.chrome.toolbar_tips", false);

// prevent video elements from preloading too much data
pref("media.preload.default", 1); // default to preload none
pref("media.preload.auto", 2);    // preload metadata if preload=auto
pref("media.cache_size", 32768);    // 32MB media cache
// Try to save battery by not resuming reading from a connection until we fall
// below 10s of buffered data.
pref("media.cache_resume_threshold", 10);
pref("media.cache_readahead_limit", 30);

// Number of video frames we buffer while decoding video.
// On Android this is decided by a similar value which varies for
// each OMX decoder |OMX_PARAM_PORTDEFINITIONTYPE::nBufferCountMin|. This
// number must be less than the OMX equivalent or gecko will think it is
// chronically starved of video frames. All decoders seen so far have a value
// of at least 4.
pref("media.video-queue.default-size", 3);

// Enable the MediaCodec PlatformDecoderModule by default.
pref("media.fragmented-mp4.exposed", true);
pref("media.fragmented-mp4.enabled", true);
pref("media.fragmented-mp4.android-media-codec.enabled", true);
pref("media.fragmented-mp4.android-media-codec.preferred", true);

// Enable MSE
pref("media.mediasource.enabled", true);

// optimize images memory usage
pref("image.downscale-during-decode.enabled", true);

#ifdef NIGHTLY_BUILD
// Shumway component (SWF player) is disabled by default. Also see bug 904346.
pref("shumway.disabled", true);
#endif

// enable touch events interfaces
pref("dom.w3c_touch_events.enabled", 1);

#ifdef MOZ_SAFE_BROWSING
pref("browser.safebrowsing.enabled", true);
pref("browser.safebrowsing.malware.enabled", true);
pref("browser.safebrowsing.downloads.enabled", false);
pref("browser.safebrowsing.downloads.remote.enabled", false);
pref("browser.safebrowsing.downloads.remote.timeout_ms", 10000);
pref("browser.safebrowsing.debug", false);

pref("browser.safebrowsing.provider.google.lists", "goog-badbinurl-shavar,goog-downloadwhite-digest256,goog-phish-shavar,goog-malware-shavar,goog-unwanted-shavar");
pref("browser.safebrowsing.provider.google.updateURL", "https://safebrowsing.google.com/safebrowsing/downloads?client=SAFEBROWSING_ID&appver=%VERSION%&pver=2.2&key=%GOOGLE_API_KEY%");
pref("browser.safebrowsing.provider.google.gethashURL", "https://safebrowsing.google.com/safebrowsing/gethash?client=SAFEBROWSING_ID&appver=%VERSION%&pver=2.2");
pref("browser.safebrowsing.provider.google.reportURL", "https://safebrowsing.google.com/safebrowsing/diagnostic?client=%NAME%&hl=%LOCALE%&site=");
pref("browser.safebrowsing.provider.google.appRepURL", "https://sb-ssl.google.com/safebrowsing/clientreport/download?key=%GOOGLE_API_KEY%");

pref("browser.safebrowsing.reportPhishMistakeURL", "https://%LOCALE%.phish-error.mozilla.com/?hl=%LOCALE%&url=");
pref("browser.safebrowsing.reportPhishURL", "https://%LOCALE%.phish-report.mozilla.com/?hl=%LOCALE%&url=");
pref("browser.safebrowsing.reportMalwareMistakeURL", "https://%LOCALE%.malware-error.mozilla.com/?hl=%LOCALE%&url=");

pref("browser.safebrowsing.id", @MOZ_APP_UA_NAME@);

// Name of the about: page contributed by safebrowsing to handle display of error
// pages on phishing/malware hits.  (bug 399233)
pref("urlclassifier.alternate_error_page", "blocked");

// The number of random entries to send with a gethash request.
pref("urlclassifier.gethashnoise", 4);

// Gethash timeout for Safebrowsing.
pref("urlclassifier.gethash.timeout_ms", 5000);

// If an urlclassifier table has not been updated in this number of seconds,
// a gethash request will be forced to check that the result is still in
// the database.
pref("urlclassifier.max-complete-age", 2700);
#endif

// URL for posting tiles metrics.
#ifdef RELEASE_BUILD
pref("browser.tiles.reportURL", "https://tiles.services.mozilla.com/v2/links/click");
#endif

// True if this is the first time we are showing about:firstrun
pref("browser.firstrun.show.uidiscovery", true);
pref("browser.firstrun.show.localepicker", false);

// True if you always want dump() to work
//
// On Android, you also need to do the following for the output
// to show up in logcat:
//
// $ adb shell stop
// $ adb shell setprop log.redirect-stdio true
// $ adb shell start
pref("browser.dom.window.dump.enabled", true);

// SimplePush
pref("services.push.enabled", false);

// controls if we want camera support
pref("device.camera.enabled", true);
pref("media.realtime_decoder.enabled", true);

pref("dom.report_all_js_exceptions", true);
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
// Negate scrollY, true will make the mouse scroll wheel move the screen the same direction as with most desktops or laptops.
pref("ui.scrolling.negate_wheel_scrollY", true);
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

// increase the timeout clamp for background tabs to 15 minutes
pref("dom.min_background_timeout_value", 900000);

// Media plugins for libstagefright playback on android
pref("media.plugins.enabled", true);

// Stagefright's OMXCodec::CreationFlags. The interesting flag values are:
//  0 = Let Stagefright choose hardware or software decoding (default)
//  8 = Force software decoding
// 16 = Force hardware decoding
pref("media.stagefright.omxcodec.flags", 0);

// Coalesce touch events to prevent them from flooding the event queue
pref("dom.event.touch.coalescing.enabled", false);

// default orientation for the app, default to undefined
// the java GeckoScreenOrientationListener needs this to be defined
pref("app.orientation.default", "");

// On memory pressure, release dirty but unused pages held by jemalloc
// back to the system.
pref("memory.free_dirty_pages", true);

pref("layout.imagevisibility.numscrollportwidths", 1);
pref("layout.imagevisibility.numscrollportheights", 1);

pref("layers.enable-tiles", true);

// Enable the dynamic toolbar
pref("browser.chrome.dynamictoolbar", true);

// The mode of browser titlebar
// 0: Show a current page title.
// 1: Show a current page url.
pref("browser.chrome.titlebarMode", 1);

// Hide common parts of URLs like "www." or "http://"
pref("browser.urlbar.trimURLs", true);

#ifdef MOZ_PKG_SPECIAL
// Disable webgl on ARMv6 because running the reftests takes
// too long for some reason (bug 843738)
pref("webgl.disabled", true);
#endif

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

// WebPayment
pref("dom.mozPay.enabled", true);

pref("dom.payment.provider.0.name", "Firefox Marketplace");
pref("dom.payment.provider.0.description", "marketplace.firefox.com");
pref("dom.payment.provider.0.uri", "https://marketplace.firefox.com/mozpay/?req=");
pref("dom.payment.provider.0.type", "mozilla/payments/pay/v1");
pref("dom.payment.provider.0.requestMethod", "GET");

// Shortnumber matching needed for e.g. Brazil:
// 01187654321 can be found with 87654321
pref("dom.phonenumber.substringmatching.BR", 8);
pref("dom.phonenumber.substringmatching.CO", 10);
pref("dom.phonenumber.substringmatching.VE", 7);

// Enable hardware-accelerated Skia canvas
pref("gfx.canvas.azure.backends", "skia");
pref("gfx.canvas.azure.accelerated", true);

// See ua-update.json.in for the packaged UA override list
pref("general.useragent.updates.enabled", true);
pref("general.useragent.updates.url", "https://dynamicua.cdn.mozilla.net/0/%APP_ID%");
pref("general.useragent.updates.interval", 604800); // 1 week
pref("general.useragent.updates.retry", 86400); // 1 day

// When true, phone number linkification is enabled.
pref("browser.ui.linkify.phone", false);

// Enables/disables Spatial Navigation
pref("snav.enabled", true);

// This url, if changed, MUST continue to point to an https url. Pulling arbitrary content to inject into
// this page over http opens us up to a man-in-the-middle attack that we'd rather not face. If you are a downstream
// repackager of this code using an alternate snippet url, please keep your users safe
pref("browser.snippets.updateUrl", "https://snippets.mozilla.com/json/%SNIPPETS_VERSION%/%NAME%/%VERSION%/%APPBUILDID%/%BUILD_TARGET%/%LOCALE%/%CHANNEL%/%OS_VERSION%/%DISTRIBUTION%/%DISTRIBUTION_VERSION%/");

// How frequently we check for new snippets, in seconds (1 day)
pref("browser.snippets.updateInterval", 86400);

// URL used to check for user's country code
pref("browser.snippets.geoUrl", "https://geo.mozilla.org/country.json");

// URL used to ping metrics with stats about which snippets have been shown
pref("browser.snippets.statsUrl", "https://snippets-stats.mozilla.org/mobile");

// These prefs require a restart to take effect.
pref("browser.snippets.enabled", true);
pref("browser.snippets.syncPromo.enabled", true);
pref("browser.snippets.firstrunHomepage.enabled", true);

// The URL of the APK factory from which we obtain APKs for webapps.
pref("browser.webapps.apkFactoryUrl", "https://controller.apk.firefox.com/application.apk");

// How frequently to check for webapp updates, in seconds (86400 is daily).
pref("browser.webapps.updateInterval", 86400);

// Whether or not to check for updates.  Enabled by default, but the runtime
// disables it for webapp profiles on firstrun, so only the main Fennec process
// checks for updates (to avoid duplicate update notifications).
//
// In the future, we might want to make each webapp process check for updates
// for its own webapp, in which case we'll need to have a third state for this
// preference.  Thus it's an integer rather than a boolean.
//
// Possible values:
//   0: don't check for updates
//   1: do check for updates
pref("browser.webapps.checkForUpdates", 1);

// The URL of the service that checks for updates.
// To test updates, set this to http://apk-update-checker.paas.allizom.org,
// which is a test server that always reports all apps as having updates.
pref("browser.webapps.updateCheckUrl", "https://controller.apk.firefox.com/app_updates");

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
pref("reader.color_scheme", "auto");

// Color scheme values available in reader mode UI.
pref("reader.color_scheme.values", "[\"dark\",\"auto\",\"light\"]");

// Whether to use a vertical or horizontal toolbar.
pref("reader.toolbar.vertical", false);

// Whether or not to display buttons related to reading list in reader view.
pref("browser.readinglist.enabled", true);

// Telemetry settings.
// Whether to use the unified telemetry behavior, requires a restart.
pref("toolkit.telemetry.unified", false);

// Turn off selection caret by default
pref("selectioncaret.enabled", false);

// Selection carets never fall-back to internal LongTap detector.
pref("selectioncaret.detects.longtap", false);

// Selection carets override caret visibility.
pref("selectioncaret.visibility.affectscaret", true);

// Selection caret visibility observes composition
// selections generated by soft keyboard managers.
pref("selectioncaret.observes.compositions", true);

// Turn off touch caret by default.
pref("touchcaret.enabled", false);

// TouchCaret never auto-hides.
pref("touchcaret.expiration.time", 0);

// Touch caret stays visible under a wider range of conditions
// than the default b2g. We can display the caret in empty editables
// for example, and do not auto-hide until loss of focus.
pref("touchcaret.extendedvisibility", true);

// The TouchCaret and the SelectionCarets will indicate when the
// TextSelection actionbar is to be openned or closed.
pref("caret.manages-android-actionbar", true);

// Disable sending console to logcat on release builds.
#ifdef RELEASE_BUILD
pref("consoleservice.logcat", false);
#else
pref("consoleservice.logcat", true);
#endif

// Enable Cardboard VR on mobile, assuming VR at all is enabled
pref("dom.vr.cardboard.enabled", true);

pref("browser.tabs.showAudioPlayingIcon", true);

// Enable service workers and fetch interception on Nightly Fennec
#ifdef NIGHTLY_BUILD
pref("dom.serviceWorkers.enabled", true);
pref("dom.serviceWorkers.interception.enabled", true);
#endif

// The remote content URL where FxAccountsWebChannel messages originate.  Must use HTTPS.
pref("identity.fxaccounts.remote.webchannel.uri", "https://accounts.firefox.com");

// The remote URL of the Firefox Account profile server.
pref("identity.fxaccounts.remote.profile.uri", "https://profile.accounts.firefox.com/v1");

// The remote URL of the Firefox Account oauth server.
pref("identity.fxaccounts.remote.oauth.uri", "https://oauth.accounts.firefox.com/v1"); 

// Token server used by Firefox Account-authenticated Sync.
pref("identity.sync.tokenserver.uri", "https://token.services.mozilla.com/1.0/sync/1.5");
