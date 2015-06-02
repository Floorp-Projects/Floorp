/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const protocol = require("devtools/server/protocol");
const { method, RetVal, Arg, types } = protocol;
const { expectState } = require("devtools/server/actors/common");

/**
 * The Promises Actor provides support for getting the list of live promises and
 * observing changes to their settlement state.
 */
let PromisesActor = protocol.ActorClass({
  typeName: "promises",

  /**
   * @param conn DebuggerServerConnection.
   * @param parent TabActor|RootActor
   */
  initialize: function(conn, parent) {
    protocol.Actor.prototype.initialize.call(this, conn);

    this.parent = parent;
    this.state = "detached";
    this._dbg = null;
  },

  destroy: function() {
    protocol.Actor.prototype.destroy.call(this, conn);

    if (this.state === "attached") {
      this.detach();
    }
  },

  get dbg() {
    if (!this._dbg) {
      this._dbg = this.parent.makeDebugger();
    }
    return this._dbg;
  },

  /**
   * Attach to the PromisesActor.
   */
  attach: method(expectState("detached", function() {
    this.dbg.addDebuggees();
    this.state = "attached";
  }, `attaching to the PromisesActor`), {
    request: {},
    response: {}
  }),

  /**
   * Detach from the PromisesActor upon Debugger closing.
   */
  detach: method(expectState("attached", function() {
    this.dbg.removeAllDebuggees();
    this.dbg.enabled = false;
    this._dbg = null;
    this.state = "detached";
  }, `detaching from the PromisesActor`), {
    request: {},
    response: {}
  }),
});

exports.PromisesActor = PromisesActor;

exports.PromisesFront = protocol.FrontClass(PromisesActor, {
  initialize: function(client, form) {
    protocol.Front.prototype.initialize.call(this, client, form);
    this.actorID = form.promisesActor;
    this.manage(this);
  }
});
