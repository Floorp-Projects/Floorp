/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
 * ContentFrameMessageManager, with an appropriate set of filters:
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

export let MessageChannel;

import { AppConstants } from "resource://gre/modules/AppConstants.sys.mjs";

import { ExtensionUtils } from "resource://gre/modules/ExtensionUtils.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  MessageManagerProxy: "resource://gre/modules/MessageManagerProxy.sys.mjs",
});

function getMessageManager(target) {
  if (typeof target.sendAsyncMessage === "function") {
    return target;
  }
  return new lazy.MessageManagerProxy(target);
}

function matches(target, messageManager) {
  return target === messageManager || target.messageManager === messageManager;
}

const { DEBUG } = AppConstants;

// Idle callback timeout for low-priority message dispatch.
const LOW_PRIORITY_TIMEOUT_MS = 250;

const MESSAGE_MESSAGES = "MessageChannel:Messages";
const MESSAGE_RESPONSE = "MessageChannel:Response";

var _deferredResult;
var _makeDeferred = (resolve, reject) => {
  // We use arrow functions here and refer to the outer variables via
  // `this`, to avoid a lexical name lookup. Yes, it makes a difference.
  // No, I don't like it any more than you do.
  _deferredResult.resolve = resolve;
  _deferredResult.reject = reject;
};

/**
 * Helper to create a new Promise without allocating any closures to
 * receive its resolution functions.
 *
 * I know what you're thinking: "This is crazy. There is no possible way
 * this can be necessary. Just use the ordinary Promise constructor the
 * way it was meant to be used, you lunatic."
 *
 * And, against all odds, it turns out that you're wrong. Creating
 * lambdas to receive promise resolution functions consistently turns
 * out to be one of the most expensive parts of message dispatch in this
 * code.
 *
 * So we do the stupid micro-optimization, and try to live with
 * ourselves for it.
 *
 * (See also bug 1404950.)
 *
 * @returns {object}
 */
