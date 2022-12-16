/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { Module } from "chrome://remote/content/shared/messagehandler/Module.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  NetworkListener:
    "chrome://remote/content/shared/listeners/NetworkListener.sys.mjs",
  TabManager: "chrome://remote/content/shared/TabManager.sys.mjs",
  WindowGlobalMessageHandler:
    "chrome://remote/content/shared/messagehandler/WindowGlobalMessageHandler.sys.mjs",
});

/**
 * @typedef {Object} BaseParameters
 * @property {string=} context
 * @property {boolean} isRedirect
 * @property {Navigation=} navigation
 * @property {RequestData} request
 * @property {number} timestamp
 */

/**
 * @typedef {Object} Cookie
 * @property {Array<number>=} binaryValue
 * @property {string} domain
 * @property {number=} expires
 * @property {boolean} httpOnly
 * @property {string} name
 * @property {string} path
 * @property {('lax' | 'none' | 'strict')} sameSite
 * @property {boolean} secure
 * @property {number} size
 * @property {string=} value
 */

/**
 * @typedef {Object} FetchTimingInfo
 * @property {number} originTime
 * @property {number} requestTime
 * @property {number} redirectStart
 * @property {number} redirectEnd
 * @property {number} fetchStart
 * @property {number} dnsStart
 * @property {number} dnsEnd
 * @property {number} connectStart
 * @property {number} connectEnd
 * @property {number} tlsStart
 * @property {number} requestStart
 * @property {number} responseStart
 * @property {number} responseEnd
 */

/**
 * @typedef {Object} Header
 * @property {Array<number>=} binaryValue
 * @property {string} name
 * @property {string=} value
 */

/**
 * @typedef {string} InitiatorType
 **/

/**
 * Enum of possible initiator types.
 *
 * @readonly
 * @enum {InitiatorType}
 **/
const InitiatorType = {
  Other: "other",
  Parser: "parser",
  Preflight: "preflight",
  Script: "script",
};
/**
 * @typedef {Object} Initiator
 * @property {InitiatorType} type
 * @property {number=} columnNumber
 * @property {number=} lineNumber
 * @property {string=} request
 * @property {StackTrace=} stackTrace
 */

/**
 * @typedef {Object} RequestData
 * @property {number} bodySize
 * @property {Array<Cookie>} cookies
 * @property {Array<Header>} headers
 * @property {number} headersSize
 * @property {string} method
 * @property {string} request
 * @property {FetchTimingInfo} timings
 * @property {string} url
 */

/**
 * @typedef {Object} BeforeRequestSentParametersProperties
 * @property {Initiator} initiator
 */

/**
 * Parameters for the BeforeRequestSent event
 *
 * @typedef {BaseParameters & BeforeRequestSentParametersProperties} BeforeRequestSentParameters
 */

class NetworkModule extends Module {
  #beforeRequestSentMap;
  #networkListener;
  #subscribedEvents;

  constructor(messageHandler) {
    super(messageHandler);

    // Map of request ids to redirect counts. A WebDriver BiDi request id is
    // identical for redirects of a given request, this map allows to know if we
    // already emitted a beforeRequestSent event for a given request with a
    // specific redirectCount.
    this.#beforeRequestSentMap = new Map();

    this.#networkListener = new lazy.NetworkListener(messageHandler);

    // Set of event names which have active subscriptions
    this.#subscribedEvents = new Set();

    this.#networkListener.on("before-request-sent", this.#onBeforeRequestSent);
  }

  destroy() {
    this.#networkListener.off("before-request-sent", this.#onBeforeRequestSent);

    this.#beforeRequestSentMap = null;
    this.#subscribedEvents = null;
  }

  #getContextInfo(browsingContext) {
    return {
      contextId: browsingContext.id,
      type: lazy.WindowGlobalMessageHandler.type,
    };
  }

  #onBeforeRequestSent = (name, data) => {
    const { contextId, requestData, timestamp, redirectCount } = data;

    const isRedirect = redirectCount > 0;
    this.#beforeRequestSentMap.set(requestData.requestId, redirectCount);

    // Bug 1805479: Handle the initiator, including stacktrace details.
    const initiator = {
      type: InitiatorType.Other,
    };

    const baseParameters = {
      context: contextId,
      isRedirect,
      redirectCount,
      // Bug 1805405: Handle the navigation id.
      navigation: null,
      request: requestData,
      timestamp,
    };

    const beforeRequestSentEvent = {
      ...baseParameters,
      initiator,
    };

    const browsingContext = lazy.TabManager.getBrowsingContextById(contextId);
    this.emitEvent(
      "network.beforeRequestSent",
      beforeRequestSentEvent,
      this.#getContextInfo(browsingContext)
    );
  };

  #startListening(event) {
    if (this.#subscribedEvents.size == 0) {
      this.#networkListener.startListening();
    }
    this.#subscribedEvents.add(event);
  }

  #stopListening(event) {
    this.#subscribedEvents.delete(event);
    if (this.#subscribedEvents.size == 0) {
      this.#networkListener.stopListening();
    }
  }

  #subscribeEvent(event) {
    if (this.constructor.supportedEvents.includes(event)) {
      this.#startListening(event);
    }
  }

  #unsubscribeEvent(event) {
    if (this.constructor.supportedEvents.includes(event)) {
      this.#stopListening(event);
    }
  }

  /**
   * Internal commands
   */

  _applySessionData(params) {
    const { category, added = [], removed = [] } = params;
    if (category === "event") {
      for (const event of added) {
        this.#subscribeEvent(event);
      }
      for (const event of removed) {
        this.#unsubscribeEvent(event);
      }
    }
  }

  static get supportedEvents() {
    return ["network.beforeRequestSent"];
  }
}

export const network = NetworkModule;
