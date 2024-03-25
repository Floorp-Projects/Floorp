/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { Module } from "chrome://remote/content/shared/messagehandler/Module.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  assert: "chrome://remote/content/shared/webdriver/Assert.sys.mjs",
  error: "chrome://remote/content/shared/webdriver/Errors.sys.mjs",
  generateUUID: "chrome://remote/content/shared/UUID.sys.mjs",
  matchURLPattern:
    "chrome://remote/content/shared/webdriver/URLPattern.sys.mjs",
  notifyNavigationStarted:
    "chrome://remote/content/shared/NavigationManager.sys.mjs",
  NetworkListener:
    "chrome://remote/content/shared/listeners/NetworkListener.sys.mjs",
  parseChallengeHeader:
    "chrome://remote/content/shared/ChallengeHeaderParser.sys.mjs",
  parseURLPattern:
    "chrome://remote/content/shared/webdriver/URLPattern.sys.mjs",
  TabManager: "chrome://remote/content/shared/TabManager.sys.mjs",
  WindowGlobalMessageHandler:
    "chrome://remote/content/shared/messagehandler/WindowGlobalMessageHandler.sys.mjs",
});

/**
 * @typedef {object} AuthChallenge
 * @property {string} scheme
 * @property {string} realm
 */

/**
 * @typedef {object} AuthCredentials
 * @property {'password'} type
 * @property {string} username
 * @property {string} password
 */

/**
 * @typedef {object} BaseParameters
 * @property {string=} context
 * @property {Array<string>?} intercepts
 * @property {boolean} isBlocked
 * @property {Navigation=} navigation
 * @property {number} redirectCount
 * @property {RequestData} request
 * @property {number} timestamp
 */

/**
 * @typedef {object} BlockedRequest
 * @property {NetworkEventRecord} networkEventRecord
 * @property {InterceptPhase} phase
 */

/**
 * Enum of possible BytesValue types.
 *
 * @readonly
 * @enum {BytesValueType}
 */
export const BytesValueType = {
  Base64: "base64",
  String: "string",
};

/**
 * @typedef {object} BytesValue
 * @property {BytesValueType} type
 * @property {string} value
 */

/**
 * Enum of possible continueWithAuth actions.
 *
 * @readonly
 * @enum {ContinueWithAuthAction}
 */
const ContinueWithAuthAction = {
  Cancel: "cancel",
  Default: "default",
  ProvideCredentials: "provideCredentials",
};

/**
 * @typedef {object} Cookie
 * @property {string} domain
 * @property {number=} expires
 * @property {boolean} httpOnly
 * @property {string} name
 * @property {string} path
 * @property {SameSite} sameSite
 * @property {boolean} secure
 * @property {number} size
 * @property {BytesValue} value
 */

/**
 * @typedef {object} CookieHeader
 * @property {string} name
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
 * Enum of intercept phases.
 *
 * @readonly
 * @enum {InterceptPhase}
 */
const InterceptPhase = {
  AuthRequired: "authRequired",
  BeforeRequestSent: "beforeRequestSent",
  ResponseStarted: "responseStarted",
};

/**
 * @typedef {object} InterceptProperties
 * @property {Array<InterceptPhase>} phases
 * @property {Array<URLPattern>} urlPatterns
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
 * @property {Array<AuthChallenge>=} authChallenges
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

/**
 * Enum of possible sameSite values.
 *
 * @readonly
 * @enum {SameSite}
 */
const SameSite = {
  Lax: "lax",
  None: "none",
  Script: "script",
};

/**
 * @typedef {object} SetCookieHeader
 * @property {string} name
 * @property {BytesValue} value
 * @property {string=} domain
 * @property {boolean=} httpOnly
 * @property {string=} expiry
 * @property {number=} maxAge
 * @property {string=} path
 * @property {SameSite=} sameSite
 * @property {boolean=} secure
 */

/**
 * @typedef {object} URLPatternPattern
 * @property {'pattern'} type
 * @property {string=} protocol
 * @property {string=} hostname
 * @property {string=} port
 * @property {string=} pathname
 * @property {string=} search
 */

/**
 * @typedef {object} URLPatternString
 * @property {'string'} type
 * @property {string} pattern
 */

/**
 * @typedef {(URLPatternPattern|URLPatternString)} URLPattern
 */

/* eslint-disable jsdoc/valid-types */
/**
 * Parameters for the ResponseCompleted event
 *
 * @typedef {BaseParameters & ResponseCompletedParametersProperties} ResponseCompletedParameters
 */
/* eslint-enable jsdoc/valid-types */

class NetworkModule extends Module {
  #blockedRequests;
  #interceptMap;
  #networkListener;
  #subscribedEvents;

  constructor(messageHandler) {
    super(messageHandler);

    // Map of request id to BlockedRequest
    this.#blockedRequests = new Map();

    // Map of intercept id to InterceptProperties
    this.#interceptMap = new Map();

    // Set of event names which have active subscriptions
    this.#subscribedEvents = new Set();

    this.#networkListener = new lazy.NetworkListener();
    this.#networkListener.on("auth-required", this.#onAuthRequired);
    this.#networkListener.on("before-request-sent", this.#onBeforeRequestSent);
    this.#networkListener.on("fetch-error", this.#onFetchError);
    this.#networkListener.on("response-completed", this.#onResponseEvent);
    this.#networkListener.on("response-started", this.#onResponseEvent);
  }

