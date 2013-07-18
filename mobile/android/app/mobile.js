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

pref("browser.tabs.remote", false);

// If a tab has not been active for this long (seconds), then it may be
// turned into a zombie tab to preemptively free up memory. -1 disables time-based
// expiration (but low-memory conditions may still require the tab to be zombified).
pref("browser.tabs.expireTime", 3600);

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

/* image cache prefs */
pref("image.cache.size", 1048576); // bytes
pref("image.high_quality_downscaling.enabled", false);

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
pref("network.http.keep-alive.timeout", 600);
pref("network.http.max-connections", 20);
pref("network.http.max-persistent-connections-per-server", 6);
pref("network.http.max-persistent-connections-per-proxy", 20);

// spdy
pref("network.http.spdy.push-allowance", 32768);

// See bug 545869 for details on why these are set the way they are
pref("network.buffer.cache.count", 24);
pref("network.buffer.cache.size",  16384);

/* history max results display */
pref("browser.display.history.maxresults", 100);

/* How many times should have passed before the remote tabs list is refreshed */
pref("browser.display.remotetabs.timeout", 10);

/* session history */
pref("browser.sessionhistory.max_total_viewers", 1);
pref("browser.sessionhistory.max_entries", 50);

/* session store */
pref("browser.sessionstore.resume_session_once", false);
pref("browser.sessionstore.resume_from_crash", true);
pref("browser.sessionstore.interval", 10000); // milliseconds
pref("browser.sessionstore.max_tabs_undo", 1);
pref("browser.sessionstore.max_resumed_crashes", 1);
pref("browser.sessionstore.recent_crashes", 0);

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

/* download helper */
pref("browser.helperApps.deleteTempFileOnExit", false);

/* password manager */
pref("signon.rememberSignons", true);
pref("signon.expireMasterPassword", false);
pref("signon.SignonFileName", "signons.txt");
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

/* extension manager and xpinstall */
pref("xpinstall.whitelist.add", "addons.mozilla.org");
pref("xpinstall.whitelist.add.180", "marketplace.firefox.com");

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
pref("extensions.blocklist.interval", 86400);
pref("extensions.blocklist.url", "https://addons.mozilla.org/blocklist/3/%APP_ID%/%APP_VERSION%/%PRODUCT%/%BUILD_ID%/%BUILD_TARGET%/%LOCALE%/%CHANNEL%/%OS_VERSION%/%DISTRIBUTION%/%DISTRIBUTION_VERSION%/%PING_COUNT%/%TOTAL_PING_COUNT%/%DAYS_SINCE_LAST_PING%/");
pref("extensions.blocklist.detailsURL", "https://www.mozilla.com/%LOCALE%/blocklist/");

/* block popups by default, and notify the user about blocked popups */
pref("dom.disable_open_during_load", true);
pref("privacy.popups.showBrowserMessage", true);

/* disable opening windows with the dialog feature */
pref("dom.disable_window_open_dialog_feature", true);
pref("dom.disable_window_showModalDialog", true);
pref("dom.disable_window_print", true);
pref("dom.disable_window_find", true);

pref("keyword.enabled", true);

pref("accessibility.typeaheadfind", false);
pref("accessibility.typeaheadfind.timeout", 5000);
pref("accessibility.typeaheadfind.flashBar", 1);
pref("accessibility.typeaheadfind.linksonly", false);
pref("accessibility.typeaheadfind.casesensitive", 0);
// zoom key(F7) conflicts with caret browsing on maemo
pref("accessibility.browsewithcaret_shortcut.enabled", false);

// Whether the character encoding menu is under the main Firefox button. This
// preference is a string so that localizers can alter it.
pref("browser.menu.showCharacterEncoding", "chrome://browser/locale/browser.properties");
pref("intl.charsetmenu.browser.static", "chrome://browser/locale/browser.properties");

// pointer to the default engine name
pref("browser.search.defaultenginename", "chrome://browser/locale/region.properties");
// SSL error page behaviour
pref("browser.ssl_override_behavior", 2);
pref("browser.xul.error_pages.expert_bad_cert", false);

