/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Mobile Browser.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Matt Brubeck <mbrubeck@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
pref("general.useragent.compatMode.firefox", true);
pref("browser.chromeURL", "chrome://browser/content/");

pref("browser.tabs.warnOnClose", true);
pref("browser.tabs.remote", true);

pref("toolkit.screen.lock", false);

// From libpref/src/init/all.js, extended to allow a slightly wider zoom range.
pref("zoom.minPercent", 20);
pref("zoom.maxPercent", 400);
pref("toolkit.zoomManager.zoomValues", ".2,.3,.5,.67,.8,.9,1,1.1,1.2,1.33,1.5,1.7,2,2.4,3,4");

// Device pixel to CSS px ratio, in percent. Set to -1 to calculate based on display density.
pref("browser.viewport.scaleRatio", -1);
pref("browser.viewport.desktopWidth", 980);

/* allow scrollbars to float above chrome ui */
pref("ui.scrollbarsCanOverlapContent", 1);

/* use long press to display a context menu */
pref("ui.click_hold_context_menus", true);

/* cache prefs */
pref("browser.cache.disk.enable", false);
pref("browser.cache.disk.capacity", 0); // kilobytes
pref("browser.cache.disk.smart_size.enabled", false);
pref("browser.cache.disk.smart_size.first_run", false);

pref("browser.cache.memory.enable", true);
pref("browser.cache.memory.capacity", 1024); // kilobytes

/* image cache prefs */
pref("image.cache.size", 1048576); // bytes

/* offline cache prefs */
pref("browser.offline-apps.notify", true);
pref("browser.cache.offline.enable", true);
pref("browser.cache.offline.capacity", 5120); // kilobytes
pref("offline-apps.quota.max", 2048); // kilobytes
pref("offline-apps.quota.warn", 1024); // kilobytes

/* protocol warning prefs */
pref("network.protocol-handler.warn-external.tel", false);
pref("network.protocol-handler.warn-external.mailto", false);
pref("network.protocol-handler.warn-external.vnd.youtube", false);

/* http prefs */
pref("network.http.pipelining", true);
pref("network.http.pipelining.ssl", true);
pref("network.http.proxy.pipelining", true);
pref("network.http.pipelining.maxrequests" , 6);
pref("network.http.keep-alive.timeout", 600);
pref("network.http.max-connections", 6);
pref("network.http.max-connections-per-server", 4);
pref("network.http.max-persistent-connections-per-server", 4);
pref("network.http.max-persistent-connections-per-proxy", 4);
#ifdef MOZ_PLATFORM_MAEMO
pref("network.autodial-helper.enabled", true);
#endif

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
pref("browser.sessionstore.resume_from_crash_timeout", 60); // minutes
pref("browser.sessionstore.interval", 10000); // milliseconds
pref("browser.sessionstore.max_tabs_undo", 1);

/* these should help performance */
pref("mozilla.widget.force-24bpp", true);
pref("mozilla.widget.use-buffer-pixmap", true);
pref("mozilla.widget.disable-native-theme", true);
pref("layout.reflow.synthMouseMove", false);

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

/* download alerts (disabled above) */
pref("alerts.slideIncrement", 1);
pref("alerts.slideIncrementTime", 10);
pref("alerts.totalOpenTime", 6000);
pref("alerts.height", 50);

/* download helper */
pref("browser.helperApps.deleteTempFileOnExit", false);

/* password manager */
pref("signon.rememberSignons", true);
pref("signon.expireMasterPassword", false);
pref("signon.SignonFileName", "signons.txt");

/* form helper */
// 0 = disabled, 1 = enabled, 2 = dynamic depending on screen size
pref("formhelper.mode", 2);
pref("formhelper.autozoom", true);
pref("formhelper.autozoom.caret", true);
pref("formhelper.restore", false);

/* find helper */
pref("findhelper.autozoom", true);

/* autocomplete */
pref("browser.formfill.enable", true);

/* spellcheck */
pref("layout.spellcheckDefault", 0);

