/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/Log.jsm");
Cu.import("chrome://marionette/content/modal.js");

this.EXPORTED_SYMBOLS = ["proxy"];

const MARIONETTE_OK = "Marionette:ok";
const MARIONETTE_DONE = "Marionette:done";
const MARIONETTE_ERROR = "Marionette:error";

const logger = Log.repository.getLogger("Marionette");
const uuidgen = Cc["@mozilla.org/uuid-generator;1"].getService(Ci.nsIUUIDGenerator);

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
 * Calls to this object will be proxied via the message manager to the active
 * browsing context (content) and responses will be provided back as
 * promises.
 *
 * The argument sequence is serialised and passed as an array, unless it
 * consists of a single object type that isn't null, in which case it's
 * passed literally.  The latter specialisation is temporary to achieve
 * backwards compatibility with listener.js.
 *
 * @param {function(): (nsIMessageSender|nsIMessageBroadcaster)} mmFn
 *     Function returning the current message manager.
 * @param {function(string, Object, number)} sendAsyncFn
 *     Callback for sending async messages to the current listener.
 */
proxy.toListener = function(mmFn, sendAsyncFn) {
  let sender = new AsyncContentSender(mmFn, sendAsyncFn);
  return new Proxy(sender, ownPriorityGetterTrap);
};

/**
 * The AsyncContentSender allows one to make synchronous calls to the
 * message listener of the content frame of the current browsing context.
 *
 * Presumptions about the responses from content space are made so we
 * can provide a nicer API on top of the message listener primitives that
 * make calls from chrome- to content space seem synchronous by leveraging
 * promises.
 *
 * The promise is guaranteed not to resolve until the execution of the
 * command in content space is complete.
 */
this.AsyncContentSender = class {
  constructor(mmFn, sendAsyncFn) {
    this.curId = null;
    this.sendAsync = sendAsyncFn;
    this.mmFn_ = mmFn;
    this._listeners = [];
  }

  get mm() {
    return this.mmFn_();
  }

  removeListeners() {
    this._listeners.map(l => this.mm.removeMessageListener(l[0], l[1]));
    this._listeners = [];
  }

  /**
   * Call registered function in the frame script environment of the
   * current browsing context's content frame.
   *
   * @param {string} name
   *     Function to call in the listener, e.g. for "Marionette:foo8",
   *     use "foo".
   * @param {Array} args
   *     Argument list to pass the function.  If args has a single entry
   *     that is an object, we assume it's an old style dispatch, and
   *     the object will passed literally.
   *
   * @return {Promise}
   *     A promise that resolves to the result of the command.
   */
  send(name, args) {
    if (this._listeners[0]) {
      // A prior (probably timed-out) request has left listeners behind.
      // Remove them before proceeding.
      logger.warn("A previous failed command left content listeners behind!");
      this.removeListeners();
    }

    this.curId = uuidgen.generateUUID().toString();

    let proxy = new Promise((resolve, reject) => {
      let removeListeners = (n, fn) => {
        let rmFn = msg => {
          if (this.curId !== msg.json.command_id) {
            logger.warn("Skipping out-of-sync response from listener: " +
                `Expected response to ${name} with ID ${this.curId}, ` +
                "but got: " + msg.name + msg.json.toSource());
            return;
          }

          this.removeListeners();
          modal.removeHandler(handleDialog);

          fn(msg);
          this.curId = null;
        };

        this._listeners.push([n, rmFn]);
        return rmFn;
      };

      let okListener = () => resolve();
      let valListener = msg => resolve(msg.json.value);
      let errListener = msg => reject(msg.objects.error);

      let handleDialog = (subject, topic) => {
        this.removeListeners()
        modal.removeHandler(handleDialog);
        this.sendAsync("cancelRequest");
        resolve();
      };

      // start content process listeners, and install observers for global-
      // and tab modal dialogues
      this.mm.addMessageListener(MARIONETTE_OK, removeListeners(MARIONETTE_OK, okListener));
      this.mm.addMessageListener(MARIONETTE_DONE, removeListeners(MARIONETTE_DONE, valListener));
      this.mm.addMessageListener(MARIONETTE_ERROR, removeListeners(MARIONETTE_ERROR, errListener));
      modal.addHandler(handleDialog);

      this.sendAsync(name, marshal(args), this.curId);
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
  let sender = new SyncChromeSender(sendSyncMessageFn);
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
this.SyncChromeSender = class {
  constructor(sendSyncMessage) {
    this.sendSyncMessage_ = sendSyncMessage;
  }

  send(func, args) {
    let name = "Marionette:" + func;
    return this.sendSyncMessage_(name, marshal(args));
  }
};

function marshal(args) {
  if (args.length == 1 && typeof args[0] == "object") {
    return args[0];
  }
  return args;
}
