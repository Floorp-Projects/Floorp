/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["Domains"];

const {Domain} = ChromeUtils.import("chrome://remote/content/Domain.jsm");

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
