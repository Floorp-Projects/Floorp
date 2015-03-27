/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Ci } = require("chrome");
const Services = require("Services");
const { DebuggerServer } = require("../main");
const { getChildDocShells, TabActor } = require("./webbrowser");
const makeDebugger = require("./utils/make-debugger");

/**
 * Creates a TabActor for debugging all the chrome content in the
 * current process. Most of the implementation is inherited from TabActor.
 * ChromeActor is a child of RootActor, it can be instanciated via
 * RootActor.getProcess request.
 * ChromeActor exposes all tab actors via its form() request, like TabActor.
 *
 * History lecture:
 * All tab actors used to also be registered as global actors,
 * so that the root actor was also exposing tab actors for the main process.
 * Tab actors ended up having RootActor as parent actor,
 * but more and more features of the tab actors were relying on TabActor.
 * So we are now exposing a process actor that offers the same API as TabActor
 * by inheriting its functionality.
 * Global actors are now only the actors that are meant to be global,
 * and are no longer related to any specific scope/document.
 *
 * @param aConnection DebuggerServerConnection
 *        The connection to the client.
 */
function ChromeActor(aConnection) {
  TabActor.call(this, aConnection);

  // This creates a Debugger instance for chrome debugging all globals.
  this.makeDebugger = makeDebugger.bind(null, {
    findDebuggees: dbg => dbg.findAllGlobals(),
    shouldAddNewGlobalAsDebuggee: () => true
  });

  // Ensure catching the creation of any new content docshell
  this.listenForNewDocShells = true;

  // Defines the default docshell selected for the tab actor
  let window = Services.wm.getMostRecentWindow(DebuggerServer.chromeWindowType);

  // Default to any available top level window if there is no expected window
  // (for example when we open firefox with -webide argument)
  if (!window) {
    window = Services.wm.getMostRecentWindow(null);
  }
  // On xpcshell, there is no window/docshell
  let docShell = window ? window.QueryInterface(Ci.nsIInterfaceRequestor)
                                .getInterface(Ci.nsIDocShell)
                        : null;
  Object.defineProperty(this, "docShell", {
    value: docShell,
    configurable: true
  });
}
exports.ChromeActor = ChromeActor;

ChromeActor.prototype = Object.create(TabActor.prototype);

ChromeActor.prototype.constructor = ChromeActor;

ChromeActor.prototype.isRootActor = true;

/**
 * Getter for the list of all docshells in this tabActor
 * @return {Array}
 */
Object.defineProperty(ChromeActor.prototype, "docShells", {
  get: function () {
    // Iterate over all top-level windows and all their docshells.
    let docShells = [];
    let e = Services.ww.getWindowEnumerator();
    while (e.hasMoreElements()) {
      let window = e.getNext();
      let docShell = window.QueryInterface(Ci.nsIInterfaceRequestor)
                           .getInterface(Ci.nsIWebNavigation)
                           .QueryInterface(Ci.nsIDocShell);
      docShells = docShells.concat(getChildDocShells(docShell));
    }

    return docShells;
  }
});

ChromeActor.prototype.observe = function(aSubject, aTopic, aData) {
  TabActor.prototype.observe.call(this, aSubject, aTopic, aData);
  if (!this.attached) {
    return;
  }
  if (aTopic == "chrome-webnavigation-create") {
    aSubject.QueryInterface(Ci.nsIDocShell);
    this._onDocShellCreated(aSubject);
  } else if (aTopic == "chrome-webnavigation-destroy") {
    this._onDocShellDestroy(aSubject);
  }
}

ChromeActor.prototype._attach = function() {
  if (this.attached) {
    return false;
  }

  TabActor.prototype._attach.call(this);

  // Listen for any new/destroyed chrome docshell
  Services.obs.addObserver(this, "chrome-webnavigation-create", false);
  Services.obs.addObserver(this, "chrome-webnavigation-destroy", false);

  // Iterate over all top-level windows.
  let docShells = [];
  let e = Services.ww.getWindowEnumerator();
  while (e.hasMoreElements()) {
    let window = e.getNext();
    let docShell = window.QueryInterface(Ci.nsIInterfaceRequestor)
                         .getInterface(Ci.nsIWebNavigation)
                         .QueryInterface(Ci.nsIDocShell);
    if (docShell == this.docShell) {
      continue;
    }
    this._progressListener.watch(docShell);
  }
};

ChromeActor.prototype._detach = function() {
  if (!this.attached) {
    return false;
  }

  Services.obs.removeObserver(this, "chrome-webnavigation-create");
  Services.obs.removeObserver(this, "chrome-webnavigation-destroy");

  // Iterate over all top-level windows.
  let docShells = [];
  let e = Services.ww.getWindowEnumerator();
  while (e.hasMoreElements()) {
    let window = e.getNext();
    let docShell = window.QueryInterface(Ci.nsIInterfaceRequestor)
                         .getInterface(Ci.nsIWebNavigation)
                         .QueryInterface(Ci.nsIDocShell);
    if (docShell == this.docShell) {
      continue;
    }
    this._progressListener.unwatch(docShell);
  }

  TabActor.prototype._detach.call(this);
};

/* ThreadActor hooks. */

/**
 * Prepare to enter a nested event loop by disabling debuggee events.
 */
ChromeActor.prototype.preNest = function() {
  // Disable events in all open windows.
  let e = Services.wm.getEnumerator(null);
  while (e.hasMoreElements()) {
    let win = e.getNext();
    let windowUtils = win.QueryInterface(Ci.nsIInterfaceRequestor)
                         .getInterface(Ci.nsIDOMWindowUtils);
    windowUtils.suppressEventHandling(true);
    windowUtils.suspendTimeouts();
  }
}

/**
 * Prepare to exit a nested event loop by enabling debuggee events.
 */
ChromeActor.prototype.postNest = function(aNestData) {
  // Enable events in all open windows.
  let e = Services.wm.getEnumerator(null);
  while (e.hasMoreElements()) {
    let win = e.getNext();
    let windowUtils = win.QueryInterface(Ci.nsIInterfaceRequestor)
                         .getInterface(Ci.nsIDOMWindowUtils);
    windowUtils.resumeTimeouts();
    windowUtils.suppressEventHandling(false);
  }
}
