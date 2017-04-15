// Base preferences file used by most test harnesses

user_pref("browser.console.showInPanel", true);
user_pref("browser.dom.window.dump.enabled", true);
user_pref("browser.firstrun.show.localepicker", false);
user_pref("browser.firstrun.show.uidiscovery", false);
user_pref("browser.startup.page", 0); // use about:blank, not browser.startup.homepage
user_pref("browser.search.suggest.timeout", 10000); // use a 10s suggestion timeout in tests
user_pref("browser.ui.layout.tablet", 0); // force tablet UI off
user_pref("dom.allow_scripts_to_close_windows", true);
user_pref("dom.disable_open_during_load", false);
user_pref("dom.experimental_forms", true); // on for testing
user_pref("dom.forms.number", true); // on for testing
user_pref("dom.forms.color", true); // on for testing
user_pref("dom.forms.datetime", true); // on for testing
user_pref("dom.forms.datetime.others", true); // on for testing
user_pref("dom.max_script_run_time", 0); // no slow script dialogs
user_pref("hangmonitor.timeout", 0); // no hang monitor
user_pref("dom.max_chrome_script_run_time", 0);
user_pref("dom.ipc.reportProcessHangs", false); // process hang monitor
user_pref("dom.popup_maximum", -1);
user_pref("dom.send_after_paint_to_content", true);
user_pref("dom.successive_dialog_time_limit", 0);
user_pref("signed.applets.codebase_principal_support", true);
user_pref("browser.shell.checkDefaultBrowser", false);
user_pref("shell.checkDefaultClient", false);
user_pref("browser.warnOnQuit", false);
user_pref("accessibility.typeaheadfind.autostart", false);
user_pref("findbar.highlightAll", false);
user_pref("findbar.modalHighlight", false);
user_pref("javascript.options.showInConsole", true);
user_pref("devtools.browsertoolbox.panel", "jsdebugger");
user_pref("devtools.debugger.remote-port", 6023);
user_pref("devtools.devedition.promo.enabled", false);
user_pref("browser.EULA.override", true);
user_pref("gfx.color_management.force_srgb", true);
user_pref("gfx.logging.level", 1);
user_pref("network.manage-offline-status", false);
// Disable speculative connections so they aren't reported as leaking when they're hanging around.
user_pref("network.http.speculative-parallel-limit", 0);
user_pref("dom.min_background_timeout_value", 1000);
user_pref("test.mousescroll", true);
user_pref("security.default_personal_cert", "Select Automatically"); // Need to client auth test be w/o any dialogs
user_pref("network.http.prompt-temp-redirect", false);
user_pref("media.preload.default", 2); // default = metadata
user_pref("media.preload.auto", 3); // auto = enough
user_pref("media.cache_size", 1000);
user_pref("media.volume_scale", "0.01");
user_pref("media.test.dumpDebugInfo", true);
user_pref("media.dormant-on-pause-timeout-ms", 0); // Enter dormant immediately without waiting for timeout.
user_pref("media.suspend-bkgnd-video.enabled", false);
user_pref("security.warn_viewing_mixed", false);
user_pref("app.update.enabled", false);
user_pref("app.update.staging.enabled", false);
user_pref("app.update.url.android", "");
// Make sure GMPInstallManager won't hit the network.
user_pref("media.gmp-manager.url.override", "http://%(server)s/dummy-gmp-manager.xml");
user_pref("media.gmp-manager.updateEnabled", false);
user_pref("dom.w3c_touch_events.enabled", 1);
user_pref("layout.accessiblecaret.enabled_on_touch", false);
user_pref("dom.webcomponents.enabled", true);
user_pref("dom.webcomponents.customelements.enabled", true);
user_pref("dom.htmlimports.enabled", true);
// Existing tests assume there is no font size inflation.
user_pref("font.size.inflation.emPerLine", 0);
user_pref("font.size.inflation.minTwips", 0);
// Disable the caret blinking so we get stable snapshot
user_pref("ui.caretBlinkTime", -1);

