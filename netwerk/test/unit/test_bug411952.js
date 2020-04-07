"use strict";

function run_test() {
  try {
    var cm = Cc["@mozilla.org/cookiemanager;1"].getService(Ci.nsICookieManager);
    Assert.notEqual(cm, null, "Retrieving the cookie manager failed");

    const time = new Date("Jan 1, 2030").getTime() / 1000;
    cm.add(
      "example.com",
      "/",
      "C",
      "V",
      false,
      true,
      false,
      time,
      {},
      Ci.nsICookie.SAMESITE_NONE
    );
    const now = Math.floor(new Date().getTime() / 1000);

    var found = false;
    for (let cookie of cm.cookies) {
      if (
        cookie.host == "example.com" &&
        cookie.path == "/" &&
        cookie.name == "C"
      ) {
        Assert.ok(
          "creationTime" in cookie,
          "creationTime attribute is not accessible on the cookie"
        );
        var creationTime = Math.floor(cookie.creationTime / 1000000);
        // allow the times to slip by one second at most,
        // which should be fine under normal circumstances.
        Assert.ok(
          Math.abs(creationTime - now) <= 1,
          "Cookie's creationTime is set incorrectly"
        );
        found = true;
        break;
      }
    }

    Assert.ok(found, "Didn't find the cookie we were after");
  } catch (e) {
    do_throw("Unexpected exception: " + e.toString());
  }

  do_test_finished();
}
