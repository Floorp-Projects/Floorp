/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://testing-common/MockRegistrar.jsm");

function userInfo(username) {
  this.username = username;
}

userInfo.prototype = {
  fullname: "fullname",
  emailAddress: "emailAddress",
  domain: "domain",
  QueryInterface: ChromeUtils.generateQI([Ci.nsIUserInfo]),
};

add_test(function test_register() {
  let localUserInfo = {
    fullname: "fullname",
    username: "localusername",
    emailAddress: "emailAddress",
    domain: "domain",
    QueryInterface: ChromeUtils.generateQI([Ci.nsIUserInfo]),
  };

  MockRegistrar.register("@mozilla.org/userinfo;1", localUserInfo);
  Assert.equal(Cc["@mozilla.org/userinfo;1"].createInstance(Ci.nsIUserInfo).username, "localusername");
  run_next_test();
});

add_test(function test_register_with_arguments() {
  MockRegistrar.register("@mozilla.org/userinfo;1", userInfo, ["username"]);
  Assert.equal(Cc["@mozilla.org/userinfo;1"].createInstance(Ci.nsIUserInfo).username, "username");
  run_next_test();
});

add_test(function test_register_twice() {
  MockRegistrar.register("@mozilla.org/userinfo;1", userInfo, ["originalname"]);
  Assert.equal(Cc["@mozilla.org/userinfo;1"].createInstance(Ci.nsIUserInfo).username, "originalname");

  MockRegistrar.register("@mozilla.org/userinfo;1", userInfo, ["newname"]);
  Assert.equal(Cc["@mozilla.org/userinfo;1"].createInstance(Ci.nsIUserInfo).username, "newname");
  run_next_test();
});