// AddonManager tests require that the experiments provider be present.
user_pref("experiments.supported", true);
// Point the manifest at something local so we don't risk it hitting production
// data and installing experiments that may vary over time.
user_pref("experiments.manifest.uri", "http://%(server)s/experiments-dummy/manifest");

// Don't allow background tabs to be zombified, otherwise for tests that
// open additional tabs, the test harness tab itself might get unloaded.
user_pref("browser.tabs.disableBackgroundZombification", true);

// Only load extensions from the application and user profile
// AddonManager.SCOPE_PROFILE + AddonManager.SCOPE_APPLICATION
user_pref("extensions.enabledScopes", 5);
user_pref("extensions.autoDisableScopes", 0);
// Disable metadata caching for installed add-ons by default
user_pref("extensions.getAddons.cache.enabled", false);
// Disable intalling any distribution add-ons
user_pref("extensions.installDistroAddons", false);
// XPI extensions are required for test harnesses to load
user_pref("extensions.defaultProviders.enabled", true);
user_pref("xpinstall.signatures.required", false);

user_pref("geo.wifi.uri", "http://%(server)s/tests/dom/tests/mochitest/geolocation/network_geolocation.sjs");
user_pref("geo.wifi.timeToWaitBeforeSending", 2000);
user_pref("geo.wifi.scan", false);
user_pref("geo.wifi.logging.enabled", true);

// Prevent connection to the push server for tests.
user_pref("dom.push.connection.enabled", false);

// Make url-classifier updates so rare that they won't affect tests
user_pref("urlclassifier.updateinterval", 172800);
// Point the url-classifier to the local testing server for fast failures
user_pref("browser.safebrowsing.downloads.remote.url", "http://%(server)s/safebrowsing-dummy/update");
user_pref("browser.safebrowsing.provider.google.gethashURL", "http://%(server)s/safebrowsing-dummy/gethash");
user_pref("browser.safebrowsing.provider.google.updateURL", "http://%(server)s/safebrowsing-dummy/update");
user_pref("browser.safebrowsing.provider.google4.gethashURL", "http://%(server)s/safebrowsing4-dummy/gethash");
user_pref("browser.safebrowsing.provider.google4.updateURL", "http://%(server)s/safebrowsing4-dummy/update");
user_pref("browser.safebrowsing.provider.mozilla.gethashURL", "http://%(server)s/safebrowsing-dummy/gethash");
user_pref("browser.safebrowsing.provider.mozilla.updateURL", "http://%(server)s/safebrowsing-dummy/update");
user_pref("privacy.trackingprotection.introURL", "http://%(server)s/trackingprotection/tour");
// Point update checks to the local testing server for fast failures
user_pref("extensions.update.url", "http://%(server)s/extensions-dummy/updateURL");
user_pref("extensions.update.background.url", "http://%(server)s/extensions-dummy/updateBackgroundURL");
user_pref("extensions.blocklist.detailsURL", "http://%(server)s/extensions-dummy/blocklistDetailsURL");
user_pref("extensions.blocklist.itemURL", "http://%(server)s/extensions-dummy/blocklistItemURL");
user_pref("extensions.blocklist.url", "http://%(server)s/extensions-dummy/blocklistURL");
user_pref("extensions.hotfix.url", "http://%(server)s/extensions-dummy/hotfixURL");
user_pref("extensions.systemAddon.update.url", "http://%(server)s/dummy-system-addons.xml");
// Turn off extension updates so they don't bother tests
user_pref("extensions.update.enabled", false);
// Make sure opening about:addons won't hit the network
user_pref("extensions.webservice.discoverURL", "http://%(server)s/extensions-dummy/discoveryURL");
// Make sure AddonRepository won't hit the network
user_pref("extensions.getAddons.maxResults", 0);
user_pref("extensions.getAddons.get.url", "http://%(server)s/extensions-dummy/repositoryGetURL");
user_pref("extensions.getAddons.getWithPerformance.url", "http://%(server)s/extensions-dummy/repositoryGetWithPerformanceURL");
user_pref("extensions.getAddons.search.browseURL", "http://%(server)s/extensions-dummy/repositoryBrowseURL");
user_pref("extensions.getAddons.search.url", "http://%(server)s/extensions-dummy/repositorySearchURL");
// Ensure blocklist updates don't hit the network
user_pref("services.settings.server", "http://%(server)s/dummy-kinto/v1");
// Make sure SNTP requests don't hit the network
user_pref("network.sntp.pools", "%(server)s");
// We know the SNTP request will fail, since localhost isn't listening on
// port 135. The default number of retries (10) is excessive, but retrying
// at least once will mean that codepath is still tested in automation.
user_pref("network.sntp.maxRetryCount", 1);

