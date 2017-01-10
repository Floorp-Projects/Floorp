/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

Components.utils.import("resource://gre/modules/AppConstants.jsm");

const MODULE_BASE = "resource://services-common/";
const shared_modules = [
  "async.js",
  "logmanager.js",
  "rest.js",
  "stringbundle.js",
  "utils.js",
];

const non_android_modules = [
  "tokenserverclient.js",
];

const TEST_BASE = "resource://testing-common/services/common/";
const shared_test_modules = [
  "logging.js",
];

const non_android_test_modules = [
  "storageserver.js",
];

function expectImportsToSucceed(mm, base = MODULE_BASE) {
  for (let m of mm) {
    let resource = base + m;
    let succeeded = false;
    try {
      Components.utils.import(resource, {});
      succeeded = true;
    } catch (e) {}

    if (!succeeded) {
      throw "Importing " + resource + " should have succeeded!";
    }
  }
}

function expectImportsToFail(mm, base = MODULE_BASE) {
  for (let m of mm) {
    let resource = base + m;
    let succeeded = false;
    try {
      Components.utils.import(resource, {});
      succeeded = true;
    } catch (e) {}

    if (succeeded) {
      throw "Importing " + resource + " should have failed!";
    }
  }
}

function run_test() {
  expectImportsToSucceed(shared_modules);
  expectImportsToSucceed(shared_test_modules, TEST_BASE);

  if (AppConstants.platform != "android") {
    expectImportsToSucceed(non_android_modules);
    expectImportsToSucceed(non_android_test_modules, TEST_BASE);
  } else {
    expectImportsToFail(non_android_modules);
    expectImportsToFail(non_android_test_modules, TEST_BASE);
  }
}
