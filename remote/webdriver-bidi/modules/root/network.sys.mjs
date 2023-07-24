/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { Module } from "chrome://remote/content/shared/messagehandler/Module.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  notifyNavigationStarted:
    "chrome://remote/content/shared/NavigationManager.sys.mjs",
  NetworkListener:
    "chrome://remote/content/shared/listeners/NetworkListener.sys.mjs",
  TabManager: "chrome://remote/content/shared/TabManager.sys.mjs",
  WindowGlobalMessageHandler:
    "chrome://remote/content/shared/messagehandler/WindowGlobalMessageHandler.sys.mjs",
});

/**
 * @typedef {object} BaseParameters
 * @property {string=} context
 * @property {Navigation=} navigation
 * @property {number} redirectCount
 * @property {RequestData} request
 * @property {number} timestamp
 */

/**
 * Enum of possible BytesValue types.
 *
 * @readonly
 * @enum {BytesValueType}
 */
const BytesValueType = {
  Base64: "base64",
  String: "string",
};

/**
 * @typedef {object} BytesValue
 * @property {BytesValueType} type
 * @property {string} value
 */

/**
 * @typedef {object} Cookie
 * @property {string} domain
 * @property {number=} expires
 * @property {boolean} httpOnly
 * @property {string} name
 * @property {string} path
 * @property {('lax' | 'none' | 'strict')} sameSite
 * @property {boolean} secure
 * @property {number} size
 * @property {BytesValue} value
 */

/**
 * @typedef {object} FetchTimingInfo
 * @property {number} timeOrigin
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
 * @typedef {object} Header
 * @property {string} name
 * @property {BytesValue} value
 */

/**
 * @typedef {string} InitiatorType
 */

/**
 * Enum of possible initiator types.
 *
 * @readonly
 * @enum {InitiatorType}
 */
const InitiatorType = {
  Other: "other",
  Parser: "parser",
  Preflight: "preflight",
  Script: "script",
};
/**
 * @typedef {object} Initiator
 * @property {InitiatorType} type
 * @property {number=} columnNumber
 * @property {number=} lineNumber
 * @property {string=} request
 * @property {StackTrace=} stackTrace
 */

/**
 * @typedef {object} RequestData
 * @property {number|null} bodySize
 *     Defaults to null.
 * @property {Array<Cookie>} cookies
 * @property {Array<Header>} headers
 * @property {number} headersSize
 * @property {string} method
 * @property {string} request
 * @property {FetchTimingInfo} timings
 * @property {string} url
 */

/**
 * @typedef {object} BeforeRequestSentParametersProperties
 * @property {Initiator} initiator
 */

/* eslint-disable jsdoc/valid-types */
/**
 * Parameters for the BeforeRequestSent event
 *
 * @typedef {BaseParameters & BeforeRequestSentParametersProperties} BeforeRequestSentParameters
 */
/* eslint-enable jsdoc/valid-types */

/**
 * @typedef {object} ResponseContent
 * @property {number|null} size
 *     Defaults to null.
 */

/**
 * @typedef {object} ResponseData
 * @property {string} url
 * @property {string} protocol
 * @property {number} status
 * @property {string} statusText
 * @property {boolean} fromCache
 * @property {Array<Header>} headers
 * @property {string} mimeType
 * @property {number} bytesReceived
 * @property {number|null} headersSize
 *     Defaults to null.
 * @property {number|null} bodySize
 *     Defaults to null.
 * @property {ResponseContent} content
 */

/**
 * @typedef {object} ResponseStartedParametersProperties
 * @property {ResponseData} response
 */

/* eslint-disable jsdoc/valid-types */
/**
 * Parameters for the ResponseStarted event
 *
 * @typedef {BaseParameters & ResponseStartedParametersProperties} ResponseStartedParameters
 */
/* eslint-enable jsdoc/valid-types */

/**
 * @typedef {object} ResponseCompletedParametersProperties
 * @property {ResponseData} response
 */

/* eslint-disable jsdoc/valid-types */
/**
 * Parameters for the ResponseCompleted event
 *
 * @typedef {BaseParameters & ResponseCompletedParametersProperties} ResponseCompletedParameters
 */
/* eslint-enable jsdoc/valid-types */

class NetworkModule extends Module {
  #networkListener;
  #subscribedEvents;

  constructor(messageHandler) {
    super(messageHandler);

    // Set of event names which have active subscriptions
    this.#subscribedEvents = new Set();

    this.#networkListener = new lazy.NetworkListener();
    this.#networkListener.on("before-request-sent", this.#onBeforeRequestSent);
    this.#networkListener.on("response-completed", this.#onResponseEvent);
    this.#networkListener.on("response-started", this.#onResponseEvent);
  }

  destroy() {
    this.#networkListener.off("before-request-sent", this.#onBeforeRequestSent);
    this.#networkListener.off("response-completed", this.#onResponseEvent);
    this.#networkListener.off("response-started", this.#onResponseEvent);
    this.#networkListener.destroy();

    this.#subscribedEvents = null;
  }

