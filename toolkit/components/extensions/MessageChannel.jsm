/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * This module provides wrappers around standard message managers to
 * simplify bidirectional communication. It currently allows a caller to
 * send a message to a single listener, and receive a reply. If there
 * are no matching listeners, or the message manager disconnects before
 * a reply is received, the caller is returned an error.
 *
 * The listener end may specify filters for the messages it wishes to
 * receive, and the sender end likewise may specify recipient tags to
 * match the filters.
 *
 * The message handler on the listener side may return its response
 * value directly, or may return a promise, the resolution or rejection
 * of which will be returned instead. The sender end likewise receives a
 * promise which resolves or rejects to the listener's response.
 *
 *
 * A basic setup works something like this:
 *
 * A content script adds a message listener to its global
 * nsIContentFrameMessageManager, with an appropriate set of filters:
 *
 *  {
 *    init(messageManager, window, extensionID) {
 *      this.window = window;
 *
 *      MessageChannel.addListener(
 *        messageManager, "ContentScript:TouchContent",
 *        this);
 *
 *      this.messageFilterStrict = {
 *        innerWindowID: getInnerWindowID(window),
 *        extensionID: extensionID,
 *      };
 *
 *      this.messageFilterPermissive = {
 *        outerWindowID: getOuterWindowID(window),
 *      };
 *    },
 *
 *    receiveMessage({ target, messageName, sender, recipient, data }) {
 *      if (messageName == "ContentScript:TouchContent") {
 *        return new Promise(resolve => {
 *          this.touchWindow(data.touchWith, result => {
 *            resolve({ touchResult: result });
 *          });
 *        });
 *      }
 *    },
 *  };
 *
 * A script in the parent process sends a message to the content process
 * via a tab message manager, including recipient tags to match its
 * filter, and an optional sender tag to identify itself:
 *
 *  let data = { touchWith: "pencil" };
 *  let sender = { extensionID, contextID };
 *  let recipient = { innerWindowID: tab.linkedBrowser.innerWindowID, extensionID };
 *
 *  MessageChannel.sendMessage(
 *    tab.linkedBrowser.messageManager, "ContentScript:TouchContent",
 *    data, {recipient, sender}
 *  ).then(result => {
 *    alert(result.touchResult);
 *  });
 *
 * Since the lifetimes of message senders and receivers may not always
 * match, either side of the message channel may cancel pending
 * responses which match its sender or recipient tags.
 *
 * For the above client, this might be done from an
 * inner-window-destroyed observer, when its target scope is destroyed:
 *
 *  observe(subject, topic, data) {
 *    if (topic == "inner-window-destroyed") {
 *      let innerWindowID = subject.QueryInterface(Ci.nsISupportsPRUint64).data;
 *
 *      MessageChannel.abortResponses({ innerWindowID });
 *    }
 *  },
 *
 * From the parent, it may be done when its context is being destroyed:
 *
 *  onDestroy() {
 *    MessageChannel.abortResponses({
 *      extensionID: this.extensionID,
 *      contextID: this.contextID,
 *    });
 *  },
 *
 */

this.EXPORTED_SYMBOLS = ["MessageChannel"];

/* globals MessageChannel */

const Ci = Components.interfaces;
const Cc = Components.classes;
const Cu = Components.utils;
const Cr = Components.results;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "PromiseUtils",
                                  "resource://gre/modules/PromiseUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Task",
                                  "resource://gre/modules/Task.jsm");


/**
 * Handles the mapping and dispatching of messages to their registered
 * handlers. There is one broker per message manager and class of
 * messages. Each class of messages is mapped to one native message
 * name, e.g., "MessageChannel:Message", and is dispatched to handlers
 * based on an internal message name, e.g., "Extension:ExecuteScript".
 */
class FilteringMessageManager {
  /**
   * @param {string} messageName
   *     The name of the native message this broker listens for.
   * @param {function} callback
   *     A function which is called for each message after it has been
   *     mapped to its handler. The function receives two arguments:
   *
   *       result:
   *         An object containing either a `handler` or an `error` property.
   *         If no error occurs, `handler` will be a matching handler that
   *         was registered by `addHandler`. Otherwise, the `error` property
   *         will contain an object describing the error.
   *
   *        data:
   *          An object describing the message, as defined in
   *          `MessageChannel.addListener`.
   * @param {nsIMessageManager} messageManager
   */
  constructor(messageName, callback, messageManager) {
    this.messageName = messageName;
    this.callback = callback;
    this.messageManager = messageManager;

    this.messageManager.addMessageListener(this.messageName, this);

    this.handlers = new Map();
  }

