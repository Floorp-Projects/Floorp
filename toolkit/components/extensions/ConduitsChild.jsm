/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/**
 * This @file implements the child side of Conduits, an abstraction over
 * Fission IPC for extension API subject.  See {@link ConduitsParent.jsm}
 * for more details about the overall design.
 *
 * @typedef {object} MessageData
 * @prop {ConduitID} [target]
 * @prop {ConduitID} [sender]
 * @prop {boolean} query
 * @prop {object} arg
 */

const EXPORTED_SYMBOLS = ["BaseConduit", "ConduitsChild"];

/**
 * Base class for both child (Point) and parent (Broadcast) side of conduits,
 * handles setting up send/receive method stubs.
 */
class BaseConduit {
  /**
   * @param {object} subject
   * @param {ConduitAddress} address
   */
  constructor(subject, address) {
    this.subject = subject;
    this.address = address;
    this.id = address.id;

    for (let name of address.send || []) {
      this[`send${name}`] = this._send.bind(this, name, false);
    }
    for (let name of address.query || []) {
      this[`query${name}`] = this._send.bind(this, name, true);
    }

    this.recv = new Map();
    for (let name of address.recv || []) {
      let method = this.subject[`recv${name}`];
      if (!method) {
        throw new Error(`recv${name} not found for conduit ${this.id}`);
      }
      this.recv.set(name, method.bind(this.subject));
    }
  }

  /**
   * Internal, partially @abstract, uses the actor to send the message/query.
   * @param {string} method
   * @param {boolean} query Flag indicating a response is expected.
   * @param {JSWindowActor} actor
   * @param {MessageData} data
   * @returns {Promise?}
   */
  _send(method, query, actor, data) {
    if (query) {
      return actor.sendQuery(method, data);
    }
    actor.sendAsyncMessage(method, data);
  }

  /**
   * Internal, calls the specific recvX method based on the message.
   * @param {string} name Message/method name.
   * @param {object} arg  Message data, the one and only method argument.
   * @param {object} meta Metadata about the method call.
   */
  async _recv(name, arg, meta) {
    let method = this.recv.get(name);
    if (!method) {
      throw new Error(`recv${name} not found for conduit ${this.id}`);
    }
    try {
      return await method(arg, meta);
    } catch (e) {
      if (meta.query) {
        return Promise.reject(e);
      }
      Cu.reportError(e);
    }
  }
}

/**
 * Child side conduit, can only send/receive point-to-point messages via the
 * one specific ConduitsChild actor.
 */
class PointConduit extends BaseConduit {
  constructor(subject, address, actor) {
    super(subject, address);
    this.actor = actor;
    this.actor.sendAsyncMessage("ConduitOpened", { arg: address });
  }

  /**
   * Internal, sends messages via the actor, used by sendX stubs.
   * @param {string} method
   * @param {boolean} query
   * @param {object?} arg
   * @returns {Promise?}
   */
  _send(method, query, arg = {}) {
    if (!this.actor) {
      throw new Error(`send${method} on closed conduit ${this.id}`);
    }
    let sender = this.id;
    return super._send(method, query, this.actor, { arg, query, sender });
  }

  /**
   * Closes the conduit from further IPC, notifies the parent side by default.
   * @param {boolean} silent
   */
  close(silent = false) {
    let { actor } = this;
    if (actor) {
      this.actor = null;
      actor.conduits.delete(this.id);
      if (!silent) {
        actor.sendAsyncMessage("ConduitClosed", { sender: this.id });
      }
    }
  }
}

/**
 * Implements the child side of the Conduits actor, manages conduit lifetimes.
 */
class ConduitsChild extends JSWindowActorChild {
  constructor() {
    super();
    this.conduits = new Map();
  }

  /**
   * Public entry point a child-side subject uses to open a conduit.
   * @param {object} subject
   * @param {ConduitAddress} address
   * @returns {PointConduit}
   */
  openConduit(subject, address) {
    let conduit = new PointConduit(subject, address, this);
    this.conduits.set(conduit.id, conduit);
    return conduit;
  }

  /**
   * JSWindowActor method, routes the message to the target subject.
   * @param {string} name
   * @param {MessageData|MessageData[]} data
   * @returns {Promise?}
   */
  receiveMessage({ name, data }) {
    // Batch of webRequest events, run each and return results, ignoring errors.
    if (Array.isArray(data)) {
      let run = data => this.receiveMessage({ name, data });
      return Promise.all(data.map(data => run(data).catch(Cu.reportError)));
    }

    let { target, arg, query, sender } = data;
    let conduit = this.conduits.get(target);
    if (!conduit) {
      throw new Error(`${name} for closed conduit ${target}: ${uneval(arg)}`);
    }
    return conduit._recv(name, arg, { sender, query, actor: this });
  }

  /**
   * JSWindowActor method, called before actor is destroyed.
   * Parent side will get the same call, so just silently close all conduits.
   */
  willDestroy() {
    for (let conduit of this.conduits.values()) {
      conduit.close(true);
    }
    this.conduits.clear();
  }

  /**
   * JSWindowActor method, ensure cleanup (see bug 1596187).
   */
  didDestroy() {
    this.willDestroy();
  }
}
