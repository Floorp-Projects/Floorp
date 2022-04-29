"use strict";

const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);
const { Utils } = ChromeUtils.import("resource://services-settings/Utils.jsm");

Cu.importGlobalProperties(["fetch"]);

async function getLocalDumpLastModified(bucket, collection) {
  let res = await fetch(
    `resource://app/defaults/settings/${bucket}/${collection}.json`
  );
  const { timestamp } = await res.json();
  ok(timestamp >= 0, `${bucket}/${collection} dump has timestamp`);
  return timestamp;
}

add_task(async function lastModified_of_non_existing_dump() {
  ok(!Utils._dumpStats, "_dumpStats not initialized");
  equal(
    await Utils.getLocalDumpLastModified("did not", "exist"),
    -1,
    "A non-existent dump has value -1"
  );
  ok(Utils._dumpStats, "_dumpStats was initialized");

  ok("did not/exist" in Utils._dumpStats, "cached non-existing dump result");
  delete Utils._dumpStats["did not/exist"];
});

add_task(async function lastModified_summary_is_correct() {
  if (AppConstants.platform == "android") {
    // TODO bug 1719560: When implemented, remove this condition.
    equal(JSON.stringify(Utils._dumpStats), "{}", "No dumps on Android yet");
    return;
  }
  ok(Object.keys(Utils._dumpStats).length > 0, "Contains summary of dumps");

  for (let [identifier, lastModified] of Object.entries(Utils._dumpStats)) {
    info(`Checking correctness of ${identifier}`);
    let [bucket, collection] = identifier.split("/");
    equal(
      await Utils.getLocalDumpLastModified(bucket, collection),
      lastModified,
      `Expected last_modified value for ${identifier}`
    );

    let actual = await getLocalDumpLastModified(bucket, collection);
    equal(lastModified, actual, `last_modified should match collection`);
  }
});