// disable logging for the search service by default
pref("browser.search.log", false);

// ordering of search engines in the engine list.
pref("browser.search.order.1", "chrome://browser/locale/region.properties");
pref("browser.search.order.2", "chrome://browser/locale/region.properties");

// disable updating
pref("browser.search.update", false);
pref("browser.search.update.log", false);
pref("browser.search.updateinterval", 6);

// disable search suggestions by default
pref("browser.search.suggest.enabled", false);
pref("browser.search.suggest.prompted", false);

// Tell the search service to load search plugins from the locale JAR
pref("browser.search.loadFromJars", true);
pref("browser.search.jarURIs", "chrome://browser/locale/searchplugins/");

// tell the search service that we don't really expose the "current engine"
pref("browser.search.noCurrentEngine", true);

#ifdef MOZ_OFFICIAL_BRANDING
// {moz:official} expands to "official"
pref("browser.search.official", true);
#endif

// Enable sparse localization by setting a few package locale overrides
pref("chrome.override_package.global", "browser");
pref("chrome.override_package.mozapps", "browser");
pref("chrome.override_package.passwordmgr", "browser");

// enable xul error pages
pref("browser.xul.error_pages.enabled", true);

// Specify emptyRestriction = 0 so that bookmarks appear in the list by default
pref("browser.urlbar.default.behavior", 0);
pref("browser.urlbar.default.behavior.emptyRestriction", 0);

// Let the faviconservice know that we display favicons as 32x32px so that it
// uses the right size when optimizing favicons
pref("places.favicons.optimizeToDimension", 32);

// various and sundry awesomebar prefs (should remove/re-evaluate
// these once bug 447900 is fixed)
pref("browser.urlbar.clickSelectsAll", true);
pref("browser.urlbar.doubleClickSelectsAll", true);
pref("browser.urlbar.autoFill", false);
pref("browser.urlbar.matchOnlyTyped", false);
pref("browser.urlbar.matchBehavior", 1);
pref("browser.urlbar.filter.javascript", true);
pref("browser.urlbar.maxRichResults", 24); // increased so we see more results when portrait
pref("browser.urlbar.search.chunkSize", 1000);
pref("browser.urlbar.search.timeout", 100);
pref("browser.urlbar.restrict.history", "^");
pref("browser.urlbar.restrict.bookmark", "*");
pref("browser.urlbar.restrict.tag", "+");
pref("browser.urlbar.match.title", "#");
pref("browser.urlbar.match.url", "@");
pref("browser.urlbar.autocomplete.search_threshold", 5);
pref("browser.history.grouping", "day");
pref("browser.history.showSessions", false);
pref("browser.sessionhistory.max_entries", 50);
pref("browser.history_expire_days", 180);
pref("browser.history_expire_days_min", 90);
pref("browser.history_expire_sites", 40000);
pref("browser.places.migratePostDataAnnotations", true);
pref("browser.places.updateRecentTagsUri", true);
pref("places.frecency.numVisits", 10);
pref("places.frecency.numCalcOnIdle", 50);
pref("places.frecency.numCalcOnMigrate", 50);
pref("places.frecency.updateIdleTime", 60000);
pref("places.frecency.firstBucketCutoff", 4);
pref("places.frecency.secondBucketCutoff", 14);
pref("places.frecency.thirdBucketCutoff", 31);
pref("places.frecency.fourthBucketCutoff", 90);
pref("places.frecency.firstBucketWeight", 100);
pref("places.frecency.secondBucketWeight", 70);
pref("places.frecency.thirdBucketWeight", 50);
pref("places.frecency.fourthBucketWeight", 30);
pref("places.frecency.defaultBucketWeight", 10);
pref("places.frecency.embedVisitBonus", 0);
pref("places.frecency.linkVisitBonus", 100);
pref("places.frecency.typedVisitBonus", 2000);
pref("places.frecency.bookmarkVisitBonus", 150);
pref("places.frecency.downloadVisitBonus", 0);
pref("places.frecency.permRedirectVisitBonus", 0);
pref("places.frecency.tempRedirectVisitBonus", 0);
pref("places.frecency.defaultVisitBonus", 0);
pref("places.frecency.unvisitedBookmarkBonus", 140);
pref("places.frecency.unvisitedTypedBonus", 200);

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

