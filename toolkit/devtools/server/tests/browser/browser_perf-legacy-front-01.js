/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test basic functionality of PerformanceFront with mock memory and timeline actors.
 */

let WAIT_TIME = 100;

const { TargetFactory } = require("devtools/framework/target");
const { LegacyPerformanceFront } = require("devtools/toolkit/performance/legacy/front");
const { merge } = require("sdk/util/object");

add_task(function*() {
  let tab = yield getTab(MAIN_DOMAIN + "doc_perf.html");
  let target = TargetFactory.forTab(tab);
  yield target.makeRemote();

  merge(target, {
    TEST_MOCK_TIMELINE_ACTOR: true,
    TEST_PERFORMANCE_LEGACY_FRONT: true,
  });

  let front = new LegacyPerformanceFront(target);
  yield front.connect();

  ok(front.LEGACY_FRONT, true, "Using legacy front");
  front.on("timeline-data", () => ok(false, "There should not be any timeline-data events when mocked"));

  let recording = yield front.startRecording({
    withTicks: true,
    withMarkers: true,
    withMemory: true,
    withAllocations: true,
  });

  is(recording.getConfiguration().withMarkers, false, "overrides withMarkers based off of actor support");
  is(recording.getConfiguration().withTicks, false, "overrides withTicks based off of actor support");
  is(recording.getConfiguration().withMemory, false, "overrides withMemory based off of actor support");
  is(recording.getConfiguration().withAllocations, false, "overrides withAllocations based off of actor support");

  yield busyWait(WAIT_TIME);

  yield front.stopRecording(recording);

  ok(typeof recording.getDuration() === "number",
    "The front.stopRecording() allows recording to get a duration.");
  ok(recording.getDuration() >= 0, "duration is a positive number");
  isEmptyArray(recording.getMarkers(), "markers");
  isEmptyArray(recording.getTicks(), "ticks");
  isEmptyArray(recording.getMemory(), "memory");
  isEmptyArray(recording.getAllocations().sites, "allocations.sites");
  isEmptyArray(recording.getAllocations().timestamps, "allocations.timestamps");
  isEmptyArray(recording.getAllocations().frames, "allocations.frames");
  ok(recording.getProfile().threads[0].samples.data.length, "profile data has some samples");
  checkSystemInfo(recording, "Host");
  checkSystemInfo(recording, "Client");

  yield front.destroy();
  yield closeDebuggerClient(target.client);
  gBrowser.removeCurrentTab();
});

function isEmptyArray (array, name) {
  ok(Array.isArray(array), `${name} is an array`);
  ok(array.length === 0, `${name} is empty`);
}

function getTab (url) {
  let tab = gBrowser.selectedTab = gBrowser.addTab();
  let loaded = once(gBrowser.selectedBrowser, "load", true);

  content.location = url;
  return loaded.then(() => {
    return new Promise(resolve => {
      let isBlank = url == "about:blank";
      waitForFocus(() => resolve(tab), content, isBlank);
    });
  });
}

function checkSystemInfo (recording, type) {
  let data = recording[`get${type}SystemInfo`]();
  for (let field of ["appid", "apptype", "vendor", "name", "version"]) {
    ok(data[field], `get${type}SystemInfo() has ${field} property`);
  }
}
