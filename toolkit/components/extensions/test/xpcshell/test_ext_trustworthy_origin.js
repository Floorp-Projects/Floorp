"use strict";

/**
 * This test is asserting that moz-extension: URLs are recognized as trustworthy local origins
 */

add_task(function test_isOriginPotentiallyTrustworthnsIContentSecurityManagery() {
  let contentSecManager = Cc["@mozilla.org/contentsecuritymanager;1"]
                          .getService(Ci.nsIContentSecurityManager);
  let uri = NetUtil.newURI("moz-extension://foobar/something.html");
  let principal = Services.scriptSecurityManager.createCodebasePrincipal(uri, {});
  Assert.equal(contentSecManager.isOriginPotentiallyTrustworthy(principal), true, "it is potentially trustworthy");
});
