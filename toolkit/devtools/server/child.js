/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Encapsulate in its own scope to allows loading this frame script
// more than once.
(function () {
  const {DevToolsUtils} = Cu.import("resource://gre/modules/devtools/DevToolsUtils.jsm", {});
  const {DebuggerServer, ActorPool} = Cu.import("resource://gre/modules/devtools/dbg-server.jsm", {});

  if (!DebuggerServer.initialized) {
    DebuggerServer.init();
  }

  // In case of apps being loaded in parent process, DebuggerServer is already
  // initialized, but child specific actors are not registered.
  // Otherwise, for apps in child process, we need to load actors the first
  // time we load child.js
  DebuggerServer.addChildActors();

  let onConnect = DevToolsUtils.makeInfallible(function (msg) {
    removeMessageListener("debug:connect", onConnect);

    let mm = msg.target;

    let prefix = msg.data.prefix + docShell.appId;

    let conn = DebuggerServer.connectToParent(prefix, mm);

    let actor = new DebuggerServer.ContentAppActor(conn, content);
    let actorPool = new ActorPool(conn);
    actorPool.addActor(actor);
    conn.addActorPool(actorPool);

    sendAsyncMessage("debug:actor", {actor: actor.grip(),
                                     appId: docShell.appId,
                                     prefix: prefix});
  });

  addMessageListener("debug:connect", onConnect);
})();
