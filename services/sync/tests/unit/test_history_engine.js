/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://services-sync/service.js");
Cu.import("resource://services-sync/engines/history.js");
Cu.import("resource://testing-common/services/sync/utils.js");

add_task(async function setup() {
  initTestLogging("Trace");
});

add_task(async function test_history_download_limit() {
  let engine = new HistoryEngine(Service);
  await engine.initialize();

  let server = await serverForFoo(engine);
  await SyncTestingInfrastructure(server);

  let lastSync = Date.now() / 1000;

  let collection = server.user("foo").collection("history");
  for (let i = 0; i < 15; i++) {
    let id = "place" + i.toString(10).padStart(7, "0");
    let wbo = new ServerWBO(id, encryptPayload({
      id,
      histUri: "http://example.com/" + i,
      title: "Page " + i,
      visits: [{
        date: Date.now() * 1000,
        type: PlacesUtils.history.TRANSITIONS.TYPED,
      }, {
        date: Date.now() * 1000,
        type: PlacesUtils.history.TRANSITIONS.LINK,
      }],
    }), lastSync + 1 + i);
    wbo.sortindex = 15 - i;
    collection.insertWBO(wbo);
  }

  // We have 15 records on the server since the last sync, but our download
  // limit is 5 records at a time. We should eventually fetch all 15.
  engine.lastSync = lastSync;
  engine.downloadBatchSize = 4;
  engine.downloadLimit = 5;

  // Don't actually fetch any backlogged records, so that we can inspect
  // the backlog between syncs.
  engine.guidFetchBatchSize = 0;

  let ping = await sync_engine_and_validate_telem(engine, false);
  deepEqual(ping.engines[0].incoming, { applied: 5 });

  let backlogAfterFirstSync = engine.toFetch.slice(0);
  deepEqual(backlogAfterFirstSync, ["place0000000", "place0000001",
    "place0000002", "place0000003", "place0000004", "place0000005",
    "place0000006", "place0000007", "place0000008", "place0000009"]);

  // We should have fast-forwarded the last sync time.
  equal(engine.lastSync, lastSync + 15);

  engine.lastModified = collection.modified;
  ping = await sync_engine_and_validate_telem(engine, false);
  ok(!ping.engines[0].incoming);

  // After the second sync, our backlog still contains the same GUIDs: we
  // weren't able to make progress on fetching them, since our
  // `guidFetchBatchSize` is 0.
  let backlogAfterSecondSync = engine.toFetch.slice(0);
  deepEqual(backlogAfterFirstSync, backlogAfterSecondSync);

  // Now add a newer record to the server.
  let newWBO = new ServerWBO("placeAAAAAAA", encryptPayload({
    id: "placeAAAAAAA",
    histUri: "http://example.com/a",
    title: "New Page A",
    visits: [{
      date: Date.now() * 1000,
      type: PlacesUtils.history.TRANSITIONS.TYPED,
    }],
  }), lastSync + 20);
  newWBO.sortindex = -1;
  collection.insertWBO(newWBO);

  engine.lastModified = collection.modified;
  ping = await sync_engine_and_validate_telem(engine, false);
  deepEqual(ping.engines[0].incoming, { applied: 1 });

  // Our backlog should remain the same.
  let backlogAfterThirdSync = engine.toFetch.slice(0);
  deepEqual(backlogAfterSecondSync, backlogAfterThirdSync);

  equal(engine.lastSync, lastSync + 20);

  // Bump the fetch batch size to let the backlog make progress. We should
  // make 3 requests to fetch 5 backlogged GUIDs.
  engine.guidFetchBatchSize = 2;

  engine.lastModified = collection.modified;
  ping = await sync_engine_and_validate_telem(engine, false);
  deepEqual(ping.engines[0].incoming, { applied: 5 });

  deepEqual(engine.toFetch, ["place0000005", "place0000006", "place0000007",
    "place0000008", "place0000009"]);

  // Sync again to clear out the backlog.
  engine.lastModified = collection.modified;
  ping = await sync_engine_and_validate_telem(engine, false);
  deepEqual(ping.engines[0].incoming, { applied: 5 });

  deepEqual(engine.toFetch, []);
});
