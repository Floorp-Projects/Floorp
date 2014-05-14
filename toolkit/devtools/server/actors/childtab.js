/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

let { TabActor } = require("devtools/server/actors/webbrowser");

/**
 * Tab actor for documents living in a child process.
 *
 * Depends on TabActor, defined in webbrowser.js.
 */

/**
 * Creates a tab actor for handling requests to the single tab, like
 * attaching and detaching. ContentActor respects the actor factories
 * registered with DebuggerServer.addTabActor.
 *
 * @param connection DebuggerServerConnection
 *        The conection to the client.
 * @param chromeGlobal
 *        The content script global holding |content| and |docShell| properties for a tab.
 */
function ContentActor(connection, chromeGlobal)
{
  this._chromeGlobal = chromeGlobal;
  TabActor.call(this, connection, chromeGlobal);
  this.traits.reconfigure = false;
}

ContentActor.prototype = Object.create(TabActor.prototype);

ContentActor.prototype.constructor = ContentActor;

Object.defineProperty(ContentActor.prototype, "docShell", {
  get: function() {
    return this._chromeGlobal.docShell;
  },
  enumerable: true,
  configurable: false
});

ContentActor.prototype.exit = function() {
  TabActor.prototype.exit.call(this);
};

// Override grip just to rename this._tabActorPool to this._tabActorPool2
// in order to prevent it to be cleaned on detach.
// We have to keep tab actors alive as we keep the ContentActor
// alive after detach and reuse it for multiple debug sessions.
ContentActor.prototype.grip = function () {
  let response = {
    'actor': this.actorID,
    'title': this.title,
    'url': this.url
  };

  // Walk over tab actors added by extensions and add them to a new ActorPool.
  let actorPool = new ActorPool(this.conn);
  this._createExtraActors(DebuggerServer.tabActorFactories, actorPool);
  if (!actorPool.isEmpty()) {
    this._tabActorPool2 = actorPool;
    this.conn.addActorPool(this._tabActorPool2);
  }

  this._appendExtraActors(response);
  return response;
};