/* extension manager and xpinstall */
pref("xpinstall.whitelist.add", "addons.mozilla.org");

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

pref("extensions.update.url", "https://versioncheck.addons.mozilla.org/update/VersionCheck.php?reqVersion=%REQ_VERSION%&id=%ITEM_ID%&version=%ITEM_VERSION%&maxAppVersion=%ITEM_MAXAPPVERSION%&status=%ITEM_STATUS%&appID=%APP_ID%&appVersion=%APP_VERSION%&appOS=%APP_OS%&appABI=%APP_ABI%&locale=%APP_LOCALE%&currentAppVersion=%CURRENT_APP_VERSION%&updateType=%UPDATE_TYPE%");

/* preferences for the Get Add-ons pane */
pref("extensions.getAddons.cache.enabled", true);
pref("extensions.getAddons.maxResults", 15);
pref("extensions.getAddons.recommended.browseURL", "https://addons.mozilla.org/%LOCALE%/mobile/recommended/");
pref("extensions.getAddons.recommended.url", "https://services.addons.mozilla.org/%LOCALE%/mobile/api/%API_VERSION%/list/featured/all/%MAX_RESULTS%/%OS%/%VERSION%");
pref("extensions.getAddons.search.browseURL", "https://addons.mozilla.org/%LOCALE%/mobile/search?q=%TERMS%");
pref("extensions.getAddons.search.url", "https://services.addons.mozilla.org/%LOCALE%/mobile/api/%API_VERSION%/search/%TERMS%/all/%MAX_RESULTS%/%OS%/%VERSION%");
pref("extensions.getAddons.browseAddons", "https://addons.mozilla.org/%LOCALE%/mobile/");
pref("extensions.getAddons.get.url", "https://services.addons.mozilla.org/%LOCALE%/mobile/api/%API_VERSION%/search/guid:%IDS%?src=mobile&appOS=%OS%&appVersion=%VERSION%&tMain=%TIME_MAIN%&tFirstPaint=%TIME_FIRST_PAINT%&tSessionRestored=%TIME_SESSION_RESTORED%");

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

pref("keyword.enabled", true);
pref("keyword.URL", "http://www.google.com/m?ie=UTF-8&oe=UTF-8&sourceid=navclient&gfns=1&q=");

pref("accessibility.typeaheadfind", false);
pref("accessibility.typeaheadfind.timeout", 5000);
pref("accessibility.typeaheadfind.flashBar", 1);
pref("accessibility.typeaheadfind.linksonly", false);
pref("accessibility.typeaheadfind.casesensitive", 0);
// zoom key(F7) conflicts with caret browsing on maemo
pref("accessibility.browsewithcaret_shortcut.enabled", false);

// Whether or not we show a dialog box informing the user that the update was
// successfully applied.
pref("app.update.showInstalledUI", false);

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

// enable search suggestions by default
pref("browser.search.suggest.enabled", true);

// Tell the search service to load search plugins from the locale JAR
pref("browser.search.loadFromJars", true);
pref("browser.search.jarURIs", "chrome://browser/locale/searchplugins/");

// tell the search service that we don't really expose the "current engine"
pref("browser.search.noCurrentEngine", true);

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

#ifdef MOZ_PLATFORM_MAEMO
pref("plugins.force.wmode", "opaque");
#endif

// URL to the Learn More link XXX this is the firefox one.  Bug 495578 fixes this.
pref("browser.geolocation.warning.infoURL", "http://www.mozilla.com/%LOCALE%/firefox/geolocation/");

// base url for the wifi geolocation network provider
pref("geo.wifi.uri", "https://maps.googleapis.com/maps/api/browserlocation/json");

// enable geo
pref("geo.enabled", true);

// content sink control -- controls responsiveness during page load
// see https://bugzilla.mozilla.org/show_bug.cgi?id=481566#c9
pref("content.sink.enable_perf_mode",  2); // 0 - switch, 1 - interactive, 2 - perf
pref("content.sink.pending_event_mode", 0);
pref("content.sink.perf_deflect_count", 1000000);
pref("content.sink.perf_parse_time", 50000000);

