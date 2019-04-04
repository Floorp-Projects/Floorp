/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const {MockRegistrar} = ChromeUtils.import("resource://testing-common/MockRegistrar.jsm");

function platformInfo(injectedValue) {
  this.platformVersion = injectedValue;
}

platformInfo.prototype = {
  platformVersion: "some version",
  platformBuildID: "some id",
  QueryInterface: ChromeUtils.generateQI([Ci.nsIPlatformInfo]),
};

add_test(function test_register() {
  let localPlatformInfo = {
    platformVersion: "local version",
    platformBuildID: "local id",
    QueryInterface: ChromeUtils.generateQI([Ci.nsIPlatformInfo]),
  };

  MockRegistrar.register("@mozilla.org/xre/app-info;1", localPlatformInfo);
  Assert.equal(Cc["@mozilla.org/xre/app-info;1"].createInstance(Ci.nsIPlatformInfo).platformVersion, "local version");
  run_next_test();
});

add_test(function test_register_with_arguments() {
  MockRegistrar.register("@mozilla.org/xre/app-info;1", platformInfo, ["override"]);
  Assert.equal(Cc["@mozilla.org/xre/app-info;1"].createInstance(Ci.nsIPlatformInfo).platformVersion, "override");
  run_next_test();
});

add_test(function test_register_twice() {
  MockRegistrar.register("@mozilla.org/xre/app-info;1", platformInfo, ["override"]);
  Assert.equal(Cc["@mozilla.org/xre/app-info;1"].createInstance(Ci.nsIPlatformInfo).platformVersion, "override");

  MockRegistrar.register("@mozilla.org/xre/app-info;1", platformInfo, ["override again"]);
  Assert.equal(Cc["@mozilla.org/xre/app-info;1"].createInstance(Ci.nsIPlatformInfo).platformVersion, "override again");
  run_next_test();
});
