/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check that a player's currentTime can be changed.

const {AnimationsFront} = require("devtools/server/actors/animation");
const {InspectorFront} = require("devtools/server/actors/inspector");

add_task(function*() {
  let doc = yield addTab(MAIN_DOMAIN + "animation.html");

  initDebuggerServer();
  let client = new DebuggerClient(DebuggerServer.connectPipe());
  let form = yield connectDebuggerClient(client);
  let inspector = InspectorFront(client, form);
  let walker = yield inspector.getWalker();
  let animations = AnimationsFront(client, form);

  info("Retrieve an animated node");
  let node = yield walker.querySelector(walker.rootNode, ".simple-animation");

  info("Retrieve the animation player for the node");
  let [player] = yield animations.getAnimationPlayersForNode(node);

  ok(player.setCurrentTime, "Player has the setCurrentTime method");

  info("Check that the setCurrentTime method can be called");
  // Note that we don't check that it sets the animation to the right time here,
  // this is too prone to intermittent failures, we'll do this later after
  // pausing the animation. Here we merely test that the method doesn't fail.
  yield player.setCurrentTime(player.initialState.currentTime + 1000);

  info("Pause the animation so we can really test if setCurrentTime works");
  yield player.pause();
  let pausedState = yield player.getCurrentState();

  info("Set the current time to currentTime + 5s");
  yield player.setCurrentTime(pausedState.currentTime + 5000);

  let updatedState1 = yield player.getCurrentState();
  is(Math.round(updatedState1.currentTime - pausedState.currentTime), 5000,
    "The currentTime was updated to +5s");

  info("Set the current time to currentTime - 2s");
  yield player.setCurrentTime(updatedState1.currentTime - 2000);
  let updatedState2 = yield player.getCurrentState();
  is(Math.round(updatedState2.currentTime - updatedState1.currentTime), -2000,
    "The currentTime was updated to -2s");

  yield closeDebuggerClient(client);
  gBrowser.removeCurrentTab();
});
