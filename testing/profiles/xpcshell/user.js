/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Base preferences file used by the xpcshell harness
/* globals user_pref */
/* eslint quotes: 0 */
user_pref("app.normandy.api_url", "https://%(server)s/selfsupport-dummy/");
user_pref("browser.safebrowsing.downloads.remote.url", "https://%(server)s/safebrowsing-dummy");
user_pref("extensions.systemAddon.update.url", "http://%(server)s/dummy-system-addons.xml");
// Treat WebExtension API/schema warnings as errors.
user_pref("extensions.webextensions.warnings-as-errors", true);
// Always use network provider for geolocation tests
// so we bypass the OSX dialog raised by the corelocation provider
user_pref("geo.provider.testing", true);
user_pref("browser.region.network.url", "");
user_pref("geo.provider.network.compare.url", "");
user_pref("media.gmp-manager.updateEnabled", false);
user_pref("media.gmp-manager.url.override", "http://%(server)s/dummy-gmp-manager.xml");
user_pref("toolkit.telemetry.server", "https://%(server)s/telemetry-dummy");
user_pref("telemetry.fog.test.localhost_port", -1);
// Prevent Remote Settings to issue non local connections.
user_pref("services.settings.server", "http://localhost/remote-settings-dummy/v1");
// Prevent intermediate preloads to be downloaded on Remote Settings polling.
user_pref("security.remote_settings.intermediates.enabled", false);
// The process priority manager only shifts priorities when it has at least
// one active tab. xpcshell tabs don't have any active tabs, which would mean
// all processes would run at low priority, which is not desirable, so we
// disable the process priority manager entirely here.
user_pref("dom.ipc.processPriorityManager.enabled", false);
// Avoid idle-daily notifications, to avoid expensive operations that may
// cause unexpected test timeouts.
user_pref("idle.lastDailyNotification", -1);
// Bug 455077 - Ensure we use sRGB as the output profile for test consistency.
user_pref("gfx.color_management.force_srgb", true);
user_pref("gfx.color_management.mode", 1);
// Don't enable remote tiles on new-tab pages in xpcshell
user_pref("browser.topsites.contile.enabled", false);
