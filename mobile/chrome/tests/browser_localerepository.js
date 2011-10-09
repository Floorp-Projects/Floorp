var localeList = serverRoot + "locales_list.sjs";
var PREF_LOCALE_LIST = "extensions.getLocales.get.url";

Components.utils.import("resource://gre/modules/Services.jsm");
Components.utils.import("resource://gre/modules/LocaleRepository.jsm");

function test() {
  waitForExplicitFinish();
  runNextTest();
}

function end_test() {
  Services.prefs.clearUserPref(PREF_LOCALE_LIST);
}

registerCleanupFunction(end_test);

gTests.push({
  desc: "Test getting a list of compatable locales",
  run: function() {
    Services.prefs.setCharPref(PREF_LOCALE_LIST, localeList);
    LocaleRepository.getLocales(this.listLoaded);
  },

  listLoaded: function(aLocales) {
    is(aLocales.length, 1, "Correct number of locales were found");
    isnot(aLocales[0].addon, null, "Locale has an addon");
    is(aLocales[0].xpiURL, "http://www.example.com/browser/mobile/chrome/tests/addons/browser_locale1.xpi", "Locale has correct xpi url");
    is(aLocales[0].xpiHash, null, "Locale has correct hash");

    is(aLocales[0].addon.id, "langpack-test1@firefox-mobile.mozilla.org", "Locale has correct id");
    is(aLocales[0].addon.name, "Test Locale 1", "Locale has correct name");
    is(aLocales[0].addon.type, "language", "Locale has correct type");

    is(aLocales[0].addon.targetLocale, "test1", "Locale has correct target locale");
    is(aLocales[0].addon.version, "1.0", "Locale has correct version");
    runNextTest();
  }
});
