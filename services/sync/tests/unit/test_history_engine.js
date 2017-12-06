/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://services-sync/service.js");
Cu.import("resource://services-sync/engines/history.js");
Cu.import("resource://services-common/utils.js");
Cu.import("resource://testing-common/services/sync/utils.js");

add_task(async function setup() {
  initTestLogging("Trace");
});

async function rawAddVisit(id, uri, visitPRTime, transitionType) {
  return new Promise((resolve, reject) => {
    let results = [];
    let handler = {
      handleResult(result) {
        results.push(result);
      },
      handleError(resultCode, placeInfo) {
        do_throw(`updatePlaces gave error ${resultCode}!`);
      },
      handleCompletion(count) {
        resolve({ results, count });
      }
    };
    PlacesUtils.asyncHistory.updatePlaces([{
      guid: id,
      uri: typeof uri == "string" ? CommonUtils.makeURI(uri) : uri,
      visits: [{ visitDate: visitPRTime, transitionType }]
    }], handler);
  });
}


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
  await PlacesTestUtils.clearHistory();
});

add_task(async function test_history_visit_roundtrip() {
  let engine = new HistoryEngine(Service);
  await engine.initialize();
  let server = await serverForFoo(engine);
  await SyncTestingInfrastructure(server);

  Svc.Obs.notify("weave:engine:start-tracking");

  let id = "aaaaaaaaaaaa";
  let oneHourMS = 60 * 60 * 1000;
  // Insert a visit with a non-round microsecond timestamp (e.g. it's not evenly
  // divisible by 1000). This will typically be the case for visits that occur
  // during normal navigation.
  let time = (Date.now() - oneHourMS) * 1000 + 555;
  // We use the low level updatePlaces api since it lets us provide microseconds
  let {count} = await rawAddVisit(id, "https://www.example.com", time,
                                  PlacesUtils.history.TRANSITIONS.TYPED);
  equal(count, 1);
  // Check that it was inserted and that we didn't round on the insert.
  let visits = await PlacesSyncUtils.history.fetchVisitsForURL("https://www.example.com");
  equal(visits.length, 1);
  equal(visits[0].date, time);

  let collection = server.user("foo").collection("history");

  // Sync the visit up to the server.
  await sync_engine_and_validate_telem(engine, false);

  let wbo = collection.wbo(id);
  let data = JSON.parse(JSON.parse(wbo.payload).ciphertext);
  // Double-check that we didn't round the visit's timestamp to the nearest
  // millisecond when uploading.
  equal(data.visits[0].date, time);

  // Add a remote visit so that we get past the deepEquals check in reconcile
  // (otherwise the history engine will skip applying this record). The contents
  // of this visit don't matter, beyond the fact that it needs to exist.
  data.visits.push({
    date: (Date.now() - oneHourMS / 2) * 1000,
    type: PlacesUtils.history.TRANSITIONS.LINK
  });
  collection.insertWBO(new ServerWBO(id, encryptPayload(data), Date.now() / 1000 + 10));

  // Force a remote sync.
  engine.lastSync = Date.now() / 1000 - 30;
  await sync_engine_and_validate_telem(engine, false);

  // Make sure that we didn't duplicate the visit when inserting. (Prior to bug
  // 1423395, we would insert a duplicate visit, where the timestamp was
  // effectively `Math.round(microsecondTimestamp / 1000) * 1000`.)
  visits = await PlacesSyncUtils.history.fetchVisitsForURL("https://www.example.com");
  equal(visits.length, 2);
  await PlacesTestUtils.clearHistory();
});
