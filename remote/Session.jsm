/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["Session"];

const {ParentProcessDomains} = ChromeUtils.import("chrome://remote/content/domains/ParentProcessDomains.jsm");
const {Domains} = ChromeUtils.import("chrome://remote/content/domains/Domains.jsm");
const {formatError} = ChromeUtils.import("chrome://remote/content/Error.jsm");

class Session {
  constructor(connection, target) {
    this.connection = connection;
    this.target = target;

    this.connection.onmessage = this.dispatch.bind(this);

    this.domains = new Domains(this, ParentProcessDomains);
    this.mm.addMessageListener("remote:event", this);
    this.mm.addMessageListener("remote:result", this);
    this.mm.addMessageListener("remote:error", this);

    this.mm.loadFrameScript("chrome://remote/content/frame-script.js", false);
  }

  destructor() {
    this.domains.clear();
    this.connection.onmessage = null;

    this.mm.sendAsyncMessage("remote:destroy", {
      browsingContextId: this.browsingContext.id,
    });

    this.mm.removeMessageListener("remote:event", this);
    this.mm.removeMessageListener("remote:result", this);
    this.mm.removeMessageListener("remote:error", this);
  }

  async dispatch({id, method, params}) {
    try {
      if (typeof id == "undefined") {
        throw new TypeError("Message missing 'id' field");
      }
      if (typeof method == "undefined") {
        throw new TypeError("Message missing 'method' field");
      }

      const [domainName, methodName] = split(method, ".", 1);
      if (this.domains.domainSupportsMethod(domainName, methodName)) {
        const inst = this.domains.get(domainName);
        const methodFn = inst[methodName];
        if (!methodFn || typeof methodFn != "function") {
          throw new Error(`Method implementation of ${methodName} missing`);
        }

        const result = await methodFn.call(inst, params);
        this.connection.send({id, result});
      } else {
        this.mm.sendAsyncMessage("remote:request", {
          browsingContextId: this.browsingContext.id,
          request: {id, domainName, methodName, params},
        });
      }
    } catch (e) {
      const error = formatError(e, {stack: true});
      this.connection.send({id, error});
    }
  }

  get mm() {
    return this.target.mm;
  }

  get browsingContext() {
    return this.target.browsingContext;
  }

  // nsIMessageListener

  receiveMessage({name, data}) {
    const {id, result, event, error} = data;

    switch (name) {
    case "remote:result":
      this.connection.send({id, result});
      break;

    case "remote:event":
      this.connection.send(event);
      break;

    case "remote:error":
      this.connection.send({id, error: formatError(error, {stack: true})});
      break;
    }
  }

  // EventEmitter

  onevent(eventName, params) {
    this.connection.send({
      method: eventName,
      params,
    });
  }
}

/**
 * Split s by sep, returning list of substrings.
 * If max is given, at most max splits are done.
 * If max is 0, there is no limit on the number of splits.
 */
function split(s, sep, max = 0) {
  if (typeof s != "string" ||
      typeof sep != "string" ||
      typeof max != "number") {
    throw new TypeError();
  }
  if (!Number.isInteger(max) || max < 0) {
    throw new RangeError();
  }

  const rv = [];
  let i = 0;

  while (rv.length < max) {
    const si = s.indexOf(sep, i);
    if (!si) {
      break;
    }

    rv.push(s.substring(i, si));
    i = si + sep.length;
  }

  rv.push(s.substring(i));
  return rv;
}
