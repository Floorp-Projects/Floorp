/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
 * @property {ConduitID} [id] Globally unique across all processes.
 * @property {string[]} [recv]
 * @property {string[]} [send]
 * @property {string[]} [query]
 * @property {string[]} [cast]
 *
 * @property {*} [actor]
 * @property {boolean} [verified]
 * @property {string} [url]
 * @property {number} [frameId]
 * @property {string} [workerScriptURL]
 * @property {string} [extensionId]
 * @property {string} [envType]
 * @property {string} [instanceId]
 * @property {number} [portId]
 * @property {boolean} [native]
 * @property {boolean} [source]
 * @property {string} [reportOnClosed]
 *
 * Lists of recvX, sendX, queryX and castX methods this subject will use.
 *
 * @typedef {"messenger"|"port"|"tab"} BroadcastKind
 * Kinds of broadcast targeting filters.
 *
 * @example
 * ```js
 * {
 *    init(actor) {
 *      this.conduit = actor.openConduit(this, {
 *        id: this.id,
 *        recv: ["recvAddNumber"],
 *        send: ["sendNumberUpdate"],
 *      });
 *    },
 *
 *    recvAddNumber({ num }, { actor, sender }) {
 *      num += 1;
 *      this.conduit.sendNumberUpdate(sender.id, { num });
 *    }
 * }
 * ```
 */

import { BaseConduit } from "resource://gre/modules/ConduitsChild.sys.mjs";
import { ExtensionUtils } from "resource://gre/modules/ExtensionUtils.sys.mjs";
import { WebNavigationFrames } from "resource://gre/modules/WebNavigationFrames.sys.mjs";

const { DefaultWeakMap, ExtensionError } = ExtensionUtils;

const BATCH_TIMEOUT_MS = 250;
const ADDON_ENV = new Set(["addon_child", "devtools_child"]);

/**
 * Internal, keeps track of all parent and remote (child) conduits.
 */
