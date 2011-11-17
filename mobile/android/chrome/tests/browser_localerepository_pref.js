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
  desc: "Test dynamically changing extensions.getLocales.get.url",
  run: function() {
    Services.prefs.setCharPref(PREF_LOCALE_LIST, localeList);
    LocaleRepository.getLocales(this.listLoaded.bind(this));
  },

  listLoaded: function(aLocales) {
    is(aLocales.length, 1, "Correct number of locales were found");
    Services.prefs.setCharPref(PREF_LOCALE_LIST, localeList + "?numvalid=2");
    LocaleRepository.getLocales(this.secondListLoaded.bind(this));
  },

  secondListLoaded: function(aLocales) {
    is(aLocales.length, 2, "Correct number of locales were found");
    runNextTest();
  }
});
