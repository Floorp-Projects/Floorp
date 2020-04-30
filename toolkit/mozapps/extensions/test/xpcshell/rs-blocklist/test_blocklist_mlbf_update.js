/* Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * @fileOverview Checks that the MLBF updating logic works reasonably.
 */

const { ExtensionBlocklistMLBF } = ChromeUtils.import(
  "resource://gre/modules/Blocklist.jsm",
  null
);

// This test needs to interact with the RemoteSettings client.
ExtensionBlocklistMLBF.ensureInitialized();

// Multiple internal calls to update should be coalesced and end up with the
// MLBF attachment from the last update call.
add_task(async function collapse_multiple_pending_update_requests() {
  const observed = [];

  // The first step of starting an update is to read from the RemoteSettings
  // collection. When a non-forced update is requested while another update is
  // pending, the non-forced update should return/await the previous call
  // instead of starting a new read/fetch from the RemoteSettings collection.
  // Add a spy to the RemoteSettings client, so we can verify that the number
  // of RemoteSettings accesses matches with what we expect.
  const originalClientGet = ExtensionBlocklistMLBF._client.get;
  const spyClientGet = (tag, returnValue) => {
    ExtensionBlocklistMLBF._client.get = async function() {
      // Record the method call.
      observed.push(tag);
      // Clone a valid record and tag it so we can identify it below.
      let dummyRecord = JSON.parse(JSON.stringify(MLBF_RECORD));
      dummyRecord.tagged = tag;
      return [dummyRecord];
    };
  };

  // Another significant part of updating is fetching the MLBF attachment.
  // Add a spy too, so we can check which attachment is being requested.
  const originalFetchMLBF = ExtensionBlocklistMLBF._fetchMLBF;
  ExtensionBlocklistMLBF._fetchMLBF = async function(record) {
    observed.push(`fetchMLBF:${record.tagged}`);
    throw new Error(`Deliberately ignoring call to MLBF:${record.tagged}`);
  };

  spyClientGet("initial"); // Very first call = read RS.
  let update1 = ExtensionBlocklistMLBF._updateMLBF(false);
  spyClientGet("unexpected update2"); // Non-forced update = reuse update1.
  let update2 = ExtensionBlocklistMLBF._updateMLBF(false);
  spyClientGet("forced1"); // forceUpdate=true = supersede previous update.
  let forcedUpdate1 = ExtensionBlocklistMLBF._updateMLBF(true);
  spyClientGet("forced2"); // forceUpdate=true = supersede previous update.
  let forcedUpdate2 = ExtensionBlocklistMLBF._updateMLBF(true);

  let res = await Promise.all([update1, update2, forcedUpdate1, forcedUpdate2]);

  Assert.equal(observed.length, 4, "expected number of observed events");
  Assert.equal(observed[0], "initial", "First update should request records");
  Assert.equal(observed[1], "forced1", "Forced update supersedes initial");
  Assert.equal(observed[2], "forced2", "Forced update supersedes forced1");
  // We call the _updateMLBF methods immediately after each other. Every update
  // request starts with an asynchronous operation (looking up the RS records),
  // so the implementation should return early for all update requests except
  // for the last one. So we should only observe a fetch for the last request.
  Assert.equal(observed[3], "fetchMLBF:forced2", "expected fetch result");

  // All update requests should end up with the same result.
  Assert.equal(res[0], res[1], "update1 == update2");
  Assert.equal(res[1], res[2], "update2 == forcedUpdate1");
  Assert.equal(res[2], res[3], "forcedUpdate1 == forcedUpdate2");

  ExtensionBlocklistMLBF._client.get = originalClientGet;
  ExtensionBlocklistMLBF._fetchMLBF = originalFetchMLBF;
});
