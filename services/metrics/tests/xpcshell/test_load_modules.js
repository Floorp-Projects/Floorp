/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const modules = [
  "collector.jsm",
  "dataprovider.jsm",
];

const test_modules = [
  "mocks.jsm",
];

function run_test() {
  for (let m of modules) {
    let resource = "resource://gre/modules/services/metrics/" + m;
    Components.utils.import(resource, {});
  }

  for (let m of test_modules) {
    let resource = "resource://testing-common/services/metrics/" + m;
    Components.utils.import(resource, {});
  }
}

