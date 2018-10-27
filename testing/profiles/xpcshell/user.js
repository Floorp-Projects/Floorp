// Base preferences file used by the xpcshell harness
/* globals user_pref */
/* eslint quotes: 0 */
user_pref("app.normandy.api_url", "https://%(server)s/selfsupport-dummy/");
user_pref("browser.safebrowsing.downloads.remote.url", "https://%(server)s/safebrowsing-dummy");
user_pref("browser.search.geoip.url", "https://%(server)s/geoip-dummy");
user_pref("extensions.systemAddon.update.url", "http://%(server)s/dummy-system-addons.xml");
// Always use network provider for geolocation tests
// so we bypass the OSX dialog raised by the corelocation provider
user_pref("geo.provider.testing", true);
// Bug 1500975 - this is not being applied properly (it should be a bool)
user_pref("media.gmp-manager.updateEnabled", 'false');
user_pref("media.gmp-manager.url.override", "http://%(server)s/dummy-gmp-manager.xml");
user_pref("toolkit.telemetry.server", "https://%(server)s/telemetry-dummy");
