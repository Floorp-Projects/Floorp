var localeList = serverRoot + "locales_list.sjs";
var PREF_LOCALE_LIST = "extensions.getLocales.get.url";

Components.utils.import("resource://gre/modules/Services.jsm");
Components.utils.import("resource://gre/modules/LocaleRepository.jsm");
Components.utils.import("resource://gre/modules/AddonManager.jsm");

function test() {
  waitForExplicitFinish();
  runNextTest();
}

function end_test() {
  Services.prefs.clearUserPref(PREF_LOCALE_LIST);
}

registerCleanupFunction(end_test);

gTests.push({
  addon: null,
  desc: "Test the values returned from _getLocalesInAddon",
  run: function() {
    Services.prefs.setCharPref(PREF_LOCALE_LIST, localeList);
    LocaleRepository.getLocales(this.listLoaded.bind(this));
  },

  listLoaded: function(aLocales) {
    is(aLocales.length, 1, "Correct number of locales were found");
    aLocales[0].addon.install.addListener(this);
    aLocales[0].addon.install.install();
  },

  onInstallEnded: function(aInstall, aAddon) {
    aInstall.removeListener(this);
    this.addon = aAddon;
    info("Installed " + aAddon.id);

    try {
      ExtensionsView._getLocalesInAddon(aAddon, null);
      ok(false, "_getLocalesInAddon should have thrown with a null callback");
    } catch(ex) {
      ok(ex, "_getLocalesInAddon requires a callback")
    }

    try {
      ExtensionsView._getLocalesInAddon(aAddon, "foo");
      ok(false, "_getLocalesInAddons should have thrown without a non-function callback");
    } catch(ex) {
      ok(ex, "_getLocalesInAddon requires the callback be a function")
    }

    ExtensionsView._getLocalesInAddon(aAddon, this.gotLocales.bind(this));
  },

  gotLocales: function(aLocales) {
    is(aLocales.length, 2, "Correct number of locales were found");
    ok(aLocales.indexOf("te-st") > -1, "te-st locale found");
    ok(aLocales.indexOf("te-st-a") > -1, "te-st-a locale found");

    // locales can't be restartless yet, so we can't really test the uninstall code
    this.addon.install.cancel();
    runNextTest();
  }
});
