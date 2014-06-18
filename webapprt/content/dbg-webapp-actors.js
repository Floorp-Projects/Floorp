/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

const { Promise } = Cu.import("resource://gre/modules/Promise.jsm", {});
const { devtools } = Cu.import("resource://gre/modules/devtools/Loader.jsm", {});
const { BrowserTabActor, BrowserTabList, allAppShellDOMWindows,
        sendShutdownEvent } = devtools.require("devtools/server/actors/webbrowser");
const { RootActor } = devtools.require("devtools/server/actors/root");

/**
 * WebappRT-specific actors.
 */

/**
 * Construct a root actor appropriate for use in a server running in the webapp
 * runtime. The returned root actor:
 * - respects the factories registered with DebuggerServer.addGlobalActor,
 * - uses a WebappTabList to supply tab actors,
 * - sends all webapprt:webapp window documents a Debugger:Shutdown event
 *   when it exits.
 *
 * * @param connection DebuggerServerConnection
 *        The conection to the client.
 */
function createRootActor(connection)
{
  let parameters = {
    tabList: new WebappTabList(connection),
    globalActorFactories: DebuggerServer.globalActorFactories,
    onShutdown: sendShutdownEvent
  };
  return new RootActor(connection, parameters);
}

/**
 * A live list of BrowserTabActors representing the current webapp windows,
 * to be provided to the root actor to answer 'listTabs' requests. In the
 * webapp runtime, only a single tab per window is ever present.
 *
 * @param connection DebuggerServerConnection
 *     The connection in which this list's tab actors may participate.
 *
 * @see BrowserTabList for more a extensive description of how tab list objects
 *      work.
 */
function WebappTabList(connection)
{
  BrowserTabList.call(this, connection);
}

WebappTabList.prototype = Object.create(BrowserTabList.prototype);

WebappTabList.prototype.constructor = WebappTabList;

WebappTabList.prototype.getList = function() {
  let topXULWindow = Services.wm.getMostRecentWindow(this._windowType);

  // As a sanity check, make sure all the actors presently in our map get
  // picked up when we iterate over all windows.
  let initialMapSize = this._actorByBrowser.size;
  let foundCount = 0;

  // To avoid mysterious behavior if windows are closed or opened mid-iteration,
  // we update the map first, and then make a second pass over it to yield
  // the actors. Thus, the sequence yielded is always a snapshot of the
  // actors that were live when we began the iteration.

  // Iterate over all webapprt:webapp XUL windows.
  for (let win of allAppShellDOMWindows(this._windowType)) {
    let browser = win.document.getElementById("content");
    if (!browser) {
      continue;
    }

    // Do we have an existing actor for this browser? If not, create one.
    let actor = this._actorByBrowser.get(browser);
    if (actor) {
      foundCount++;
    } else {
      actor = new WebappTabActor(this._connection, browser);
      this._actorByBrowser.set(browser, actor);
    }

    actor.selected = (win == topXULWindow);
  }

  if (this._testing && initialMapSize !== foundCount) {
    throw Error("_actorByBrowser map contained actors for dead tabs");
  }

  this._mustNotify = true;
  this._checkListening();

  return Promise.resolve([actor for ([_, actor] of this._actorByBrowser)]);
};

/**
 * Creates a tab actor for handling requests to the single tab, like
 * attaching and detaching. WebappTabActor respects the actor factories
 * registered with DebuggerServer.addTabActor.
 *
 * We override the title of the XUL window in content/webapp.js so here
 * we need to override the title property to avoid confusion to the user.
 * We won't return the title of the contained browser, but the title of
 * the webapp window.
 *
 * @param connection DebuggerServerConnection
 *        The conection to the client.
 * @param browser browser
 *        The browser instance that contains this tab.
 */
function WebappTabActor(connection, browser)
{
  BrowserTabActor.call(this, connection, browser);
}

WebappTabActor.prototype.constructor = WebappTabActor;

WebappTabActor.prototype = Object.create(BrowserTabActor.prototype);

Object.defineProperty(WebappTabActor.prototype, "title", {
  get: function() {
    return this.browser.ownerDocument.defaultView.document.title;
  },
  enumerable: true,
  configurable: false
});
