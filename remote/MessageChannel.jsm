/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["MessageChannel"];

const {AtomicMap} = ChromeUtils.import("chrome://remote/content/Collections.jsm");
const {FatalError} = ChromeUtils.import("chrome://remote/content/Error.jsm");
const {Log} = ChromeUtils.import("chrome://remote/content/Log.jsm");
const {XPCOMUtils} = ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyGetter(this, "log", Log.get);

this.MessageChannel = class {
  constructor(target, channelName, messageManager) {
    this.target = target;
    this.name = channelName;
    this.mm = messageManager;
    this.mm.addMessageListener(this.name, this);

    this.ids = 0;
    this.pending = new AtomicMap();
  }

  destructor() {
    this.mm.removeMessageListener(this.name, this);
    this.ids = 0;
    this.pending.clear();
  }

  send(methodName, params = {}) {
    const id = ++this.ids;
    const promise = new Promise((resolve, reject) => {
      this.pending.set(id, {resolve, reject});
    });

    const msg = {id, methodName, params};
    log.trace(`(channel ${this.name})--> ${JSON.stringify(msg)}`);
    this.mm.sendAsyncMessage(this.name, msg);

    return promise;
  }

  onevent() {}

  onresponse(id, result) {
    const {resolve} = this.pending.pop(id);
    resolve(result);
  }

  onerror(id, error) {
    const {reject} = this.pending.pop(id);
    reject(new Error(error));
  }

  receiveMessage({data}) {
    log.trace(`<--(channel ${this.name}) ${JSON.stringify(data)}`);

    if (data.methodName) {
      throw new FatalError("Circular message channel!", this);
    }

    if (data.id) {
      const {id, error, result} = data;
      if (error) {
        this.onerror(id, error);
      } else {
        this.onresponse(id, result);
      }
    } else {
      const {eventName, params = {}} = data;
      this.onevent(eventName, params);
    }
  }

  toString() {
    return `[object MessageChannel ${this.name}]`;
  }

  // XPCOM

  get QueryInterface() {
    return ChromeUtils.generateQI([
      Ci.nsIMessageListener,
      Ci.nsISupportsWeakReference,
    ]);
  }
};
