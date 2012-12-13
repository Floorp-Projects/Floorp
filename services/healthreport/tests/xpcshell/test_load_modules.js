/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const modules = [
  "healthreporter.jsm",
  "policy.jsm",
  "profile.jsm",
  "providers.jsm",
];

const test_modules = [
  "mocks.jsm",
];

function run_test() {
  for (let m of modules) {
    let resource = "resource://gre/modules/services/healthreport/" + m;
    Components.utils.import(resource, {});
  }

  for (let m of test_modules) {
    let resource = "resource://testing-common/services/healthreport/" + m;
    Components.utils.import(resource, {});
  }
}

