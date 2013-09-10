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
user_pref("dom.experimental_forms_range", true); // on for testing
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
user_pref("javascript.options.jit_hardening", true);
user_pref("gfx.color_management.force_srgb", true);
user_pref("network.manage-offline-status", false);
user_pref("dom.min_background_timeout_value", 1000);
user_pref("test.mousescroll", true);
user_pref("security.default_personal_cert", "Select Automatically"); // Need to client auth test be w/o any dialogs
user_pref("network.http.prompt-temp-redirect", false);
user_pref("media.cache_size", 100);
user_pref("media.volume_scale", "0.01");
user_pref("security.warn_viewing_mixed", false);
user_pref("app.update.enabled", false);
user_pref("app.update.staging.enabled", false);
user_pref("browser.panorama.experienced_first_run", true); // Assume experienced
user_pref("dom.w3c_touch_events.enabled", 1);
user_pref("dom.undo_manager.enabled", true);
user_pref("dom.webcomponents.enabled", true);
// Set a future policy version to avoid the telemetry prompt.
user_pref("toolkit.telemetry.prompted", 999);
user_pref("toolkit.telemetry.notifiedOptOut", 999);
// Existing tests assume there is no font size inflation.
user_pref("font.size.inflation.emPerLine", 0);
user_pref("font.size.inflation.minTwips", 0);

// Only load extensions from the application and user profile
// AddonManager.SCOPE_PROFILE + AddonManager.SCOPE_APPLICATION
user_pref("extensions.enabledScopes", 5);
// Disable metadata caching for installed add-ons by default
user_pref("extensions.getAddons.cache.enabled", false);
// Disable intalling any distribution add-ons
user_pref("extensions.installDistroAddons", false);

user_pref("geo.wifi.uri", "http://%(server)s/tests/dom/tests/mochitest/geolocation/network_geolocation.sjs");
user_pref("geo.wifi.testing", true);
user_pref("geo.ignore.location_filter", true);

user_pref("camino.warn_when_closing", false); // Camino-only, harmless to others

// Make url-classifier updates so rare that they won't affect tests
user_pref("urlclassifier.updateinterval", 172800);
// Point the url-classifier to the local testing server for fast failures
user_pref("browser.safebrowsing.gethashURL", "http://%(server)s/safebrowsing-dummy/gethash");
user_pref("browser.safebrowsing.keyURL", "http://%(server)s/safebrowsing-dummy/newkey");
user_pref("browser.safebrowsing.updateURL", "http://%(server)s/safebrowsing-dummy/update");
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

// Don't allow the Data Reporting service to prompt for policy acceptance.
user_pref("datareporting.policy.dataSubmissionPolicyBypassAcceptance", true);

// Point Firefox Health Report at a local server. We don't care if it actually
// works. It just can't hit the default production endpoint.
user_pref("datareporting.healthreport.documentServerURI", "http://%(server)s/healthreport/");

// Make sure CSS error reporting is enabled for tests
user_pref("layout.css.report_errors", true);

// Enable mozContacts
user_pref("dom.mozContacts.enabled", true);
user_pref("dom.navigator-property.disable.mozContacts", false);
user_pref("dom.global-constructor.disable.mozContact", false);

// Enable mozSettings
user_pref("dom.mozSettings.enabled", true);

// Make sure the disk cache doesn't get auto disabled
user_pref("network.http.bypass-cachelock-threshold", 200000);

// Enable Gamepad
user_pref("dom.gamepad.enabled", true);
user_pref("dom.gamepad.non_standard_events.enabled", true);

// Enable Web Audio
user_pref("media.webaudio.enabled", true);

// Enable Web Audio legacy APIs
user_pref("media.webaudio.legacy.AudioBufferSourceNode", true);
user_pref("media.webaudio.legacy.AudioContext", true);
user_pref("media.webaudio.legacy.AudioParam", true);
user_pref("media.webaudio.legacy.BiquadFilterNode", true);
user_pref("media.webaudio.legacy.PannerNode", true);
user_pref("media.webaudio.legacy.OscillatorNode", true);

// Always use network provider for geolocation tests
// so we bypass the OSX dialog raised by the corelocation provider
user_pref("geo.provider.testing", true);

// Background thumbnails in particular cause grief, and disabling thumbnails
// in general can't hurt - we re-enable them when tests need them.
user_pref("browser.pagethumbnails.capturing_disabled", true);

// Indicate that the download panel has been shown once so that whichever
// download test runs first doesn't show the popup inconsistently.
user_pref("browser.download.panel.shown", true);