// Disable methodjit in chrome to save memory
pref("javascript.options.methodjit.chrome",  false);

pref("javascript.options.mem.high_water_mark", 32);

// Disable the JS engine's gc on memory pressure, since we do one in the mobile
// browser (bug 669346).
pref("javascript.options.gc_on_memory_pressure", false);

pref("dom.max_chrome_script_run_time", 0); // disable slow script dialog for chrome
pref("dom.max_script_run_time", 20);

// JS error console
pref("devtools.errorconsole.enabled", false);

pref("browser.ui.layout.tablet", -1); // on: 1, off: 0, auto: -1

// kinetic tweakables
pref("browser.ui.kinetic.updateInterval", 16);
pref("browser.ui.kinetic.exponentialC", 1400);
pref("browser.ui.kinetic.polynomialC", 100);
pref("browser.ui.kinetic.swipeLength", 160);

// zooming
pref("browser.ui.zoom.pageFitGranularity", 9); // don't zoom to fit by less than 1/9 (11%)
pref("browser.ui.zoom.animationDuration", 200); // ms duration of double-tap zoom animation
pref("browser.ui.zoom.reflow", false); // Change text wrapping on double-tap
pref("browser.ui.zoom.reflow.fontSize", 720);

// pinch gesture
pref("browser.ui.pinch.maxGrowth", 150);     // max pinch distance growth
pref("browser.ui.pinch.maxShrink", 200);     // max pinch distance shrinkage
pref("browser.ui.pinch.scalingFactor", 500); // scaling factor for above pinch limits

// Touch radius (area around the touch location to look for target elements),
// in 1/240-inch pixels:
pref("browser.ui.touch.left", 8);
pref("browser.ui.touch.right", 8);
pref("browser.ui.touch.top", 12);
pref("browser.ui.touch.bottom", 4);
pref("browser.ui.touch.weight.visited", 120); // percentage

// plugins
#if MOZ_PLATFORM_MAEMO == 6
pref("plugin.disable", false);
#else
pref("plugin.disable", true);
#endif
pref("dom.ipc.plugins.enabled", true);

// process priority
// higher values give content process less CPU time
#if MOZ_PLATFORM_MAEMO == 5
pref("dom.ipc.content.nice", 10);
#else
pref("dom.ipc.content.nice", 1);
#endif

// product URLs
// The breakpad report server to link to in about:crashes
pref("breakpad.reportURL", "http://crash-stats.mozilla.com/report/index/");
pref("app.releaseNotesURL", "http://www.mozilla.com/%LOCALE%/mobile/%VERSION%/releasenotes/");
pref("app.sync.tutorialURL", "https://support.mozilla.com/kb/sync-firefox-between-desktop-and-mobile");
pref("app.support.baseURL", "http://support.mozilla.com/mobile");
pref("app.feedbackURL", "http://input.mozilla.com/feedback/");
pref("app.privacyURL", "http://www.mozilla.com/%LOCALE%/m/privacy.html");
pref("app.creditsURL", "http://www.mozilla.org/credits/");
#if MOZ_UPDATE_CHANNEL == beta
pref("app.featuresURL", "http://www.mozilla.com/%LOCALE%/mobile/beta/features/");
pref("app.faqURL", "http://www.mozilla.com/%LOCALE%/mobile/beta/faq/");
#else
pref("app.featuresURL", "http://www.mozilla.com/%LOCALE%/mobile/features/");
pref("app.faqURL", "http://www.mozilla.com/%LOCALE%/mobile/faq/");
#endif

// Name of alternate about: page for certificate errors (when undefined, defaults to about:neterror)
pref("security.alternate_certificate_error_page", "certerror");

pref("security.warn_viewing_mixed", false); // Warning is disabled.  See Bug 616712.

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

/* app update prefs */
pref("app.update.timer", 60000); // milliseconds (1 min)

