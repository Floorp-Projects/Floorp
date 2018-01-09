/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {utils: Cu} = Components;

Cu.import("resource://gre/modules/Log.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

const {
  error,
  WebDriverError,
} = Cu.import("chrome://marionette/content/error.js", {});
Cu.import("chrome://marionette/content/evaluate.js");
Cu.import("chrome://marionette/content/modal.js");

this.EXPORTED_SYMBOLS = ["proxy"];

XPCOMUtils.defineLazyServiceGetter(
    this, "uuidgen", "@mozilla.org/uuid-generator;1", "nsIUUIDGenerator");
XPCOMUtils.defineLazyServiceGetter(
    this, "globalMessageManager", "@mozilla.org/globalmessagemanager;1",
    "nsIMessageBroadcaster");

const log = Log.repository.getLogger("Marionette");

// Proxy handler that traps requests to get a property.  Will prioritise
// properties that exist on the object's own prototype.
const ownPriorityGetterTrap = {
  get: (obj, prop) => {
    if (obj.hasOwnProperty(prop)) {
      return obj[prop];
    }
    return (...args) => obj.send(prop, args);
  },
};

/** @namespace */
this.proxy = {};

/**
 * Creates a transparent interface between the chrome- and content
 * contexts.
 *
 * Calls to this object will be proxied via the message manager to a
 * content frame script, and responses are returend as promises.
 *
 * The argument sequence is serialised and passed as an array, unless it
 * consists of a single object type that isn't null, in which case it's
 * passed literally.  The latter specialisation is temporary to achieve
 * backwards compatibility with listener.js.
 *
 * @param {function(string, Object, number)} sendAsyncFn
 *     Callback for sending async messages.
 * @param {function(): browser.Context} browserFn
 *     Closure that returns the current browsing context.
 */
proxy.toListener = function(sendAsyncFn, browserFn) {
  let sender = new proxy.AsyncMessageChannel(sendAsyncFn, browserFn);
  return new Proxy(sender, ownPriorityGetterTrap);
};

/**
 * Provides a transparent interface between chrome- and content space.
 *
 * The AsyncMessageChannel is an abstraction of the message manager
 * IPC architecture allowing calls to be made to any registered message
 * listener in Marionette.  The <code>#send(...)</code> method
 * returns a promise that gets resolved when the message handler calls
 * <code>.reply(...)</code>.
 */
proxy.AsyncMessageChannel = class {
  constructor(sendAsyncFn, browserFn) {
    this.sendAsync = sendAsyncFn;
    this.browserFn_ = browserFn;

    // TODO(ato): Bug 1242595
    this.activeMessageId = null;

    this.listeners_ = new Map();
    this.dialogueObserver_ = null;
    this.closeHandler = null;
  }

  get browser() {
    return this.browserFn_();
  }

  /**
   * Send a message across the channel.  The name of the function to
   * call must be registered as a message listener.
   *
   * Usage:
   *
   * <pre><code>
   *     let channel = new AsyncMessageChannel(
   *         messageManager, sendAsyncMessage.bind(this));
   *     let rv = await channel.send("remoteFunction", ["argument"]);
   * </code></pre>
   *
   * @param {string} name
   *     Function to call in the listener, e.g. for the message listener
   *     <tt>Marionette:foo8</tt>, use <tt>foo</tt>.
   * @param {Array.<?>=} args
   *     Argument list to pass the function. If args has a single entry
   *     that is an object, we assume it's an old style dispatch, and
   *     the object will passed literally.
   *
   * @return {Promise}
   *     A promise that resolves to the result of the command.
   * @throws {TypeError}
   *     If an unsupported reply type is received.
   * @throws {WebDriverError}
   *     If an error is returned over the channel.
   */
  send(name, args = []) {
    let uuid = uuidgen.generateUUID().toString();
    // TODO(ato): Bug 1242595
    this.activeMessageId = uuid;

    return new Promise((resolve, reject) => {
      let path = proxy.AsyncMessageChannel.makePath(uuid);
      let cb = msg => {
        this.activeMessageId = null;
        let {data, type} = msg.json;

        switch (msg.json.type) {
          case proxy.AsyncMessageChannel.ReplyType.Ok:
          case proxy.AsyncMessageChannel.ReplyType.Value:
            let payload = evaluate.fromJSON(data);
            resolve(payload);
            break;

          case proxy.AsyncMessageChannel.ReplyType.Error:
            let err = WebDriverError.fromJSON(data);
            reject(err);
            break;

          default:
            throw new TypeError(`Unknown async response type: ${type}`);
        }
      };

      // The currently selected tab or window has been closed. No clean-up
      // is necessary to do because all loaded listeners are gone.
      this.closeHandler = ({type, target}) => {
        log.debug(`Received DOM event ${type} for ${target}`);

        switch (type) {
          case "TabClose":
          case "unload":
            this.removeHandlers();
            resolve();
            break;
        }
      };

      // A modal or tab modal dialog has been opened. To be able to handle it,
      // the active command has to be aborted. Therefore remove all handlers,
      // and cancel any ongoing requests in the listener.
      this.dialogueObserver_ = (subject, topic) => {
        log.debug(`Received observer notification ${topic}`);

        this.removeAllListeners_();
        // TODO(ato): It's not ideal to have listener specific behaviour here:
        this.sendAsync("cancelRequest");

        this.removeHandlers();
        resolve();
      };

      // start content message listener, and install handlers for
      // modal dialogues, and window/tab state changes.
      this.addListener_(path, cb);
      this.addHandlers();

      // sendAsync is GeckoDriver#sendAsync
      this.sendAsync(name, marshal(args), uuid);
    });
  }

  /**
   * Add all necessary handlers for events and observer notifications.
   */
  addHandlers() {
    modal.addHandler(this.dialogueObserver_);

    // Register event handlers in case the command closes the current
    // tab or window, and the promise has to be escaped.
    if (this.browser) {
      this.browser.window.addEventListener("unload", this.closeHandler);

      if (this.browser.tab) {
        let node = this.browser.tab.addEventListener ?
            this.browser.tab : this.browser.contentBrowser;
        node.addEventListener("TabClose", this.closeHandler);
      }
    }
  }

  /**
   * Remove all registered handlers for events and observer notifications.
   */
  removeHandlers() {
    modal.removeHandler(this.dialogueObserver_);

    if (this.browser) {
      this.browser.window.removeEventListener("unload", this.closeHandler);

      if (this.browser.tab) {
        let node = this.browser.tab.addEventListener ?
            this.browser.tab : this.browser.contentBrowser;
        node.removeEventListener("TabClose", this.closeHandler);
      }
    }
  }

  /**
   * Reply to an asynchronous request.
   *
   * Passing an {@link WebDriverError} prototype will cause the receiving
   * channel to throw this error.
   *
   * Usage:
   *
   * <pre><code>
   *     let channel = proxy.AsyncMessageChannel(
   *         messageManager, sendAsyncMessage.bind(this));
   *
   *     // throws in requester:
   *     channel.reply(uuid, new WebDriverError());
   *
   *     // returns with value:
   *     channel.reply(uuid, "hello world!");
   *
   *     // returns with undefined:
   *     channel.reply(uuid);
   * </pre></code>
   *
   * @param {UUID} uuid
   *     Unique identifier of the request.
   * @param {*} obj
   *     Message data to reply with.
   */
  reply(uuid, obj = undefined) {
    // TODO(ato): Eventually the uuid will be hidden in the dispatcher
    // in listener, and passing it explicitly to this function will be
    // unnecessary.
    if (typeof obj == "undefined") {
      this.sendReply_(uuid, proxy.AsyncMessageChannel.ReplyType.Ok);
    } else if (error.isError(obj)) {
      let err = error.wrap(obj);
      this.sendReply_(uuid, proxy.AsyncMessageChannel.ReplyType.Error, err);
    } else {
      this.sendReply_(uuid, proxy.AsyncMessageChannel.ReplyType.Value, obj);
    }
  }

  sendReply_(uuid, type, payload = undefined) {
    const path = proxy.AsyncMessageChannel.makePath(uuid);

    let data = evaluate.toJSON(payload);
    const msg = {type, data};

    // here sendAsync is actually the content frame's
    // sendAsyncMessage(path, message) global
    this.sendAsync(path, msg);
  }

  /**
   * Produces a path, or a name, for the message listener handler that
   * listens for a reply.
   *
   * @param {UUID} uuid
   *     Unique identifier of the channel request.
   *
   * @return {string}
   *     Path to be used for nsIMessageListener.addMessageListener.
   */
  static makePath(uuid) {
    return "Marionette:asyncReply:" + uuid;
  }

  addListener_(path, callback) {
    let autoRemover = msg => {
      this.removeListener_(path);
      this.removeHandlers();
      callback(msg);
    };

    globalMessageManager.addMessageListener(path, autoRemover);
    this.listeners_.set(path, autoRemover);
  }

  removeListener_(path) {
    if (!this.listeners_.has(path)) {
      return true;
    }

    let l = this.listeners_.get(path);
    globalMessageManager.removeMessageListener(path, l[1]);
    return this.listeners_.delete(path);
  }

  removeAllListeners_() {
    let ok = true;
    for (let [p] of this.listeners_) {
      ok |= this.removeListener_(p);
    }
    return ok;
  }
};
proxy.AsyncMessageChannel.ReplyType = {
  Ok: 0,
  Value: 1,
  Error: 2,
};

function marshal(args) {
  if (args.length == 1 && typeof args[0] == "object") {
    return args[0];
  }
  return args;
}
