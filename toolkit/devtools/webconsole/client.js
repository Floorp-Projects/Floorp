/* -*- js2-basic-offset: 2; indent-tabs-mode: nil; -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {Cc, Ci, Cu} = require("chrome");

loader.lazyImporter(this, "LongStringClient", "resource://gre/modules/devtools/dbg-client.jsm");

/**
 * A WebConsoleClient is used as a front end for the WebConsoleActor that is
 * created on the server, hiding implementation details.
 *
 * @param object aDebuggerClient
 *        The DebuggerClient instance we live for.
 * @param object aResponse
 *        The response packet received from the "startListeners" request sent to
 *        the WebConsoleActor.
 */
function WebConsoleClient(aDebuggerClient, aResponse)
{
  this._actor = aResponse.from;
  this._client = aDebuggerClient;
  this._longStrings = {};
  this.traits = aResponse.traits || {};
}
exports.WebConsoleClient = WebConsoleClient;

WebConsoleClient.prototype = {
  _longStrings: null,
  traits: null,

  get actor() { return this._actor; },

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
   * @param object [aOptions={}]
   *        Options for evaluation:
   *
   *        - bindObjectActor: an ObjectActor ID. The OA holds a reference to
   *        a Debugger.Object that wraps a content object. This option allows
   *        you to bind |_self| to the D.O of the given OA, during string
   *        evaluation.
   *
   *        See: Debugger.Object.evalInGlobalWithBindings() for information
   *        about bindings.
   *
   *        Use case: the variable view needs to update objects and it does so
   *        by knowing the ObjectActor it inspects and binding |_self| to the
   *        D.O of the OA. As such, variable view sends strings like these for
   *        eval:
   *          _self["prop"] = value;
   *
   *        - frameActor: a FrameActor ID. The FA holds a reference to
   *        a Debugger.Frame. This option allows you to evaluate the string in
   *        the frame of the given FA.
   *
   *        - url: the url to evaluate the script as. Defaults to
   *        "debugger eval code".
   */
  evaluateJS: function WCC_evaluateJS(aString, aOnResponse, aOptions = {})
  {
    let packet = {
      to: this._actor,
      type: "evaluateJS",
      text: aString,
      bindObjectActor: aOptions.bindObjectActor,
      frameActor: aOptions.frameActor,
      url: aOptions.url,
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
   * @param string aFrameActor
   *        The id of the frame actor that made the call.
   */
  autocomplete: function WCC_autocomplete(aString, aCursor, aOnResponse, aFrameActor)
  {
    let packet = {
      to: this._actor,
      type: "autocomplete",
      text: aString,
      cursor: aCursor,
      frameActor: aFrameActor,
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
   * Get Web Console-related preferences on the server.
   *
   * @param array aPreferences
   *        An array with the preferences you want to retrieve.
   * @param function [aOnResponse]
   *        Optional function to invoke when the response is received.
   */
  getPreferences: function WCC_getPreferences(aPreferences, aOnResponse)
  {
    let packet = {
      to: this._actor,
      type: "getPreferences",
      preferences: aPreferences,
    };
    this._client.request(packet, aOnResponse);
  },

  /**
   * Set Web Console-related preferences on the server.
   *
   * @param object aPreferences
   *        An object with the preferences you want to change.
   * @param function [aOnResponse]
   *        Optional function to invoke when the response is received.
   */
  setPreferences: function WCC_setPreferences(aPreferences, aOnResponse)
  {
    let packet = {
      to: this._actor,
      type: "setPreferences",
      preferences: aPreferences,
    };
    this._client.request(packet, aOnResponse);
  },

  /**
   * Retrieve the request headers from the given NetworkEventActor.
   *
   * @param string aActor
   *        The NetworkEventActor ID.
   * @param function aOnResponse
   *        The function invoked when the response is received.
   */
  getRequestHeaders: function WCC_getRequestHeaders(aActor, aOnResponse)
  {
    let packet = {
      to: aActor,
      type: "getRequestHeaders",
    };
    this._client.request(packet, aOnResponse);
  },

  /**
   * Retrieve the request cookies from the given NetworkEventActor.
   *
   * @param string aActor
   *        The NetworkEventActor ID.
   * @param function aOnResponse
   *        The function invoked when the response is received.
   */
  getRequestCookies: function WCC_getRequestCookies(aActor, aOnResponse)
  {
    let packet = {
      to: aActor,
      type: "getRequestCookies",
    };
    this._client.request(packet, aOnResponse);
  },

  /**
   * Retrieve the request post data from the given NetworkEventActor.
   *
   * @param string aActor
   *        The NetworkEventActor ID.
   * @param function aOnResponse
   *        The function invoked when the response is received.
   */
  getRequestPostData: function WCC_getRequestPostData(aActor, aOnResponse)
  {
    let packet = {
      to: aActor,
      type: "getRequestPostData",
    };
    this._client.request(packet, aOnResponse);
  },

  /**
   * Retrieve the response headers from the given NetworkEventActor.
   *
   * @param string aActor
   *        The NetworkEventActor ID.
   * @param function aOnResponse
   *        The function invoked when the response is received.
   */
  getResponseHeaders: function WCC_getResponseHeaders(aActor, aOnResponse)
  {
    let packet = {
      to: aActor,
      type: "getResponseHeaders",
    };
    this._client.request(packet, aOnResponse);
  },

  /**
   * Retrieve the response cookies from the given NetworkEventActor.
   *
   * @param string aActor
   *        The NetworkEventActor ID.
   * @param function aOnResponse
   *        The function invoked when the response is received.
   */
  getResponseCookies: function WCC_getResponseCookies(aActor, aOnResponse)
  {
    let packet = {
      to: aActor,
      type: "getResponseCookies",
    };
    this._client.request(packet, aOnResponse);
  },

  /**
   * Retrieve the response content from the given NetworkEventActor.
   *
   * @param string aActor
   *        The NetworkEventActor ID.
   * @param function aOnResponse
   *        The function invoked when the response is received.
   */
  getResponseContent: function WCC_getResponseContent(aActor, aOnResponse)
  {
    let packet = {
      to: aActor,
      type: "getResponseContent",
    };
    this._client.request(packet, aOnResponse);
  },

  /**
   * Retrieve the timing information for the given NetworkEventActor.
   *
   * @param string aActor
   *        The NetworkEventActor ID.
   * @param function aOnResponse
   *        The function invoked when the response is received.
   */
  getEventTimings: function WCC_getEventTimings(aActor, aOnResponse)
  {
    let packet = {
      to: aActor,
      type: "getEventTimings",
    };
    this._client.request(packet, aOnResponse);
  },

  /**
   * Send a HTTP request with the given data.
   *
   * @param string aData
   *        The details of the HTTP request.
   * @param function aOnResponse
   *        The function invoked when the response is received.
   */
  sendHTTPRequest: function WCC_sendHTTPRequest(aData, aOnResponse) {
    let packet = {
      to: this._actor,
      type: "sendHTTPRequest",
      request: aData
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
   * Return an instance of LongStringClient for the given long string grip.
   *
   * @param object aGrip
   *        The long string grip returned by the protocol.
   * @return object
   *         The LongStringClient for the given long string grip.
   */
  longString: function WCC_longString(aGrip)
  {
    if (aGrip.actor in this._longStrings) {
      return this._longStrings[aGrip.actor];
    }

    let client = new LongStringClient(this._client, aGrip);
    this._longStrings[aGrip.actor] = client;
    return client;
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
    this._longStrings = null;
    this._client = null;
  },
};
