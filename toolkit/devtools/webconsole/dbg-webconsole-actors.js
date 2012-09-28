/* -*- Mode: js2; js2-basic-offset: 2; indent-tabs-mode: nil; -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

let Cc = Components.classes;
let Ci = Components.interfaces;
let Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Services",
                                  "resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "PageErrorListener",
                                  "resource://gre/modules/devtools/WebConsoleUtils.jsm");

/**
 * The WebConsoleActor implements capabilities needed for the Web Console
 * feature.
 *
 * @constructor
 * @param object aConnection
 *        The connection to the client, DebuggerServerConnection.
 * @param object aTabActor
 *        The parent tab actor.
 */
function WebConsoleActor(aConnection, aTabActor)
{
  this.conn = aConnection;
  this._browser = aTabActor.browser;
}

WebConsoleActor.prototype =
{
  /**
   * The xul:browser we work with.
   * @private
   * @type nsIDOMElement
   */
  _browser: null,

  /**
   * The debugger server connection instance.
   * @type object
   */
  conn: null,

  /**
   * The content window we work with.
   * @type nsIDOMWindow
   */
  get window() this._browser.contentWindow,

  /**
   * The PageErrorListener instance.
   * @type object
   */
  pageErrorListener: null,

  actorPrefix: "console",

  grip: function WCA_grip()
  {
    return { actor: this.actorID };
  },

  /**
   * Destroy the current WebConsoleActor instance.
   */
  disconnect: function WCA_disconnect()
  {
    if (this.pageErrorListener) {
      this.pageErrorListener.destroy();
      this.pageErrorListener = null;
    }
    this.conn = this._browser = null;
  },

  /**
   * Handler for the "startListeners" request.
   *
   * @param object aRequest
   *        The JSON request object received from the Web Console client.
   * @return object
   *         The response object which holds the startedListeners array.
   */
  onStartListeners: function WCA_onStartListeners(aRequest)
  {
    let startedListeners = [];

    while (aRequest.listeners.length > 0) {
      let listener = aRequest.listeners.shift();
      switch (listener) {
        case "PageError":
          if (!this.pageErrorListener) {
            this.pageErrorListener =
              new PageErrorListener(this.window, this);
            this.pageErrorListener.init();
          }
          startedListeners.push(listener);
          break;
      }
    }
    return { startedListeners: startedListeners };
  },

  /**
   * Handler for the "stopListeners" request.
   *
   * @param object aRequest
   *        The JSON request object received from the Web Console client.
   * @return object
   *         The response packet to send to the client: holds the
   *         stoppedListeners array.
   */
  onStopListeners: function WCA_onStopListeners(aRequest)
  {
    let stoppedListeners = [];

    // If no specific listeners are requested to be detached, we stop all
    // listeners.
    let toDetach = aRequest.listeners || ["PageError"];

    while (toDetach.length > 0) {
      let listener = toDetach.shift();
      switch (listener) {
        case "PageError":
          if (this.pageErrorListener) {
            this.pageErrorListener.destroy();
            this.pageErrorListener = null;
          }
          stoppedListeners.push(listener);
          break;
      }
    }

    return { stoppedListeners: stoppedListeners };
  },

  /**
   * Handler for page errors received from the PageErrorListener. This method
   * sends the nsIScriptError to the remote Web Console client.
   *
   * @param nsIScriptError aPageError
   *        The page error we need to send to the client.
   */
  onPageError: function WCA_onPageError(aPageError)
  {
    let packet = {
      from: this.actorID,
      type: "pageError",
      pageError: this.preparePageErrorForRemote(aPageError),
    };
    this.conn.send(packet);
  },

  /**
   * Prepare an nsIScriptError to be sent to the client.
   *
   * @param nsIScriptError aPageError
   *        The page error we need to send to the client.
   * @return object
   *         The object you can send to the remote client.
   */
  preparePageErrorForRemote: function WCA_preparePageErrorForRemote(aPageError)
  {
    return {
      message: aPageError.message,
      errorMessage: aPageError.errorMessage,
      sourceName: aPageError.sourceName,
      lineText: aPageError.sourceLine,
      lineNumber: aPageError.lineNumber,
      columnNumber: aPageError.columnNumber,
      category: aPageError.category,
      timeStamp: aPageError.timeStamp,
      warning: !!(aPageError.flags & aPageError.warningFlag),
      error: !!(aPageError.flags & aPageError.errorFlag),
      exception: !!(aPageError.flags & aPageError.exceptionFlag),
      strict: !!(aPageError.flags & aPageError.strictFlag),
    };
  },
};

WebConsoleActor.prototype.requestTypes =
{
  startListeners: WebConsoleActor.prototype.onStartListeners,
  stopListeners: WebConsoleActor.prototype.onStopListeners,
};

