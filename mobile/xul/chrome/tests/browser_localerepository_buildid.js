var localeList = serverRoot + "locales_list.sjs";
var PREF_LOCALE_LIST = "extensions.getLocales.get.url";

let tmp = {};
Components.utils.import("resource://gre/modules/LocaleRepository.jsm", tmp);
let LocaleRepository = tmp.LocaleRepository;

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
    Services.prefs.setCharPref(PREF_LOCALE_LIST, localeList + "?buildid=%BUILDID_EXPANDED%");
    LocaleRepository.getLocales(this.listLoaded.bind(this), {buildID: "00001122334455"});
  },

  listLoaded: function(aLocales) {
    is(aLocales.length, 1, "Correct number of locales were found");
    is(aLocales[0].addon.name, "0000-11-22-33-44-55", "Buildid was correctly replaced");

    Services.prefs.setCharPref(PREF_LOCALE_LIST, localeList + "?buildid=%BUILDID_EXPANDED%");
    LocaleRepository.getLocales(this.secondListLoaded.bind(this));
  },

  secondListLoaded: function(aLocales) {
    is(aLocales.length, 1, "Correct number of locales were found");

    let buildID = Cc["@mozilla.org/xre/app-info;1"].getService(Ci.nsIXULAppInfo).QueryInterface(Ci.nsIXULRuntime).appBuildID;
    is(aLocales[0].addon.name.replace(/-/g, ""), buildID, "Buildid was correctly replaced");

    runNextTest();
  }
});
