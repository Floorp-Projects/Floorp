/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// When a transition finishes, no "removed" event is sent because it may still
// be used, but when it restarts again (transitions back), then a new
// AnimationPlayerFront should be sent, and the old one should be removed.

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

  info("Retrieve the test node");
  let node = yield walker.querySelector(walker.rootNode, ".all-transitions");

  info("Retrieve the animation players for the node");
  let players = yield animations.getAnimationPlayersForNode(node);
  is(players.length, 0, "The node has no animation players yet");

  info("Listen for new animations");
  let reportedMutations = [];
  function onMutations(mutations) {
    reportedMutations = [...reportedMutations, ...mutations];
  }
  animations.on("mutations", onMutations);

  info("Transition the node by adding the expand class");
  let cpow = content.document.querySelector(".all-transitions");
  cpow.classList.add("expand");
  info("Wait for longer than the transition");
  yield wait(500);

  is(reportedMutations.length, 2, "2 mutation events were received");
  is(reportedMutations[0].type, "added", "The first event was 'added'");
  is(reportedMutations[1].type, "added", "The second event was 'added'");

  reportedMutations = [];

  info("Transition back by removing the expand class");
  cpow.classList.remove("expand");
  info("Wait for longer than the transition");
  yield wait(500);

  is(reportedMutations.length, 4, "4 new mutation events were received");
  is(reportedMutations.filter(m => m.type === "removed").length, 2,
    "2 'removed' events were sent (for the old transitions)");
  is(reportedMutations.filter(m => m.type === "added").length, 2,
    "2 'added' events were sent (for the new transitions)");

  animations.off("mutations", onMutations);

  yield closeDebuggerClient(client);
  gBrowser.removeCurrentTab();
});

function wait(ms) {
  return new Promise(resolve => {
    setTimeout(resolve, ms);
  });
}
