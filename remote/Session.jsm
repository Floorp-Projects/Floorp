/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["Session"];

const {Domain} = ChromeUtils.import("chrome://remote/content/Domain.jsm");
const {formatError} = ChromeUtils.import("chrome://remote/content/Error.jsm");
const {Protocol} = ChromeUtils.import("chrome://remote/content/Protocol.jsm");

this.Session = class {
  constructor(connection, target) {
    this.connection = connection;
    this.target = target;

    this.domains = new Domains(this);

    this.connection.onmessage = this.dispatch.bind(this);
  }

  destructor() {
    this.connection.onmessage = null;
    this.domains.clear();
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
      assertSchema(domainName, methodName, params);

      const inst = this.domains.get(domainName);
      const methodFn = inst[methodName];
      if (!methodFn || typeof methodFn != "function") {
        throw new Error(`Method implementation of ${method} missing`);
      }

      const result = await methodFn.call(inst, params);
      this.connection.send({id, result});
    } catch (e) {
      const error = formatError(e, {stack: true});
      this.connection.send({id, error});
    }
  }

  // EventEmitter

  onevent(eventName, params) {
    this.connection.send({method: eventName, params});
  }
};

function assertSchema(domainName, methodName, params) {
  const domain = Domain[domainName];
  if (!domain) {
    throw new TypeError("No such domain: " + domainName);
  }
  if (!domain.schema) {
    throw new Error(`Domain ${domainName} missing schema description`);
  }

  let details = {};
  const descriptor = (domain.schema.methods || {})[methodName];
  if (!Protocol.checkSchema(descriptor.params || {}, params, details)) {
    const {errorType, propertyName, propertyValue} = details;
    throw new TypeError(`${domainName}.${methodName} called ` +
        `with ${errorType} ${propertyName}: ${propertyValue}`);
  }
}

class Domains extends Map {
  constructor(session) {
    super();
    this.session = session;
  }

  get(name) {
    let inst = super.get(name);
    if (!inst) {
      inst = this.new(name);
      this.set(inst);
    }
    return inst;
  }

  set(domain) {
    super.set(domain.name, domain);
  }

  new(name) {
    const Cls = Domain[name];
    if (!Cls) {
      throw new Error("No such domain: " + name);
    }

    const inst = new Cls(this.session, this.session.target);
    inst.on("*", this.session);

    return inst;
  }

  delete(name) {
    const inst = super.get(name);
    if (inst) {
      inst.off("*");
      inst.destructor();
      super.delete(inst.name);
    }
  }

  clear() {
    for (const domainName of this.keys()) {
      this.delete(domainName);
    }
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
