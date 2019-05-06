// Base preferences file to allow unittests to run successfully.
// NOTE: Toggling prefs for testing features should happen in
// unittest-features/user.js or in harness/test manifests, not here!
/* globals user_pref */
user_pref("accessibility.typeaheadfind.autostart", false);
// Make sure Shield doesn't hit the network.
user_pref("app.normandy.api_url", "");
// Make sure the notification permission migration test doesn't hit the network.
user_pref("app.support.baseURL", "http://{server}/support-dummy/");
user_pref("app.update.staging.enabled", false);
user_pref("app.update.url.android", "");
// Increase the APZ content response timeout in tests to 1 minute.
// This is to accommodate the fact that test environments tends to be slower
// than production environments (with the b2g emulator being the slowest of them
// all), resulting in the production timeout value sometimes being exceeded
// and causing false-positive test failures. See bug 1176798, bug 1177018,
// bug 1210465.
user_pref("apz.content_response_timeout", 60000);
user_pref("browser.EULA.override", true);
// Disable Bookmark backups by default.
user_pref("browser.bookmarks.max_backups", 0);
user_pref("browser.console.showInPanel", true);
// Don't connect to Yahoo! for RSS feed tests.
// en-US only uses .types.0.uri, but set all of them just to be sure.
user_pref("browser.contentHandlers.types.0.uri", "http://test1.example.org/rss?url=%s");
user_pref("browser.contentHandlers.types.1.uri", "http://test1.example.org/rss?url=%s");
user_pref("browser.contentHandlers.types.2.uri", "http://test1.example.org/rss?url=%s");
user_pref("browser.contentHandlers.types.3.uri", "http://test1.example.org/rss?url=%s");
user_pref("browser.contentHandlers.types.4.uri", "http://test1.example.org/rss?url=%s");
user_pref("browser.contentHandlers.types.5.uri", "http://test1.example.org/rss?url=%s");
// Indicate that the download panel has been shown once so that whichever
// download test runs first doesn't show the popup inconsistently.
user_pref("browser.download.panel.shown", true);
user_pref("browser.newtabpage.activity-stream.default.sites", "");
user_pref("browser.newtabpage.activity-stream.telemetry", false);
// Make sure PingCentre doesn't hit the network.
user_pref("browser.ping-centre.production.endpoint", "");
user_pref("browser.ping-centre.staging.endpoint", "");
// Point the url-classifier to the local testing server for fast failures
user_pref("browser.safebrowsing.downloads.remote.url", "http://{server}/safebrowsing-dummy/update");
user_pref("browser.safebrowsing.provider.google.gethashURL", "http://{server}/safebrowsing-dummy/gethash");
user_pref("browser.safebrowsing.provider.google.updateURL", "http://{server}/safebrowsing-dummy/update");
user_pref("browser.safebrowsing.provider.google4.gethashURL", "http://{server}/safebrowsing4-dummy/gethash");
user_pref("browser.safebrowsing.provider.google4.updateURL", "http://{server}/safebrowsing4-dummy/update");
user_pref("browser.safebrowsing.provider.mozilla.gethashURL", "http://{server}/safebrowsing-dummy/gethash");
user_pref("browser.safebrowsing.provider.mozilla.updateURL", "http://{server}/safebrowsing-dummy/update");
user_pref("browser.search.suggest.timeout", 10000); // use a 10s suggestion timeout in tests
user_pref("browser.shell.checkDefaultBrowser", false);
user_pref("browser.startup.page", 0); // use about:blank, not browser.startup.homepage
// Don't show a delay when hiding the audio indicator during tests
user_pref("browser.tabs.delayHidingAudioPlayingIconMS", 0);
// Don't allow background tabs to be zombified, otherwise for tests that
// open additional tabs, the test harness tab itself might get unloaded.
user_pref("browser.tabs.disableBackgroundZombification", true);
// Don't use auto-enabled e10s
user_pref("browser.tabs.remote.autostart", false);
// Make sure Translation won't hit the network.
user_pref("browser.translation.bing.authURL", "http://{server}/browser/browser/components/translation/test/bing.sjs");
user_pref("browser.translation.bing.translateArrayURL", "http://{server}/browser/browser/components/translation/test/bing.sjs");
user_pref("browser.translation.engine", "Bing");
user_pref("browser.translation.yandex.translateURLOverride", "http://{server}/browser/browser/components/translation/test/yandex.sjs");
user_pref("browser.ui.layout.tablet", 0); // force tablet UI off
// Ensure UITour won't hit the network
user_pref("browser.uitour.pinnedTabUrl", "http://{server}/uitour-dummy/pinnedTab");
user_pref("browser.uitour.url", "http://{server}/uitour-dummy/tour");
user_pref("browser.urlbar.speculativeConnect.enabled", false);
// Turn off search suggestions in the location bar so as not to trigger network
// connections.
user_pref("browser.urlbar.suggest.searches", false);
user_pref("browser.urlbar.usepreloadedtopurls.enabled", false);
// Turn off the location bar search suggestions opt-in.  It interferes with
// tests that don't expect it to be there.
user_pref("browser.urlbar.userMadeSearchSuggestionsChoice", true);
user_pref("browser.warnOnQuit", false);
// Enable webapps testing mode, which bypasses native installation.
user_pref("browser.webapps.testing", true);
user_pref("captivedetect.canonicalURL", "http://{server}/captive-detect/success.txt");
// Point Firefox Health Report at a local server. We don't care if it actually
// works. It just can't hit the default production endpoint.
user_pref("datareporting.healthreport.documentServerURI", "http://{server}/healthreport/");
user_pref("datareporting.healthreport.uploadEnabled", false);
user_pref("devtools.browsertoolbox.panel", "jsdebugger");
user_pref("devtools.debugger.remote-port", 6023);
user_pref("devtools.testing", true);
// Required to set values in wpt metadata files (bug 1485842)
user_pref("dom.audioworklet.enabled", false);
user_pref("dom.allow_scripts_to_close_windows", true);
user_pref("dom.disable_open_during_load", false);
user_pref("dom.ipc.reportProcessHangs", false); // process hang monitor
// Don't forceably kill content processes after a timeout
user_pref("dom.ipc.tabs.shutdownTimeoutSecs", 0);
user_pref("dom.min_background_timeout_value", 1000);
user_pref("dom.popup_maximum", -1);
user_pref("dom.block_multiple_popups", false);
user_pref("dom.presentation.testing.simulate-receiver", false);
// Prevent connection to the push server for tests.
user_pref("dom.push.connection.enabled", false);
user_pref("dom.successive_dialog_time_limit", 0);
// In the default configuration, we bypass XBL scopes (a security feature) for
// domains whitelisted for remote XUL, so that intranet apps and such continue
// to work without major rewrites. However, we also use the whitelist mechanism
// to run our XBL tests in automation, in which case we really want to be testing
// the configuration that we ship to users without special whitelisting. So we
// use an additional pref here to allow automation to use the "normal" behavior.
user_pref("dom.use_xbl_scopes_for_remote_xul", true);
user_pref("extensions.autoDisableScopes", 0);
user_pref("extensions.blocklist.detailsURL", "http://{server}/extensions-dummy/blocklistDetailsURL");
user_pref("extensions.blocklist.itemURL", "http://{server}/extensions-dummy/blocklistItemURL");
user_pref("extensions.blocklist.url", "http://{server}/extensions-dummy/blocklistURL");
// XPI extensions are required for test harnesses to load
user_pref("extensions.defaultProviders.enabled", true);
// Disable metadata caching for installed add-ons by default
user_pref("extensions.getAddons.cache.enabled", false);
// Make sure AddonRepository won't hit the network
user_pref("extensions.getAddons.get.url", "http://{server}/extensions-dummy/repositoryGetURL");
user_pref("extensions.getAddons.getWithPerformance.url", "http://{server}/extensions-dummy/repositoryGetWithPerformanceURL");
user_pref("extensions.getAddons.search.browseURL", "http://{server}/extensions-dummy/repositoryBrowseURL");
user_pref("extensions.hotfix.url", "http://{server}/extensions-dummy/hotfixURL");
// Disable intalling any distribution add-ons
user_pref("extensions.installDistroAddons", false);
// Disable Screenshots by default for now
user_pref("extensions.screenshots.disabled", true);
user_pref("extensions.systemAddon.update.url", "http://{server}/dummy-system-addons.xml");
user_pref("extensions.update.background.url", "http://{server}/extensions-dummy/updateBackgroundURL");
// Point update checks to the local testing server for fast failures
user_pref("extensions.update.url", "http://{server}/extensions-dummy/updateURL");
// Make sure opening about:addons won't hit the network
user_pref("extensions.webservice.discoverURL", "http://{server}/extensions-dummy/discoveryURL");
user_pref("extensions.privatebrowsing.notification", true);
user_pref("findbar.highlightAll", false);
user_pref("findbar.modalHighlight", false);
// Existing tests assume there is no font size inflation.
user_pref("font.size.inflation.emPerLine", 0);
user_pref("font.size.inflation.minTwips", 0);
user_pref("general.useragent.updates.url", "https://example.com/0/%APP_ID%");
// Always use network provider for geolocation tests
// so we bypass the OSX dialog raised by the corelocation provider
user_pref("geo.provider.testing", true);
user_pref("geo.wifi.logging.enabled", true);
user_pref("geo.wifi.scan", false);
user_pref("geo.wifi.timeToWaitBeforeSending", 2000);
user_pref("geo.wifi.uri", "http://{server}/tests/dom/tests/mochitest/geolocation/network_geolocation.sjs");
user_pref("gfx.color_management.force_srgb", true);
user_pref("gfx.logging.level", 1);
// We don't want to hit the real Firefox Accounts server for tests.  We don't
// actually need a functioning FxA server, so just set it to something that
// resolves and accepts requests, even if they all fail.
user_pref("identity.fxaccounts.auth.uri", "https://{server}/fxa-dummy/");
// Ditto for all the FxA content root URI.
user_pref("identity.fxaccounts.remote.root", "https://{server}/");
// Avoid idle-daily notifications, to avoid expensive operations that may
// cause unexpected test timeouts.
user_pref("idle.lastDailyNotification", -1);
user_pref("javascript.options.showInConsole", true);
// Make sure CSS error reporting is enabled for tests
user_pref("layout.css.report_errors", true);
// Disable spammy layout warnings because they pollute test logs
user_pref("layout.spammy_warnings.enabled", false);
// Disable all recommended Marionette preferences for Gecko tests.
// The prefs recommended by Marionette are typically geared towards
// consumer automation; not vendor testing.
user_pref("marionette.prefs.recommended", false);
user_pref("media.cache_size", 1000);
user_pref("media.dormant-on-pause-timeout-ms", 0); // Enter dormant immediately without waiting for timeout.
// Set the number of shmems the PChromiumCDM protocol pre-allocates to 0,
// so that we test the case where we under-estimate how many shmems we need
// to send decoded video frames from the CDM to Gecko.
user_pref("media.eme.chromium-api.video-shmems", 0);
// Make sure GMPInstallManager won't hit the network.
user_pref("media.gmp-manager.url.override", "http://{server}/dummy-gmp-manager.xml");
user_pref("media.hls.server.url", "http://{server}/tests/dom/media/test/hls");
// Don't block old libavcodec libraries when testing, because our test systems
// cannot easily be upgraded.
user_pref("media.libavcodec.allow-obsolete", true);
user_pref("media.memory_cache_max_size", 32);
user_pref("media.memory_caches_combined_limit_kb", 256);
user_pref("media.openUnsupportedTypeWithExternalApp", false);
user_pref("media.preload.auto", 3); // auto = enough
user_pref("media.preload.default", 2); // default = metadata
user_pref("media.preload.default.cellular", 2); // default = metadata
user_pref("media.suspend-bkgnd-video.enabled", false);
user_pref("media.test.dumpDebugInfo", true);
user_pref("media.volume_scale", "0.01");
// Enable speech synth test service, and disable built in platform services.
user_pref("media.webspeech.synth.test", true);
user_pref("network.http.prompt-temp-redirect", false);
// Disable speculative connections so they aren't reported as leaking when they're hanging around.
user_pref("network.http.speculative-parallel-limit", 0);
user_pref("network.manage-offline-status", false);
// We know the SNTP request will fail, since localhost isn't listening on
// port 135. The default number of retries (10) is excessive, but retrying
// at least once will mean that codepath is still tested in automation.
user_pref("network.sntp.maxRetryCount", 1);
// Make sure SNTP requests don't hit the network
user_pref("network.sntp.pools", "{server}");
// Set places maintenance far in the future (the maximum time possible in an
// int32_t) to avoid it kicking in during tests. The maintenance can take a
// relatively long time which may cause unnecessary intermittents and slow down
// tests. This, like many things, will stop working correctly in 2038.
user_pref("places.database.lastMaintenance", 2147483647);
user_pref("privacy.trackingprotection.introURL", "http://{server}/trackingprotection/tour");
user_pref("security.default_personal_cert", "Select Automatically"); // Need to client auth test be w/o any dialogs
// Existing tests don't wait for the notification button security delay
user_pref("security.notification_enable_delay", 0);
// Make sure SSL Error reports don't hit the network
user_pref("security.ssl.errorReporting.url", "https://example.com/browser/browser/base/content/test/general/ssl_error_reports.sjs?succeed");
user_pref("security.warn_viewing_mixed", false);
// Ensure blocklist updates don't hit the network
user_pref("services.settings.server", "http://{server}/dummy-kinto/v1");
user_pref("shell.checkDefaultClient", false);
// Disable password capture, so that mochitests that include forms aren't
// influenced by the presence of the persistent doorhanger notification.
user_pref("signon.rememberSignons", false);
user_pref("startup.homepage_welcome_url", "about:blank");
user_pref("startup.homepage_welcome_url.additional", "");
user_pref("test.mousescroll", true);
// Don't send 'bhr' ping during tests, otherwise the testing framework might
// wait on the pingsender to finish and slow down tests.
user_pref("toolkit.telemetry.bhrPing.enabled", false);
// Don't send the 'first-shutdown' during tests, otherwise tests expecting
// main and subsession pings will fail.
user_pref("toolkit.telemetry.firstShutdownPing.enabled", false);
// Don't send 'new-profile' ping on new profiles during tests, otherwise the testing framework
// might wait on the pingsender to finish and slow down tests.
user_pref("toolkit.telemetry.newProfilePing.enabled", false);
// We want to collect telemetry, but we don't want to send in the results.
user_pref("toolkit.telemetry.server", "https://{server}/telemetry-dummy/");
// Don't send the 'shutdown' ping using the pingsender on the first session using
// the 'pingsender' process. Valgrind marks the process as leaky (e.g. see bug 1364068
// for the 'new-profile' ping) but does not provide enough information
// to suppress the leak. Running locally does not reproduce the issue,
// so disable this until we rewrite the pingsender in Rust (bug 1339035).
user_pref("toolkit.telemetry.shutdownPingSender.enabledFirstSession", false);
// A couple of preferences with default values to test that telemetry preference
// watching is working.
user_pref("toolkit.telemetry.test.pref1", true);
user_pref("toolkit.telemetry.test.pref2", false);
// Disable the caret blinking so we get stable snapshot
user_pref("ui.caretBlinkTime", -1);
user_pref("webextensions.tests", true);
// Disable intermittent telemetry collection
user_pref("toolkit.telemetry.initDelay", 99999999);

// We use data: to tell the Quitter extension to quit.
user_pref("security.data_uri.block_toplevel_data_uri_navigations", false);

// We use data: to tell the Quitter extension to quit.
user_pref("security.data_uri.block_toplevel_data_uri_navigations", false);