// Make sure the notification permission migration test doesn't hit the network.
user_pref("app.support.baseURL", "http://%(server)s/support-dummy/");

// Existing tests don't wait for the notification button security delay
user_pref("security.notification_enable_delay", 0);

// Make enablePrivilege continue to work for test code. :-(
user_pref("security.turn_off_all_security_so_that_viruses_can_take_over_this_computer", true);

// In the default configuration, we bypass XBL scopes (a security feature) for
// domains whitelisted for remote XUL, so that intranet apps and such continue
// to work without major rewrites. However, we also use the whitelist mechanism
// to run our XBL tests in automation, in which case we really want to be testing
// the configuration that we ship to users without special whitelisting. So we
// use an additional pref here to allow automation to use the "normal" behavior.
user_pref("dom.use_xbl_scopes_for_remote_xul", true);

user_pref("captivedetect.canonicalURL", "http://%(server)s/captive-detect/success.txt");
// Get network events.
user_pref("network.activity.blipIntervalMilliseconds", 250);

// We do not wish to display datareporting policy notifications as it might
// cause other tests to fail. Tests that wish to test the notification functionality
// should explicitly disable this pref.
user_pref("datareporting.policy.dataSubmissionPolicyBypassNotification", true);

// Point Firefox Health Report at a local server. We don't care if it actually
// works. It just can't hit the default production endpoint.
user_pref("datareporting.healthreport.documentServerURI", "http://%(server)s/healthreport/");
user_pref("datareporting.healthreport.about.reportUrl", "http://%(server)s/abouthealthreport/v4/");

// Make sure CSS error reporting is enabled for tests
user_pref("layout.css.report_errors", true);

// Enable CSS Grid 'subgrid' feature for testing
user_pref("layout.css.grid-template-subgrid-value.enabled", true);

// Enable CSS 'contain' for testing
user_pref("layout.css.contain.enabled", true);

// Enable CSS initial-letter for testing
user_pref("layout.css.initial-letter.enabled", true);

// Enable webkit prefixed CSS features for testing
user_pref("layout.css.prefixes.webkit", true);

// Enable -webkit-{min|max}-device-pixel-ratio media queries for testing
user_pref("layout.css.prefixes.device-pixel-ratio-webkit", true);

// Enable CSS shape-outside for testing
user_pref("layout.css.shape-outside.enabled", true);

// Enable CSS text-justify for testing
user_pref("layout.css.text-justify.enabled", true);

// Disable spammy layout warnings because they pollute test logs
user_pref("layout.spammy_warnings.enabled", false);

// Enable Media Source Extensions for testing
user_pref("media.mediasource.mp4.enabled", true);
user_pref("media.mediasource.webm.enabled", true);

// Make sure the disk cache doesn't get auto disabled
user_pref("network.http.bypass-cachelock-threshold", 200000);

// Enable Gamepad
user_pref("dom.gamepad.enabled", true);
user_pref("dom.gamepad.non_standard_events.enabled", true);

// Always use network provider for geolocation tests
// so we bypass the OSX dialog raised by the corelocation provider
user_pref("geo.provider.testing", true);

