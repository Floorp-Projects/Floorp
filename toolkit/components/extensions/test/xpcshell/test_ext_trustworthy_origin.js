"use strict";

/**
 * This test is asserting that moz-extension: URLs are recognized as trustworthy local origins
 */

add_task(
  function test_isOriginPotentiallyTrustworthnsIContentSecurityManagery() {
    let uri = NetUtil.newURI("moz-extension://foobar/something.html");
    let principal = Services.scriptSecurityManager.createContentPrincipal(
      uri,
      {}
    );
    Assert.equal(
      principal.isOriginPotentiallyTrustworthy,
      true,
      "it is potentially trustworthy"
    );
  }
);
