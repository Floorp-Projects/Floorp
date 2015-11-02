// Base preferences file used by most test harnesses

user_pref("browser.console.showInPanel", true);
user_pref("browser.dom.window.dump.enabled", true);
user_pref("browser.firstrun.show.localepicker", false);
user_pref("browser.firstrun.show.uidiscovery", false);
user_pref("browser.startup.page", 0); // use about:blank, not browser.startup.homepage
user_pref("browser.ui.layout.tablet", 0); // force tablet UI off
user_pref("dom.allow_scripts_to_close_windows", true);
user_pref("dom.disable_open_during_load", false);
user_pref("dom.experimental_forms", true); // on for testing
user_pref("dom.forms.number", true); // on for testing
user_pref("dom.forms.color", true); // on for testing
user_pref("dom.max_script_run_time", 0); // no slow script dialogs
user_pref("hangmonitor.timeout", 0); // no hang monitor
user_pref("dom.max_chrome_script_run_time", 0);
user_pref("dom.max_child_script_run_time", 0);
user_pref("dom.ipc.reportProcessHangs", false); // process hang monitor
user_pref("dom.popup_maximum", -1);
user_pref("dom.send_after_paint_to_content", true);
user_pref("dom.successive_dialog_time_limit", 0);
user_pref("signed.applets.codebase_principal_support", true);
user_pref("browser.shell.checkDefaultBrowser", false);
user_pref("shell.checkDefaultClient", false);
user_pref("browser.warnOnQuit", false);
user_pref("accessibility.typeaheadfind.autostart", false);
user_pref("javascript.options.showInConsole", true);
user_pref("devtools.browsertoolbox.panel", "jsdebugger");
user_pref("devtools.errorconsole.enabled", true);
user_pref("devtools.debugger.remote-port", 6023);
user_pref("browser.EULA.override", true);
user_pref("gfx.color_management.force_srgb", true);
user_pref("network.manage-offline-status", false);
// Disable speculative connections so they aren't reported as leaking when they're hanging around.
user_pref("network.http.speculative-parallel-limit", 0);
user_pref("dom.min_background_timeout_value", 1000);
user_pref("test.mousescroll", true);
user_pref("security.default_personal_cert", "Select Automatically"); // Need to client auth test be w/o any dialogs
user_pref("network.http.prompt-temp-redirect", false);
user_pref("media.cache_size", 1000);
user_pref("media.volume_scale", "0.01");
user_pref("security.warn_viewing_mixed", false);
user_pref("app.update.enabled", false);
user_pref("app.update.staging.enabled", false);
user_pref("app.update.url.android", "");
// Make sure GMPInstallManager won't hit the network.
user_pref("media.gmp-manager.url.override", "http://%(server)s/dummy-gmp-manager.xml");
user_pref("browser.panorama.experienced_first_run", true); // Assume experienced
user_pref("dom.w3c_touch_events.enabled", 1);
user_pref("dom.undo_manager.enabled", true);
user_pref("dom.webcomponents.enabled", true);
user_pref("dom.htmlimports.enabled", true);
// Set a future policy version to avoid the telemetry prompt.
user_pref("toolkit.telemetry.prompted", 999);
user_pref("toolkit.telemetry.notifiedOptOut", 999);
// Existing tests assume there is no font size inflation.
user_pref("font.size.inflation.emPerLine", 0);
user_pref("font.size.inflation.minTwips", 0);

// AddonManager tests require that the experiments provider be present.
user_pref("experiments.supported", true);
// Point the manifest at something local so we don't risk it hitting production
// data and installing experiments that may vary over time.
user_pref("experiments.manifest.uri", "http://%(server)s/experiments-dummy/manifest");

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
// Disable signature requirements where possible
user_pref("xpinstall.signatures.required", false);

user_pref("geo.wifi.uri", "http://%(server)s/tests/dom/tests/mochitest/geolocation/network_geolocation.sjs");
user_pref("geo.wifi.timeToWaitBeforeSending", 2000);
user_pref("geo.wifi.scan", false);
user_pref("geo.wifi.logging.enabled", true);

