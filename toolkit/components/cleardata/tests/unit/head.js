"use strict";

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

function getOAWithPartitionKey(
  { scheme = "https", topLevelBaseDomain, port = null } = {},
  originAttributes = {}
) {
  if (!topLevelBaseDomain || !scheme) {
    return originAttributes;
  }

  return {
    ...originAttributes,
    partitionKey: `(${scheme},${topLevelBaseDomain}${port ? `,${port}` : ""})`,
  };
}