// controls which bits of private data to clear. by default we clear them all.
pref("privacy.item.cache", true);
pref("privacy.item.cookies", true);
pref("privacy.item.offlineApps", true);
pref("privacy.item.history", true);
pref("privacy.item.formdata", true);
pref("privacy.item.downloads", true);
pref("privacy.item.passwords", true);
pref("privacy.item.sessions", true);
pref("privacy.item.geolocation", true);
pref("privacy.item.siteSettings", true);
pref("privacy.item.syncAccount", true);

// enable geo
pref("geo.enabled", true);
pref("app.geo.reportdata", 0);

// content sink control -- controls responsiveness during page load
// see https://bugzilla.mozilla.org/show_bug.cgi?id=481566#c9
//pref("content.sink.enable_perf_mode",  2); // 0 - switch, 1 - interactive, 2 - perf
//pref("content.sink.pending_event_mode", 0);
//pref("content.sink.perf_deflect_count", 1000000);
//pref("content.sink.perf_parse_time", 50000000);

// Disable methodjit in chrome to save memory
pref("javascript.options.methodjit.chrome",  false);

// Disable the JS engine's gc on memory pressure, since we do one in the mobile
// browser (bug 669346).
pref("javascript.options.gc_on_memory_pressure", false);

#ifdef MOZ_PKG_SPECIAL
// low memory devices
pref("javascript.options.mem.gc_high_frequency_heap_growth_max", 120);
pref("javascript.options.mem.gc_high_frequency_heap_growth_min", 101);
pref("javascript.options.mem.gc_high_frequency_high_limit_mb", 40);
pref("javascript.options.mem.gc_high_frequency_low_limit_mb", 10);
pref("javascript.options.mem.gc_low_frequency_heap_growth", 105);
pref("javascript.options.mem.high_water_mark", 16);
pref("javascript.options.mem.gc_allocation_threshold_mb", 3);
pref("javascript.options.mem.gc_decommit_threshold_mb", 1);
#else
pref("javascript.options.mem.high_water_mark", 32);
#endif

pref("dom.max_chrome_script_run_time", 0); // disable slow script dialog for chrome
pref("dom.max_script_run_time", 20);

// JS error console
pref("devtools.errorconsole.enabled", false);

pref("font.size.inflation.minTwips", 120);

// When true, zooming will be enabled on all sites, even ones that declare user-scalable=no.
pref("browser.ui.zoom.force-user-scalable", false);

// Touch radius (area around the touch location to look for target elements),
// in 1/240-inch pixels:
pref("browser.ui.touch.left", 32);
pref("browser.ui.touch.right", 32);
pref("browser.ui.touch.top", 48);
pref("browser.ui.touch.bottom", 16);
pref("browser.ui.touch.weight.visited", 120); // percentage

// The percentage of the screen that needs to be scrolled before margins are exposed.
pref("browser.ui.show-margins-threshold", 20);

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
pref("app.feedback.postURL", "https://input.mozilla.org/%LOCALE%/feedback");
pref("app.privacyURL", "https://www.mozilla.org/legal/privacy/firefox.html");
pref("app.creditsURL", "http://www.mozilla.org/credits/");
pref("app.channelURL", "http://www.mozilla.org/%LOCALE%/firefox/channel/");
#if MOZ_UPDATE_CHANNEL == aurora
pref("app.releaseNotesURL", "http://www.mozilla.com/%LOCALE%/mobile/%VERSION%/auroranotes/");
#else
pref("app.releaseNotesURL", "http://www.mozilla.com/%LOCALE%/mobile/%VERSION%/releasenotes/");
#endif
#if MOZ_UPDATE_CHANNEL == beta
pref("app.faqURL", "http://www.mozilla.com/%LOCALE%/mobile/beta/faq/");
#else
pref("app.faqURL", "http://www.mozilla.com/%LOCALE%/mobile/faq/");
#endif
pref("app.marketplaceURL", "https://marketplace.firefox.com/");