// Make url-classifier updates so rare that they won't affect tests
user_pref("urlclassifier.updateinterval", 172800);
// Point the url-classifier to the local testing server for fast failures
user_pref("browser.safebrowsing.provider.google.gethashURL", "http://%(server)s/safebrowsing-dummy/gethash");
user_pref("browser.safebrowsing.provider.google.updateURL", "http://%(server)s/safebrowsing-dummy/update");
user_pref("browser.safebrowsing.provider.google.appRepURL", "http://%(server)s/safebrowsing-dummy/update");
user_pref("browser.safebrowsing.provider.mozilla.gethashURL", "http://%(server)s/safebrowsing-dummy/gethash");
user_pref("browser.safebrowsing.provider.mozilla.updateURL", "http://%(server)s/safebrowsing-dummy/update");
user_pref("privacy.trackingprotection.introURL", "http://%(server)s/trackingprotection/tour");
// Point update checks to the local testing server for fast failures
user_pref("extensions.update.url", "http://%(server)s/extensions-dummy/updateURL");
user_pref("extensions.update.background.url", "http://%(server)s/extensions-dummy/updateBackgroundURL");
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
// Make sure that opening the plugins check page won't hit the network
user_pref("plugins.update.url", "http://%(server)s/plugins-dummy/updateCheckURL");
// Make sure SNTP requests don't hit the network
user_pref("network.sntp.pools", "%(server)s");
// We know the SNTP request will fail, since localhost isn't listening on
// port 135. The default number of retries (10) is excessive, but retrying
// at least once will mean that codepath is still tested in automation.
user_pref("network.sntp.maxRetryCount", 1);

// Make sure the notification permission migration test doesn't hit the network.
user_pref("browser.push.warning.infoURL", "http://%(server)s/alerts-dummy/infoURL");
user_pref("browser.push.warning.migrationURL", "http://%(server)s/alerts-dummy/migrationURL");

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

// Get network events.
user_pref("network.activity.blipIntervalMilliseconds", 250);

// We do not wish to display datareporting policy notifications as it might
// cause other tests to fail. Tests that wish to test the notification functionality
// should explicitly disable this pref.
user_pref("datareporting.policy.dataSubmissionPolicyBypassNotification", true);

// Point Firefox Health Report at a local server. We don't care if it actually
// works. It just can't hit the default production endpoint.
user_pref("datareporting.healthreport.documentServerURI", "http://%(server)s/healthreport/");
user_pref("datareporting.healthreport.about.reportUrl", "http://%(server)s/abouthealthreport/");
user_pref("datareporting.healthreport.about.reportUrlUnified", "http://%(server)s/abouthealthreport/v4/");

// Make sure CSS error reporting is enabled for tests
user_pref("layout.css.report_errors", true);

// Enable CSS Grid for testing
user_pref("layout.css.grid.enabled", true);

// Enable CSS 'contain' for testing
user_pref("layout.css.contain.enabled", true);

// Enable CSS object-fit & object-position for testing
user_pref("layout.css.object-fit-and-position.enabled", true);

// Enable CSS Ruby for testing
user_pref("layout.css.ruby.enabled", true);

// Enable unicode-range for testing
user_pref("layout.css.unicode-range.enabled", true);

// Enable webkit prefixed CSS features for testing
user_pref("layout.css.prefixes.webkit", true);

// Disable spammy layout warnings because they pollute test logs
user_pref("layout.spammy_warnings.enabled", false);

// Enable Media Source Extensions for testing
user_pref("media.mediasource.mp4.enabled", true);
user_pref("media.mediasource.webm.enabled", true);

// Enable mozContacts
user_pref("dom.mozContacts.enabled", true);

// Enable mozSettings
user_pref("dom.mozSettings.enabled", true);

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

// Tell the PBackground infrastructure to run a test at startup.
user_pref("pbackground.testing", true);

// Enable webapps testing mode, which bypasses native installation.
user_pref("browser.webapps.testing", true);

// Disable android snippets
user_pref("browser.snippets.enabled", false);
user_pref("browser.snippets.syncPromo.enabled", false);
user_pref("browser.snippets.firstrunHomepage.enabled", false);

// Disable useragent updates.
user_pref("general.useragent.updates.enabled", false);

// Disable webapp updates.  Yes, it is supposed to be an integer.
user_pref("browser.webapps.checkForUpdates", 0);

// Enable debug logging in the tcp presentation server.
user_pref("dom.presentation.tcp_server.debug", true);

// Don't connect to Yahoo! for RSS feed tests.
// en-US only uses .types.0.uri, but set all of them just to be sure.
user_pref('browser.contentHandlers.types.0.uri', 'http://test1.example.org/rss?url=%%s')
user_pref('browser.contentHandlers.types.1.uri', 'http://test1.example.org/rss?url=%%s')
user_pref('browser.contentHandlers.types.2.uri', 'http://test1.example.org/rss?url=%%s')
user_pref('browser.contentHandlers.types.3.uri', 'http://test1.example.org/rss?url=%%s')
user_pref('browser.contentHandlers.types.4.uri', 'http://test1.example.org/rss?url=%%s')
user_pref('browser.contentHandlers.types.5.uri', 'http://test1.example.org/rss?url=%%s')

// Set dummy server for Android tiles testing.
user_pref('browser.tiles.reportURL', 'http://%(server)s/tests/robocop/robocop_tiles.sjs')

