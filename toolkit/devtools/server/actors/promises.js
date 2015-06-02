/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const protocol = require("devtools/server/protocol");
const { method, RetVal, Arg, types } = protocol;

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
  },

  destroy: function() {
    protocol.Actor.prototype.destroy.call(this, conn);
  },
});

exports.PromisesActor = PromisesActor;

exports.PromisesFront = protocol.FrontClass(PromisesActor, {
  initialize: function(client, form) {
    protocol.Front.prototype.initialize.call(this, client, form);
    this.actorID = form.promisesActor;
    this.manage(this);
  }
});
