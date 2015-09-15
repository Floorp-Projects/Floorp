/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that buffer status is correctly updated in recording models.
 */

var BUFFER_SIZE = 20000;
var config = { bufferSize: BUFFER_SIZE };

const { PerformanceFront } = require("devtools/server/actors/performance");

add_task(function*() {
  let doc = yield addTab(MAIN_DOMAIN + "doc_perf.html");

  initDebuggerServer();
  let client = new DebuggerClient(DebuggerServer.connectPipe());
  let form = yield connectDebuggerClient(client);
  let front = PerformanceFront(client, form);
  yield front.connect();

  yield front.setProfilerStatusInterval(10);
  let model = yield front.startRecording(config);
  let stats = yield once(front, "profiler-status");
  is(stats.totalSize, BUFFER_SIZE, `profiler-status event has totalSize: ${stats.totalSize}`);
  ok(stats.position < BUFFER_SIZE, `profiler-status event has position: ${stats.position}`);
  ok(stats.generation >= 0, `profiler-status event has generation: ${stats.generation}`);
  ok(stats.isActive, `profiler-status event is isActive`);
  is(typeof stats.currentTime, "number", `profiler-status event has currentTime`);

  // Halt once more for a buffer status to ensure we're beyond 0
  yield once(front, "profiler-status");

  let lastBufferStatus = 0;
  let checkCount = 0;
  while (lastBufferStatus < 1) {
    let currentBufferStatus = front.getBufferUsageForRecording(model);
    ok(currentBufferStatus > lastBufferStatus, `buffer is more filled than before: ${currentBufferStatus} > ${lastBufferStatus}`);
    lastBufferStatus = currentBufferStatus;
    checkCount++;
    yield once(front, "profiler-status");
  }

  ok(checkCount >= 1, "atleast 1 event were fired until the buffer was filled");
  is(lastBufferStatus, 1, "buffer usage cannot surpass 100%");
  yield front.stopRecording(model);

  is(front.getBufferUsageForRecording(model), null, "buffer usage should be null when no longer recording.");

  yield front.destroy();
  yield closeDebuggerClient(client);
  gBrowser.removeCurrentTab();
});
