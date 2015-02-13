/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check that the right duration/iterationCount/delay are retrieved even when
// the node has multiple animations and one of them already ended before getting
// the player objects.
// See toolkit/devtools/server/actors/animation.js |getPlayerIndex| for more
// information.

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

  info("Retrieve a non animated node");
  let node = yield walker.querySelector(walker.rootNode, ".not-animated");

  info("Apply the multiple-animations-2 class to start the animations");
  yield node.modifyAttributes([
    {attributeName: "class", newValue: "multiple-animations-2"}
  ]);

  info("Retrieve the list of animation players for the node");
  let players = yield front.getAnimationPlayersForNode(node);
  is(players.length, 3, "3 animations are currently applied to the node");

  info("Waiting for the first animation to end");
  let player = players[0];
  player.startAutoRefresh();

  let onFinished = new Promise(resolve => {
    let onNewState = (e, state) => {
      if (state.playState === "finished") {
        info("Received the 'finished' playState event");
        player.off(player.AUTO_REFRESH_EVENT, onNewState);
        resolve();
      }
    };
    info("Listening for auto-refresh events");
    player.on(player.AUTO_REFRESH_EVENT, onNewState);
  });
  yield onFinished;

  info("Get the list of players again");
  players = yield front.getAnimationPlayersForNode(node);
  is(players.length, 2, "2 animations remain on the node");

  is(players[0].state.duration, 1000, "The duration of the first animation is correct");
  is(players[0].state.delay, 2000, "The delay of the first animation is correct");
  is(players[0].state.iterationCount, null, "The iterationCount of the first animation is correct");

  is(players[1].state.duration, 3000, "The duration of the second animation is correct");
  is(players[1].state.delay, 1000, "The delay of the second animation is correct");
  is(players[1].state.iterationCount, 100, "The iterationCount of the second animation is correct");

  yield closeDebuggerClient(client);
  gBrowser.removeCurrentTab();
});
