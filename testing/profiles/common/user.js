/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Common preferences file used by both unittest and perf harnesses.
/* globals user_pref */
user_pref("app.update.checkInstallTime", false);
user_pref("app.update.disabledForTesting", true);
user_pref("browser.chrome.guess_favicon", false);
user_pref("browser.dom.window.dump.enabled", true);
user_pref("devtools.console.stdout.chrome", true);
// asrouter expects a plain object or null
user_pref("browser.newtabpage.activity-stream.asrouter.providers.cfr", "null");
user_pref("browser.newtabpage.activity-stream.asrouter.providers.snippets", "null");
user_pref("browser.newtabpage.activity-stream.asrouter.providers.message-groups", "null");
user_pref("browser.newtabpage.activity-stream.asrouter.providers.whats-new-panel", "null");
user_pref("browser.newtabpage.activity-stream.asrouter.providers.messaging-experiments", "null");
user_pref("browser.newtabpage.activity-stream.feeds.system.topstories", false);
user_pref("browser.newtabpage.activity-stream.feeds.snippets", false);
user_pref("browser.newtabpage.activity-stream.tippyTop.service.endpoint", "");
user_pref("browser.newtabpage.activity-stream.discoverystream.config", "[]");

// For Activity Stream firstrun page, use an empty string to avoid fetching.
user_pref("browser.newtabpage.activity-stream.fxaccounts.endpoint", "");
// Background thumbnails in particular cause grief, and disabling thumbnails
// in general can't hurt - we re-enable them when tests need them.
user_pref("browser.pagethumbnails.capturing_disabled", true);
// Tell the search service we are running in the US.  This also has the desired
// side-effect of preventing our geoip lookup.
user_pref("browser.search.region", "US");
// disable infobar for tests
user_pref("browser.search.removeEngineInfobar.enabled", false);
// Disable webapp updates.  Yes, it is supposed to be an integer.
user_pref("browser.webapps.checkForUpdates", 0);
// We do not wish to display datareporting policy notifications as it might
// cause other tests to fail. Tests that wish to test the notification functionality
// should explicitly disable this pref.
user_pref("datareporting.policy.dataSubmissionPolicyBypassNotification", true);
user_pref("dom.max_chrome_script_run_time", 0);
user_pref("dom.max_script_run_time", 0); // no slow script dialogs
user_pref("dom.send_after_paint_to_content", true);
// Only load extensions from the application and user profile
// AddonManager.SCOPE_PROFILE + AddonManager.SCOPE_APPLICATION
user_pref("extensions.enabledScopes", 5);
user_pref("extensions.experiments.enabled", true);
// Turn off extension updates so they don't bother tests
user_pref("extensions.update.enabled", false);
// Prevent network access for recommendations by default. The payload is {"results":[]}.
user_pref("extensions.getAddons.discovery.api_url", "data:;base64,eyJyZXN1bHRzIjpbXX0%3D");
// Treat WebExtension API/schema warnings as errors.
user_pref("extensions.webextensions.warnings-as-errors", true);
// Disable useragent updates.
user_pref("general.useragent.updates.enabled", false);
user_pref("hangmonitor.timeout", 0); // no hang monitor
user_pref("media.gmp-manager.updateEnabled", false);
// Don't do network connections for mitm priming
user_pref("security.certerrors.mitm.priming.enabled", false);
// Enable some dangerous features for test code. :-(
user_pref("security.turn_off_all_security_so_that_viruses_can_take_over_this_computer", true);
user_pref("xpinstall.signatures.required", false);
// Prevent Remote Settings to issue non local connections.
user_pref("services.settings.server", "data:,#remote-settings-dummy/v1");
// Ensure autoplay is enabled for all platforms.
user_pref("media.autoplay.default", 0); // 0=Allowed, 1=Blocked, 2=Prompt
user_pref("media.autoplay.blocking_policy", 0);
user_pref("media.autoplay.ask-permission", false);
user_pref("media.allowed-to-play.enabled", true);
// Ensure media can always play without delay
user_pref("media.block-autoplay-until-in-foreground", false);
user_pref("toolkit.telemetry.coverage.endpoint.base", "http://localhost");
// Don't ask for a request in testing unless explicitly set this as true.
user_pref("media.geckoview.autoplay.request", false);
user_pref("geo.provider.network.compare.url", "");
user_pref("browser.region.network.url", "");
// Do not unload tabs on low memory when testing
user_pref("browser.tabs.unloadOnLowMemory", false);
// Don't pull Top Sites content from the network
user_pref("browser.topsites.contile.enabled", false);
// Don't pull sponsored Top Sites content from the network
user_pref("browser.newtabpage.activity-stream.showSponsoredTopSites", false);
// Default Glean to "record but don't report" mode. Docs:
// https://firefox-source-docs.mozilla.org/toolkit/components/glean/dev/preferences.html
user_pref("telemetry.fog.test.localhost_port", -1);
// Disable overlay scrollbars on GTK for testing. A bunch of tests (specially
// mochitests) assume scrollbars take space. We disable them on macOS (where
// overlay is also the default) at the system level as well, so this is
// probably ok.
//
// We test the relevant overlay scrollbar code-paths on Android.
user_pref("widget.gtk.overlay-scrollbars.enabled", false);
// Generally, we don't want daily idle tasks run during tests. Specific tests
// can re-enable if needed.
user_pref("idle.lastDailyNotification", -1);
