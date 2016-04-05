/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function run_test() {
  do_check_false(Services.search.isInitialized);

  useHttpServer();
  run_next_test();
}

function checkTelemetry(histogramName, expected) {
  let histogram = Services.telemetry.getHistogramById(histogramName);
  let snapshot = histogram.snapshot();
  let expectedCounts = [0, 0, 0];
  expectedCounts[expected ? 1 : 0] = 1;
  Assert.deepEqual(snapshot.counts, expectedCounts,
                   "histogram has expected content");
  histogram.clear();
}

add_task(function* ignore_cache_files_without_engines() {
  yield asyncInit();

  checkTelemetry("SEARCH_SERVICE_HAS_UPDATES", false);
  checkTelemetry("SEARCH_SERVICE_HAS_ICON_UPDATES", false);

  // Add an engine with update urls and re-init, as we record the presence of
  // engine update urls only while initializing the search service.
  yield addTestEngines([
    { name: "update", xmlFileName: "engine-update.xml" },
  ]);
  yield asyncReInit();

  checkTelemetry("SEARCH_SERVICE_HAS_UPDATES", true);
  checkTelemetry("SEARCH_SERVICE_HAS_ICON_UPDATES", true);
});
