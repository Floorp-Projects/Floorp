_("Make sure lazySvc get the desired services");
Cu.import("resource://weave/util.js");

function run_test() {
  _("Load the xul app info service as obj.app");
  let obj = {}
  do_check_eq(typeof obj.app, "undefined");
  Utils.lazySvc(obj, "app", "@mozilla.org/xre/app-info;1", "nsIXULAppInfo");
  do_check_eq(typeof obj.app.QueryInterface, "function");
  do_check_eq(typeof obj.app.vendor, "string");
  do_check_eq(typeof obj.app.name, "string");

  _("Check other types of properties on profiles");
  Utils.lazySvc(obj, "prof", "@mozilla.org/toolkit/profile-service;1", "nsIToolkitProfileService");
  do_check_eq(typeof obj.prof.QueryInterface, "function");
  do_check_eq(typeof obj.prof.startOffline, "boolean");
  do_check_eq(typeof obj.prof.profileCount, "number");
  do_check_eq(typeof obj.prof.createProfile, "function");

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
