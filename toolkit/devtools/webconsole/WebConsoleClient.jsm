/* -*- Mode: js2; js2-basic-offset: 2; indent-tabs-mode: nil; -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

var EXPORTED_SYMBOLS = ["WebConsoleClient"];

/**
 * A WebConsoleClient is used as a front end for the WebConsoleActor that is
 * created on the server, hiding implementation details.
 *
 * @param object aDebuggerClient
 *        The DebuggerClient instance we live for.
 * @param string aActor
 *        The WebConsoleActor ID.
 */
function WebConsoleClient(aDebuggerClient, aActor)
{
  this._actor = aActor;
  this._client = aDebuggerClient;
}

WebConsoleClient.prototype = {
  /**
   * Retrieve the cached messages from the server.
   *
   * @see this.CACHED_MESSAGES
   * @param array aTypes
   *        The array of message types you want from the server. See
   *        this.CACHED_MESSAGES for known types.
   * @param function aOnResponse
   *        The function invoked when the response is received.
   */
  getCachedMessages: function WCC_getCachedMessages(aTypes, aOnResponse)
  {
    let packet = {
      to: this._actor,
      type: "getCachedMessages",
      messageTypes: aTypes,
    };
    this._client.request(packet, aOnResponse);
  },

  /**
   * Start the given Web Console listeners.
   *
   * @see this.LISTENERS
   * @param array aListeners
   *        Array of listeners you want to start. See this.LISTENERS for
   *        known listeners.
   * @param function aOnResponse
   *        Function to invoke when the server response is received.
   */
  startListeners: function WCC_startListeners(aListeners, aOnResponse)
  {
    let packet = {
      to: this._actor,
      type: "startListeners",
      listeners: aListeners,
    };
    this._client.request(packet, aOnResponse);
  },

  /**
   * Stop the given Web Console listeners.
   *
   * @see this.LISTENERS
   * @param array aListeners
   *        Array of listeners you want to stop. See this.LISTENERS for
   *        known listeners.
   * @param function aOnResponse
   *        Function to invoke when the server response is received.
   */
  stopListeners: function WCC_stopListeners(aListeners, aOnResponse)
  {
    let packet = {
      to: this._actor,
      type: "stopListeners",
      listeners: aListeners,
    };
    this._client.request(packet, aOnResponse);
  },

  /**
   * Close the WebConsoleClient. This stops all the listeners on the server and
   * detaches from the console actor.
   *
   * @param function aOnResponse
   *        Function to invoke when the server response is received.
   */
  close: function WCC_close(aOnResponse)
  {
    this.stopListeners(null, aOnResponse);
    this._client = null;
  },
};
