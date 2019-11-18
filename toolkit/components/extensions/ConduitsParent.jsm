/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const EXPORTED_SYMBOLS = ["BroadcastConduit", "ConduitsParent"];

/**
 * This @file implements the parent side of Conduits, an abstraction over
 * Fission IPC for extension Contexts, API managers, Ports/Messengers, and
 * other types of "subjects" participating in implementation of extension APIs.
 *
 * Additionally, knowledge about conduits from all child processes is gathered
 * here, and used together with the full CanonicalBrowsingContext tree to route
 * IPC messages and queries directly to the right subjects.
 *
 * Each Conduit is tied to one subject, attached to a ConduitAddress descriptor,
 * and exposes an API for sending/receiving via an actor, or multiple actors in
 * case of the parent process.
 *
 * @typedef {number|string} ConduitID
 *
 * @typedef {object} ConduitAddress
 * @prop {ConduitID} id Globally unique across all processes.
 * @prop {string[]} [recv]
 * @prop {string[]} [send]
 * @prop {string[]} [query]
 * Lists of recvX, sendX, and queryX methods this subject will use.
 *
 * @example:
 *
 *    init(actor) {
 *      this.conduit = actor.openConduit(this, {
 *        id: this.id,
 *        recv: ["recvAddNumber"],
 *        send: ["sendNumberUpdate"],
 *      });
 *    }
 *
 *    recvAddNumber({ num }, { actor, sender }) {
 *      num += 1;
 *      this.conduit.sendNumberUpdate(sender.id, { num });
 *    }
 *
 */

const {
  ExtensionUtils: { DefaultMap, DefaultWeakMap },
} = ChromeUtils.import("resource://gre/modules/ExtensionUtils.jsm");

const { BaseConduit } = ChromeUtils.import(
  "resource://gre/modules/ConduitsChild.jsm"
);

/**
 * Internal, keeps track of all parent and remote (child) conduits.
 */
const Hub = {
  /** @type Map<ConduitID, ConduitAddress> Info about all child conduits. */
  remotes: new Map(),

  /** @type Map<ConduitID, BroadcastConduit> All open parent conduits. */
  conduits: new Map(),

  /** @type Map<string, BroadcastConduit> Parent conduits by recvMethod. */
  byMethod: new Map(),

  /** @type WeakMap<ConduitsParent, Set<ConduitAddress>> Conduits by actor. */
  byActor: new DefaultWeakMap(() => new Set()),

  /** @type Map<ConduitID, Set<BroadcastConduit>> */
  onRemoteClosed: new DefaultMap(() => new Set()),

  /**
   * Save info about a new parent conduit, register it as a global listener.
   * @param {BroadcastConduit} conduit
   */
  openConduit(conduit) {
    this.conduits.set(conduit.id, conduit);
    for (let name of conduit.address.recv || []) {
      if (this.byMethod.get(name)) {
        // For now, we only allow one parent conduit handling each recv method.
        throw new Error(`Duplicate BroadcastConduit method name recv${name}`);
      }
      this.byMethod.set(name, conduit);
    }
  },

  /**
   * Cleanup.
   * @param {BroadcastConduit} conduit
   */
  closeConduit({ id, address }) {
    this.conduits.delete(id);
    for (let name of address.recv || []) {
      this.byMethod.remove(name);
    }
  },

  /**
   * Save info about a new remote conduit.
   * @param {ConduitAddress} address
   * @param {ConduitsParent} actor
   */
  recvConduitOpened(address, actor) {
    address.actor = actor;
    this.remotes.set(address.id, address);
    this.byActor.get(actor).add(address);
  },

  /**
   * Notifies listeners and cleans up after the remote conduit is closed.
   * @param {ConduitAddress} remote
   */
  recvConduitClosed(remote) {
    this.remotes.delete(remote.id);
    this.byActor.get(remote.actor).delete(remote);

    remote.actor = null;
    for (let conduit of this.onRemoteClosed.get(remote.id)) {
      conduit.subject.recvConduitClosed(remote);
    }
  },

  /**
   * Close all remote conduits when the actor goes away.
   * @param {ConduitsParent} actor
   */
  actorClosed(actor) {
    for (let remote of this.byActor.get(actor)) {
      this.recvConduitClosed(remote);
    }
    this.byActor.delete(actor);
  },
};

/**
 * Parent side conduit, registers as a global listeners for certain messages,
 * and can target specific child conduits when sending.
 */
class BroadcastConduit extends BaseConduit {
  /**
   * @param {object} subject
   * @param {ConduitAddress} address
   */
  constructor(subject, address) {
    super(subject, address);

    this.open = true;
    Hub.openConduit(this);
  }

  /**
   * Internal, sends a message to a specific conduit, used by sendX stubs.
   * @param {string} method
   * @param {boolean} query
   * @param {ConduitID} target
   * @param {object?} arg
   * @returns {Promise<any>}
   */
  _send(method, query, target, arg = {}) {
    if (!this.open) {
      throw new Error(`send${method} on closed conduit ${this.id}`);
    }
    let sender = this.address;
    let { actor } = Hub.remotes.get(target);
    return super._send(method, query, actor, { target, arg, query, sender });
  }

  /**
   * Indicate the subject wants to listen for the specific conduit closing.
   * The method `recvConduitClosed(address)` will be called.
   * @param {ConduitID} target
   */
  reportOnClosed(target) {
    Hub.onRemoteClosed.get(target).add(this);
  }

  async close() {
    this.open = false;
    Hub.closeConduit(this);
  }
}

/**
 * Implements the parent side of the Conduits actor.
 */
class ConduitsParent extends JSWindowActorParent {
  constructor() {
    super();
  }

  /**
   * JSWindowActor method, routes the message to the target subject.
   * @returns {Promise?}
   */
  receiveMessage({ name, data: { arg, query, sender } }) {
    switch (name) {
      case "ConduitOpened":
        return Hub.recvConduitOpened(arg, this);
      case "ConduitClosed":
        return Hub.recvConduitClosed(Hub.remotes.get(arg));
    }

    let conduit = Hub.byMethod.get(name);
    if (!conduit) {
      throw new Error(`Parent conduit for recv${name} not found`);
    }

    sender = Hub.remotes.get(sender);
    return conduit._recv(name, arg, { actor: this, query, sender });
  }

  /**
   * JSWindowActor method, called before actor is destroyed.
   */
  willDestroy() {
    Hub.actorClosed(this);
  }

  /**
   * JSWindowActor method, ensure cleanup (see bug 1596187).
   */
  didDestroy() {
    this.willDestroy();
  }
}
