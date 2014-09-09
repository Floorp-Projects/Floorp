/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

let test = asyncTest(function*() {
  const {TimelineFront} = require("devtools/server/actors/timeline");
  const Cu = Components.utils;
  let tempScope = {};
  Cu.import("resource://gre/modules/devtools/dbg-client.jsm", tempScope);
  Cu.import("resource://gre/modules/devtools/dbg-server.jsm", tempScope);
  let {DebuggerServer, DebuggerClient} = tempScope;

  let doc = yield addTab("data:text/html;charset=utf-8,mop");

  DebuggerServer.init(function () { return true; });
  DebuggerServer.addBrowserActors();
  let client = new DebuggerClient(DebuggerServer.connectPipe());
  let onListTabs = promise.defer();
  client.connect(() => {
    client.listTabs(onListTabs.resolve);
  });

  let listTabs = yield onListTabs.promise;

  let form = listTabs.tabs[listTabs.selected];
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

  let onClose = promise.defer();
  client.close(onClose.resolve);
  yield onClose;

  gBrowser.removeCurrentTab();
});
