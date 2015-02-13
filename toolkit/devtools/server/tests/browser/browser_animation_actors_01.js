/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Simple checks for the AnimationsActor

const {AnimationsFront} = require("devtools/server/actors/animation");
const {InspectorFront} = require("devtools/server/actors/inspector");

add_task(function*() {
  let doc = yield addTab("data:text/html;charset=utf-8,<title>test</title><div></div>");

  initDebuggerServer();
  let client = new DebuggerClient(DebuggerServer.connectPipe());
  let form = yield connectDebuggerClient(client);
  let inspector = InspectorFront(client, form);
  let walker = yield inspector.getWalker();
  let front = AnimationsFront(client, form);

  ok(front, "The AnimationsFront was created");
  ok(front.getAnimationPlayersForNode, "The getAnimationPlayersForNode method exists");
  ok(front.toggleAll, "The toggleAll method exists");
  ok(front.playAll, "The playAll method exists");
  ok(front.pauseAll, "The pauseAll method exists");

  let didThrow = false;
  try {
    yield front.getAnimationPlayersForNode(null);
  } catch (e) {
    didThrow = true;
  }
  ok(didThrow, "An exception was thrown for a missing NodeActor");

  let invalidNode = yield walker.querySelector(walker.rootNode, "title");
  let players = yield front.getAnimationPlayersForNode(invalidNode);
  ok(Array.isArray(players), "An array of players was returned");
  is(players.length, 0, "0 players have been returned for the invalid node");

  yield closeDebuggerClient(client);
  gBrowser.removeCurrentTab();
});
