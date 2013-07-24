/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Tab actor for documents living in a child process.
 *
 * Depends on BrowserTabActor, defined in webbrowser.js actor.
 */

/**
 * Creates a tab actor for handling requests to the single tab, like
 * attaching and detaching. ContentTabActor respects the actor factories
 * registered with DebuggerServer.addTabActor.
 *
 * @param connection DebuggerServerConnection
 *        The conection to the client.
 * @param browser browser
 *        The browser instance that contains this tab.
 */
function ContentTabActor(connection, browser)
{
  BrowserTabActor.call(this, connection, browser);
}

ContentTabActor.prototype = Object.create(BrowserTabActor.prototype);

ContentTabActor.prototype.constructor = ContentTabActor;

Object.defineProperty(ContentTabActor.prototype, "title", {
  get: function() {
    return this.browser.title;
  },
  enumerable: true,
  configurable: false
});

Object.defineProperty(ContentTabActor.prototype, "url", {
  get: function() {
    return this.browser.document.documentURI;
  },
  enumerable: true,
  configurable: false
});

Object.defineProperty(ContentTabActor.prototype, "contentWindow", {
  get: function() {
    return this.browser;
  },
  enumerable: true,
  configurable: false
});

// Override grip just to rename this._tabActorPool to this._tabActorPool2
// in order to prevent it to be cleaned on detach.
// We have to keep tab actors alive as we keep the ContentTabActor
// alive after detach and reuse it for multiple debug sessions.
ContentTabActor.prototype.grip = function () {
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

