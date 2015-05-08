/* -*- js-indent-level: 2; indent-tabs-mode: nil -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {Cc, Ci, Cu} = require("chrome");
const DevToolsUtils = require("devtools/toolkit/DevToolsUtils");
const EventEmitter = require("devtools/toolkit/event-emitter");

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
  this.events = [];
  this._networkRequests = new Map();

  this.pendingEvaluationResults = new Map();
  this.onEvaluationResult = this.onEvaluationResult.bind(this);
  this.onNetworkEvent = this._onNetworkEvent.bind(this);
  this.onNetworkEventUpdate = this._onNetworkEventUpdate.bind(this);

  this._client.addListener("evaluationResult", this.onEvaluationResult);
  this._client.addListener("networkEvent", this.onNetworkEvent);
  this._client.addListener("networkEventUpdate", this.onNetworkEventUpdate);
  EventEmitter.decorate(this);
}

exports.WebConsoleClient = WebConsoleClient;

WebConsoleClient.prototype = {
  _longStrings: null,
  traits: null,

  /**
   * Holds the network requests currently displayed by the Web Console. Each key
   * represents the connection ID and the value is network request information.
   * @private
   * @type object
   */
  _networkRequests: null,

  getNetworkRequest(actorId) {
    return this._networkRequests.get(actorId);
  },

  hasNetworkRequest(actorId) {
    return this._networkRequests.has(actorId);
  },

  removeNetworkRequest(actorId) {
    this._networkRequests.delete(actorId);
  },

  getNetworkEvents() {
    return this._networkRequests.values();
  },

  get actor() { return this._actor; },

  /**
   * The "networkEvent" message type handler. We redirect any message to
   * the UI for displaying.
   *
   * @private
   * @param string type
   *        Message type.
   * @param object packet
   *        The message received from the server.
   */
  _onNetworkEvent: function (type, packet)
  {
    if (packet.from == this._actor) {
      let actor = packet.eventActor;
      let networkInfo = {
        _type: "NetworkEvent",
        timeStamp: actor.timeStamp,
        node: null,
        actor: actor.actor,
        discardRequestBody: true,
        discardResponseBody: true,
        startedDateTime: actor.startedDateTime,
        request: {
          url: actor.url,
          method: actor.method,
        },
        isXHR: actor.isXHR,
        response: {},
        timings: {},
        updates: [], // track the list of network event updates
        private: actor.private,
        fromCache: actor.fromCache
      };
      this._networkRequests.set(actor.actor, networkInfo);

      this.emit("networkEvent", networkInfo);
    }
  },

  /**
   * The "networkEventUpdate" message type handler. We redirect any message to
   * the UI for displaying.
   *
   * @private
   * @param string type
   *        Message type.
   * @param object packet
   *        The message received from the server.
   */
  _onNetworkEventUpdate: function (type, packet)
  {
    let networkInfo = this.getNetworkRequest(packet.from);
    if (!networkInfo) {
      return;
    }

    networkInfo.updates.push(packet.updateType);

    switch (packet.updateType) {
      case "requestHeaders":
        networkInfo.request.headersSize = packet.headersSize;
        break;
      case "requestPostData":
        networkInfo.discardRequestBody = packet.discardRequestBody;
        networkInfo.request.bodySize = packet.dataSize;
        break;
      case "responseStart":
        networkInfo.response.httpVersion = packet.response.httpVersion;
        networkInfo.response.status = packet.response.status;
        networkInfo.response.statusText = packet.response.statusText;
        networkInfo.response.headersSize = packet.response.headersSize;
        networkInfo.response.remoteAddress = packet.response.remoteAddress;
        networkInfo.response.remotePort = packet.response.remotePort;
        networkInfo.discardResponseBody = packet.response.discardResponseBody;
        break;
      case "responseContent":
        networkInfo.response.content = {
          mimeType: packet.mimeType,
        };
        networkInfo.response.bodySize = packet.contentSize;
        networkInfo.response.transferredSize = packet.transferredSize;
        networkInfo.discardResponseBody = packet.discardResponseBody;
        break;
      case "eventTimings":
        networkInfo.totalTime = packet.totalTime;
        break;
      case "securityInfo":
        networkInfo.securityInfo = packet.state;
        break;
    }

    this.emit("networkEventUpdate", {
      packet: packet,
      networkInfo
    });
  },

  /**
   * Retrieve the cached messages from the server.
   *
   * @see this.CACHED_MESSAGES
   * @param array types
   *        The array of message types you want from the server. See
   *        this.CACHED_MESSAGES for known types.
   * @param function aOnResponse
   *        The function invoked when the response is received.
   */
  getCachedMessages: function WCC_getCachedMessages(types, aOnResponse)
  {
    let packet = {
      to: this._actor,
      type: "getCachedMessages",
      messageTypes: types,
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
   *
   *        - selectedNodeActor: the NodeActor ID of the current selection in the
   *        Inspector, if such a selection exists. This is used by helper functions
   *        that can reference the currently selected node in the Inspector, like
   *        $0.
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
      selectedNodeActor: aOptions.selectedNodeActor,
    };
    this._client.request(packet, aOnResponse);
  },

  /**
   * Evaluate a JavaScript expression asynchronously.
   * See evaluateJS for parameter and response information.
   */
  evaluateJSAsync: function(aString, aOnResponse, aOptions = {})
  {
    // Pre-37 servers don't support async evaluation.
    if (!this.traits.evaluateJSAsync) {
      this.evaluateJS(aString, aOnResponse, aOptions);
      return;
    }

    let packet = {
      to: this._actor,
      type: "evaluateJSAsync",
      text: aString,
      bindObjectActor: aOptions.bindObjectActor,
      frameActor: aOptions.frameActor,
      url: aOptions.url,
      selectedNodeActor: aOptions.selectedNodeActor,
    };

    this._client.request(packet, response => {
      // Null check this in case the client has been detached while waiting
      // for a response.
      if (this.pendingEvaluationResults) {
        this.pendingEvaluationResults.set(response.resultID, aOnResponse);
      }
    });
  },

  /**
   * Handler for the actors's unsolicited evaluationResult packet.
   */
  onEvaluationResult: function(aNotification, aPacket) {
    // Find the associated callback based on this ID, and fire it.
    // In a sync evaluation, this would have already been called in
    // direct response to the client.request function.
    let onResponse = this.pendingEvaluationResults.get(aPacket.resultID);
    if (onResponse) {
      onResponse(aPacket);
      this.pendingEvaluationResults.delete(aPacket.resultID);
    } else {
      DevToolsUtils.reportException("onEvaluationResult",
        "No response handler for an evaluateJSAsync result (resultID: " + aPacket.resultID + ")");
    }
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
   * Retrieve the security information for the given NetworkEventActor.
   *
   * @param string aActor
   *        The NetworkEventActor ID.
   * @param function aOnResponse
   *        The function invoked when the response is received.
   */
  getSecurityInfo: function WCC_getSecurityInfo(aActor, aOnResponse)
  {
    let packet = {
      to: aActor,
      type: "getSecurityInfo",
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
  detach: function WCC_detach(aOnResponse)
  {
    this._client.removeListener("evaluationResult", this.onEvaluationResult);
    this._client.removeListener("networkEvent", this.onNetworkEvent);
    this._client.removeListener("networkEventUpdate", this.onNetworkEventUpdate);
    this.stopListeners(null, aOnResponse);
    this._longStrings = null;
    this._client = null;
    this.pendingEvaluationResults.clear();
    this.pendingEvaluationResults = null;
    this.clearNetworkRequests();
    this._networkRequests = null;
  },

  clearNetworkRequests: function () {
    this._networkRequests.clear();
  }
};
