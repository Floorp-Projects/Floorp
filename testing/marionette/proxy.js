/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("chrome://marionette/content/error.js");
Cu.import("chrome://marionette/content/modal.js");

this.EXPORTED_SYMBOLS = ["proxy"];

const uuidgen = Cc["@mozilla.org/uuid-generator;1"]
    .getService(Ci.nsIUUIDGenerator);

// Proxy handler that traps requests to get a property.  Will prioritise
// properties that exist on the object's own prototype.
var ownPriorityGetterTrap = {
  get: (obj, prop) => {
    if (obj.hasOwnProperty(prop)) {
      return obj[prop];
    }
    return (...args) => obj.send(prop, args);
  }
};

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
 * @param {function(): (nsIMessageSender|nsIMessageBroadcaster)} mmFn
 *     Closure function returning the current message manager.
 * @param {function(string, Object, number)} sendAsyncFn
 *     Callback for sending async messages.
 */
proxy.toListener = function(mmFn, sendAsyncFn) {
  let sender = new proxy.AsyncMessageChannel(mmFn, sendAsyncFn);
  return new Proxy(sender, ownPriorityGetterTrap);
};

/**
 * Provides a transparent interface between chrome- and content space.
 *
 * The AsyncMessageChannel is an abstraction of the message manager
 * IPC architecture allowing calls to be made to any registered message
 * listener in Marionette.  The {@code #send(...)} method returns a promise
 * that gets resolved when the message handler calls {@code .reply(...)}.
 */
