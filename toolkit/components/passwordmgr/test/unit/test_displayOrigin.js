/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test nsILoginInfo.displayOrigin
 */

"use strict";

add_task(function test_displayOrigin() {
  // Trying to access `displayOrigin` for each login shouldn't throw.
  for (let loginInfo of TestData.loginList()) {
    let { displayOrigin } = loginInfo;
    info(loginInfo.origin);
    info(displayOrigin);
    Assert.equal(typeof displayOrigin, "string", "Check type");
    Assert.greater(displayOrigin.length, 0, "Check length");
    if (
      loginInfo.origin.startsWith("chrome://") ||
      loginInfo.origin.startsWith("file:///")
    ) {
      Assert.ok(displayOrigin.startsWith(loginInfo.origin), "Contains origin");
    } else {
      Assert.ok(
        displayOrigin.startsWith(loginInfo.origin.replace(/.+:\/\//, "")),
        "Contains domain"
      );
    }
    let matches;
    if ((matches = loginInfo.origin.match(/:([0-9]+)$/))) {
      Assert.ok(displayOrigin.includes(matches[1]), "Check port is included");
    }
    Assert.ok(!displayOrigin.includes("null"), "Doesn't contain `null`");
    Assert.ok(
      !displayOrigin.includes("undefined"),
      "Doesn't contain `undefined`"
    );
    if (loginInfo.httpRealm !== null) {
      Assert.ok(
        displayOrigin.includes(loginInfo.httpRealm),
        "Contains httpRealm"
      );
    }
  }
});
