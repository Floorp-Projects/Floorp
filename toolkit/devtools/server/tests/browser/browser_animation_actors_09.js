/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check that the AnimationsActor can pause/play all animations even those
// within iframes.

const {AnimationsFront} = require("devtools/server/actors/animation");
const {InspectorFront} = require("devtools/server/actors/inspector");

const URL = MAIN_DOMAIN + "animation.html";

add_task(function*() {
  info("Creating a test document with 2 iframes containing animated nodes");
  let doc = yield addTab("data:text/html;charset=utf-8," +
                         "<iframe id='i1' src='" + URL + "'></iframe>" +
                         "<iframe id='i2' src='" + URL + "'></iframe>");

  initDebuggerServer();
  let client = new DebuggerClient(DebuggerServer.connectPipe());
  let form = yield connectDebuggerClient(client);
  let inspector = InspectorFront(client, form);
  let walker = yield inspector.getWalker();
  let front = AnimationsFront(client, form);

  info("Getting the 2 iframe container nodes and animated nodes in them");
  let nodeInFrame1 = yield getNodeInFrame(walker, "#i1", ".simple-animation");
  let nodeInFrame2 = yield getNodeInFrame(walker, "#i2", ".simple-animation");

  info("Pause all animations in the test document");
  yield front.pauseAll();
  yield checkState(front, nodeInFrame1, "paused");
  yield checkState(front, nodeInFrame2, "paused");

  info("Play all animations in the test document");
  yield front.playAll();
  yield checkState(front, nodeInFrame1, "running");
  yield checkState(front, nodeInFrame2, "running");

  yield closeDebuggerClient(client);
  gBrowser.removeCurrentTab();
});

function* checkState(front, nodeFront, playState) {
  info("Getting the AnimationPlayerFront for the test node");
  let [player] = yield front.getAnimationPlayersForNode(nodeFront);
  yield player.ready;
  let state = yield player.getCurrentState();
  is(state.playState, playState, "The playState of the test node is " + playState);
}

function* getNodeInFrame(walker, frameSelector, nodeSelector) {
  let iframe = yield walker.querySelector(walker.rootNode, frameSelector);
  let {nodes} = yield walker.children(iframe);
  return walker.querySelector(nodes[0], nodeSelector);
}
