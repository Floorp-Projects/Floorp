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
   * Inspect the properties of an object.
   *
   * @param string aActor
   *        The WebConsoleObjectActor ID to send the request to.
   * @param function aOnResponse
   *        The function invoked when the response is received.
   */
  inspectObjectProperties:
  function WCC_inspectObjectProperties(aActor, aOnResponse)
  {
    let packet = {
      to: aActor,
      type: "inspectProperties",
    };
    this._client.request(packet, aOnResponse);
  },

  /**
   * Evaluate a JavaScript expression.
   *
   * @param string aString
   *        The code you want to evaluate.
   * @param function aOnResponse
   *        The function invoked when the response is received.
   */
  evaluateJS: function WCC_evaluateJS(aString, aOnResponse)
  {
    let packet = {
      to: this._actor,
      type: "evaluateJS",
      text: aString,
    };
    this._client.request(packet, aOnResponse);
  },

  /**
   * Autocomplete a JavaScript expression.
   *
   * @param string aString
   *        The code you want to autocomplete.
   * @param number aCursor
   *        Cursor location inside the string. Index starts from 0.
   * @param function aOnResponse
   *        The function invoked when the response is received.
   */
  autocomplete: function WCC_autocomplete(aString, aCursor, aOnResponse)
  {
    let packet = {
      to: this._actor,
      type: "autocomplete",
      text: aString,
      cursor: aCursor,
    };
    this._client.request(packet, aOnResponse);
  },

  /**
   * Clear the cache of messages (page errors and console API calls).
   */
  clearMessagesCache: function WCC_clearMessagesCache()
  {
    let packet = {
      to: this._actor,
      type: "clearMessagesCache",
    };
    this._client.request(packet);
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
