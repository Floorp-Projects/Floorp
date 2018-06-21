/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/
*/
"use strict";

ChromeUtils.import("resource://gre/modules/Services.jsm", this);

const CONTENT_ONLY_UINT_SCALAR = "telemetry.test.content_only_uint";
const ALL_PROCESSES_UINT_SCALAR = "telemetry.test.all_processes_uint";
const DEFAULT_PRODUCTS_SCALAR = "telemetry.test.default_products";
const CHILD_KEYED_UNSIGNED_INT = "telemetry.test.child_keyed_unsigned_int";

// Note: this test file is only supposed to be run by
// test_GeckoView_ScalarSemantics.js.
// It assumes to be in the content process.
add_task(async function run_child() {
  // Set initial values in content process.
  Services.telemetry.scalarSet(CONTENT_ONLY_UINT_SCALAR, 14);
  Services.telemetry.scalarSet(ALL_PROCESSES_UINT_SCALAR, 24);
  Services.telemetry.keyedScalarSet(CHILD_KEYED_UNSIGNED_INT, "chewbacca", 44);

  // Wait for the parent to inform us to continue.
  await do_await_remote_message("child-scalar-semantics");

  // Modifications to probes will be recorded and applied later.
  Services.telemetry.scalarAdd(CONTENT_ONLY_UINT_SCALAR, 10);
  Services.telemetry.scalarAdd(ALL_PROCESSES_UINT_SCALAR, 10);
  Services.telemetry.keyedScalarAdd(CHILD_KEYED_UNSIGNED_INT, "chewbacca", 10);
  Services.telemetry.scalarSet(DEFAULT_PRODUCTS_SCALAR, 1);
});
