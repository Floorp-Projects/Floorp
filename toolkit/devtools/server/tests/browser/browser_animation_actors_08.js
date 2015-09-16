/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check that the AnimationsActor can pause/play all animations at once, and
// check that it can also pause/play a given list of animations at once.

const {AnimationsFront} = require("devtools/server/actors/animation");
const {InspectorFront} = require("devtools/server/actors/inspector");

// List of selectors that match "all" animated nodes in the test page.
// This list misses a bunch of animated nodes on purpose. Only the ones that
// have infinite animations are listed. This is done to avoid intermittents
// caused when finite animations are already done playing by the time the test
// runs.
const ALL_ANIMATED_NODES = [".simple-animation", ".multiple-animations",
                            ".delayed-animation"];
// List of selectors that match some animated nodes in the test page only.
const SOME_ANIMATED_NODES = [".simple-animation", ".delayed-animation"];

add_task(function*() {
  yield addTab(MAIN_DOMAIN + "animation.html");

  initDebuggerServer();
  let client = new DebuggerClient(DebuggerServer.connectPipe());
  let form = yield connectDebuggerClient(client);
  let inspector = InspectorFront(client, form);
  let walker = yield inspector.getWalker();
  let front = AnimationsFront(client, form);

  info("Pause all animations in the test document");
  yield front.pauseAll();
  yield checkAnimationsStates(walker, front, ALL_ANIMATED_NODES, "paused");

  info("Play all animations in the test document");
  yield front.playAll();
  yield checkAnimationsStates(walker, front, ALL_ANIMATED_NODES, "running");

  info("Pause all animations in the test document using toggleAll");
  yield front.toggleAll();
  yield checkAnimationsStates(walker, front, ALL_ANIMATED_NODES, "paused");

  info("Play all animations in the test document using toggleAll");
  yield front.toggleAll();
  yield checkAnimationsStates(walker, front, ALL_ANIMATED_NODES, "running");

  info("Pause a given list of animations only");
  let players = [];
  for (let selector of SOME_ANIMATED_NODES) {
    let [player] = yield getPlayersFor(walker, front, selector);
    players.push(player);
  }
  yield front.toggleSeveral(players, true);
  yield checkAnimationsStates(walker, front, SOME_ANIMATED_NODES, "paused");
  yield checkAnimationsStates(walker, front, [".multiple-animations"], "running");

  info("Play the same list of animations");
  yield front.toggleSeveral(players, false);
  yield checkAnimationsStates(walker, front, ALL_ANIMATED_NODES, "running");

  yield closeDebuggerClient(client);
  gBrowser.removeCurrentTab();
});

function* checkAnimationsStates(walker, front, selectors, playState) {
  info("Checking the playState of all the nodes that have infinite running " +
       "animations");

  for (let selector of selectors) {
    info("Getting the AnimationPlayerFront for node " + selector);
    let [player] = yield getPlayersFor(walker, front, selector);
    yield player.ready();
    yield checkPlayState(player, selector, playState);
  }
}

function* getPlayersFor(walker, front, selector) {
  let node = yield walker.querySelector(walker.rootNode, selector);
  return front.getAnimationPlayersForNode(node);
}

function* checkPlayState(player, selector, expectedState) {
  let state = yield player.getCurrentState();
  is(state.playState, expectedState,
    "The playState of node " + selector + " is " + expectedState);
}
