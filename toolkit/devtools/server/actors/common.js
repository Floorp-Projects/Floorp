/* -*- tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Methods shared between RootActor and BrowserTabActor.
 */

/**
 * Populate |this._extraActors| as specified by |aFactories|, reusing whatever
 * actors are already there. Add all actors in the final extra actors table to
 * |aPool|.
 *
 * The root actor and the tab actor use this to instantiate actors that other
 * parts of the browser have specified with DebuggerServer.addTabActor antd
 * DebuggerServer.addGlobalActor.
 *
 * @param aFactories
 *     An object whose own property names are the names of properties to add to
 *     some reply packet (say, a tab actor grip or the "listTabs" response
 *     form), and whose own property values are actor constructor functions, as
 *     documented for addTabActor and addGlobalActor.
 *
 * @param this
 *     The BrowserRootActor or BrowserTabActor with which the new actors will
 *     be associated. It should support whatever API the |aFactories|
 *     constructor functions might be interested in, as it is passed to them.
 *     For the sake of CommonCreateExtraActors itself, it should have at least
 *     the following properties:
 *
 *     - _extraActors
 *        An object whose own property names are factory table (and packet)
 *        property names, and whose values are no-argument actor constructors,
 *        of the sort that one can add to an ActorPool.
 *
 *     - conn
 *        The DebuggerServerConnection in which the new actors will participate.
 *
 *     - actorID
 *        The actor's name, for use as the new actors' parentID.
 */
exports.createExtraActors = function createExtraActors(aFactories, aPool) {
  // Walk over global actors added by extensions.
  for (let name in aFactories) {
    let actor = this._extraActors[name];
    if (!actor) {
      actor = aFactories[name].bind(null, this.conn, this);
      actor.prototype = aFactories[name].prototype;
      actor.parentID = this.actorID;
      this._extraActors[name] = actor;
    }
    aPool.addActor(actor);
  }
}

/**
 * Append the extra actors in |this._extraActors|, constructed by a prior call
 * to CommonCreateExtraActors, to |aObject|.
 *
 * @param aObject
 *     The object to which the extra actors should be added, under the
 *     property names given in the |aFactories| table passed to
 *     CommonCreateExtraActors.
 *
 * @param this
 *     The BrowserRootActor or BrowserTabActor whose |_extraActors| table we
 *     should use; see above.
 */
exports.appendExtraActors = function appendExtraActors(aObject) {
  for (let name in this._extraActors) {
    let actor = this._extraActors[name];
    aObject[name] = actor.actorID;
  }
}

/**
 * Construct an ActorPool.
 *
 * ActorPools are actorID -> actor mapping and storage.  These are
 * used to accumulate and quickly dispose of groups of actors that
 * share a lifetime.
 */
function ActorPool(aConnection)
{
  this.conn = aConnection;
  this._cleanups = {};
  this._actors = {};
}

ActorPool.prototype = {
  /**
   * Add an actor to the actor pool.  If the actor doesn't have an ID,
   * allocate one from the connection.
   *
   * @param aActor object
   *        The actor implementation.  If the object has a
   *        'disconnect' property, it will be called when the actor
   *        pool is cleaned up.
   */
  addActor: function AP_addActor(aActor) {
    aActor.conn = this.conn;
    if (!aActor.actorID) {
      let prefix = aActor.actorPrefix;
      if (typeof aActor == "function") {
        // typeName is a convention used with protocol.js-based actors
        prefix = aActor.prototype.actorPrefix || aActor.prototype.typeName;
      }
      aActor.actorID = this.conn.allocID(prefix || undefined);
    }

    if (aActor.registeredPool) {
      aActor.registeredPool.removeActor(aActor);
    }
    aActor.registeredPool = this;

    this._actors[aActor.actorID] = aActor;
    if (aActor.disconnect) {
      this._cleanups[aActor.actorID] = aActor;
    }
  },

  get: function AP_get(aActorID) {
    return this._actors[aActorID] || undefined;
  },

  has: function AP_has(aActorID) {
    return aActorID in this._actors;
  },

  /**
   * Returns true if the pool is empty.
   */
  isEmpty: function AP_isEmpty() {
    return Object.keys(this._actors).length == 0;
  },

  /**
   * Remove an actor from the actor pool.
   */
  removeActor: function AP_remove(aActor) {
    delete this._actors[aActor.actorID];
    delete this._cleanups[aActor.actorID];
  },

  /**
   * Match the api expected by the protocol library.
   */
  unmanage: function(aActor) {
    return this.removeActor(aActor);
  },

  /**
   * Run all actor cleanups.
   */
  cleanup: function AP_cleanup() {
    for each (let actor in this._cleanups) {
      actor.disconnect();
    }
    this._cleanups = {};
  }
}

exports.ActorPool = ActorPool;

