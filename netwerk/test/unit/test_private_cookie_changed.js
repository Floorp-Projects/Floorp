/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
Components.utils.import("resource://gre/modules/Services.jsm");
Components.utils.import("resource://gre/modules/NetUtil.jsm");

function makeChan(uri, isPrivate) {
  var ios = Cc["@mozilla.org/network/io-service;1"].getService(Ci.nsIIOService);
  var chan = ios.newChannel(uri.spec, null, null)
                .QueryInterface(Ci.nsIHttpChannel);
  chan.QueryInterface(Ci.nsIPrivateBrowsingChannel).setPrivate(isPrivate);
  return chan;
}

function run_test() {
  // Allow all cookies.
  Services.prefs.setIntPref("network.cookie.cookieBehavior", 0);
  
  let publicNotifications = 0;
  let privateNotifications = 0;
  Services.obs.addObserver(function() {publicNotifications++;}, "cookie-changed", false);
  Services.obs.addObserver(function() {privateNotifications++;}, "private-cookie-changed", false);

  let uri = NetUtil.newURI("http://foo.com/");
  let publicChan = makeChan(uri, false);
  let svc = Services.cookies.QueryInterface(Ci.nsICookieService);
  svc.setCookieString(uri, null, "oh=hai", publicChan);
  let privateChan = makeChan(uri, true);
  svc.setCookieString(uri, null, "oh=hai", privateChan);
  do_check_eq(publicNotifications, 1);
  do_check_eq(privateNotifications, 1);
}
