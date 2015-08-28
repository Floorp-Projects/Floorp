/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check that AnimationPlayers can auto-refresh their states.

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

  let node = yield walker.querySelector(walker.rootNode, ".simple-animation");
  let [player] = yield front.getAnimationPlayersForNode(node);

  ok(player.startAutoRefresh, "The startAutoRefresh function is available");
  ok(player.stopAutoRefresh, "The stopAutoRefresh function is available");
  ok(player.state, "The current state is stored on the player");

  info("Subscribe to the refresh event, start the auto-refresh and wait for " +
    "a few events to be received");

  player.startAutoRefresh();

  let onAllEventsReceived = new Promise(resolve => {
    let expected = 5;
    let previousState = player.initialState;
    let onNewState = state => {
      ok(state.currentTime !== previousState.currentTime,
        "The time has changed since the last update");
      expected --;
      previousState = state;
      if (expected === 0) {
        player.off(player.AUTO_REFRESH_EVENT, onNewState);

        info("Stop the auto-refresh");
        player.stopAutoRefresh();

        if (player.pendingRefreshStatePromise) {
          // A new request was fired before we had chance to stop it. Wait for
          // it to complete.
          player.pendingRefreshStatePromise.then(resolve);
        } else {
          resolve();
        }
      }
    };
    player.on(player.AUTO_REFRESH_EVENT, onNewState);
  });

  yield onAllEventsReceived;

  yield closeDebuggerClient(client);
  gBrowser.removeCurrentTab();
});
