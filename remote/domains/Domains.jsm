/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["Domains"];

class Domains extends Map {
  constructor(session, modules) {
    super();
    this.session = session;
    this.modules = modules;
  }

  domainSupportsMethod(name, method) {
    const domain = this.modules[name];
    return domain && typeof domain.prototype[method] == "function";
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
    const Cls = this.modules[name];
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
    for (const name of this.keys()) {
      this.delete(name);
    }
  }

  // Splits a method name, e.g. "Browser.getVersion", into two components.
  static splitMethod(method) {
    return split(method, ".", 1);
  }
}

// Split s by sep, returning list of substrings.
// If max is given, at most max splits are done.
// If max is 0, there is no limit on the number of splits.
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