// Background thumbnails in particular cause grief, and disabling thumbnails
// in general can't hurt - we re-enable them when tests need them.
user_pref("browser.pagethumbnails.capturing_disabled", true);

// Indicate that the download panel has been shown once so that whichever
// download test runs first doesn't show the popup inconsistently.
user_pref("browser.download.panel.shown", true);

// Assume the about:newtab page's intro panels have been shown to not depend on
// which test runs first and happens to open about:newtab
user_pref("browser.newtabpage.introShown", true);

// Enable webapps testing mode, which bypasses native installation.
user_pref("browser.webapps.testing", true);

// Disable android snippets
user_pref("browser.snippets.enabled", false);
user_pref("browser.snippets.syncPromo.enabled", false);
user_pref("browser.snippets.firstrunHomepage.enabled", false);

// Disable useragent updates.
user_pref("general.useragent.updates.enabled", false);
user_pref("general.useragent.updates.url", "https://example.com/0/%%APP_ID%%");

// Disable webapp updates.  Yes, it is supposed to be an integer.
user_pref("browser.webapps.checkForUpdates", 0);

user_pref("dom.presentation.testing.simulate-receiver", false);

// Don't connect to Yahoo! for RSS feed tests.
// en-US only uses .types.0.uri, but set all of them just to be sure.
user_pref('browser.contentHandlers.types.0.uri', 'http://test1.example.org/rss?url=%%s')
user_pref('browser.contentHandlers.types.1.uri', 'http://test1.example.org/rss?url=%%s')
user_pref('browser.contentHandlers.types.2.uri', 'http://test1.example.org/rss?url=%%s')
user_pref('browser.contentHandlers.types.3.uri', 'http://test1.example.org/rss?url=%%s')
user_pref('browser.contentHandlers.types.4.uri', 'http://test1.example.org/rss?url=%%s')
user_pref('browser.contentHandlers.types.5.uri', 'http://test1.example.org/rss?url=%%s')

// We want to collect telemetry, but we don't want to send in the results.
user_pref('toolkit.telemetry.server', 'https://%(server)s/telemetry-dummy/');

// A couple of preferences with default values to test that telemetry preference
// watching is working.
user_pref('toolkit.telemetry.test.pref1', true);
user_pref('toolkit.telemetry.test.pref2', false);

// We don't want to hit the real Firefox Accounts server for tests.  We don't
// actually need a functioning FxA server, so just set it to something that
// resolves and accepts requests, even if they all fail.
user_pref('identity.fxaccounts.auth.uri', 'https://%(server)s/fxa-dummy/');

// Ditto for all the other Firefox accounts URIs used for about:accounts et al.:
user_pref("identity.fxaccounts.remote.signup.uri", "https://%(server)s/fxa-signup");
user_pref("identity.fxaccounts.remote.force_auth.uri", "https://%(server)s/fxa-force-auth");
user_pref("identity.fxaccounts.remote.signin.uri", "https://%(server)s/fxa-signin");
user_pref("identity.fxaccounts.settings.uri", "https://%(server)s/fxa-settings");
user_pref('identity.fxaccounts.remote.webchannel.uri', 'https://%(server)s/');

// We don't want browser tests to perform FxA device registration.
user_pref('identity.fxaccounts.skipDeviceRegistration', true);

// Increase the APZ content response timeout in tests to 1 minute.
// This is to accommodate the fact that test environments tends to be slower
// than production environments (with the b2g emulator being the slowest of them
// all), resulting in the production timeout value sometimes being exceeded
// and causing false-positive test failures. See bug 1176798, bug 1177018,
// bug 1210465.
user_pref("apz.content_response_timeout", 60000);

// Make sure SSL Error reports don't hit the network
user_pref("security.ssl.errorReporting.url", "https://example.com/browser/browser/base/content/test/general/ssl_error_reports.sjs?succeed");

