/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["Session"];

const { ParentProcessDomains } = ChromeUtils.import(
  "chrome://remote/content/domains/ParentProcessDomains.jsm"
);
const { DomainCache } = ChromeUtils.import(
  "chrome://remote/content/domains/DomainCache.jsm"
);

const { NetworkObserver } = ChromeUtils.import(
  "chrome://remote/content/observers/NetworkObserver.jsm"
);

/**
 * A session represents exactly one client WebSocket connection.
 *
 * Every new WebSocket connections is associated with one session that
 * deals with dispatching incoming command requests to the right
 * target, sending back responses, and propagating events originating
 * from domains.
 * Then, some subsequent Sessions may be created over a single WebSocket
 * connection. In this case, the subsequent session will have an `id`
 * being passed to their constructor and each packet of these sessions
 * will have a `sessionId` attribute in order to filter the packets
 * by session both on client and server side.
 */
class Session {
  /**
   * @param Connection connection
   *        The connection used to communicate with the server.
   * @param Target target
   *        The target to which this session communicates with.
   * @param Number id (optional)
   *        If this session isn't the default one used for the HTTP endpoint we
   *        connected to, the session requires an id to distinguish it from the default
   *        one. This id is used to filter our request, responses and events between
   *        all active sessions. For now, this is only passed by `Target.attachToTarget()`.
   */
  constructor(connection, target, id) {
    this.connection = connection;
    this.target = target;
    this.id = id;

    this.domains = new DomainCache(this, ParentProcessDomains);
  }

  destructor() {
    if (
      this.networkObserver &&
      this.networkObserver.isActive(this.target.browser)
    ) {
      this.networkObserver.dispose();
    }
    this.domains.clear();
  }

  execute(id, domain, command, params) {
    return this.domains.execute(domain, command, params);
  }

  get networkObserver() {
    if (!this._networkObserver) {
      this._networkObserver = new NetworkObserver();
    }
    return this._networkObserver;
  }

  /**
   * Domains event listener. Called when an event is fired
   * by any Domain and has to be sent to the client.
   */
  onEvent(eventName, params) {
    this.connection.onEvent(eventName, params, this.id);
  }

  toString() {
    return `[object ${this.constructor.name} ${this.connection.id}]`;
  }
}
