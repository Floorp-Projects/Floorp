/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { BasePromiseWorker } = ChromeUtils.importESModule(
  "resource://gre/modules/PromiseWorker.sys.mjs"
);

// Worker must be loaded from a chrome:// uri, not a file://
// uri, so we first need to load it.

var WORKER_SOURCE_URI = "chrome://promiseworker/content/worker.mjs";
do_load_manifest("data/chrome.manifest");
var worker = new BasePromiseWorker(WORKER_SOURCE_URI, { type: "module" });

add_task(async function test_simple_args() {
  let message = ["test_simple_args", Math.random()];
  let result = await worker.post("bounce", message);
  Assert.equal(JSON.stringify(result), JSON.stringify(message));
});
