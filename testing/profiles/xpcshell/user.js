// Base preferences file used by the xpcshell harness
/* globals user_pref */
/* eslint quotes: 0 */
user_pref("app.normandy.api_url", "https://localhost/selfsupport-dummy/");
user_pref("browser.safebrowsing.downloads.remote.url", "https://localhost/safebrowsing-dummy");
user_pref("browser.search.geoip.url", "https://localhost/geoip-dummy");
user_pref("extensions.systemAddon.update.url", "http://localhost/dummy-system-addons.xml");
// Always use network provider for geolocation tests
// so we bypass the OSX dialog raised by the corelocation provider
user_pref("geo.provider.testing", true);
user_pref("media.gmp-manager.updateEnabled", false);
user_pref("media.gmp-manager.url.override", "http://localhost/dummy-gmp-manager.xml");
user_pref("toolkit.telemetry.server", "https://localhost/telemetry-dummy");
// The process priority manager only shifts priorities when it has at least
// one active tab. xpcshell tabs don't have any active tabs, which would mean
// all processes would run at low priority, which is not desirable, so we
// disable the process priority manager entirely here.
user_pref("dom.ipc.processPriorityManager.enabled", false);
