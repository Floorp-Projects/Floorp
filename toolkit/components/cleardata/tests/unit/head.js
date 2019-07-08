"use strict";

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { SiteDataTestUtils } = ChromeUtils.import(
  "resource://testing-common/SiteDataTestUtils.jsm"
);

function run_test() {
  do_get_profile();
  run_next_test();
}
