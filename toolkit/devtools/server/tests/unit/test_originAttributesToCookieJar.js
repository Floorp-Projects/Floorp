/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://gre/modules/Services.jsm");
var ssm = Services.scriptSecurityManager;

function run_test() {
  const appId = 12;
  var browserAttrs = {appId: appId, inBrowser: true};

  // ChromeUtils.originAttributesToCookieJar should return the same value with
  // the cookieJar of the principal created from the same origin attribute.
  var cookieJar_1 = ChromeUtils.originAttributesToCookieJar(browserAttrs);
  var dummy = Services.io.newURI("http://example.com", null, null);
  var cookieJar_2 = ssm.createCodebasePrincipal(dummy, browserAttrs).cookieJar;
  do_check_eq(cookieJar_1, cookieJar_2);

  // App and mozbrowser shouldn't have the same cookieJar identifier.
  var appAttrs = {appId: appId, inBrowser: false};
  var cookieJar_3 = ChromeUtils.originAttributesToCookieJar(appAttrs);
  do_check_neq(cookieJar_1, cookieJar_3);

  // If the attribute is null the cookieJar identifier should be empty.
  var cookieJar_4 = ChromeUtils.originAttributesToCookieJar();
  do_check_eq(cookieJar_4, "");
}
