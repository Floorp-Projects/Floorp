/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

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
  let sender = new proxy.AsyncContentSender(mmFn, sendAsyncFn);
  return new Proxy(sender, ownPriorityGetterTrap);
};

/**
 * With the AsyncContentSender it is possible to make asynchronous calls
 * to the message listener in a frame script.
 *
 * The responses from content are expected to be JSON Objects, where an
 * {@code error} key indicates that an error occured, and a {@code value}
 * entry that the operation was successful.  It is the value of the
 * {@code value} key that is returned to the consumer through a promise.
 */
proxy.AsyncContentSender = class {
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
   * Call registered function in the frame script environment of the
   * current browsing context's content frame.
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
   */
  send(name, args = []) {
    let uuid = uuidgen.generateUUID().toString();
    // TODO(ato): Bug 1242595
    this.activeMessageId = uuid;

    return new Promise((resolve, reject) => {
      let path = proxy.AsyncContentSender.makeReplyPath(uuid);
      let cb = msg => {
        this.activeMessageId = null;
        if ("error" in msg.json) {
          reject(msg.objects.error);
        } else {
          resolve(msg.json.value);
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

      this.sendAsync(name, marshal(args), uuid);
    });
  }

  cancelAll() {
    this.removeAllListeners_();
    modal.removeHandler(this.dialogueObserver_);
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

  static makeReplyPath(uuid) {
    return "Marionette:asyncReply:" + uuid;
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
 * context, using a frame's sendSyncMessage (nsISyncMessageSender) function.
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
    let name = "Marionette:" + func;
    return this.sendSyncMessage_(name, marshal(args));
  }
};

var marshal = function(args) {
  if (args.length == 1 && typeof args[0] == "object") {
    return args[0];
  }
  return args;
};
