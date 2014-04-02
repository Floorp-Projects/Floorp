/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

let chromeGlobal = this;

// Encapsulate in its own scope to allows loading this frame script
// more than once.
(function () {
  let Cu = Components.utils;
  let { devtools } = Cu.import("resource://gre/modules/devtools/Loader.jsm", {});
  const DevToolsUtils = devtools.require("devtools/toolkit/DevToolsUtils.js");
  const {DebuggerServer, ActorPool} = Cu.import("resource://gre/modules/devtools/dbg-server.jsm", {});

  if (!DebuggerServer.initialized) {
    DebuggerServer.init();
  }

  // In case of apps being loaded in parent process, DebuggerServer is already
  // initialized, but child specific actors are not registered.
  // Otherwise, for apps in child process, we need to load actors the first
  // time we load child.js
  DebuggerServer.addChildActors();

  let conn;

  let onConnect = DevToolsUtils.makeInfallible(function (msg) {
    removeMessageListener("debug:connect", onConnect);

    let mm = msg.target;

    conn = DebuggerServer.connectToParent(msg.data.prefix, mm);

    let actor = new DebuggerServer.ContentActor(conn, chromeGlobal);
    let actorPool = new ActorPool(conn);
    actorPool.addActor(actor);
    conn.addActorPool(actorPool);

    sendAsyncMessage("debug:actor", {actor: actor.grip()});
  });

  addMessageListener("debug:connect", onConnect);

  let onDisconnect = DevToolsUtils.makeInfallible(function (msg) {
    removeMessageListener("debug:disconnect", onDisconnect);

    // Call DebuggerServerConnection.close to destroy all child actors
    // (It should end up calling DebuggerServerConnection.onClosed
    // that would actually cleanup all actor pools)
    conn.close();
    conn = null;
  });
  addMessageListener("debug:disconnect", onDisconnect);
})();