// Make sure Translation won't hit the network.
user_pref("browser.translation.bing.authURL", "http://%(server)s/browser/browser/components/translation/test/bing.sjs");
user_pref("browser.translation.bing.translateArrayURL", "http://%(server)s/browser/browser/components/translation/test/bing.sjs");
user_pref("browser.translation.yandex.translateURLOverride", "http://%(server)s/browser/browser/components/translation/test/yandex.sjs");
user_pref("browser.translation.engine", "bing");

// Make sure we don't try to load snippets from the network.
user_pref("browser.aboutHomeSnippets.updateUrl", "nonexistent://test");

// Don't fetch or send directory tiles data from real servers
user_pref("browser.newtabpage.directory.source", 'data:application/json,{"testing":1}');
user_pref("browser.newtabpage.directory.ping", "");

// Ensure UITour won't hit the network
user_pref("browser.uitour.pinnedTabUrl", "http://%(server)s/uitour-dummy/pinnedTab");
user_pref("browser.uitour.url", "http://%(server)s/uitour-dummy/tour");

// Tell the search service we are running in the US.  This also has the desired
// side-effect of preventing our geoip lookup.
user_pref("browser.search.isUS", true);
user_pref("browser.search.countryCode", "US");
// This will prevent HTTP requests for region defaults.
user_pref("browser.search.geoSpecificDefaults", false);

// Make sure self support doesn't hit the network.
user_pref("browser.selfsupport.url", "https://example.com/selfsupport-dummy/");
user_pref("extensions.shield-recipe-client.api_url", "https://example.com/selfsupport-dummy/");

user_pref("media.eme.enabled", true);

user_pref("media.autoplay.enabled", true);

// Don't use auto-enabled e10s
user_pref("browser.tabs.remote.autostart.1", false);
user_pref("browser.tabs.remote.autostart.2", false);
// Don't show a delay when hiding the audio indicator during tests
user_pref("browser.tabs.delayHidingAudioPlayingIconMS", 0);
// Don't forceably kill content processes after a timeout
user_pref("dom.ipc.tabs.shutdownTimeoutSecs", 0);

// Don't block add-ons for e10s
user_pref("extensions.e10sBlocksEnabling", false);

// Avoid performing Reader Mode intros during tests.
user_pref("browser.reader.detectedFirstArticle", true);

// Make tests run consistently on DevEdition (which has a lightweight theme
// selected by default).
user_pref("lightweightThemes.selectedThemeID", "");

// Enable speech synth test service, and disable built in platform services.
user_pref("media.webspeech.synth.test", true);

// Turn off search suggestions in the location bar so as not to trigger network
// connections.
user_pref("browser.urlbar.suggest.searches", false);

// Turn off the location bar search suggestions opt-in.  It interferes with
// tests that don't expect it to be there.
user_pref("browser.urlbar.userMadeSearchSuggestionsChoice", true);

user_pref("browser.urlbar.usepreloadedtopurls.enabled", false);

user_pref("dom.audiochannel.mutedByDefault", false);

user_pref("webextensions.tests", true);
user_pref("startup.homepage_welcome_url", "about:blank");
user_pref("startup.homepage_welcome_url.additional", "");

// For Firefox 52 only, ESR will support non-Flash plugins while release will
// not, so we keep testing the non-Flash pathways
user_pref("plugin.load_flash_only", false);

// Don't block old libavcodec libraries when testing, because our test systems
// cannot easily be upgraded.
user_pref("media.libavcodec.allow-obsolete", true);

user_pref("media.openUnsupportedTypeWithExternalApp", false);

// Disable password capture, so that mochitests that include forms aren't
// influenced by the presence of the persistent doorhanger notification.
user_pref("signon.rememberSignons", false);

// Enable form autofill feature testing.
user_pref("browser.formautofill.experimental", true);

// Disable all recommended Marionette preferences for Gecko tests.
// The prefs recommended by Marionette are typically geared towards
// consumer automation; not vendor testing.
user_pref("marionette.prefs.recommended", false);

// Disable Screenshots by default for now
user_pref("extensions.screenshots.system-disabled", true);