proxy.AsyncMessageChannel = class {
  constructor(mmFn, sendAsyncFn) {
    this.sendAsync = sendAsyncFn;
    // TODO(ato): Bug 1242595
    this.activeMessageId = null;

    this.mmFn_ = mmFn;
    this.listeners_ = new Map();
    this.dialogueObserver_ = null;
  }

  get mm() {
    return this.mmFn_();
  }

  /**
   * Send a message across the channel.  The name of the function to
   * call must be registered as a message listener.
   *
   * Usage:
   *
   *     let channel = new AsyncMessageChannel(
   *         messageManager, sendAsyncMessage.bind(this));
   *     let rv = yield channel.send("remoteFunction", ["argument"]);
   *
   * @param {string} name
   *     Function to call in the listener, e.g. for the message listener
   *     "Marionette:foo8", use "foo".
   * @param {Array.<?>=} args
   *     Argument list to pass the function.  If args has a single entry
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

        switch (msg.json.type) {
          case proxy.AsyncMessageChannel.ReplyType.Ok:
          case proxy.AsyncMessageChannel.ReplyType.Value:
            resolve(msg.json.data);
            break;

          case proxy.AsyncMessageChannel.ReplyType.Error:
            let err = error.fromJson(msg.json.data);
            reject(err);
            break;

          default:
            throw new TypeError(
                `Unknown async response type: ${msg.json.type}`);
        }
      };

      this.dialogueObserver_ = (subject, topic) => {
        this.cancelAll();
        resolve();
      };

      // start content message listener
      // and install observers for global- and tab modal dialogues
      this.addListener_(path, cb);
      modal.addHandler(this.dialogueObserver_);

      // sendAsync is GeckoDriver#sendAsync
      this.sendAsync(name, marshal(args), uuid);
    });
  }

  /**
   * Reply to an asynchronous request.
   *
   * Passing an WebDriverError prototype will cause the receiving channel
   * to throw this error.
   *
   * Usage:
   *
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
   *
   * @param {UUID} uuid
   *     Unique identifier of the request.
   * @param {?=} obj
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
      let serr = error.toJson(err);
      this.sendReply_(uuid, proxy.AsyncMessageChannel.ReplyType.Error, serr);
    } else {
      this.sendReply_(uuid, proxy.AsyncMessageChannel.ReplyType.Value, obj);
    }
  }

  sendReply_(uuid, type, data = undefined) {
    let path = proxy.AsyncMessageChannel.makePath(uuid);
    let msg = {type: type, data: data};
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

  /**
   * Abort listening for responses, remove all modal dialogue handlers,
   * and cancel any ongoing requests in the listener.
   */
  cancelAll() {
    this.removeAllListeners_();
    modal.removeHandler(this.dialogueObserver_);
    // TODO(ato): It's not ideal to have listener specific behaviour here:
    this.sendAsync("cancelRequest");
  }

  addListener_(path, callback) {
    let autoRemover = msg => {
      this.removeListener_(path);
      modal.removeHandler(this.dialogueObserver_);
      callback(msg);
    };

    this.mm.addMessageListener(path, autoRemover);
    this.listeners_.set(path, autoRemover);
  }

  removeListener_(path) {
    if (!this.listeners_.has(path)) {
      return true;
    }

    let l = this.listeners_.get(path);
    this.mm.removeMessageListener(path, l[1]);
    return this.listeners_.delete(path);
  }

  removeAllListeners_() {
    let ok = true;
    for (let [p, cb] of this.listeners_) {
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

/**
 * A transparent content-to-chrome RPC interface where responses are
 * presented as promises.
 *
 * @param {nsIFrameMessageManager} frameMessageManager
 *     The content frame's message manager, which itself is usually an
 *     implementor of.
 */
proxy.toChromeAsync = function(frameMessageManager) {
  let sender = new AsyncChromeSender(frameMessageManager);
  return new Proxy(sender, ownPriorityGetterTrap);
};

/**
 * Sends asynchronous RPC messages to chrome space using a frame's
 * sendAsyncMessage (nsIAsyncMessageSender) function.
 *
 * Example on how to use from a frame content script:
 *
 *     let sender = new AsyncChromeSender(messageManager);
 *     let promise = sender.send("runEmulatorCmd", "my command");
 *     let rv = yield promise;
 */
this.AsyncChromeSender = class {
  constructor(frameMessageManager) {
    this.mm = frameMessageManager;
  }

  /**
   * Call registered function in chrome context.
   *
   * @param {string} name
   *     Function to call in the chrome, e.g. for "Marionette:foo", use
   *     "foo".
   * @param {?} args
   *     Argument list to pass the function.  Must be JSON serialisable.
   *
   * @return {Promise}
   *     A promise that resolves to the value sent back.
   */
  send(name, args) {
    let uuid = uuidgen.generateUUID().toString();

    let proxy = new Promise((resolve, reject) => {
      let responseListener = msg => {
        if (msg.json.id != uuid) {
          return;
        }

        this.mm.removeMessageListener(
            "Marionette:listenerResponse", responseListener);

        if ("value" in msg.json) {
          resolve(msg.json.value);
        } else if ("error" in msg.json) {
          reject(msg.json.error);
        } else {
          throw new TypeError(
              `Unexpected response: ${msg.name} ${JSON.stringify(msg.json)}`);
        }
      };

      let msg = {arguments: marshal(args), id: uuid};
      this.mm.addMessageListener(
          "Marionette:listenerResponse", responseListener);
      this.mm.sendAsyncMessage("Marionette:" + name, msg);
    });

    return proxy;
  }
};

/**
 * Creates a transparent interface from the content- to the chrome context.
 *
 * Calls to this object will be proxied via the frame's sendSyncMessage
 * (nsISyncMessageSender) function.  Since the message is synchronous,
 * the return value is presented as a return value.
 *
 * Example on how to use from a frame content script:
 *
 *     let chrome = proxy.toChrome(sendSyncMessage.bind(this));
 *     let cookie = chrome.getCookie("foo");
 *
 * @param {nsISyncMessageSender} sendSyncMessageFn
 *     The frame message manager's sendSyncMessage function.
 */
proxy.toChrome = function(sendSyncMessageFn) {
  let sender = new proxy.SyncChromeSender(sendSyncMessageFn);
  return new Proxy(sender, ownPriorityGetterTrap);
};

/**
 * The SyncChromeSender sends synchronous RPC messages to the chrome
 * context, using a frame's sendSyncMessage (nsISyncMessageSender)
 * function.
 *
 * Example on how to use from a frame content script:
 *
 *     let sender = new SyncChromeSender(sendSyncMessage.bind(this));
 *     let res = sender.send("addCookie", cookie);
 */
proxy.SyncChromeSender = class {
  constructor(sendSyncMessage) {
    this.sendSyncMessage_ = sendSyncMessage;
  }

  send(func, args) {
    let name = "Marionette:" + func.toString();
    return this.sendSyncMessage_(name, marshal(args));
  }
};

var marshal = function(args) {
  if (args.length == 1 && typeof args[0] == "object") {
    return args[0];
  }
  return args;
};
