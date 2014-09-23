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
user_pref("dom.popup_maximum", -1);
user_pref("dom.send_after_paint_to_content", true);
user_pref("dom.successive_dialog_time_limit", 0);
user_pref("signed.applets.codebase_principal_support", true);
user_pref("browser.shell.checkDefaultBrowser", false);
user_pref("shell.checkDefaultClient", false);
user_pref("browser.warnOnQuit", false);
user_pref("accessibility.typeaheadfind.autostart", false);
user_pref("javascript.options.showInConsole", true);
user_pref("devtools.errorconsole.enabled", true);
user_pref("devtools.debugger.remote-port", 6023);
user_pref("layout.debug.enable_data_xbl", true);
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
// Make sure GMPInstallManager won't hit the network.
user_pref("media.gmp-manager.url.override", "http://%(server)s/dummy-gmp-manager.xml");
user_pref("browser.panorama.experienced_first_run", true); // Assume experienced
user_pref("dom.w3c_touch_events.enabled", 1);
user_pref("dom.undo_manager.enabled", true);
user_pref("dom.webcomponents.enabled", true);
user_pref("dom.animations-api.core.enabled", true);
// Set a future policy version to avoid the telemetry prompt.
user_pref("toolkit.telemetry.prompted", 999);
user_pref("toolkit.telemetry.notifiedOptOut", 999);
// Existing tests assume there is no font size inflation.
user_pref("font.size.inflation.emPerLine", 0);
user_pref("font.size.inflation.minTwips", 0);

// AddonManager tests require that the experiments provider be present.
user_pref("experiments.supported", true);
user_pref("experiments.logging.level", "Trace");
user_pref("experiments.logging.dump", true);
// Point the manifest at something local so we don't risk it hitting production
// data and installing experiments that may vary over time.
user_pref("experiments.manifest.uri", "http://%(server)s/experiments-dummy/manifest");

// Only load extensions from the application and user profile
// AddonManager.SCOPE_PROFILE + AddonManager.SCOPE_APPLICATION
user_pref("extensions.enabledScopes", 5);
// Disable metadata caching for installed add-ons by default
user_pref("extensions.getAddons.cache.enabled", false);
// Disable intalling any distribution add-ons
user_pref("extensions.installDistroAddons", false);
// XPI extensions are required for test harnesses to load
user_pref("extensions.defaultProviders.enabled", true);

user_pref("geo.wifi.uri", "http://%(server)s/tests/dom/tests/mochitest/geolocation/network_geolocation.sjs");
user_pref("geo.wifi.timeToWaitBeforeSending", 2000);
user_pref("geo.wifi.scan", false);
user_pref("geo.wifi.logging.enabled", true);

user_pref("camino.warn_when_closing", false); // Camino-only, harmless to others

// Make url-classifier updates so rare that they won't affect tests
user_pref("urlclassifier.updateinterval", 172800);
// Point the url-classifier to the local testing server for fast failures
user_pref("browser.safebrowsing.gethashURL", "http://%(server)s/safebrowsing-dummy/gethash");
user_pref("browser.safebrowsing.updateURL", "http://%(server)s/safebrowsing-dummy/update");
user_pref("browser.safebrowsing.appRepURL", "http://%(server)s/safebrowsing-dummy/update");
user_pref("browser.trackingprotection.gethashURL", "http://%(server)s/safebrowsing-dummy/gethash");
user_pref("browser.trackingprotection.updateURL", "http://%(server)s/safebrowsing-dummy/update");
// Point update checks to the local testing server for fast failures
user_pref("extensions.update.url", "http://%(server)s/extensions-dummy/updateURL");
user_pref("extensions.update.background.url", "http://%(server)s/extensions-dummy/updateBackgroundURL");
user_pref("extensions.blocklist.url", "http://%(server)s/extensions-dummy/blocklistURL");
user_pref("extensions.hotfix.url", "http://%(server)s/extensions-dummy/hotfixURL");
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

// Make sure CSS error reporting is enabled for tests
user_pref("layout.css.report_errors", true);

// Enable CSS Grid for testing
user_pref("layout.css.grid.enabled", true);

// Enable CSS object-fit & object-position for testing
user_pref("layout.css.object-fit-and-position.enabled", true);

// Enable CSS Ruby for testing
user_pref("layout.css.ruby.enabled", true);

// Disable spammy layout warnings because they pollute test logs
user_pref("layout.spammy_warnings.enabled", false);

// Enable Media Source Extensions for testing
user_pref("media.mediasource.enabled", true);

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

// prefs for firefox metro.
// Disable first-tun tab
user_pref("browser.firstrun.count", 0);

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

// We don't want to hit the real Firefox Accounts server for tests.  We don't
// actually need a functioning FxA server, so just set it to something that
// resolves and accepts requests, even if they all fail.
user_pref('identity.fxaccounts.auth.uri', 'https://%(server)s/fxa-dummy/');

// Ditto for all the other Firefox accounts URIs used for about:accounts et al.:
user_pref("identity.fxaccounts.remote.signup.uri", "https://%(server)s/fxa-signup");
user_pref("identity.fxaccounts.remote.force_auth.uri", "https://%(server)s/fxa-force-auth");
user_pref("identity.fxaccounts.remote.signin.uri", "https://%(server)s/fxa-signin");
user_pref("identity.fxaccounts.settings.uri", "https://%(server)s/fxa-settings");

// Enable logging of APZ test data (see bug 961289).
user_pref('apz.test.logging_enabled', true);

// Make sure Translation won't hit the network.
user_pref("browser.translation.bing.authURL", "http://%(server)s/browser/browser/components/translation/test/bing.sjs");
user_pref("browser.translation.bing.translateArrayURL", "http://%(server)s/browser/browser/components/translation/test/bing.sjs");

// Make sure we don't try to load snippets from the network.
user_pref("browser.aboutHomeSnippets.updateUrl", "nonexistent://test");

// Enable debug logging in the mozApps implementation.
user_pref("dom.mozApps.debug", true);

// Don't fetch or send directory tiles data from real servers
user_pref("browser.newtabpage.directory.source", 'data:application/json,{"testing":1}');
user_pref("browser.newtabpage.directory.ping", "");

// Enable Loop
user_pref("loop.enabled", true);
user_pref("loop.throttled", false);

// Ensure UITour won't hit the network
user_pref("browser.uitour.pinnedTabUrl", "http://%(server)s/uitour-dummy/pinnedTab");
user_pref("browser.uitour.url", "http://%(server)s/uitour-dummy/tour");

// Don't prompt about e10s
user_pref("browser.displayedE10SPrompt", 5);