  /**
   * Receives a message from our message manager, maps it to a handler, and
   * passes the result to our message callback.
   */
  receiveMessage({data, target}) {
    let handlers = Array.from(this.getHandlers(data.messageName, data.recipient));

    data.target = target;
    this.callback(handlers, data);
  }

  /**
   * Iterates over all handlers for the given message name. If `recipient`
   * is provided, only iterates over handlers whose filters match it.
   *
   * @param {string|number} messageName
   *     The message for which to return handlers.
   * @param {object} recipient
   *     The recipient data on which to filter handlers.
   */
  * getHandlers(messageName, recipient) {
    let handlers = this.handlers.get(messageName) || new Set();
    for (let handler of handlers) {
      if (MessageChannel.matchesFilter(handler.messageFilterStrict || {}, recipient) &&
          MessageChannel.matchesFilter(handler.messageFilterPermissive || {}, recipient, false)) {
        yield handler;
      }
    }
  }

  /**
   * Registers a handler for the given message.
   *
   * @param {string} messageName
   *     The internal message name for which to register the handler.
   * @param {object} handler
   *     An opaque handler object. The object may have a
   *     `messageFilterStrict` and/or a `messageFilterPermissive`
   *     property on which to filter messages.
   *
   *     Final dispatching is handled by the message callback passed to
   *     the constructor.
   */
  addHandler(messageName, handler) {
    if (!this.handlers.has(messageName)) {
      this.handlers.set(messageName, new Set());
    }

    this.handlers.get(messageName).add(handler);
  }

  /**
   * Unregisters a handler for the given message.
   *
   * @param {string} messageName
   *     The internal message name for which to unregister the handler.
   * @param {object} handler
   *     The handler object to unregister.
   */
  removeHandler(messageName, handler) {
    this.handlers.get(messageName).delete(handler);
  }
}

/**
 * Manages mappings of message managers to their corresponding message
 * brokers. Brokers are lazily created for each message manager the
 * first time they are accessed. In the case of content frame message
 * managers, they are also automatically destroyed when the frame
 * unload event fires.
 */
class FilteringMessageManagerMap extends Map {
  // Unfortunately, we can't use a WeakMap for this, because message
  // managers do not support preserved wrappers.

  /**
   * @param {string} messageName
   *     The native message name passed to `FilteringMessageManager` constructors.
   * @param {function} callback
   *     The message callback function passed to
   *     `FilteringMessageManager` constructors.
   */
  constructor(messageName, callback) {
    super();

    this.messageName = messageName;
    this.callback = callback;
  }

  /**
   * Returns, and possibly creates, a message broker for the given
   * message manager.
   *
   * @param {nsIMessageSender} target
   *     The message manager for which to return a broker.
   *
   * @returns {FilteringMessageManager}
   */
  get(target) {
    if (this.has(target)) {
      return super.get(target);
    }

    let broker = new FilteringMessageManager(this.messageName, this.callback, target);
    this.set(target, broker);

    if (target instanceof Ci.nsIDOMEventTarget) {
      let onUnload = event => {
        target.removeEventListener("unload", onUnload);
        this.delete(target);
      };
      target.addEventListener("unload", onUnload);
    }

    return broker;
  }
}

const MESSAGE_MESSAGE = "MessageChannel:Message";
const MESSAGE_RESPONSE = "MessageChannel:Response";

let gChannelId = 0;