const Hub = {
  /** @type {Map<ConduitID, ConduitAddress>} Info about all child conduits. */
  remotes: new Map(),

  /** @type {Map<ConduitID, BroadcastConduit>} All open parent conduits. */
  conduits: new Map(),

  /** @type {Map<string, BroadcastConduit>} Parent conduits by recvMethod. */
  byMethod: new Map(),

  /** @type {WeakMap<ConduitsParent, Set<ConduitAddress>>} Conduits by actor. */
  byActor: new DefaultWeakMap(() => new Set()),

  /** @type {Map<string, BroadcastConduit>} */
  reportOnClosed: new Map(),

  /**
   * Save info about a new parent conduit, register it as a global listener.
   *
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
   *
   * @param {BroadcastConduit} conduit
   */
  closeConduit({ id, address }) {
    this.conduits.delete(id);
    for (let name of address.recv || []) {
      this.byMethod.delete(name);
    }
  },

  /**
   * Confirm that a remote conduit comes from an extension background
   * service worker.
   *
   * @see ExtensionPolicyService::CheckParentFrames
   * @param {ConduitAddress} remote
   * @returns {boolean}
   */
  verifyWorkerEnv({ actor, extensionId, workerScriptURL }) {
    const addonPolicy = WebExtensionPolicy.getByID(extensionId);
    if (!addonPolicy) {
      throw new Error(`No WebExtensionPolicy found for ${extensionId}`);
    }
    if (actor.manager.remoteType !== addonPolicy.extension.remoteType) {
      throw new Error(
        `Bad ${extensionId} process: ${actor.manager.remoteType}`
      );
    }
    if (!addonPolicy.isManifestBackgroundWorker(workerScriptURL)) {
      throw new Error(
        `Bad ${extensionId} background service worker script url: ${workerScriptURL}`
      );
    }
    return true;
  },

  /**
   * Confirm that a remote conduit comes from an extension page or
   * an extension background service worker.
   *
   * @see ExtensionPolicyService::CheckParentFrames
   * @param {ConduitAddress} remote
   * @returns {boolean}
   */
  verifyEnv({ actor, envType, extensionId, ...rest }) {
    if (!extensionId || !ADDON_ENV.has(envType)) {
      return false;
    }

    // ProcessConduit related to a background service worker context.
    if (actor.manager && actor.manager instanceof Ci.nsIDOMProcessParent) {
      return this.verifyWorkerEnv({ actor, envType, extensionId, ...rest });
    }

    let windowGlobal = actor.manager;

    while (windowGlobal) {
      let { browsingContext: bc, documentPrincipal: prin } = windowGlobal;

      if (prin.addonId !== extensionId) {
        throw new Error(`Bad ${extensionId} principal: ${prin.URI.spec}`);
      }
      if (bc.currentRemoteType !== prin.addonPolicy.extension.remoteType) {
        throw new Error(`Bad ${extensionId} process: ${bc.currentRemoteType}`);
      }

      if (!bc.parent) {
        return true;
      }
      windowGlobal = bc.embedderWindowGlobal;
    }
    throw new Error(`Missing WindowGlobalParent for ${extensionId}`);
  },

  /**
   * Fill in common address fields knowable from the parent process.
   *
   * @param {ConduitAddress} address
   * @param {ConduitsParent} actor
   */
  fillInAddress(address, actor) {
    address.actor = actor;
    address.verified = this.verifyEnv(address);
    if (JSWindowActorParent.isInstance(actor)) {
      address.frameId = WebNavigationFrames.getFrameId(actor.browsingContext);
      address.url = actor.manager.documentURI?.spec;
    } else {
      // Background service worker contexts do not have an associated frame
      // and there is no browsingContext to retrieve the expected url from.
      //
      // WorkerContextChild sent in the address part of the ConduitOpened request
      // the worker script URL as address.workerScriptURL, and so we can use that
      // as the address.url too.
      address.frameId = -1;
      address.url = address.workerScriptURL;
    }
  },

  /**
   * Save info about a new remote conduit.
   *
   * @param {ConduitAddress} address
   * @param {ConduitsParent} actor
   */
  recvConduitOpened(address, actor) {
    this.fillInAddress(address, actor);
    this.remotes.set(address.id, address);
    this.byActor.get(actor).add(address);
  },

  /**
   * Notifies listeners and cleans up after the remote conduit is closed.
   *
   * @param {ConduitAddress} remote
   */
  recvConduitClosed(remote) {
    this.remotes.delete(remote.id);
    this.byActor.get(remote.actor).delete(remote);

    remote.actor = null;
    for (let [key, conduit] of Hub.reportOnClosed.entries()) {
      if (remote[key]) {
        conduit.subject.recvConduitClosed(remote);
      }
    }
  },

  /**
   * Close all remote conduits when the actor goes away.
   *
   * @param {ConduitsParent} actor
   */
  actorClosed(actor) {
    for (let remote of this.byActor.get(actor)) {
      // When a Port is closed, we notify the other side, but it might share
      // an actor, so we shouldn't sendQeury() in that case (see bug 1623976).
      this.remotes.delete(remote.id);
    }
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
export class BroadcastConduit extends BaseConduit {
  /**
   * @param {object} subject
   * @param {ConduitAddress} address
   */
  constructor(subject, address) {
    super(subject, address);

    // Create conduit.castX() bidings.
    for (let name of address.cast || []) {
      this[`cast${name}`] = this._cast.bind(this, name);
    }

    // Wants to know when conduits with a specific attribute are closed.
    // `subject.recvConduitClosed(address)` method will be called.
    if (address.reportOnClosed) {
      Hub.reportOnClosed.set(address.reportOnClosed, this);
    }

    this.open = true;
    Hub.openConduit(this);
  }

  /**
   * Internal, sends a message to a specific conduit, used by sendX stubs.
   *
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

    let sender = this.id;
    let { actor } = Hub.remotes.get(target);

    if (method === "RunListener" && arg.path.startsWith("webRequest.")) {
      return actor.batch(method, { target, arg, query, sender });
    }
    return super._send(method, query, actor, { target, arg, query, sender });
  }

  /**
   * Broadcasts a method call to all conduits of kind that satisfy filtering by
   * kind-specific properties from arg, returns an array of response promises.
   *
   * @param {string} method
   * @param {BroadcastKind} kind
   * @param {object} arg
   * @returns {Promise<any[]> | Promise<Response>}
   */
  _cast(method, kind, arg) {
    let filters = {
      // Target Ports by portId and side (connect caller/onConnect receiver).
      port: remote =>
        remote.portId === arg.portId &&
        (arg.source == null || remote.source === arg.source),

      // Target Messengers in extension pages by extensionId and envType.
      messenger: r =>
        r.verified &&
        r.id !== arg.sender.contextId &&
        r.extensionId === arg.extensionId &&
        r.recv.includes(method) &&
        // TODO: Bug 1453343 - get rid of this:
        (r.envType === "addon_child" || arg.sender.envType !== "content_child"),

      // Target Messengers by extensionId, tabId (topBC) and frameId.
      tab: remote =>
        remote.extensionId === arg.extensionId &&
        remote.actor.manager.browsingContext?.top.id === arg.topBC &&
        (arg.frameId == null || remote.frameId === arg.frameId) &&
        remote.recv.includes(method),

      // Target Messengers by extensionId.
      extension: remote => remote.instanceId === arg.instanceId,
    };

    let targets = Array.from(Hub.remotes.values()).filter(filters[kind]);
    let promises = targets.map(c => this._send(method, true, c.id, arg));

    return arg.firstResponse
      ? this._raceResponses(promises)
      : Promise.allSettled(promises);
  }

  /**
   * Custom Promise.race() function that ignores certain resolutions and errors.
   *
   * @typedef {{response?: any, received?: boolean}} Response
   *
   * @param {Promise<Response>[]} promises
   * @returns {Promise<Response?>}
   */
  _raceResponses(promises) {
    return new Promise((resolve, reject) => {
      let result;
      promises.map(p =>
        p
          .then(value => {
            if (value.response) {
              // We have an explicit response, resolve immediately.
              resolve(value);
            } else if (value.received) {
              // Message was received, but no response.
              // Resolve with this only if there is no other explicit response.
              result = value;
            }
          })
          .catch(err => {
            // Forward errors that are exposed to extension, but ignore
            // internal errors such as actor destruction and DataCloneError.
            if (err instanceof ExtensionError || err?.mozWebExtLocation) {
              reject(err);
            } else {
              Cu.reportError(err);
            }
          })
      );
      // Ensure resolving when there are no responses.
      Promise.allSettled(promises).then(() => resolve(result));
    });
  }

  async close() {
    this.open = false;
    Hub.closeConduit(this);
  }
}

/**
 * Implements the parent side of the Conduits actor.
 */
export class ConduitsParent extends JSWindowActorParent {
  constructor() {
    super();
    this.batchData = [];
    this.batchPromise = null;
    this.batchResolve = null;
    this.timerActive = false;
  }

  /**
   * Group webRequest events to send them as a batch, reducing IPC overhead.
   *
   * @param {string} name
   * @param {import("ConduitsChild.sys.mjs").MessageData} data
   * @returns {Promise<object>}
   */
  batch(name, data) {
    let pos = this.batchData.length;
    this.batchData.push(data);

    let sendNow = idleDispatch => {
      if (this.batchData.length && this.manager) {
        this.batchResolve(this.sendQuery(name, this.batchData));
      } else {
        this.batchResolve([]);
      }
      this.batchData = [];
      this.timerActive = !idleDispatch;
    };

    if (!pos) {
      this.batchPromise = new Promise(r => (this.batchResolve = r));
      if (!this.timerActive) {
        ChromeUtils.idleDispatch(sendNow, { timeout: BATCH_TIMEOUT_MS });
        this.timerActive = true;
      }
    }

    if (data.arg.urgentSend) {
      // If this is an urgent blocking event, run this batch right away.
      sendNow(false);
    }

    return this.batchPromise.then(results => results[pos]);
  }

  /**
   * JSWindowActor method, routes the message to the target subject.
   *
   * @param {object} options
   * @param {string} options.name
   * @param {import("ConduitsChild.sys.mjs").MessageData} options.data
   * @returns {Promise?}
   */
  async receiveMessage({ name, data: { arg, query, sender } }) {
    if (name === "ConduitOpened") {
      return Hub.recvConduitOpened(arg, this);
    }

    let remote = Hub.remotes.get(sender);
    if (!remote || remote.actor !== this) {
      throw new Error(`Unknown sender or wrong actor for recv${name}`);
    }

    if (name === "ConduitClosed") {
      return Hub.recvConduitClosed(remote);
    }

    let conduit = Hub.byMethod.get(name);
    if (!conduit) {
      throw new Error(`Parent conduit for recv${name} not found`);
    }

    return conduit._recv(name, arg, { actor: this, query, sender: remote });
  }

  /**
   * JSWindowActor method, ensure cleanup.
   */
  didDestroy() {
    Hub.actorClosed(this);
  }
}

/**
 * Parent side of the Conduits process actor.  Same code as JSWindowActor.
 */
export class ProcessConduitsParent extends JSProcessActorParent {
  receiveMessage = ConduitsParent.prototype.receiveMessage;
  willDestroy = ConduitsParent.prototype.willDestroy;
  didDestroy = ConduitsParent.prototype.didDestroy;
}
