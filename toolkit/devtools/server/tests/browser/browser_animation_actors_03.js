/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check the animation player's initial state

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

  yield playerHasAnInitialState(walker, front);
  yield playerStateIsCorrect(walker, front);

  yield closeDebuggerClient(client);
  gBrowser.removeCurrentTab();
});

function* playerHasAnInitialState(walker, front) {
  let node = yield walker.querySelector(walker.rootNode, ".simple-animation");
  let [player] = yield front.getAnimationPlayersForNode(node);

  ok(player.initialState, "The player front has an initial state");
  ok("startTime" in player.initialState, "Player's state has startTime");
  ok("currentTime" in player.initialState, "Player's state has currentTime");
  ok("playState" in player.initialState, "Player's state has playState");
  ok("playbackRate" in player.initialState, "Player's state has playbackRate");
  ok("name" in player.initialState, "Player's state has name");
  ok("duration" in player.initialState, "Player's state has duration");
  ok("delay" in player.initialState, "Player's state has delay");
  ok("iterationCount" in player.initialState, "Player's state has iterationCount");
  ok("isRunningOnCompositor" in player.initialState, "Player's state has isRunningOnCompositor");
}

function* playerStateIsCorrect(walker, front) {
  info("Checking the state of the simple animation");

  let state = yield getAnimationStateForNode(walker, front, ".simple-animation", 0);
  is(state.name, "move", "Name is correct");
  is(state.duration, 2000, "Duration is correct");
  // null = infinite count
  is(state.iterationCount, null, "Iteration count is correct");
  is(state.playState, "running", "PlayState is correct");
  is(state.playbackRate, 1, "PlaybackRate is correct");

  info("Checking the state of the transition");

  state = yield getAnimationStateForNode(walker, front, ".transition", 0);
  is(state.name, "width", "Transition name matches transition property");
  is(state.duration, 5000, "Transition duration is correct");
  // transitions run only once
  is(state.iterationCount, 1, "Transition iteration count is correct");
  is(state.playState, "running", "Transition playState is correct");
  is(state.playbackRate, 1, "Transition playbackRate is correct");

  info("Checking the state of one of multiple animations on a node");

  // Checking the 2nd player
  state = yield getAnimationStateForNode(walker, front, ".multiple-animations", 1);
  is(state.name, "glow", "The 2nd animation's name is correct");
  is(state.duration, 1000, "The 2nd animation's duration is correct");
  is(state.iterationCount, 5, "The 2nd animation's iteration count is correct");
  is(state.playState, "running", "The 2nd animation's playState is correct");
  is(state.playbackRate, 1, "The 2nd animation's playbackRate is correct");

  info("Checking the state of an animation with delay");

  state = yield getAnimationStateForNode(walker, front, ".delayed-animation", 0);
  is(state.delay, 5000, "The animation delay is correct");

  info("Checking the state of an transition with delay");

  state = yield getAnimationStateForNode(walker, front, ".delayed-transition", 0);
  is(state.delay, 3000, "The transition delay is correct");
}

function* getAnimationStateForNode(walker, front, nodeSelector, playerIndex) {
  let node = yield walker.querySelector(walker.rootNode, nodeSelector);
  let players = yield front.getAnimationPlayersForNode(node);
  let player = players[playerIndex];
  yield player.ready();
  let state = yield player.getCurrentState();
  return state;
}