this.MessageChannel = {
  init() {
    Services.obs.addObserver(this, "message-manager-close", false);
    Services.obs.addObserver(this, "message-manager-disconnect", false);

    this.messageManagers = new FilteringMessageManagerMap(
      MESSAGE_MESSAGE, this._handleMessage.bind(this));

    this.responseManagers = new FilteringMessageManagerMap(
      MESSAGE_RESPONSE, this._handleResponse.bind(this));

    /**
     * Contains a list of pending responses, either waiting to be
     * received or waiting to be sent. @see _addPendingResponse
     */
    this.pendingResponses = new Set();
  },

  RESULT_SUCCESS: 0,
  RESULT_DISCONNECTED: 1,
  RESULT_NO_HANDLER: 2,
  RESULT_MULTIPLE_HANDLERS: 3,
  RESULT_ERROR: 4,
  RESULT_NO_RESPONSE: 5,

  REASON_DISCONNECTED: {
    result: this.RESULT_DISCONNECTED,
    message: "Message manager disconnected",
  },

  /**
   * Specifies that only a single listener matching the specified
   * recipient tag may be listening for the given message, at the other
   * end of the target message manager.
   *
   * If no matching listeners exist, a RESULT_NO_HANDLER error will be
   * returned. If multiple matching listeners exist, a
   * RESULT_MULTIPLE_HANDLERS error will be returned.
   */
  RESPONSE_SINGLE: 0,

  /**
   * If multiple message managers matching the specified recipient tag
   * are listening for a message, all listeners are notified, but only
   * the first response or error is returned.
   *
   * Only handlers which return a value other than `undefined` are
   * considered to have responded. Returning a Promise which evaluates
   * to `undefined` is interpreted as an explicit response.
   *
   * If no matching listeners exist, a RESULT_NO_HANDLER error will be
   * returned. If no listeners return a response, a RESULT_NO_RESPONSE
   * error will be returned.
   */
  RESPONSE_FIRST: 1,

  /**
   * If multiple message managers matching the specified recipient tag
   * are listening for a message, all listeners are notified, and all
   * responses are returned as an array, once all listeners have
   * replied.
   */
  RESPONSE_ALL: 2,

  /**
   * Returns true if the peroperties of the `data` object match those in
   * the `filter` object. Matching is done on a strict equality basis,
   * and the behavior varies depending on the value of the `strict`
   * parameter.
   *
   * @param {object} filter
   *    The filter object to match against.
   * @param {object} data
   *    The data object being matched.
   * @param {boolean} [strict=false]
   *    If true, all properties in the `filter` object have a
   *    corresponding property in `data` with the same value. If
   *    false, properties present in both objects must have the same
   *    balue.
   * @returns {boolean} True if the objects match.
   */
  matchesFilter(filter, data, strict = true) {
    if (strict) {
      return Object.keys(filter).every(key => {
        return key in data && data[key] === filter[key];
      });
    }
    return Object.keys(filter).every(key => {
      return !(key in data) || data[key] === filter[key];
    });
  },

  /**
   * Adds a message listener to the given message manager.
   *
   * @param {nsIMessageSender|[nsIMessageSender]} targets
   *    The message managers on which to listen.
   * @param {string|number} messageName
   *    The name of the message to listen for.
   * @param {MessageReceiver} handler
   *    The handler to dispatch to. Must be an object with the following
   *    properties:
   *
   *      receiveMessage:
   *        A method which is called for each message received by the
   *        listener. The method takes one argument, an object, with the
   *        following properties:
   *
   *          messageName:
   *            The internal message name, as passed to `sendMessage`.
   *
   *          target:
   *            The message manager which received this message.
   *
   *          channelId:
   *            The internal ID of the transaction, used to map responses to
   *            the original sender.
   *
   *          sender:
   *            An object describing the sender, as passed to `sendMessage`.
   *
   *          recipient:
   *            An object describing the recipient, as passed to
   *            `sendMessage`.
   *
   *          data:
   *            The contents of the message, as passed to `sendMessage`.
   *
   *        The method may return any structured-clone-compatible
   *        object, which will be returned as a response to the message
   *        sender. It may also instead return a `Promise`, the
   *        resolution or rejection value of which will likewise be
   *        returned to the message sender.
   *
   *      messageFilterStrict:
   *        An object containing arbitrary properties on which to filter
   *        received messages. Messages will only be dispatched to this
   *        object if the `recipient` object passed to `sendMessage`
   *        matches this filter, as determined by `matchesFilter` with
   *        `strict=true`.
   *
   *      messageFilterPermissive:
   *        An object containing arbitrary properties on which to filter
   *        received messages. Messages will only be dispatched to this
   *        object if the `recipient` object passed to `sendMessage`
   *        matches this filter, as determined by `matchesFilter` with
   *        `strict=false`.
   */
  addListener(targets, messageName, handler) {
    for (let target of [].concat(targets)) {
      this.messageManagers.get(target).addHandler(messageName, handler);
    }
  },

  /**
   * Removes a message listener from the given message manager.
   *
   * @param {nsIMessageSender|Array<nsIMessageSender>} targets
   *    The message managers on which to stop listening.
   * @param {string|number} messageName
   *    The name of the message to stop listening for.
   * @param {MessageReceiver} handler
   *    The handler to stop dispatching to.
   */
  removeListener(targets, messageName, handler) {
    for (let target of [].concat(targets)) {
      if (this.messageManagers.has(target)) {
        this.messageManagers.get(target).removeHandler(messageName, handler);
      }
    }
  },

  /**
   * Sends a message via the given message manager. Returns a promise which
   * resolves or rejects with the return value of the message receiver.
   *
   * The promise also rejects if there is no matching listener, or the other
   * side of the message manager disconnects before the response is received.
   *
   * @param {nsIMessageSender} target
   *    The message manager on which to send the message.
   * @param {string} messageName
   *    The name of the message to send, as passed to `addListener`.
   * @param {object} data
   *    A structured-clone-compatible object to send to the message
   *    recipient.
   * @param {object} [options]
   *    An object containing any of the following properties:
   * @param {object} [options.recipient]
   *    A structured-clone-compatible object to identify the message
   *    recipient. The object must match the `messageFilterStrict` and
   *    `messageFilterPermissive` filters defined by recipients in order
   *    for the message to be received.
   * @param {object} [options.sender]
   *    A structured-clone-compatible object to identify the message
   *    sender. This object may also be used as a filter to prematurely
   *    abort responses when the sender is being destroyed.
   *    @see `abortResponses`.
   * @param {integer} [options.responseType=RESPONSE_SINGLE]
   *    Specifies the type of response expected. See the `RESPONSE_*`
   *    contents for details.
   * @returns {Promise}
   */
  sendMessage(target, messageName, data, options = {}) {
    let sender = options.sender || {};
    let recipient = options.recipient || {};
    let responseType = options.responseType || this.RESPONSE_SINGLE;

    let channelId = gChannelId++;
    let message = {messageName, channelId, sender, recipient, data, responseType};

    let deferred = PromiseUtils.defer();
    deferred.sender = recipient;
    deferred.messageManager = target;

    this._addPendingResponse(deferred);

    // The channel ID is used as the message name when routing responses.
    // Add a message listener to the response broker, and remove it once
    // we've gotten (or canceled) a response.
    let broker = this.responseManagers.get(target);
    broker.addHandler(channelId, deferred);

    let cleanup = () => {
      broker.removeHandler(channelId, deferred);
    };
    deferred.promise.then(cleanup, cleanup);

    target.sendAsyncMessage(MESSAGE_MESSAGE, message);
    return deferred.promise;
  },

  _callHandlers(handlers, data) {
    let responseType = data.responseType;

    // At least one handler is required for all response types but
    // RESPONSE_ALL.
    if (handlers.length == 0 && responseType != this.RESPONSE_ALL) {
      return Promise.reject({result: MessageChannel.RESULT_NO_HANDLER,
                             message: "No matching message handler"});
    }

    if (responseType == this.RESPONSE_SINGLE) {
      if (handlers.length > 1) {
        return Promise.reject({result: MessageChannel.RESULT_MULTIPLE_HANDLERS,
                               message: `Multiple matching handlers for ${data.messageName}`});
      }

      // Note: We use `new Promise` rather than `Promise.resolve` here
      // so that errors from the handler are trapped and converted into
      // rejected promises.
      return new Promise(resolve => {
        resolve(handlers[0].receiveMessage(data));
      });
    }

    let responses = handlers.map(handler => {
      try {
        return handler.receiveMessage(data);
      } catch (e) {
        return Promise.reject(e);
      }
    });
    responses = responses.filter(response => response !== undefined);

    switch (responseType) {
      case this.RESPONSE_FIRST:
        if (responses.length == 0) {
          return Promise.reject({result: MessageChannel.RESULT_NO_RESPONSE,
                                 message: "No handler returned a response"});
        }

        return Promise.race(responses);

      case this.RESPONSE_ALL:
        return Promise.all(responses);
    }
    return Promise.reject({message: "Invalid response type"});
  },

  /**
   * Handles dispatching message callbacks from the message brokers to their
   * appropriate `MessageReceivers`, and routing the responses back to the
   * original senders.
   *
   * Each handler object is a `MessageReceiver` object as passed to
   * `addListener`.
   *
   * @param {Array<MessageHandler>} handlers
   * @param {object} data
   * @param {nsIMessageSender|nsIMessageManagerOwner} data.target
   */
  _handleMessage(handlers, data) {
    // The target passed to `receiveMessage` is sometimes a message manager
    // owner instead of a message manager, so make sure to convert it to a
    // message manager first if necessary.
    let {target} = data;
    if (!(target instanceof Ci.nsIMessageSender)) {
      target = target.messageManager;
    }

    let deferred = {
      sender: data.sender,
      messageManager: target,
    };
    deferred.promise = new Promise((resolve, reject) => {
      deferred.reject = reject;

      this._callHandlers(handlers, data).then(resolve, reject);
    }).then(
      value => {
        let response = {
          result: this.RESULT_SUCCESS,
          messageName: data.channelId,
          recipient: {},
          value,
        };

        target.sendAsyncMessage(MESSAGE_RESPONSE, response);
      },
      error => {
        let response = {
          result: this.RESULT_ERROR,
          messageName: data.channelId,
          recipient: {},
          error: {},
        };

        if (error && typeof(error) == "object") {
          if (error.result) {
            response.result = error.result;
          }
          // Error objects are not structured-clonable, so just copy
          // over the important properties.
          for (let key of ["fileName", "filename", "lineNumber",
                           "columnNumber", "message", "stack", "result"]) {
            if (key in error) {
              response.error[key] = error[key];
            }
          }
        }

        target.sendAsyncMessage(MESSAGE_RESPONSE, response);
      });

    this._addPendingResponse(deferred);
  },

  /**
   * Handles message callbacks from the response brokers.
   *
   * Each handler object is a deferred object created by `sendMessage`, and
   * should be resolved or rejected based on the contents of the response.
   *
   * @param {Array<MessageHandler>} handlers
   * @param {object} data
   * @param {nsIMessageSender|nsIMessageManagerOwner} data.target
   */
  _handleResponse(handlers, data) {
    // If we have an error at this point, we have handler to report it to,
    // so just log it.
    if (handlers.length == 0) {
      Cu.reportError(`No matching message response handler for ${data.messageName}`);
    } else if (handlers.length > 1) {
      Cu.reportError(`Multiple matching response handlers for ${data.messageName}`);
    } else if (data.result === this.RESULT_SUCCESS) {
      handlers[0].resolve(data.value);
    } else {
      handlers[0].reject(data.error);
    }
  },

  /**
   * Adds a pending response to the the `pendingResponses` list.
   *
   * The response object must be a deferred promise with the following
   * properties:
   *
   *  promise:
   *    The promise object which resolves or rejects when the response
   *    is no longer pending.
   *
   *  reject:
   *    A function which, when called, causes the `promise` object to be
   *    rejected.
   *
   *  sender:
   *    A sender object, as passed to `sendMessage.
   *
   *  messageManager:
   *    The message manager the response will be sent or received on.
   *
   * When the promise resolves or rejects, it will be removed from the
   * list.
   *
   * These values are used to clear pending responses when execution
   * contexts are destroyed.
   *
   * @param {Deferred} deferred
   */
  _addPendingResponse(deferred) {
    let cleanup = () => {
      this.pendingResponses.delete(deferred);
    };
    this.pendingResponses.add(deferred);
    deferred.promise.then(cleanup, cleanup);
  },

  /**
   * Aborts any pending message responses to senders matching the given
   * filter.
   *
   * @param {object} sender
   *    The object on which to filter senders, as determined by
   *    `matchesFilter`.
   * @param {object} [reason]
   *    An optional object describing the reason the response was aborted.
   *    Will be passed to the promise rejection handler of all aborted
   *    responses.
   */
  abortResponses(sender, reason = this.REASON_DISCONNECTED) {
    for (let response of this.pendingResponses) {
      if (this.matchesFilter(sender, response.sender)) {
        response.reject(reason);
      }
    }
  },

  /**
   * Aborts any pending message responses to the broker for the given
   * message manager.
   *
   * @param {nsIMessageSender} target
   *    The message manager for which to abort brokers.
   * @param {object} reason
   *    An object describing the reason the responses were aborted.
   *    Will be passed to the promise rejection handler of all aborted
   *    responses.
   */
  abortMessageManager(target, reason) {
    for (let response of this.pendingResponses) {
      if (response.messageManager === target) {
        response.reject(reason);
      }
    }
  },

  observe(subject, topic, data) {
    switch (topic) {
      case "message-manager-close":
      case "message-manager-disconnect":
        try {
          if (this.responseManagers.has(subject)) {
            this.abortMessageManager(subject, this.REASON_DISCONNECTED);
          }
        } finally {
          this.responseManagers.delete(subject);
          this.messageManagers.delete(subject);
        }
        break;
    }
  },
};

MessageChannel.init();
