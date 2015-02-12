/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check that the AnimationsActor can pause/play all animations at once.

const {AnimationsFront} = require("devtools/server/actors/animation");
const {InspectorFront} = require("devtools/server/actors/inspector");

add_task(function*() {
  let doc = yield addTab(MAIN_DOMAIN + "animation.html");

  initDebuggerServer();
  let client = new DebuggerClient(DebuggerServer.connectPipe());
  let form = yield connectDebuggerClient(client);
  let inspector = InspectorFront(client, form);
  let walker = yield inspector.getWalker();
  let front = AnimationsFront(client, form);

  info("Pause all animations in the test document");
  yield front.pauseAll();
  yield checkAllAnimationsStates(walker, front, "paused");

  info("Play all animations in the test document");
  yield front.playAll();
  yield checkAllAnimationsStates(walker, front, "running");

  info("Pause all animations in the test document using toggleAll");
  yield front.toggleAll();
  yield checkAllAnimationsStates(walker, front, "paused");

  info("Play all animations in the test document using toggleAll");
  yield front.toggleAll();
  yield checkAllAnimationsStates(walker, front, "running");

  yield closeDebuggerClient(client);
  gBrowser.removeCurrentTab();
});

function* checkAllAnimationsStates(walker, front, playState) {
  info("Checking the playState of all the nodes that have infinite running animations");

  let selectors = [".simple-animation", ".multiple-animations", ".delayed-animation"];
  for (let selector of selectors) {
    info("Getting the AnimationPlayerFront for node " + selector);
    let node = yield walker.querySelector(walker.rootNode, selector);
    let [player] = yield front.getAnimationPlayersForNode(node);
    yield player.ready;
    let state = yield player.getCurrentState();
    is(state.playState, playState,
      "The playState of node " + selector + " is " + playState);
  }
}
