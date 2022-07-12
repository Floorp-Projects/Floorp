/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["DomainCache"];

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  Domain: "chrome://remote/content/cdp/domains/Domain.jsm",
  UnknownMethodError: "chrome://remote/content/cdp/Error.jsm",
});

/**
 * Lazy domain instance cache.
 *
 * Domains are loaded into each target's realm, and consequently
 * there exists one domain cache per realm.  Domains are preregistered
 * with this cache and then constructed lazily upon request.
 *
 * @param {Session} session
 *     Session that domains should be associated with as they
 *     are constructed.
 * @param {Map.<string, string>} modules
 *     Table defining JS modules available to this domain cache.
 *     This should be a mapping between domain name
 *     and JS module path passed to ChromeUtils.import.
 */
class DomainCache {
  constructor(session, modules) {
    this.session = session;
    this.modules = modules;
    this.instances = new Map();
  }

  /** Test if domain supports method. */
  domainSupportsMethod(name, method) {
    const domain = this.modules[name];
    if (domain) {
      return domain.implements(method);
    }
    return false;
  }

  /**
   * Gets the current instance of the domain, or creates a new one,
   * and associates it with the predefined session.
   *
   * @throws {UnknownMethodError}
   *     If domain is not preregistered with this domain cache.
   */
  get(name) {
    let inst = this.instances.get(name);
    if (!inst) {
      const Cls = this.modules[name];
      if (!Cls) {
        throw new lazy.UnknownMethodError(name);
      }
      if (!isConstructor(Cls)) {
        throw new TypeError("Domain cannot be constructed");
      }

      inst = new Cls(this.session);
      if (!(inst instanceof lazy.Domain)) {
        throw new TypeError("Instance not a domain");
      }

      inst.addEventListener(this.session);

      this.instances.set(name, inst);
    }

    return inst;
  }

  /**
   * Tells if a Domain of the given name is available
   */
  has(name) {
    return name in this.modules;
  }

  get size() {
    return this.instances.size;
  }

  /**
   * Execute the given command (function) of a given domain with the given parameters.
   * If the command doesn't exists, it will throw.
   * It returns the returned value of the command, which is most likely a promise.
   */
  execute(domain, command, params) {
    if (!this.domainSupportsMethod(domain, command)) {
      throw new lazy.UnknownMethodError(domain, command);
    }
    const inst = this.get(domain);
    return inst[command](params);
  }

  /** Calls destructor on each domain and clears the cache. */
  clear() {
    for (const inst of this.instances.values()) {
      inst.destructor();
    }
    this.instances.clear();
  }

  toString() {
    return `[object DomainCache ${this.size}]`;
  }
}

function isConstructor(obj) {
  return !!obj.prototype && !!obj.prototype.constructor.name;
}
