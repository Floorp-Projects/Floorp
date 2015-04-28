/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {utils: Cu} = Components;

Cu.import("resource://gre/modules/Log.jsm");
Cu.import("chrome://marionette/content/modal.js");

this.EXPORTED_SYMBOLS = ["proxy"];

const logger = Log.repository.getLogger("Marionette");

this.proxy = {};

/**
 * Creates a transparent interface between the chrome- and content
 * processes.
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
  let sender = new ContentSender(mmFn, sendAsyncFn);
  let handler = {
    set: (obj, prop, val) => { obj[prop] = val; return true; },
    get: (obj, prop) => (...args) => obj.send(prop, args),
  };
  return new Proxy(sender, handler);
};

/**
 * The ContentSender allows one to make synchronous calls to the
 * message listener of the content frame of the current browsing context.
 *
 * Presumptions about the responses from content space are made so we
 * can provide a nicer API on top of the message listener primitives that
 * make calls from chrome- to content space seem synchronous by leveraging
 * promises.
 *
 * The promise is guaranteed not to resolve until the execution of the
 * command in content space is complete.
 *
 * @param {function(): (nsIMessageSender|nsIMessageBroadcaster)} mmFn
 *     Function returning the current message manager.
 * @param {function(string, Object, number)} sendAsyncFn
 *     Callback for sending async messages to the current listener.
 */
let ContentSender = function(mmFn, sendAsyncFn) {
  this.curCmdId = null;
  this.sendAsync = sendAsyncFn;
  this.mmFn_ = mmFn;
};

Object.defineProperty(ContentSender.prototype, "mm", {
  get: function() { return this.mmFn_(); }
});

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
ContentSender.prototype.send = function(name, args) {
  const ok = "Marionette:ok";
  const val = "Marionette:done";
  const err = "Marionette:error";

  let proxy = new Promise((resolve, reject) => {
    let removeListeners = (name, fn) => {
      let rmFn = msg => {
        if (this.isOutOfSync(msg.json.command_id)) {
          logger.warn("Skipping out-of-sync response from listener: " +
              msg.name + msg.json.toSource());
          return;
        }

        listeners.remove();
        modal.removeHandler(handleDialog);

        fn(msg);
        this.curCmdId = null;
      };

      listeners.push([name, rmFn]);
      return rmFn;
    };

    let listeners = [];
    listeners.add = () => {
      this.mm.addMessageListener(ok, removeListeners(ok, okListener));
      this.mm.addMessageListener(val, removeListeners(val, valListener));
      this.mm.addMessageListener(err, removeListeners(err, errListener));
    };
    listeners.remove = () =>
        listeners.map(l => this.mm.removeMessageListener(l[0], l[1]));

    let okListener = () => resolve();
    let valListener = msg => resolve(msg.json.value);
    let errListener = msg => reject(msg.objects.error);

    let handleDialog = function(subject, topic) {
      listeners.remove();
      modal.removeHandler(handleDialog);
      this.sendAsync("cancelRequest");
      resolve();
    }.bind(this);

    // start content process listeners, and install observers for global-
    // and tab modal dialogues
    listeners.add();
    modal.addHandler(handleDialog);

    // new style dispatches are arrays of arguments, old style dispatches
    // are key-value objects
    let msg = args;
    if (args.length == 1 && typeof args[0] == "object") {
      msg = args[0];
    }

    this.sendAsync(name, msg, this.curCmdId);
  });

  return proxy;
};

ContentSender.prototype.isOutOfSync = function(id) {
  return this.curCmdId !== id;
};

proxy.ContentSender = ContentSender;