// Name of alternate about: page for certificate errors (when undefined, defaults to about:neterror)
pref("security.alternate_certificate_error_page", "certerror");

pref("security.warn_viewing_mixed", false); // Warning is disabled.  See Bug 616712.

#ifdef NIGHTLY_BUILD
// Block insecure active content on https pages
pref("security.mixed_content.block_active_content", true);
#endif

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

#ifdef MOZ_OFFICIAL_BRANDING
pref("browser.search.param.yahoo-fr", "moz35");
pref("browser.search.param.yahoo-fr-cjkt", "moz35");
pref("browser.search.param.yahoo-fr-ja", "mozff");
#endif

/* prefs used by the update timer system (including blocklist pings) */
pref("app.update.timerFirstInterval", 30000); // milliseconds
pref("app.update.timerMinimumDelay", 30); // seconds

// used by update service to decide whether or not to
// automatically download an update
pref("app.update.autodownload", "wifi");

#ifdef MOZ_UPDATER
/* prefs used specifically for updating the app */
pref("app.update.enabled", false);
pref("app.update.channel", "@MOZ_UPDATE_CHANNEL@");

// If you are looking for app.update.url, we no longer use it.
// See mobile/android/base/UpdateServiceHelper.java.in
#endif

// replace newlines with spaces on paste into single-line text boxes
pref("editor.singleLine.pasteNewlines", 2);

// threshold where a tap becomes a drag, in 1/240" reference pixels
// The names of the preferences are to be in sync with nsEventStateManager.cpp
pref("ui.dragThresholdX", 25);
pref("ui.dragThresholdY", 25);

pref("layers.acceleration.disabled", false);
pref("layers.offmainthreadcomposition.enabled", true);
pref("layers.async-video.enabled", true);
pref("layers.progressive-paint", true);
pref("layers.low-precision-buffer", true);
pref("layers.low-precision-resolution", 250);
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
pref("indexedDB.feature.enabled", true);
pref("dom.indexedDB.warningQuota", 5);

// prevent video elements from preloading too much data
pref("media.preload.default", 1); // default to preload none
pref("media.preload.auto", 2);    // preload metadata if preload=auto

// optimize images memory usage
pref("image.mem.decodeondraw", true);
pref("content.image.allow_locking", false);
pref("image.mem.min_discard_timeout_ms", 10000);

// enable touch events interfaces
pref("dom.w3c_touch_events.enabled", 1);

#ifdef MOZ_SAFE_BROWSING
pref("browser.safebrowsing.enabled", true);
pref("browser.safebrowsing.malware.enabled", true);
pref("browser.safebrowsing.debug", false);

pref("browser.safebrowsing.updateURL", "http://safebrowsing.clients.google.com/safebrowsing/downloads?client=SAFEBROWSING_ID&appver=%VERSION%&pver=2.2");
pref("browser.safebrowsing.keyURL", "https://sb-ssl.google.com/safebrowsing/newkey?client=SAFEBROWSING_ID&appver=%VERSION%&pver=2.2");
pref("browser.safebrowsing.gethashURL", "http://safebrowsing.clients.google.com/safebrowsing/gethash?client=SAFEBROWSING_ID&appver=%VERSION%&pver=2.2");
pref("browser.safebrowsing.reportURL", "http://safebrowsing.clients.google.com/safebrowsing/report?");
pref("browser.safebrowsing.reportGenericURL", "http://%LOCALE%.phish-generic.mozilla.com/?hl=%LOCALE%");
pref("browser.safebrowsing.reportErrorURL", "http://%LOCALE%.phish-error.mozilla.com/?hl=%LOCALE%");
pref("browser.safebrowsing.reportPhishURL", "http://%LOCALE%.phish-report.mozilla.com/?hl=%LOCALE%");
pref("browser.safebrowsing.reportMalwareURL", "http://%LOCALE%.malware-report.mozilla.com/?hl=%LOCALE%");
pref("browser.safebrowsing.reportMalwareErrorURL", "http://%LOCALE%.malware-error.mozilla.com/?hl=%LOCALE%");