  destroy() {
    this.#networkListener.off("auth-required", this.#onAuthRequired);
    this.#networkListener.off("before-request-sent", this.#onBeforeRequestSent);
    this.#networkListener.off("fetch-error", this.#onFetchError);
    this.#networkListener.off("response-completed", this.#onResponseEvent);
    this.#networkListener.off("response-started", this.#onResponseEvent);
    this.#networkListener.destroy();

    this.#blockedRequests = null;
    this.#interceptMap = null;
    this.#subscribedEvents = null;
  }

  /**
   * Adds a network intercept, which allows to intercept and modify network
   * requests and responses.
   *
   * The network intercept will be created for the provided phases
   * (InterceptPhase) and for specific url patterns. When a network event
   * corresponding to an intercept phase has a URL which matches any url pattern
   * of any intercept, the request will be suspended.
   *
   * @param {object=} options
   * @param {Array<string>=} options.contexts
   *     The list of browsing context ids where this intercept should be used.
   *     Optional, defaults to null.
   * @param {Array<InterceptPhase>} options.phases
   *     The phases where this intercept should be checked.
   * @param {Array<URLPattern>=} options.urlPatterns
   *     The URL patterns for this intercept. Optional, defaults to empty array.
   *
   * @returns {object}
   *     An object with the following property:
   *     - intercept {string} The unique id of the network intercept.
   *
   * @throws {InvalidArgumentError}
   *     Raised if an argument is of an invalid type or value.
   */
  addIntercept(options = {}) {
    const { contexts = null, phases, urlPatterns = [] } = options;

    if (contexts !== null) {
      lazy.assert.array(
        contexts,
        `Expected "contexts" to be an array, got ${contexts}`
      );

      if (!options.contexts.length) {
        throw new lazy.error.InvalidArgumentError(
          `Expected "contexts" to contain at least one item, got an empty array`
        );
      }

      for (const contextId of contexts) {
        lazy.assert.string(
          contextId,
          `Expected elements of "contexts" to be a string, got ${contextId}`
        );
        const context = this.#getBrowsingContext(contextId);

        if (context.parent) {
          throw new lazy.error.InvalidArgumentError(
            `Context with id ${contextId} is not a top-level browsing context`
          );
        }
      }
    }

    lazy.assert.array(
      phases,
      `Expected "phases" to be an array, got ${phases}`
    );

    if (!options.phases.length) {
      throw new lazy.error.InvalidArgumentError(
        `Expected "phases" to contain at least one phase, got an empty array`
      );
    }

    const supportedInterceptPhases = Object.values(InterceptPhase);
    for (const phase of phases) {
      if (!supportedInterceptPhases.includes(phase)) {
        throw new lazy.error.InvalidArgumentError(
          `Expected "phases" values to be one of ${supportedInterceptPhases}, got ${phase}`
        );
      }
    }

    lazy.assert.array(
      urlPatterns,
      `Expected "urlPatterns" to be an array, got ${urlPatterns}`
    );

    const parsedPatterns = urlPatterns.map(urlPattern =>
      lazy.parseURLPattern(urlPattern)
    );

    const interceptId = lazy.generateUUID();
    this.#interceptMap.set(interceptId, {
      contexts,
      phases,
      urlPatterns: parsedPatterns,
    });

    return {
      intercept: interceptId,
    };
  }

  /**
   * Continues a request that is blocked by a network intercept at the
   * beforeRequestSent phase.
   *
   * @param {object=} options
   * @param {string} options.request
   *     The id of the blocked request that should be continued.
   * @param {BytesValue=} options.body [unsupported]
   *     Optional BytesValue to replace the body of the request.
   * @param {Array<CookieHeader>=} options.cookies [unsupported]
   *     Optional array of cookie header values to replace the cookie header of
   *     the request.
   * @param {Array<Header>=} options.headers [unsupported]
   *     Optional array of headers to replace the headers of the request.
   *     request.
   * @param {string=} options.method [unsupported]
   *     Optional string to replace the method of the request.
   * @param {string=} options.url [unsupported]
   *     Optional string to replace the url of the request. If the provided url
   *     is not a valid URL, an InvalidArgumentError will be thrown.
   *
   * @throws {InvalidArgumentError}
   *     Raised if an argument is of an invalid type or value.
   * @throws {NoSuchRequestError}
   *     Raised if the request id does not match any request in the blocked
   *     requests map.
   */
  async continueRequest(options = {}) {
    const {
      body = null,
      cookies = null,
      headers = null,
      method = null,
      url = null,
      request: requestId,
    } = options;

    lazy.assert.string(
      requestId,
      `Expected "request" to be a string, got ${requestId}`
    );

    if (body !== null) {
      this.#assertBytesValue(
        body,
        `Expected "body" to be a network.BytesValue, got ${body}`
      );

      throw new lazy.error.UnsupportedOperationError(
        `"body" not supported yet in network.continueRequest`
      );
    }

    if (cookies !== null) {
      lazy.assert.array(
        cookies,
        `Expected "cookies" to be an array got ${cookies}`
      );

      for (const cookie of cookies) {
        this.#assertHeader(
          cookie,
          `Expected values in "cookies" to be network.CookieHeader, got ${cookie}`
        );
      }

      throw new lazy.error.UnsupportedOperationError(
        `"cookies" not supported yet in network.continueRequest`
      );
    }

    if (headers !== null) {
      lazy.assert.array(
        headers,
        `Expected "headers" to be an array got ${headers}`
      );

      for (const header of headers) {
        this.#assertHeader(
          header,
          `Expected values in "headers" to be network.Header, got ${header}`
        );
      }

      throw new lazy.error.UnsupportedOperationError(
        `"headers" not supported yet in network.continueRequest`
      );
    }

    if (method !== null) {
      lazy.assert.string(
        method,
        `Expected "method" to be a string, got ${method}`
      );

      throw new lazy.error.UnsupportedOperationError(
        `"method" not supported yet in network.continueRequest`
      );
    }

    if (url !== null) {
      lazy.assert.string(url, `Expected "url" to be a string, got ${url}`);

      throw new lazy.error.UnsupportedOperationError(
        `"url" not supported yet in network.continueRequest`
      );
    }

    if (!this.#blockedRequests.has(requestId)) {
      throw new lazy.error.NoSuchRequestError(
        `Blocked request with id ${requestId} not found`
      );
    }

    const { phase, request, resolveBlockedEvent } =
      this.#blockedRequests.get(requestId);

    if (phase !== InterceptPhase.BeforeRequestSent) {
      throw new lazy.error.InvalidArgumentError(
        `Expected blocked request to be in "beforeRequestSent" phase, got ${phase}`
      );
    }

    const wrapper = ChannelWrapper.get(request);
    wrapper.resume();

    resolveBlockedEvent();
  }

  /**
   * Continues a response that is blocked by a network intercept at the
   * responseStarted or authRequired phase.
   *
   * @param {object=} options
   * @param {string} options.request
   *     The id of the blocked request that should be continued.
   * @param {Array<SetCookieHeader>=} options.cookies [unsupported]
   *     Optional array of set-cookie header values to replace the set-cookie
   *     headers of the response.
   * @param {AuthCredentials=} options.credentials
   *     Optional AuthCredentials to use.
   * @param {Array<Header>=} options.headers [unsupported]
   *     Optional array of header values to replace the headers of the response.
   * @param {string=} options.reasonPhrase [unsupported]
   *     Optional string to replace the status message of the response.
   * @param {number=} options.statusCode [unsupported]
   *     Optional number to replace the status code of the response.
   *
   * @throws {InvalidArgumentError}
   *     Raised if an argument is of an invalid type or value.
   * @throws {NoSuchRequestError}
   *     Raised if the request id does not match any request in the blocked
   *     requests map.
   */
  async continueResponse(options = {}) {
    const {
      cookies = null,
      credentials = null,
      headers = null,
      reasonPhrase = null,
      request: requestId,
      statusCode = null,
    } = options;

    lazy.assert.string(
      requestId,
      `Expected "request" to be a string, got ${requestId}`
    );

    if (cookies !== null) {
      lazy.assert.array(
        cookies,
        `Expected "cookies" to be an array got ${cookies}`
      );

      for (const cookie of cookies) {
        this.#assertSetCookieHeader(cookie);
      }

      throw new lazy.error.UnsupportedOperationError(
        `"cookies" not supported yet in network.continueResponse`
      );
    }

    if (credentials !== null) {
      this.#assertAuthCredentials(credentials);
    }

    if (headers !== null) {
      lazy.assert.array(
        headers,
        `Expected "headers" to be an array got ${headers}`
      );

      for (const header of headers) {
        this.#assertHeader(
          header,
          `Expected values in "headers" to be network.Header, got ${header}`
        );
      }

      throw new lazy.error.UnsupportedOperationError(
        `"headers" not supported yet in network.continueResponse`
      );
    }

    if (reasonPhrase !== null) {
      lazy.assert.string(
        reasonPhrase,
        `Expected "reasonPhrase" to be a string, got ${reasonPhrase}`
      );

      throw new lazy.error.UnsupportedOperationError(
        `"reasonPhrase" not supported yet in network.continueResponse`
      );
    }

    if (statusCode !== null) {
      lazy.assert.positiveInteger(
        statusCode,
        `Expected "statusCode" to be a positive integer, got ${statusCode}`
      );

      throw new lazy.error.UnsupportedOperationError(
        `"statusCode" not supported yet in network.continueResponse`
      );
    }

    if (!this.#blockedRequests.has(requestId)) {
      throw new lazy.error.NoSuchRequestError(
        `Blocked request with id ${requestId} not found`
      );
    }

    const { authCallbacks, phase, request, resolveBlockedEvent } =
      this.#blockedRequests.get(requestId);

    if (
      phase !== InterceptPhase.ResponseStarted &&
      phase !== InterceptPhase.AuthRequired
    ) {
      throw new lazy.error.InvalidArgumentError(
        `Expected blocked request to be in "responseStarted" or "authRequired" phase, got ${phase}`
      );
    }

    if (phase === InterceptPhase.AuthRequired) {
      // Requests blocked in the AuthRequired phase should be resumed using
      // authCallbacks.
      if (credentials !== null) {
        await authCallbacks.provideAuthCredentials(
          credentials.username,
          credentials.password
        );
      } else {
        await authCallbacks.provideAuthCredentials();
      }
    } else {
      const wrapper = ChannelWrapper.get(request);
      wrapper.resume();
    }

    resolveBlockedEvent();
  }

  /**
   * Continues a response that is blocked by a network intercept at the
   * authRequired phase.
   *
   * @param {object=} options
   * @param {string} options.request
   *     The id of the blocked request that should be continued.
   * @param {string} options.action
   *     The continueWithAuth action, one of ContinueWithAuthAction.
   * @param {AuthCredentials=} options.credentials
   *     The credentials to use for the ContinueWithAuthAction.ProvideCredentials
   *     action.
   *
   * @throws {InvalidArgumentError}
   *     Raised if an argument is of an invalid type or value.
   * @throws {NoSuchRequestError}
   *     Raised if the request id does not match any request in the blocked
   *     requests map.
   */
  async continueWithAuth(options = {}) {
    const { action, credentials, request: requestId } = options;

    lazy.assert.string(
      requestId,
      `Expected "request" to be a string, got ${requestId}`
    );

    if (!Object.values(ContinueWithAuthAction).includes(action)) {
      throw new lazy.error.InvalidArgumentError(
        `Expected "action" to be one of ${Object.values(
          ContinueWithAuthAction
        )} got ${action}`
      );
    }

    if (action == ContinueWithAuthAction.ProvideCredentials) {
      this.#assertAuthCredentials(credentials);
    }

    if (!this.#blockedRequests.has(requestId)) {
      throw new lazy.error.NoSuchRequestError(
        `Blocked request with id ${requestId} not found`
      );
    }

    const { authCallbacks, phase, resolveBlockedEvent } =
      this.#blockedRequests.get(requestId);

    if (phase !== InterceptPhase.AuthRequired) {
      throw new lazy.error.InvalidArgumentError(
        `Expected blocked request to be in "authRequired" phase, got ${phase}`
      );
    }

    switch (action) {
      case ContinueWithAuthAction.Cancel: {
        authCallbacks.cancelAuthPrompt();
        break;
      }
      case ContinueWithAuthAction.Default: {
        authCallbacks.forwardAuthPrompt();
        break;
      }
      case ContinueWithAuthAction.ProvideCredentials: {
        await authCallbacks.provideAuthCredentials(
          credentials.username,
          credentials.password
        );

        break;
      }
    }

    resolveBlockedEvent();
  }

  /**
   * Fails a request that is blocked by a network intercept.
   *
   * @param {object=} options
   * @param {string} options.request
   *     The id of the blocked request that should be continued.
   *
   * @throws {InvalidArgumentError}
   *     Raised if an argument is of an invalid type or value.
   * @throws {NoSuchRequestError}
   *     Raised if the request id does not match any request in the blocked
   *     requests map.
   */
  async failRequest(options = {}) {
    const { request: requestId } = options;

    lazy.assert.string(
      requestId,
      `Expected "request" to be a string, got ${requestId}`
    );

    if (!this.#blockedRequests.has(requestId)) {
      throw new lazy.error.NoSuchRequestError(
        `Blocked request with id ${requestId} not found`
      );
    }

    const { phase, request, resolveBlockedEvent } =
      this.#blockedRequests.get(requestId);

    if (phase === InterceptPhase.AuthRequired) {
      throw new lazy.error.InvalidArgumentError(
        `Expected blocked request not to be in "authRequired" phase`
      );
    }

    const wrapper = ChannelWrapper.get(request);
    wrapper.resume();
    wrapper.cancel(
      Cr.NS_ERROR_ABORT,
      Ci.nsILoadInfo.BLOCKING_REASON_WEBDRIVER_BIDI
    );

    resolveBlockedEvent();
  }

  /**
   * Continues a request that’s blocked by a network intercept, by providing a
   * complete response.
   *
   * @param {object=} options
   * @param {string} options.request
   *     The id of the blocked request for which the response should be
   *     provided.
   * @param {BytesValue=} options.body [unsupported]
   *     Optional BytesValue to replace the body of the response.
   * @param {Array<SetCookieHeader>=} options.cookies [unsupported]
   *     Optional array of set-cookie header values to use for the provided
   *     response.
   * @param {Array<Header>=} options.headers [unsupported]
   *     Optional array of header values to use for the provided
   *     response.
   * @param {string=} options.reasonPhrase [unsupported]
   *     Optional string to use as the status message for the provided response.
   * @param {number=} options.statusCode [unsupported]
   *     Optional number to use as the status code for the provided response.
   *
   * @throws {InvalidArgumentError}
   *     Raised if an argument is of an invalid type or value.
   * @throws {NoSuchRequestError}
   *     Raised if the request id does not match any request in the blocked
   *     requests map.
   */
  async provideResponse(options = {}) {
    const {
      body = null,
      cookies = null,
      headers = null,
      reasonPhrase = null,
      request: requestId,
      statusCode = null,
    } = options;

    lazy.assert.string(
      requestId,
      `Expected "request" to be a string, got ${requestId}`
    );

    if (body !== null) {
      this.#assertBytesValue(
        body,
        `Expected "body" to be a network.BytesValue, got ${body}`
      );

      throw new lazy.error.UnsupportedOperationError(
        `"body" not supported yet in network.provideResponse`
      );
    }

    if (cookies !== null) {
      lazy.assert.array(
        cookies,
        `Expected "cookies" to be an array got ${cookies}`
      );

      for (const cookie of cookies) {
        this.#assertSetCookieHeader(cookie);
      }

      throw new lazy.error.UnsupportedOperationError(
        `"cookies" not supported yet in network.provideResponse`
      );
    }

    if (headers !== null) {
      lazy.assert.array(
        headers,
        `Expected "headers" to be an array got ${headers}`
      );

      for (const header of headers) {
        this.#assertHeader(
          header,
          `Expected values in "headers" to be network.Header, got ${header}`
        );
      }

      throw new lazy.error.UnsupportedOperationError(
        `"headers" not supported yet in network.provideResponse`
      );
    }

    if (reasonPhrase !== null) {
      lazy.assert.string(
        reasonPhrase,
        `Expected "reasonPhrase" to be a string, got ${reasonPhrase}`
      );

      throw new lazy.error.UnsupportedOperationError(
        `"reasonPhrase" not supported yet in network.provideResponse`
      );
    }

    if (statusCode !== null) {
      lazy.assert.positiveInteger(
        statusCode,
        `Expected "statusCode" to be a positive integer, got ${statusCode}`
      );

      throw new lazy.error.UnsupportedOperationError(
        `"statusCode" not supported yet in network.provideResponse`
      );
    }

    if (!this.#blockedRequests.has(requestId)) {
      throw new lazy.error.NoSuchRequestError(
        `Blocked request with id ${requestId} not found`
      );
    }

    const { authCallbacks, phase, request, resolveBlockedEvent } =
      this.#blockedRequests.get(requestId);

    if (phase === InterceptPhase.AuthRequired) {
      await authCallbacks.provideAuthCredentials();
    } else {
      const wrapper = ChannelWrapper.get(request);
      wrapper.resume();
    }

    resolveBlockedEvent();
  }

  /**
   * Removes an existing network intercept.
   *
   * @param {object=} options
   * @param {string} options.intercept
   *     The id of the intercept to remove.
   *
   * @throws {InvalidArgumentError}
   *     Raised if an argument is of an invalid type or value.
   * @throws {NoSuchInterceptError}
   *     Raised if the intercept id could not be found in the internal intercept
   *     map.
   */
  removeIntercept(options = {}) {
    const { intercept } = options;

    lazy.assert.string(
      intercept,
      `Expected "intercept" to be a string, got ${intercept}`
    );

    if (!this.#interceptMap.has(intercept)) {
      throw new lazy.error.NoSuchInterceptError(
        `Network intercept with id ${intercept} not found`
      );
    }

    this.#interceptMap.delete(intercept);
  }

  /**
   * Add a new request in the blockedRequests map.
   *
   * @param {string} requestId
   *     The request id.
   * @param {InterceptPhase} phase
   *     The phase where the request is blocked.
   * @param {object=} options
   * @param {object=} options.authCallbacks
   *     Only defined for requests blocked in the authRequired phase.
   *     Provides callbacks to handle the authentication.
   * @param {nsIChannel=} options.requestChannel
   *     The request channel.
   * @param {nsIChannel=} options.responseChannel
   *     The response channel.
   */
  #addBlockedRequest(requestId, phase, options = {}) {
    const {
      authCallbacks,
      requestChannel: request,
      responseChannel: response,
    } = options;
    const { promise: blockedEventPromise, resolve: resolveBlockedEvent } =
      Promise.withResolvers();

    this.#blockedRequests.set(requestId, {
      authCallbacks,
      request,
      response,
      resolveBlockedEvent,
      phase,
    });

    blockedEventPromise.finally(() => {
      this.#blockedRequests.delete(requestId);
    });
  }

  #assertAuthCredentials(credentials) {
    lazy.assert.object(
      credentials,
      `Expected "credentials" to be an object, got ${credentials}`
    );

    if (credentials.type !== "password") {
      throw new lazy.error.InvalidArgumentError(
        `Expected credentials "type" to be "password" got ${credentials.type}`
      );
    }

    lazy.assert.string(
      credentials.username,
      `Expected credentials "username" to be a string, got ${credentials.username}`
    );
    lazy.assert.string(
      credentials.password,
      `Expected credentials "password" to be a string, got ${credentials.password}`
    );
  }

  #assertBytesValue(obj, msg) {
    lazy.assert.object(obj, msg);
    lazy.assert.string(obj.value, msg);
    lazy.assert.in(obj.type, Object.values(BytesValueType), msg);
  }

  #assertHeader(value, msg) {
    lazy.assert.object(value, msg);
    lazy.assert.string(value.name, msg);
    this.#assertBytesValue(value.value, msg);
  }

  #assertSetCookieHeader(setCookieHeader) {
    lazy.assert.object(
      setCookieHeader,
      `Expected set-cookie header to be an object, got ${setCookieHeader}`
    );

    const {
      name,
      value,
      domain = null,
      httpOnly = null,
      expiry = null,
      maxAge = null,
      path = null,
      sameSite = null,
      secure = null,
    } = setCookieHeader;

    lazy.assert.string(
      name,
      `Expected set-cookie header "name" to be a string, got ${name}`
    );

    this.#assertBytesValue(
      value,
      `Expected set-cookie header "value" to be a BytesValue, got ${name}`
    );

    if (domain !== null) {
      lazy.assert.string(
        domain,
        `Expected set-cookie header "domain" to be a string, got ${domain}`
      );
    }
    if (httpOnly !== null) {
      lazy.assert.boolean(
        httpOnly,
        `Expected set-cookie header "httpOnly" to be a boolean, got ${httpOnly}`
      );
    }
    if (expiry !== null) {
      lazy.assert.string(
        expiry,
        `Expected set-cookie header "expiry" to be a string, got ${expiry}`
      );
    }
    if (maxAge !== null) {
      lazy.assert.integer(
        maxAge,
        `Expected set-cookie header "maxAge" to be an integer, got ${maxAge}`
      );
    }
    if (path !== null) {
      lazy.assert.string(
        path,
        `Expected set-cookie header "path" to be a string, got ${path}`
      );
    }
    if (sameSite !== null) {
      lazy.assert.in(
        sameSite,
        Object.values(SameSite),
        `Expected set-cookie header "sameSite" to be one of ${Object.values(
          SameSite
        )}, got ${sameSite}`
      );
    }
    if (secure !== null) {
      lazy.assert.boolean(
        secure,
        `Expected set-cookie header "secure" to be a boolean, got ${secure}`
      );
    }
  }

  #extractChallenges(responseData) {
    let headerName;

    // Using case-insensitive match for header names, so we use the lowercase
    // version of the "WWW-Authenticate" / "Proxy-Authenticate" strings.
    if (responseData.status === 401) {
      headerName = "www-authenticate";
    } else if (responseData.status === 407) {
      headerName = "proxy-authenticate";
    } else {
      return null;
    }

    const challenges = [];

    for (const header of responseData.headers) {
      if (header.name.toLowerCase() === headerName) {
        // A single header can contain several challenges.
        const headerChallenges = lazy.parseChallengeHeader(header.value);
        for (const headerChallenge of headerChallenges) {
          const realmParam = headerChallenge.params.find(
            param => param.name == "realm"
          );
          const realm = realmParam ? realmParam.value : undefined;
          const challenge = {
            scheme: headerChallenge.scheme,
            realm,
          };
          challenges.push(challenge);
        }
      }
    }

    return challenges;
  }

  #getBrowsingContext(contextId) {
    const context = lazy.TabManager.getBrowsingContextById(contextId);
    if (context === null) {
      throw new lazy.error.NoSuchFrameError(
        `Browsing Context with id ${contextId} not found`
      );
    }

    if (!context.currentWindowGlobal) {
      throw new lazy.error.NoSuchFrameError(
        `No window found for BrowsingContext with id ${contextId}`
      );
    }

    return context;
  }

  #getContextInfo(browsingContext) {
    return {
      contextId: browsingContext.id,
      type: lazy.WindowGlobalMessageHandler.type,
    };
  }

  #getNetworkIntercepts(event, requestData, contextId) {
    const intercepts = [];

    let phase;
    switch (event) {
      case "network.beforeRequestSent":
        phase = InterceptPhase.BeforeRequestSent;
        break;
      case "network.responseStarted":
        phase = InterceptPhase.ResponseStarted;
        break;
      case "network.authRequired":
        phase = InterceptPhase.AuthRequired;
        break;
      case "network.responseCompleted":
        // The network.responseCompleted event does not match any interception
        // phase. Return immediately.
        return intercepts;
    }

    // Retrieve the top browsing context id for this network event.
    const browsingContext = lazy.TabManager.getBrowsingContextById(contextId);
    const topLevelContextId = lazy.TabManager.getIdForBrowsingContext(
      browsingContext.top
    );

    const url = requestData.url;
    for (const [interceptId, intercept] of this.#interceptMap) {
      if (
        intercept.contexts !== null &&
        !intercept.contexts.includes(topLevelContextId)
      ) {
        // Skip this intercept if the event's context does not match the list
        // of contexts for this intercept.
        continue;
      }

      if (intercept.phases.includes(phase)) {
        const urlPatterns = intercept.urlPatterns;
        if (
          !urlPatterns.length ||
          urlPatterns.some(pattern => lazy.matchURLPattern(pattern, url))
        ) {
          intercepts.push(interceptId);
        }
      }
    }

    return intercepts;
  }

  #getNavigationId(eventName, isNavigationRequest, browsingContext, url) {
    if (!isNavigationRequest) {
      // Not a navigation request return null.
      return null;
    }

    let navigation =
      this.messageHandler.navigationManager.getNavigationForBrowsingContext(
        browsingContext
      );

    // `onBeforeRequestSent` might be too early for the NavigationManager.
    // If there is no ongoing navigation, create one ourselves.
    // TODO: Bug 1835704 to detect navigations earlier and avoid this.
    if (
      eventName === "network.beforeRequestSent" &&
      (!navigation || navigation.finished)
    ) {
      navigation = lazy.notifyNavigationStarted({
        contextDetails: { context: browsingContext },
        url,
      });
    }

    return navigation ? navigation.navigationId : null;
  }

  #getSuspendMarkerText(requestData, phase) {
    return `Request (id: ${requestData.request}) suspended by WebDriver BiDi in ${phase} phase`;
  }

  #onAuthRequired = (name, data) => {
    const {
      authCallbacks,
      contextId,
      isNavigationRequest,
      redirectCount,
      requestChannel,
      requestData,
      responseChannel,
      responseData,
      timestamp,
    } = data;

    let isBlocked = false;
    try {
      const browsingContext = lazy.TabManager.getBrowsingContextById(contextId);
      if (!browsingContext) {
        // Do not emit events if the context id does not match any existing
        // browsing context.
        return;
      }

      const protocolEventName = "network.authRequired";

      // Process the navigation to create potentially missing navigation ids
      // before the early return below.
      const navigation = this.#getNavigationId(
        protocolEventName,
        isNavigationRequest,
        browsingContext,
        requestData.url
      );

      const isListening = this.messageHandler.eventsDispatcher.hasListener(
        protocolEventName,
        { contextId }
      );
      if (!isListening) {
        // If there are no listeners subscribed to this event and this context,
        // bail out.
        return;
      }

      const baseParameters = this.#processNetworkEvent(protocolEventName, {
        contextId,
        navigation,
        redirectCount,
        requestData,
        timestamp,
      });

      const authRequiredEvent = this.#serializeNetworkEvent({
        ...baseParameters,
        response: responseData,
      });

      const authChallenges = this.#extractChallenges(responseData);
      // authChallenges should never be null for a request which triggered an
      // authRequired event.
      authRequiredEvent.response.authChallenges = authChallenges;

      this.emitEvent(
        protocolEventName,
        authRequiredEvent,
        this.#getContextInfo(browsingContext)
      );

      if (authRequiredEvent.isBlocked) {
        isBlocked = true;

        // requestChannel.suspend() is not needed here because the request is
        // already blocked on the authentication prompt notification until
        // one of the authCallbacks is called.
        this.#addBlockedRequest(
          authRequiredEvent.request.request,
          InterceptPhase.AuthRequired,
          {
            authCallbacks,
            requestChannel,
            responseChannel,
          }
        );
      }
    } finally {
      if (!isBlocked) {
        // If the request was not blocked, forward the auth prompt notification
        // to the next consumer.
        authCallbacks.forwardAuthPrompt();
      }
    }
  };

  #onBeforeRequestSent = (name, data) => {
    const {
      contextId,
      isNavigationRequest,
      redirectCount,
      requestChannel,
      requestData,
      timestamp,
    } = data;

    const browsingContext = lazy.TabManager.getBrowsingContextById(contextId);
    if (!browsingContext) {
      // Do not emit events if the context id does not match any existing
      // browsing context.
      return;
    }

    const internalEventName = "network._beforeRequestSent";
    const protocolEventName = "network.beforeRequestSent";

    // Process the navigation to create potentially missing navigation ids
    // before the early return below.
    const navigation = this.#getNavigationId(
      protocolEventName,
      isNavigationRequest,
      browsingContext,
      requestData.url
    );

    // Always emit internal events, they are used to support the browsingContext
    // navigate command.
    // Bug 1861922: Replace internal events with a Network listener helper
    // directly using the NetworkObserver.
    this.emitEvent(
      internalEventName,
      {
        navigation,
        url: requestData.url,
      },
      this.#getContextInfo(browsingContext)
    );

    const isListening = this.messageHandler.eventsDispatcher.hasListener(
      protocolEventName,
      { contextId }
    );
    if (!isListening) {
      // If there are no listeners subscribed to this event and this context,
      // bail out.
      return;
    }

    const baseParameters = this.#processNetworkEvent(protocolEventName, {
      contextId,
      navigation,
      redirectCount,
      requestData,
      timestamp,
    });

    // Bug 1805479: Handle the initiator, including stacktrace details.
    const initiator = {
      type: InitiatorType.Other,
    };

    const beforeRequestSentEvent = this.#serializeNetworkEvent({
      ...baseParameters,
      initiator,
    });

    this.emitEvent(
      protocolEventName,
      beforeRequestSentEvent,
      this.#getContextInfo(browsingContext)
    );

    if (beforeRequestSentEvent.isBlocked) {
      // TODO: Requests suspended in beforeRequestSent still reach the server at
      // the moment. https://bugzilla.mozilla.org/show_bug.cgi?id=1849686
      const wrapper = ChannelWrapper.get(requestChannel);
      wrapper.suspend(
        this.#getSuspendMarkerText(requestData, "beforeRequestSent")
      );

      this.#addBlockedRequest(
        beforeRequestSentEvent.request.request,
        InterceptPhase.BeforeRequestSent,
        {
          requestChannel,
        }
      );
    }
  };

  #onFetchError = (name, data) => {
    const {
      contextId,
      errorText,
      isNavigationRequest,
      redirectCount,
      requestData,
      timestamp,
    } = data;

    const browsingContext = lazy.TabManager.getBrowsingContextById(contextId);
    if (!browsingContext) {
      // Do not emit events if the context id does not match any existing
      // browsing context.
      return;
    }

    const internalEventName = "network._fetchError";
    const protocolEventName = "network.fetchError";

    // Process the navigation to create potentially missing navigation ids
    // before the early return below.
    const navigation = this.#getNavigationId(
      protocolEventName,
      isNavigationRequest,
      browsingContext,
      requestData.url
    );

    // Always emit internal events, they are used to support the browsingContext
    // navigate command.
    // Bug 1861922: Replace internal events with a Network listener helper
    // directly using the NetworkObserver.
    this.emitEvent(
      internalEventName,
      {
        navigation,
        url: requestData.url,
      },
      this.#getContextInfo(browsingContext)
    );

    const isListening = this.messageHandler.eventsDispatcher.hasListener(
      protocolEventName,
      { contextId }
    );
    if (!isListening) {
      // If there are no listeners subscribed to this event and this context,
      // bail out.
      return;
    }

    const baseParameters = this.#processNetworkEvent(protocolEventName, {
      contextId,
      navigation,
      redirectCount,
      requestData,
      timestamp,
    });

    const fetchErrorEvent = this.#serializeNetworkEvent({
      ...baseParameters,
      errorText,
    });

    this.emitEvent(
      protocolEventName,
      fetchErrorEvent,
      this.#getContextInfo(browsingContext)
    );
  };

  #onResponseEvent = (name, data) => {
    const {
      contextId,
      isNavigationRequest,
      redirectCount,
      requestChannel,
      requestData,
      responseChannel,
      responseData,
      timestamp,
    } = data;

    const browsingContext = lazy.TabManager.getBrowsingContextById(contextId);
    if (!browsingContext) {
      // Do not emit events if the context id does not match any existing
      // browsing context.
      return;
    }

    const protocolEventName =
      name === "response-started"
        ? "network.responseStarted"
        : "network.responseCompleted";

    const internalEventName =
      name === "response-started"
        ? "network._responseStarted"
        : "network._responseCompleted";

    // Process the navigation to create potentially missing navigation ids
    // before the early return below.
    const navigation = this.#getNavigationId(
      protocolEventName,
      isNavigationRequest,
      browsingContext,
      requestData.url
    );

    // Always emit internal events, they are used to support the browsingContext
    // navigate command.
    // Bug 1861922: Replace internal events with a Network listener helper
    // directly using the NetworkObserver.
    this.emitEvent(
      internalEventName,
      {
        navigation,
        url: requestData.url,
      },
      this.#getContextInfo(browsingContext)
    );

    const isListening = this.messageHandler.eventsDispatcher.hasListener(
      protocolEventName,
      { contextId }
    );
    if (!isListening) {
      // If there are no listeners subscribed to this event and this context,
      // bail out.
      return;
    }

    const baseParameters = this.#processNetworkEvent(protocolEventName, {
      contextId,
      navigation,
      redirectCount,
      requestData,
      timestamp,
    });

    const responseEvent = this.#serializeNetworkEvent({
      ...baseParameters,
      response: responseData,
    });

    const authChallenges = this.#extractChallenges(responseData);
    if (authChallenges !== null) {
      responseEvent.response.authChallenges = authChallenges;
    }

    this.emitEvent(
      protocolEventName,
      responseEvent,
      this.#getContextInfo(browsingContext)
    );

    if (
      protocolEventName === "network.responseStarted" &&
      responseEvent.isBlocked
    ) {
      const wrapper = ChannelWrapper.get(requestChannel);
      wrapper.suspend(
        this.#getSuspendMarkerText(requestData, "responseStarted")
      );

      this.#addBlockedRequest(
        responseEvent.request.request,
        InterceptPhase.ResponseStarted,
        {
          requestChannel,
          responseChannel,
        }
      );
    }
  };

  /**
   * Process the network event data for a given network event name and create
   * the corresponding base parameters.
   *
   * @param {string} eventName
   *     One of the supported network event names.
   * @param {object} data
   * @param {string} data.contextId
   *     The browsing context id for the network event.
   * @param {string|null} data.navigation
   *     The navigation id if this is a network event for a navigation request.
   * @param {number} data.redirectCount
   *     The redirect count for the network event.
   * @param {RequestData} data.requestData
   *     The network.RequestData information for the network event.
   * @param {number} data.timestamp
   *     The timestamp when the network event was created.
   */
  #processNetworkEvent(eventName, data) {
    const { contextId, navigation, redirectCount, requestData, timestamp } =
      data;
    const intercepts = this.#getNetworkIntercepts(
      eventName,
      requestData,
      contextId
    );
    const isBlocked = !!intercepts.length;

    const baseParameters = {
      context: contextId,
      isBlocked,
      navigation,
      redirectCount,
      request: requestData,
      timestamp,
    };

    if (isBlocked) {
      baseParameters.intercepts = intercepts;
    }

    return baseParameters;
  }

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
      "network.authRequired",
      "network.beforeRequestSent",
      "network.fetchError",
      "network.responseCompleted",
      "network.responseStarted",
    ];
  }
}

export const network = NetworkModule;
