// Prefs specific to b2g mochitests

user_pref("browser.homescreenURL","app://test-container.gaiamobile.org/index.html");
user_pref("browser.manifestURL","app://test-container.gaiamobile.org/manifest.webapp");
user_pref("dom.mozBrowserFramesEnabled", "%(OOP)s");
user_pref("dom.ipc.tabs.disabled", false);
user_pref("dom.ipc.browser_frames.oop_by_default", false);
user_pref("dom.mozBrowserFramesWhitelist","app://test-container.gaiamobile.org,http://mochi.test:8888");
user_pref("marionette.force-local", true);