  #getContextInfo(browsingContext) {
    return {
      contextId: browsingContext.id,
      type: lazy.WindowGlobalMessageHandler.type,
    };
  }

  #getOrCreateNavigationId(browsingContext, url) {
    const navigation =
      this.messageHandler.navigationManager.getNavigationForBrowsingContext(
        browsingContext
      );

    // Check if an ongoing navigation is available for this browsing context.
    // onBeforeRequestSent might be too early for the NavigationManager.
    // TODO: Bug 1835704 to detect navigations earlier and avoid this.
    if (navigation && !navigation.finished) {
      return navigation.id;
    }

    // No ongoing navigation for this browsing context, create a new one.
    return lazy.notifyNavigationStarted({
      contextDetails: { context: browsingContext },
      url,
    }).id;
  }

  #onBeforeRequestSent = (name, data) => {
    const {
      contextId,
      isNavigationRequest,
      redirectCount,
      requestData,
      timestamp,
    } = data;

    const browsingContext = lazy.TabManager.getBrowsingContextById(contextId);

    // Bug 1805479: Handle the initiator, including stacktrace details.
    const initiator = {
      type: InitiatorType.Other,
    };

    const navigationId = isNavigationRequest
      ? this.#getOrCreateNavigationId(browsingContext, requestData.url)
      : null;

    const baseParameters = {
      context: contextId,
      navigation: navigationId,
      redirectCount,
      request: requestData,
      timestamp,
    };

    const beforeRequestSentEvent = this.#serializeNetworkEvent({
      ...baseParameters,
      initiator,
    });

    this.emitEvent(
      "network.beforeRequestSent",
      beforeRequestSentEvent,
      this.#getContextInfo(browsingContext)
    );
  };

  #onResponseEvent = (name, data) => {
    const {
      contextId,
      isNavigationRequest,
      redirectCount,
      requestData,
      responseData,
      timestamp,
    } = data;

    const browsingContext = lazy.TabManager.getBrowsingContextById(contextId);

    const navigation = isNavigationRequest
      ? this.messageHandler.navigationManager.getNavigationForBrowsingContext(
          browsingContext
        )
      : null;

    const baseParameters = {
      context: contextId,
      navigation: navigation ? navigation.id : null,
      redirectCount,
      request: requestData,
      timestamp,
    };

    const responseEvent = this.#serializeNetworkEvent({
      ...baseParameters,
      response: responseData,
    });

    const protocolEventName =
      name === "response-started"
        ? "network.responseStarted"
        : "network.responseCompleted";

    this.emitEvent(
      protocolEventName,
      responseEvent,
      this.#getContextInfo(browsingContext)
    );
  };

  #serializeHeadersOrCookies(headersOrCookies) {
    return headersOrCookies.map(item => ({
      name: item.name,
      value: this.#serializeStringAsBytesValue(item.value),
    }));
  }

  /**
   * Serialize in-place all cookies and headers arrays found in a given network
   * event payload.
   *
   * @param {object} networkEvent
   *     The network event parameters object to serialize.
   * @returns {object}
   *     The serialized network event parameters.
   */
  #serializeNetworkEvent(networkEvent) {
    // Make a shallow copy of networkEvent before serializing the headers and
    // cookies arrays in request/response.
    const serialized = { ...networkEvent };

    // Make a shallow copy of the request data.
    serialized.request = { ...networkEvent.request };
    serialized.request.cookies = this.#serializeHeadersOrCookies(
      networkEvent.request.cookies
    );
    serialized.request.headers = this.#serializeHeadersOrCookies(
      networkEvent.request.headers
    );

    if (networkEvent.response?.headers) {
      // Make a shallow copy of the response data.
      serialized.response = { ...networkEvent.response };
      serialized.response.headers = this.#serializeHeadersOrCookies(
        networkEvent.response.headers
      );
    }

    return serialized;
  }

  /**
   * Serialize a string value as BytesValue.
   *
   * Note: This does not attempt to fully implement serialize protocol bytes
   * (https://w3c.github.io/webdriver-bidi/#serialize-protocol-bytes) as the
   * header values read from the Channel are already serialized as strings at
   * the moment.
   *
   * @param {string} value
   *     The value to serialize.
   */
  #serializeStringAsBytesValue(value) {
    // TODO: For now, we handle all headers and cookies with the "string" type.
    // See Bug 1835216 to add support for "base64" type and handle non-utf8
    // values.
    return {
      type: BytesValueType.String,
      value,
    };
  }

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
    // TODO: Bug 1775231. Move this logic to a shared module or an abstract
    // class.
    const { category } = params;
    if (category === "event") {
      const filteredSessionData = params.sessionData.filter(item =>
        this.messageHandler.matchesContext(item.contextDescriptor)
      );
      for (const event of this.#subscribedEvents.values()) {
        const hasSessionItem = filteredSessionData.some(
          item => item.value === event
        );
        // If there are no session items for this context, we should unsubscribe from the event.
        if (!hasSessionItem) {
          this.#unsubscribeEvent(event);
        }
      }

      // Subscribe to all events, which have an item in SessionData.
      for (const { value } of filteredSessionData) {
        this.#subscribeEvent(value);
      }
    }
  }

  static get supportedEvents() {
    return [
      "network.beforeRequestSent",
      "network.responseCompleted",
      "network.responseStarted",
    ];
  }
}

export const network = NetworkModule;
