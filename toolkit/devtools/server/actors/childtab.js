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
 * @param prefix
 *        the prefix used in protocol to create IDs for each actor.
 *        Used as ID identifying this particular TabActor from the parent process.
 */
function ContentActor(connection, chromeGlobal, prefix)
{
  this._chromeGlobal = chromeGlobal;
  this._prefix = prefix;
  TabActor.call(this, connection, chromeGlobal);
  this.traits.reconfigure = false;
  this._sendForm = this._sendForm.bind(this);
  this._chromeGlobal.addMessageListener("debug:form", this._sendForm);

  Object.defineProperty(this, "docShell", {
    value: this._chromeGlobal.docShell,
    configurable: true
  });
}

ContentActor.prototype = Object.create(TabActor.prototype);

ContentActor.prototype.constructor = ContentActor;

Object.defineProperty(ContentActor.prototype, "title", {
  get: function() {
    return this.window.document.title;
  },
  enumerable: true,
  configurable: true
});

ContentActor.prototype.exit = function() {
  if (this._sendForm) {
    this._chromeGlobal.removeMessageListener("debug:form", this._sendForm);
    this._sendForm = null;
  }
  return TabActor.prototype.exit.call(this);
};

/**
 * On navigation events, our URL and/or title may change, so we update our
 * counterpart in the parent process that participates in the tab list.
 */
ContentActor.prototype._sendForm = function() {
  this._chromeGlobal.sendAsyncMessage("debug:form", this.form());
};
