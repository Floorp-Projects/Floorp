/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that we get "Styles" markers with correct meta.
 */

const { PerformanceFront } = require("devtools/server/actors/performance");
const MARKER_NAME = "Styles";

add_task(function*() {
  let doc = yield addTab(MAIN_DOMAIN + "doc_perf.html");

  initDebuggerServer();
  let client = new DebuggerClient(DebuggerServer.connectPipe());
  let form = yield connectDebuggerClient(client);
  let front = PerformanceFront(client, form);
  yield front.connect();
  let rec = yield front.startRecording({ withMarkers: true });

  let markers = yield waitForMarkerType(front, MARKER_NAME, function (markers) {
    return markers.some(({restyleHint}) => restyleHint != void 0);
  });

  yield front.stopRecording(rec);

  ok(markers.some(m => m.name === MARKER_NAME), `got some ${MARKER_NAME} markers`);
  ok(markers.some(({restyleHint}) => restyleHint != void 0),
    "Some markers have a restyleHint.");

  yield closeDebuggerClient(client);
  gBrowser.removeCurrentTab();
});
