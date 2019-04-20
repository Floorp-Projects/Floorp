/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const {Corroborate} = ChromeUtils.import("resource://gre/modules/Corroborate.jsm");
const {FileUtils} = ChromeUtils.import("resource://gre/modules/FileUtils.jsm");
const {Services} = ChromeUtils.import("resource://gre/modules/Services.jsm");


add_task(async function test_various_jars() {
  let result = await Corroborate.verifyJar(do_get_file("data/unsigned.xpi"));
  equal(result, false, "unsigned files do not verify");

  result = await Corroborate.verifyJar(do_get_file("data/signed-amo.xpi"));
  equal(result, false, "AMO signed files do not verify");

  result = await Corroborate.verifyJar(do_get_file("data/signed-privileged.xpi"));
  equal(result, false, "Privileged signed files do not verify");

  let missingFile = do_get_file("data");
  missingFile.append("missing.xpi");

  result = await Corroborate.verifyJar(missingFile);
  equal(result, false, "Missing (but expected) files do not verify");

  result = await Corroborate.verifyJar(do_get_file("data/signed-components.xpi"));
  equal(result, true, "Components signed files do verify");
});

add_task(async function test_telemetry() {
  Corroborate.reportTelemetry(true, false);

  const scalars = Services.telemetry.getSnapshotForScalars("main", false).parent;

  ok("corroborate.omnijar_corrupted" in scalars,
     "omni jar should be corrupted");
  ok("corroborate.system_addons_corrupted" in scalars,
     "system addons should not be corrupted");
});
