// Base preferences file used by the xpcshell harness
/* globals user_pref */
/* eslint quotes: 0 */
user_pref("app.normandy.api_url", "https://%(server)s/selfsupport-dummy/");
user_pref("browser.safebrowsing.downloads.remote.url", "https://%(server)s/safebrowsing-dummy");
user_pref("browser.search.geoSpecificDefaults", false);
user_pref("extensions.systemAddon.update.url", "http://%(server)s/dummy-system-addons.xml");
// Treat WebExtension API/schema warnings as errors.
user_pref("extensions.webextensions.warnings-as-errors", true);
// Always use network provider for geolocation tests
// so we bypass the OSX dialog raised by the corelocation provider
user_pref("geo.provider.testing", true);
user_pref("media.gmp-manager.updateEnabled", false);
user_pref("media.gmp-manager.url.override", "http://%(server)s/dummy-gmp-manager.xml");
user_pref("toolkit.telemetry.server", "https://%(server)s/telemetry-dummy");
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
// Enable telemetry event ping during tests, even for geckoview, where it
// is normally disabled.
user_pref("toolkit.telemetry.eventping.enabled", true);
// XXX: Bug 1617611 - Fix all the tests broken by "cookies sameSite=lax by default"
user_pref("network.cookie.sameSite.laxByDefault", false);
// Bug 455077 - Ensure we use sRGB as the output profile for test consistency.
user_pref("gfx.color_management.force_srgb", true);
user_pref("gfx.color_management.mode", 1);