#ifdef MOZ_UPDATER
pref("app.update.enabled", true);
pref("app.update.timerFirstInterval", 20000); // milliseconds
pref("app.update.auto", false);
pref("app.update.channel", "@MOZ_UPDATE_CHANNEL@");
pref("app.update.mode", 1);
pref("app.update.silent", false);
pref("app.update.url", "https://aus2.mozilla.org/update/4/%PRODUCT%/%VERSION%/%BUILD_ID%/%BUILD_TARGET%/%LOCALE%/%CHANNEL%/%OS_VERSION%/%DISTRIBUTION%/%DISTRIBUTION_VERSION%/%PLATFORM_VERSION%/update.xml");
pref("app.update.promptWaitTime", 43200);
pref("app.update.idletime", 60);
pref("app.update.showInstalledUI", false);
pref("app.update.incompatible.mode", 0);
pref("app.update.download.backgroundInterval", 0);

#ifdef MOZ_OFFICIAL_BRANDING
pref("app.update.interval", 86400);
pref("app.update.url.manual", "http://www.mozilla.com/%LOCALE%/m/");
pref("app.update.url.details", "http://www.mozilla.com/%LOCALE%/mobile/releases/");
#else
pref("app.update.interval", 28800);
pref("app.update.url.manual", "http://www.mozilla.com/%LOCALE%/mobile/");
pref("app.update.url.details", "http://www.mozilla.com/%LOCALE%/mobile/");
#endif
#endif

// replace newlines with spaces on paste into single-line text boxes
pref("editor.singleLine.pasteNewlines", 2);

#ifdef MOZ_PLATFORM_MAEMO
// update fonts for better readability
pref("font.default.x-baltic", "SwissA");
pref("font.default.x-central-euro", "SwissA");
pref("font.default.x-cyrillic", "SwissA");
pref("font.default.x-unicode", "SwissA");
pref("font.default.x-user-def", "SwissA");
pref("font.default.x-western", "SwissA");
#endif

#ifdef MOZ_SERVICES_SYNC
pref("browser.sync.enabled", true);

// sync service
pref("services.sync.client.type", "mobile");
pref("services.sync.registerEngines", "Tab,Bookmarks,Form,History,Password,Prefs");
pref("services.sync.autoconnectDelay", 5);

// prefs to sync by default
pref("services.sync.prefs.sync.browser.startup.homepage.title", true);
pref("services.sync.prefs.sync.browser.startup.homepage", true);
pref("services.sync.prefs.sync.browser.tabs.warnOnClose", true);
pref("services.sync.prefs.sync.browser.ui.zoom.reflow", true);
pref("services.sync.prefs.sync.devtools.errorconsole.enabled", true);
pref("services.sync.prefs.sync.javascript.enabled", true);
pref("services.sync.prefs.sync.lightweightThemes.isThemeSelected", true);
pref("services.sync.prefs.sync.lightweightThemes.usedThemes", true);
pref("services.sync.prefs.sync.network.cookie.cookieBehavior", true);
pref("services.sync.prefs.sync.permissions.default.image", true);
pref("services.sync.prefs.sync.privacy.donottrackheader.enabled", true);
pref("services.sync.prefs.sync.signon.rememberSignons", true);
#endif

// threshold where a tap becomes a drag, in 1/240" reference pixels
// The names of the preferences are to be in sync with nsEventStateManager.cpp
pref("ui.dragThresholdX", 25);
pref("ui.dragThresholdY", 25);

#if MOZ_PLATFORM_MAEMO == 6
pref("layers.acceleration.disabled", false);
#elifdef ANDROID
pref("layers.acceleration.disabled", false);
#else
pref("layers.acceleration.disabled", true);
#endif

pref("notification.feature.enabled", true);

// prevent tooltips from showing up
pref("browser.chrome.toolbar_tips", false);
pref("indexedDB.feature.enabled", true);
pref("dom.indexedDB.warningQuota", 5);