pref("browser.safebrowsing.warning.infoURL", "http://www.mozilla.com/%LOCALE%/firefox/phishing-protection/");
pref("browser.safebrowsing.malware.reportURL", "http://safebrowsing.clients.google.com/safebrowsing/diagnostic?client=%NAME%&hl=%LOCALE%&site=");

pref("browser.safebrowsing.id", @MOZ_APP_UA_NAME@);

// Name of the about: page contributed by safebrowsing to handle display of error
// pages on phishing/malware hits.  (bug 399233)
pref("urlclassifier.alternate_error_page", "blocked");

// The number of random entries to send with a gethash request.
pref("urlclassifier.gethashnoise", 4);

// The list of tables that use the gethash request to confirm partial results.
pref("urlclassifier.gethashtables", "goog-phish-shavar,goog-malware-shavar");

// If an urlclassifier table has not been updated in this number of seconds,
// a gethash request will be forced to check that the result is still in
// the database.
pref("urlclassifier.max-complete-age", 2700);
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


// Enable accessibility mode if platform accessibility is enabled.
pref("accessibility.accessfu.activate", 2);
pref("accessibility.accessfu.quicknav_modes", "Link,Heading,FormElement,Landmark,ListItem");
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

// The default state of reader mode works on loaded a page.
pref("reader.parse-on-load.enabled", true);

// Force to enable reader mode to parse on loaded a page.
// Allow reader mode even on low-memory platforms
pref("reader.parse-on-load.force-enabled", false);

// The default of font size in reader (1-5)
pref("reader.font_size", 3);

// The default color scheme in reader (light, dark, sepia, auto)
// auto = color automatically adjusts according to ambient light level
pref("reader.color_scheme", "auto");

// The font type in reader (sans-serif, serif)
pref("reader.font_type", "sans-serif");

// Used to show a first-launch tip in reader
pref("reader.has_used_toolbar", false);

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

pref("layout.imagevisibility.enabled", false);

// Enable the dynamic toolbar
pref("browser.chrome.dynamictoolbar", true);

// The mode of browser titlebar
// 0: Show a current page title.
// 1: Show a current page url.
pref("browser.chrome.titlebarMode", 0);

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

#ifndef RELEASE_BUILD
// Enable Web Audio for Firefox for Android in Nightly and Aurora
pref("media.webaudio.enabled", true);
#endif

#ifndef RELEASE_BUILD
pref("dom.payment.provider.0.name", "Firefox Marketplace");
pref("dom.payment.provider.0.description", "marketplace.firefox.com");
pref("dom.payment.provider.0.uri", "https://marketplace.firefox.com/mozpay/?req=");
pref("dom.payment.provider.0.type", "mozilla/payments/pay/v1");
pref("dom.payment.provider.0.requestMethod", "GET");
#endif

// Contacts API
pref("dom.mozContacts.enabled", true);
pref("dom.navigator-property.disable.mozContacts", false);
pref("dom.global-constructor.disable.mozContact", false);

// Shortnumber matching needed for e.g. Brazil:
// 01187654321 can be found with 87654321
pref("dom.phonenumber.substringmatching.BR", 8);
pref("dom.phonenumber.substringmatching.CO", 10);
pref("dom.phonenumber.substringmatching.VE", 7);

// Support for the mozAudioChannel attribute on media elements is disabled in non-webapps
pref("media.useAudioChannelService", false);

// Turn on the CSP 1.0 parser for Content Security Policy headers
pref("security.csp.speccompliant", true);

// Enable hardware-accelerated Skia canvas
pref("gfx.canvas.azure.backends", "skia");
pref("gfx.canvas.azure.accelerated", true);
