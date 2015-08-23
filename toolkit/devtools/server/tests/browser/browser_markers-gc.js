/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that we get "GarbageCollection" markers.
 */

const { PerformanceFront } = require("devtools/server/actors/performance");
const MARKER_NAME = "GarbageCollection";

add_task(function*() {
  let doc = yield addTab(MAIN_DOMAIN + "doc_force_gc.html");

  initDebuggerServer();
  let client = new DebuggerClient(DebuggerServer.connectPipe());
  let form = yield connectDebuggerClient(client);
  let front = PerformanceFront(client, form);
  yield front.connect();
  let rec = yield front.startRecording({ withMarkers: true });

  let markers = yield waitForMarkerType(front, MARKER_NAME);
  yield front.stopRecording(rec);

  ok(markers.some(m => m.name === MARKER_NAME), `got some ${MARKER_NAME} markers`);
  ok(markers.every(({causeName}) => typeof causeName === "string"),
    "All markers have a causeName.");

  yield closeDebuggerClient(client);
  gBrowser.removeCurrentTab();
});