// prevent video elements from preloading too much data
pref("media.preload.default", 1); // default to preload none
pref("media.preload.auto", 2);    // preload metadata if preload=auto

//  0: don't show fullscreen keyboard
//  1: always show fullscreen keyboard
// -1: show fullscreen keyboard based on threshold pref
pref("widget.ime.android.landscape_fullscreen", -1);
pref("widget.ime.android.fullscreen_threshold", 250); // in hundreths of inches

// optimize images memory usage
pref("image.mem.decodeondraw", true);
pref("content.image.allow_locking", false);
pref("image.mem.min_discard_timeout_ms", 10000);

// enable touch events interfaces
pref("dom.w3c_touch_events.enabled", true);
pref("dom.w3c_touch_events.safetyX", 0); // escape borders in units of 1/240"
pref("dom.w3c_touch_events.safetyY", 120); // escape borders in units of 1/240"

#ifdef MOZ_SAFE_BROWSING
// Safe browsing does nothing unless this pref is set
pref("browser.safebrowsing.enabled", true);

// Prevent loading of pages identified as malware
pref("browser.safebrowsing.malware.enabled", true);

// Non-enhanced mode (local url lists) URL list to check for updates
pref("browser.safebrowsing.provider.0.updateURL", "http://safebrowsing.clients.google.com/safebrowsing/downloads?client={moz:client}&appver={moz:version}&pver=2.2");

pref("browser.safebrowsing.dataProvider", 0);

// Does the provider name need to be localizable?
pref("browser.safebrowsing.provider.0.name", "Google");
pref("browser.safebrowsing.provider.0.keyURL", "https://sb-ssl.google.com/safebrowsing/newkey?client={moz:client}&appver={moz:version}&pver=2.2");
pref("browser.safebrowsing.provider.0.reportURL", "http://safebrowsing.clients.google.com/safebrowsing/report?");
pref("browser.safebrowsing.provider.0.gethashURL", "http://safebrowsing.clients.google.com/safebrowsing/gethash?client={moz:client}&appver={moz:version}&pver=2.2");

// HTML report pages
pref("browser.safebrowsing.provider.0.reportGenericURL", "http://{moz:locale}.phish-generic.mozilla.com/?hl={moz:locale}");
pref("browser.safebrowsing.provider.0.reportErrorURL", "http://{moz:locale}.phish-error.mozilla.com/?hl={moz:locale}");
pref("browser.safebrowsing.provider.0.reportPhishURL", "http://{moz:locale}.phish-report.mozilla.com/?hl={moz:locale}");
pref("browser.safebrowsing.provider.0.reportMalwareURL", "http://{moz:locale}.malware-report.mozilla.com/?hl={moz:locale}");
pref("browser.safebrowsing.provider.0.reportMalwareErrorURL", "http://{moz:locale}.malware-error.mozilla.com/?hl={moz:locale}");

// FAQ URLs
pref("browser.safebrowsing.warning.infoURL", "http://www.mozilla.com/%LOCALE%/%APP%/phishing-protection/");
pref("browser.geolocation.warning.infoURL", "http://www.mozilla.com/%LOCALE%/%APP%/geolocation/");

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
pref("urlclassifier.confirm-age", 2700);

// Maximum size of the sqlite3 cache during an update, in bytes
pref("urlclassifier.updatecachemax", 4194304);

// URL for checking the reason for a malware warning.
pref("browser.safebrowsing.malware.reportURL", "http://safebrowsing.clients.google.com/safebrowsing/diagnostic?client=%NAME%&hl=%LOCALE%&site=");
#endif

// True if this is the first time we are showing about:firstrun
pref("browser.firstrun.show.uidiscovery", true);
pref("browser.firstrun.show.localepicker", true);

// initiated by a user
pref("content.ime.strict_policy", true);

// True if you always want dump() to work
//
// On Android, you also need to do the following for the output
// to show up in logcat:
//
// $ adb shell stop
// $ adb shell setprop log.redirect-stdio true
// $ adb shell start
pref("browser.dom.window.dump.enabled", false);
