"use strict";

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { SiteDataTestUtils } = ChromeUtils.import(
  "resource://testing-common/SiteDataTestUtils.jsm"
);
const { PermissionTestUtils } = ChromeUtils.import(
  "resource://testing-common/PermissionTestUtils.jsm"
);

function run_test() {
  do_get_profile();
  run_next_test();
}
