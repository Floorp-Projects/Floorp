Cu.import("resource://services-sync/util.js");

function run_test() {
  _("Test with a valid icon URI");
  let iconUri = "http://foo.bar/favicon.png";
  let icon1 = Utils.getIcon(iconUri);
  do_check_true(icon1.indexOf(iconUri) > 0);

  _("Test with an invalid icon URI and default icon");
  let icon2 = Utils.getIcon("foo", "bar");
  do_check_eq(icon2, "bar");

  _("Test with an invalid icon URI and no default icon");
  let icon3 = Utils.getIcon("foo");
  var defaultFavicon = Cc["@mozilla.org/browser/favicon-service;1"]
                         .getService(Ci.nsIFaviconService).defaultFavicon.spec;
  do_check_eq(icon3, defaultFavicon);
}