// We want to collect telemetry, but we don't want to send in the results.
user_pref('toolkit.telemetry.server', 'https://%(server)s/telemetry-dummy/');
// Our current tests expect the unified Telemetry feature to be opt-out,
// which is not true while we hold back shipping it.
user_pref('toolkit.telemetry.unifiedIsOptIn', false);

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

// Increase the APZ content response timeout in tests to 1 minute.
// This is to accommodate the fact that test environments tends to be slower
// than production environments (with the b2g emulator being the slowest of them
// all), resulting in the production timeout value sometimes being exceeded
// and causing false-positive test failures. See bug 1176798, bug 1177018,
// bug 1210465.
user_pref("apz.content_response_timeout", 60000);

// Make sure SSL Error reports don't hit the network
user_pref("security.ssl.errorReporting.url", "https://example.com/browser/browser/base/content/test/general/pinning_reports.sjs?succeed");

// Make sure Translation won't hit the network.
user_pref("browser.translation.bing.authURL", "http://%(server)s/browser/browser/components/translation/test/bing.sjs");
user_pref("browser.translation.bing.translateArrayURL", "http://%(server)s/browser/browser/components/translation/test/bing.sjs");
user_pref("browser.translation.yandex.translateURLOverride", "http://%(server)s/browser/browser/components/translation/test/yandex.sjs");
user_pref("browser.translation.engine", "bing");

// Make sure we don't try to load snippets from the network.
user_pref("browser.aboutHomeSnippets.updateUrl", "nonexistent://test");

// Enable apps customizations
user_pref("dom.apps.customization.enabled", true);

// Don't fetch or send directory tiles data from real servers
user_pref("browser.newtabpage.directory.source", 'data:application/json,{"testing":1}');
user_pref("browser.newtabpage.directory.ping", "");

// Enable Loop
user_pref("loop.debug.loglevel", "All");
user_pref("loop.enabled", true);
user_pref("loop.throttled", false);
user_pref("loop.oauth.google.URL", "http://%(server)s/browser/browser/components/loop/test/mochitest/google_service.sjs?action=");
user_pref("loop.oauth.google.getContactsURL", "http://%(server)s/browser/browser/components/loop/test/mochitest/google_service.sjs?action=contacts");
user_pref("loop.oauth.google.getGroupsURL", "http://%(server)s/browser/browser/components/loop/test/mochitest/google_service.sjs?action=groups");
user_pref("loop.server", "http://%(server)s/browser/browser/components/loop/test/mochitest/loop_fxa.sjs?");
user_pref("loop.CSP","default-src 'self' about: file: chrome: data: wss://* http://* https://*");

// Ensure UITour won't hit the network
user_pref("browser.uitour.pinnedTabUrl", "http://%(server)s/uitour-dummy/pinnedTab");
user_pref("browser.uitour.url", "http://%(server)s/uitour-dummy/tour");

// Tell the search service we are running in the US.  This also has the desired
// side-effect of preventing our geoip lookup.
user_pref("browser.search.isUS", true);
user_pref("browser.search.countryCode", "US");
// This will prevent HTTP requests for region defaults.
user_pref("browser.search.geoSpecificDefaults", false);

// Make sure the self support tab doesn't hit the network.
user_pref("browser.selfsupport.url", "https://%(server)s/selfsupport-dummy/");

user_pref("media.eme.enabled", true);

user_pref("media.autoplay.enabled", true);

#if defined(XP_WIN)
user_pref("media.decoder.heuristic.dormant.timeout", 0);
#endif

// Don't prompt about e10s
user_pref("browser.displayedE10SPrompt.1", 5);
// Don't use auto-enabled e10s
user_pref("browser.tabs.remote.autostart.1", false);
user_pref("browser.tabs.remote.autostart.2", false);
// Don't forceably kill content processes after a timeout
user_pref("dom.ipc.tabs.shutdownTimeoutSecs", 0);

// Avoid performing Reader Mode intros during tests.
user_pref("browser.reader.detectedFirstArticle", true);

// Don't let PAC generator to set PAC, as mochitest framework has its own PAC
// rules during testing.
user_pref("network.proxy.pac_generator", false);

// Make tests run consistently on DevEdition (which has a lightweight theme
// selected by default).
user_pref("lightweightThemes.selectedThemeID", "");

// Disable periodic updates of service workers.
user_pref("dom.serviceWorkers.periodic-updates.enabled", false);

// Enable speech synth test service, and disable built in platform services.
user_pref("media.webspeech.synth.test", true);

// Turn off search suggestions in the location bar so as not to trigger network
// connections.
user_pref("browser.urlbar.suggest.searches", false);

// Turn off the location bar search suggestions opt-in.  It interferes with
// tests that don't expect it to be there.
user_pref("browser.urlbar.userMadeSearchSuggestionsChoice", true);

user_pref("dom.audiochannel.mutedByDefault", false);
