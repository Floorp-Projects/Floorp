/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the timeline front's start/stop/isRecording methods work in a
// simple use case, and that markers events are sent when operations occur.

const {TimelineFront} = require("devtools/server/actors/timeline");

let test = asyncTest(function*() {
  let doc = yield addTab("data:text/html;charset=utf-8,mop");

  initDebuggerServer();
  let client = new DebuggerClient(DebuggerServer.connectPipe());

  let form = yield connectDebuggerClient(client);
  let front = TimelineFront(client, form);

  let isActive = yield front.isRecording();
  ok(!isActive, "Not initially recording");

  doc.body.innerHeight; // flush any pending reflow

  yield front.start();

  isActive = yield front.isRecording();
  ok(isActive, "Recording after start()");

  doc.body.style.padding = "10px";

  let markers = yield once(front, "markers");

  ok(markers.length > 0, "markers were returned");
  ok(markers.some(m => m.name == "Reflow"), "markers includes Reflow");
  ok(markers.some(m => m.name == "Paint"), "markers includes Paint");
  ok(markers.some(m => m.name == "Styles"), "markers includes Restyle");

  doc.body.style.backgroundColor = "red";

  markers = yield once(front, "markers");

  ok(markers.length > 0, "markers were returned");
  ok(!markers.some(m => m.name == "Reflow"), "markers doesn't include Reflow");
  ok(markers.some(m => m.name == "Paint"), "markers includes Paint");
  ok(markers.some(m => m.name == "Styles"), "markers includes Restyle");

  yield front.stop();

  isActive = yield front.isRecording();
  ok(!isActive, "Not recording after stop()");

  yield closeDebuggerClient(client);
  gBrowser.removeCurrentTab();
});
