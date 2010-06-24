_("Make sure lazySvc get the desired services");
Cu.import("resource://services-sync/util.js");

function run_test() {
  _("Load the xul app info service as obj.app");
  let obj = {}
  do_check_eq(typeof obj.app, "undefined");
  Utils.lazySvc(obj, "app", "@mozilla.org/xre/app-info;1", "nsIXULAppInfo");
  do_check_eq(typeof obj.app.QueryInterface, "function");
  do_check_eq(typeof obj.app.vendor, "string");
  do_check_eq(typeof obj.app.name, "string");

  _("Check other types of properties on other services");
  Utils.lazySvc(obj, "io", "@mozilla.org/network/io-service;1", "nsIIOService");
  do_check_eq(typeof obj.io.newURI, "function");
  do_check_eq(typeof obj.io.offline, "boolean");

  Utils.lazySvc(obj, "thread", "@mozilla.org/thread-manager;1", "nsIThreadManager");
  do_check_true(obj.thread.currentThread instanceof Ci.nsIThread);

  Utils.lazySvc(obj, "net", "@mozilla.org/network/util;1", "nsINetUtil");
  do_check_eq(typeof obj.net.ESCAPE_ALL, "number");
  do_check_eq(obj.net.ESCAPE_URL_SCHEME, 1);

  _("Make sure fake services get loaded correctly (private browsing doesnt exist on all platforms)");
  Utils.lazySvc(obj, "priv", "@mozilla.org/privatebrowsing;1", "nsIPrivateBrowsingService");
  do_check_eq(typeof obj.priv.privateBrowsingEnabled, "boolean");

  _("Definitely make sure services that should never exist will use fake service if available");
  Utils.lazySvc(obj, "fake", "@labs.mozilla.com/Fake/Thing;1", "fake");
  do_check_eq(obj.fake.isFake, true);

  _("Nonexistant services that aren't fake-implemented will get nothing");
  Utils.lazySvc(obj, "nonexist", "@something?@", "doesnt exist");
  do_check_eq(obj.nonexist, undefined);
}