let Deferred = () => {
  let res = {};
  _deferredResult = res;
  res.promise = new Promise(_makeDeferred);
  _deferredResult = null;
  return res;
};

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
   * @param {Function} callback
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
   * @param {nsIMessageListenerManager} messageManager
   */
  constructor(messageName, callback, messageManager) {
    this.messageName = messageName;
    this.callback = callback;
    this.messageManager = messageManager;

    this.messageManager.addMessageListener(this.messageName, this, true);

    this.handlers = new Map();
  }

  /**
   * Receives a set of messages from our message manager, maps each to a
   * handler, and passes the results to our message callbacks.
   */
  receiveMessage({ data, target }) {
    data.forEach(msg => {
      if (msg) {
        let handlers = Array.from(
          this.getHandlers(msg.messageName, msg.sender || null, msg.recipient)
        );

        msg.target = target;
        this.callback(handlers, msg);
      }
    });
  }

  /**
   * Iterates over all handlers for the given message name. If `recipient`
   * is provided, only iterates over handlers whose filters match it.
   *
   * @param {string|number} messageName
   *     The message for which to return handlers.
   * @param {object} sender
   *     The sender data on which to filter handlers.
   * @param {object} recipient
   *     The recipient data on which to filter handlers.
   */
  *getHandlers(messageName, sender, recipient) {
    let handlers = this.handlers.get(messageName) || new Set();
    for (let handler of handlers) {
      if (
        MessageChannel.matchesFilter(
          handler.messageFilterStrict || null,
          recipient
        ) &&
        MessageChannel.matchesFilter(
          handler.messageFilterPermissive || null,
          recipient,
          false
        ) &&
        (!handler.filterMessage || handler.filterMessage(sender, recipient))
      ) {
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
   *     property and/or a `filterMessage` method on which to filter messages.
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
    if (this.handlers.has(messageName)) {
      this.handlers.get(messageName).delete(handler);
    }
  }
}

/**
 * A message dispatch and response manager that wrapse a single native
 * message manager. Handles dispatching messages through the manager
 * (optionally coalescing several low-priority messages and dispatching
 * them during an idle slice), and mapping their responses to the
 * appropriate response callbacks.
 *
 * Note that this is a simplified subclass of FilteringMessageManager
 * that only supports one handler per message, and does not support
 * filtering.
 */
class ResponseManager extends FilteringMessageManager {
  constructor(messageName, callback, messageManager) {
    super(messageName, callback, messageManager);

    this.idleMessages = [];
    this.idleScheduled = false;
    this.onIdle = this.onIdle.bind(this);
  }

  /**
   * Schedules a new idle callback to dispatch pending low-priority
   * messages, if one is not already scheduled.
   */
  scheduleIdleCallback() {
    if (!this.idleScheduled) {
      ChromeUtils.idleDispatch(this.onIdle, {
        timeout: LOW_PRIORITY_TIMEOUT_MS,
      });
      this.idleScheduled = true;
    }
  }

  /**
   * Called when the event queue is idle, and dispatches any pending
   * low-priority messages in a single chunk.
   *
   * @param {IdleDeadline} deadline
   */
  onIdle(deadline) {
    this.idleScheduled = false;

    let messages = this.idleMessages;
    this.idleMessages = [];

    let msgs = messages.map(msg => msg.getMessage());
    try {
      this.messageManager.sendAsyncMessage(MESSAGE_MESSAGES, msgs);
    } catch (e) {
      for (let msg of messages) {
        msg.reject(e);
      }
    }
  }

  /**
   * Sends a message through our wrapped message manager, or schedules
   * it for low-priority dispatch during an idle callback.
   *
   * @param {any} message
   *        The message to send.
   * @param {object} [options]
   *        Message dispatch options.
   * @param {boolean} [options.lowPriority = false]
   *        If true, dispatches the message in a single chunk with other
   *        low-priority messages the next time the event queue is idle.
   */
  sendMessage(message, options = {}) {
    if (options.lowPriority) {
      this.idleMessages.push(message);
      this.scheduleIdleCallback();
    } else {
      this.messageManager.sendAsyncMessage(MESSAGE_MESSAGES, [
        message.getMessage(),
      ]);
    }
  }

  receiveMessage({ data, target }) {
    data.target = target;

    this.callback(this.handlers.get(data.messageName), data);
  }

  *getHandlers(messageName, sender, recipient) {
    let handler = this.handlers.get(messageName);
    if (handler) {
      yield handler;
    }
  }

  addHandler(messageName, handler) {
    if (DEBUG && this.handlers.has(messageName)) {
      throw new Error(
        `Handler already registered for response ID ${messageName}`
      );
    }
    this.handlers.set(messageName, handler);
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
    if (DEBUG && this.handlers.get(messageName) !== handler) {
      Cu.reportError(
        `Attempting to remove unexpected response handler for ${messageName}`
      );
    }
    this.handlers.delete(messageName);
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
   * @param {Function} callback
   *     The message callback function passed to
   *     `FilteringMessageManager` constructors.
   * @param {Function} [constructor = FilteringMessageManager]
   *     The constructor for the message manager class that we're
   *     mapping to.
   */
  constructor(messageName, callback, constructor = FilteringMessageManager) {
    super();

    this.messageName = messageName;
    this.callback = callback;
    this._constructor = constructor;
  }

  /**
   * Returns, and possibly creates, a message broker for the given
   * message manager.
   *
   * @param {nsIMessageListenerManager} target
   *     The message manager for which to return a broker.
   *
   * @returns {FilteringMessageManager}
   */
  get(target) {
    let broker = super.get(target);
    if (broker) {
      return broker;
    }

    broker = new this._constructor(this.messageName, this.callback, target);
    this.set(target, broker);

    // XXXbz if target is really known to be a MessageListenerManager,
    // do we need this isInstance check?
    if (EventTarget.isInstance(target)) {
      let onUnload = event => {
        target.removeEventListener("unload", onUnload);
        this.delete(target);
      };
      target.addEventListener("unload", onUnload);
    }

    return broker;
  }
}

/**
 * Represents a message being sent through a MessageChannel, which may
 * or may not have been dispatched yet, and is pending a response.
 *
 * When a response has been received, or the message has been canceled,
 * this class is responsible for settling the response promise as
 * appropriate.
 *
 * @param {number} channelId
 *        The unique ID for this message.
 * @param {any} message
 *        The message contents.
 * @param {object} sender
 *        An object describing the sender of the message, used by
 *        `abortResponses` to determine whether the message should be
 *        aborted.
 * @param {ResponseManager} broker
 *        The response broker on which we're expected to receive a
 *        reply.
 */
class PendingMessage {
  constructor(channelId, message, sender, broker) {
    this.channelId = channelId;
    this.message = message;
    this.sender = sender;
    this.broker = broker;
    this.deferred = Deferred();

    MessageChannel.pendingResponses.add(this);
  }

  /**
   * Cleans up after this message once we've received or aborted a
   * response.
   */
  cleanup() {
    if (this.broker) {
      this.broker.removeHandler(this.channelId, this);
      MessageChannel.pendingResponses.delete(this);

      this.message = null;
      this.broker = null;
    }
  }

  /**
   * Returns the promise which will resolve when we've received or
   * aborted a response to this message.
   */
  get promise() {
    return this.deferred.promise;
  }

  /**
   * Resolves the message's response promise, and cleans up.
   *
   * @param {any} value
   */
  resolve(value) {
    this.cleanup();
    this.deferred.resolve(value);
  }

  /**
   * Rejects the message's response promise, and cleans up.
   *
   * @param {any} value
   */
  reject(value) {
    this.cleanup();
    this.deferred.reject(value);
  }

  get messageManager() {
    return this.broker.messageManager;
  }

  /**
   * Returns the contents of the message to be sent over a message
   * manager, and registers the response with our response broker.
   *
   * Returns null if the response has already been canceled, and the
   * message should not be sent.
   *
   * @returns {any}
   */
  getMessage() {
    let msg = null;
    if (this.broker) {
      this.broker.addHandler(this.channelId, this);
      msg = this.message;
      this.message = null;
    }
    return msg;
  }
}

// Web workers has MessageChannel API, which is unrelated to this.
// eslint-disable-next-line no-global-assign
MessageChannel = {
  init() {
    Services.obs.addObserver(this, "message-manager-close");
    Services.obs.addObserver(this, "message-manager-disconnect");

    this.messageManagers = new FilteringMessageManagerMap(
      MESSAGE_MESSAGES,
      this._handleMessage.bind(this)
    );

    this.responseManagers = new FilteringMessageManagerMap(
      MESSAGE_RESPONSE,
      this._handleResponse.bind(this),
      ResponseManager
    );

    /**
     * @property {Set<Deferred>} pendingResponses
     * Contains a set of pending responses, either waiting to be
     * received or waiting to be sent.
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
     * When the promise resolves or rejects, it must be removed from the
     * list.
     *
     * These values are used to clear pending responses when execution
     * contexts are destroyed.
     */
    this.pendingResponses = new Set();

    /**
     * @property {LimitedSet<string>} abortedResponses
     * Contains the message name of a limited number of aborted response
     * handlers, the responses for which will be ignored.
     */
    this.abortedResponses = new ExtensionUtils.LimitedSet(30);
  },

  RESULT_SUCCESS: 0,
  RESULT_DISCONNECTED: 1,
  RESULT_NO_HANDLER: 2,
  RESULT_MULTIPLE_HANDLERS: 3,
  RESULT_ERROR: 4,
  RESULT_NO_RESPONSE: 5,

  REASON_DISCONNECTED: {
    result: 1, // this.RESULT_DISCONNECTED
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
   * Fire-and-forget: The sender of this message does not expect a reply.
   */
  RESPONSE_NONE: 3,

  /**
   * Initializes message handlers for the given message managers if needed.
   *
   * @param {Array<nsIMessageListenerManager>} messageManagers
   */
  setupMessageManagers(messageManagers) {
    for (let mm of messageManagers) {
      // This call initializes a FilteringMessageManager for |mm| if needed.
      // The FilteringMessageManager must be created to make sure that senders
      // of messages that expect a reply, such as MessageChannel:Message, do
      // actually receive a default reply even if there are no explicit message
      // handlers.
      this.messageManagers.get(mm);
    }
  },

  /**
   * Returns true if the properties of the `data` object match those in
   * the `filter` object. Matching is done on a strict equality basis,
   * and the behavior varies depending on the value of the `strict`
   * parameter.
   *
   * @param {object?} filter
   *    The filter object to match against.
   * @param {object} data
   *    The data object being matched.
   * @param {boolean} [strict=true]
   *    If true, all properties in the `filter` object have a
   *    corresponding property in `data` with the same value. If
   *    false, properties present in both objects must have the same
   *    value.
   * @returns {boolean} True if the objects match.
   */
  matchesFilter(filter, data, strict = true) {
    if (!filter) {
      return true;
    }
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
   * @param {nsIMessageListenerManager|Array<nsIMessageListenerManager>} targets
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
   *
   *      filterMessage:
   *        An optional function that prevents the handler from handling a
   *        message by returning `false`. See `getHandlers` for the parameters.
   */
  addListener(targets, messageName, handler) {
    if (!Array.isArray(targets)) {
      targets = [targets];
    }
    for (let target of targets) {
      this.messageManagers.get(target).addHandler(messageName, handler);
    }
  },

  /**
   * Removes a message listener from the given message manager.
   *
   * @param {nsIMessageListenerManager|Array<nsIMessageListenerManager>} targets
   *    The message managers on which to stop listening.
   * @param {string|number} messageName
   *    The name of the message to stop listening for.
   * @param {MessageReceiver} handler
   *    The handler to stop dispatching to.
   */
  removeListener(targets, messageName, handler) {
    if (!Array.isArray(targets)) {
      targets = [targets];
    }
    for (let target of targets) {
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
   *    sender. This object may also be used to avoid delivering the
   *    message to the sender, and as a filter to prematurely
   *    abort responses when the sender is being destroyed.
   *    @see `abortResponses`.
   * @param {boolean} [options.lowPriority = false]
   *    If true, treat this as a low-priority message, and attempt to
   *    send it in the same chunk as other messages to the same target
   *    the next time the event queue is idle. This option reduces
   *    messaging overhead at the expense of adding some latency.
   * @param {integer} [options.responseType = RESPONSE_SINGLE]
   *    Specifies the type of response expected. See the `RESPONSE_*`
   *    contents for details.
   * @returns {Promise}
   */
  sendMessage(target, messageName, data, options = {}) {
    let sender = options.sender || {};
    let recipient = options.recipient || {};
    let responseType = options.responseType || this.RESPONSE_SINGLE;

    let channelId = ExtensionUtils.getUniqueId();
    let message = {
      messageName,
      channelId,
      sender,
      recipient,
      data,
      responseType,
    };
    data = null;

    if (responseType == this.RESPONSE_NONE) {
      try {
        target.sendAsyncMessage(MESSAGE_MESSAGES, [message]);
      } catch (e) {
        // Caller is not expecting a reply, so dump the error to the console.
        Cu.reportError(e);
        return Promise.reject(e);
      }
      return Promise.resolve(); // Not expecting any reply.
    }

    let broker = this.responseManagers.get(target);
    let pending = new PendingMessage(channelId, message, recipient, broker);
    message = null;
    try {
      broker.sendMessage(pending, options);
    } catch (e) {
      pending.reject(e);
    }
    return pending.promise;
  },

  _callHandlers(handlers, data) {
    let responseType = data.responseType;

    // At least one handler is required for all response types but
    // RESPONSE_ALL.
    if (!handlers.length && responseType != this.RESPONSE_ALL) {
      return Promise.reject({
        result: MessageChannel.RESULT_NO_HANDLER,
        message: "No matching message handler",
      });
    }

    if (responseType == this.RESPONSE_SINGLE) {
      if (handlers.length > 1) {
        return Promise.reject({
          result: MessageChannel.RESULT_MULTIPLE_HANDLERS,
          message: `Multiple matching handlers for ${data.messageName}`,
        });
      }

      // Note: We use `new Promise` rather than `Promise.resolve` here
      // so that errors from the handler are trapped and converted into
      // rejected promises.
      return new Promise(resolve => {
        resolve(handlers[0].receiveMessage(data));
      });
    }

    let responses = handlers.map((handler, i) => {
      try {
        return handler.receiveMessage(data, i + 1 == handlers.length);
      } catch (e) {
        return Promise.reject(e);
      }
    });
    data = null;
    responses = responses.filter(response => response !== undefined);

    switch (responseType) {
      case this.RESPONSE_FIRST:
        if (!responses.length) {
          return Promise.reject({
            result: MessageChannel.RESULT_NO_RESPONSE,
            message: "No handler returned a response",
          });
        }

        return Promise.race(responses);

      case this.RESPONSE_ALL:
        return Promise.all(responses);
    }
    return Promise.reject({ message: "Invalid response type" });
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
   * @param {nsIMessageSender|{messageManager:nsIMessageSender}} data.target
   */
  _handleMessage(handlers, data) {
    if (data.responseType == this.RESPONSE_NONE) {
      handlers.forEach(handler => {
        // The sender expects no reply, so dump any errors to the console.
        new Promise(resolve => {
          resolve(handler.receiveMessage(data));
        }).catch(e => {
          Cu.reportError(e.stack ? `${e}\n${e.stack}` : e.message || e);
        });
      });
      data = null;
      // Note: Unhandled messages are silently dropped.
      return;
    }

    let target = getMessageManager(data.target);

    let deferred = {
      sender: data.sender,
      messageManager: target,
      channelId: data.channelId,
      respondingSide: true,
    };

    let cleanup = () => {
      this.pendingResponses.delete(deferred);
      if (target.dispose) {
        target.dispose();
      }
    };
    this.pendingResponses.add(deferred);

    deferred.promise = new Promise((resolve, reject) => {
      deferred.reject = reject;

      this._callHandlers(handlers, data).then(resolve, reject);
      data = null;
    })
      .then(
        value => {
          let response = {
            result: this.RESULT_SUCCESS,
            messageName: deferred.channelId,
            recipient: {},
            value,
          };

          if (target.isDisconnected) {
            // Target is disconnected. We can't send an error response, so
            // don't even try.
            return;
          }
          target.sendAsyncMessage(MESSAGE_RESPONSE, response);
        },
        error => {
          if (target.isDisconnected) {
            // Target is disconnected. We can't send an error response, so
            // don't even try.
            if (
              error.result !== this.RESULT_DISCONNECTED &&
              error.result !== this.RESULT_NO_RESPONSE
            ) {
              Cu.reportError(
                Cu.getClassName(error, false) === "Object"
                  ? error.message
                  : error
              );
            }
            return;
          }

          let response = {
            result: this.RESULT_ERROR,
            messageName: deferred.channelId,
            recipient: {},
            error: {},
          };

          if (error && typeof error == "object") {
            if (error.result) {
              response.result = error.result;
            }
            // Error objects are not structured-clonable, so just copy
            // over the important properties.
            for (let key of [
              "fileName",
              "filename",
              "lineNumber",
              "columnNumber",
              "message",
              "stack",
              "result",
              "mozWebExtLocation",
            ]) {
              if (key in error) {
                response.error[key] = error[key];
              }
            }
          }

          target.sendAsyncMessage(MESSAGE_RESPONSE, response);
        }
      )
      .then(cleanup, e => {
        cleanup();
        Cu.reportError(e);
      });
  },

  /**
   * Handles message callbacks from the response brokers.
   *
   * @param {MessageHandler?} handler
   *        A deferred object created by `sendMessage`, to be resolved
   *        or rejected based on the contents of the response.
   * @param {object} data
   * @param {nsIMessageSender|{messageManager:nsIMessageSender}} data.target
   */
  _handleResponse(handler, data) {
    // If we have an error at this point, we have handler to report it to,
    // so just log it.
    if (!handler) {
      if (this.abortedResponses.has(data.messageName)) {
        this.abortedResponses.delete(data.messageName);
        Services.console.logStringMessage(
          `Ignoring response to aborted listener for ${data.messageName}`
        );
      } else {
        Cu.reportError(
          `No matching message response handler for ${data.messageName}`
        );
      }
    } else if (data.result === this.RESULT_SUCCESS) {
      handler.resolve(data.value);
    } else {
      handler.reject(data.error);
    }
  },

  /**
   * Aborts pending message response for the specific channel.
   *
   * @param {string} channelId
   *    A string for channelId of the response.
   * @param {object} reason
   *    An object describing the reason the response was aborted.
   *    Will be passed to the promise rejection handler of the aborted
   *    response.
   */
  abortChannel(channelId, reason) {
    for (let response of this.pendingResponses) {
      if (channelId === response.channelId && response.respondingSide) {
        this.pendingResponses.delete(response);
        response.reject(reason);
      }
    }
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
        this.pendingResponses.delete(response);
        this.abortedResponses.add(response.channelId);
        response.reject(reason);
      }
    }
  },

  /**
   * Aborts any pending message responses to the broker for the given
   * message manager.
   *
   * @param {nsIMessageListenerManager} target
   *    The message manager for which to abort brokers.
   * @param {object} reason
   *    An object describing the reason the responses were aborted.
   *    Will be passed to the promise rejection handler of all aborted
   *    responses.
   */
  abortMessageManager(target, reason) {
    for (let response of this.pendingResponses) {
      if (matches(response.messageManager, target)) {
        this.abortedResponses.add(response.channelId);
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
