/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Creates "registered" actors factory meant for creating another kind of
 * factories, ObservedActorFactory, during the call to listTabs.
 * These factories live in DebuggerServer.{tab|global}ActorFactories.
 *
 * These actors only exposes:
 * - `name` string attribute used to match actors by constructor name
 *   in DebuggerServer.remove{Global,Tab}Actor.
 * - `createObservedActorFactory` function to create "observed" actors factory
 *
 * @param options object, function
 *        Either an object or a function.
 *        If given an object:
 *
 *        If given a function (deprecated):
 *          Constructor function of an actor.
 *          The constructor function for this actor type.
 *          This expects to be called as a constructor (i.e. with 'new'),
 *          and passed two arguments: the DebuggerServerConnection, and
 *          the BrowserTabActor with which it will be associated.
 *          Only used for deprecated eagerly loaded actors.
 *
 */
function RegisteredActorFactory(options, prefix) {
  // By default the actor name will also be used for the actorID prefix.
  this._prefix = prefix;
  if (typeof(options) != "function") {
    // actors definition registered by actorRegistryActor
    if (options.constructorFun) {
      this._getConstructor = () => options.constructorFun;
    } else {
      // Lazy actor definition, where options contains all the information
      // required to load the actor lazily.
      this._getConstructor = function () {
        // Load the module
        let mod;
        try {
          mod = require(options.id);
        } catch(e) {
          throw new Error("Unable to load actor module '" + options.id + "'.\n" +
                          e.message + "\n" + e.stack + "\n");
        }
        // Fetch the actor constructor
        let c = mod[options.constructorName];
        if (!c) {
          throw new Error("Unable to find actor constructor named '" +
                          options.constructorName + "'. (Is it exported?)");
        }
        return c;
      };
    }
    // Exposes `name` attribute in order to allow removeXXXActor to match
    // the actor by its actor constructor name.
    this.name = options.constructorName;
  } else {
    // Old actor case, where options is a function that is the actor constructor.
    this._getConstructor = () => options;
    // Exposes `name` attribute in order to allow removeXXXActor to match
    // the actor by its actor constructor name.
    this.name = options.name;

    // For old actors, we allow the use of a different prefix for actorID
    // than for listTabs actor names, by fetching a prefix on the actor prototype.
    // (Used by ChromeDebuggerActor)
    if (options.prototype && options.prototype.actorPrefix) {
      this._prefix = options.prototype.actorPrefix;
    }
  }
}
RegisteredActorFactory.prototype.createObservedActorFactory = function (conn, parentActor) {
  return new ObservedActorFactory(this._getConstructor, this._prefix, conn, parentActor);
}
exports.RegisteredActorFactory = RegisteredActorFactory;

/**
 * Creates "observed" actors factory meant for creating real actor instances.
 * These factories lives in actor pools and fake various actor attributes.
 * They will be replaced in actor pools by final actor instances during
 * the first request for the same actorID from DebuggerServer._getOrCreateActor.
 *
 * ObservedActorFactory fakes the following actors attributes:
 *   actorPrefix (string) Used by ActorPool.addActor to compute the actor id
 *   actorID (string) Set by ActorPool.addActor just after being instantiated
 *   registeredPool (object) Set by ActorPool.addActor just after being
 *                           instantiated
 * And exposes the following method:
 *   createActor (function) Instantiate an actor that is going to replace
 *                          this factory in the actor pool.
 */
function ObservedActorFactory(getConstructor, prefix, conn, parentActor) {
  this._getConstructor = getConstructor;
  this._conn = conn;
  this._parentActor = parentActor;

  this.actorPrefix = prefix;

  this.actorID = null;
  this.registeredPool = null;
}
ObservedActorFactory.prototype.createActor = function () {
  // Fetch the actor constructor
  let c = this._getConstructor();
  // Instantiate a new actor instance
  let instance = new c(this._conn, this._parentActor);
  instance.conn = this._conn;
  instance.parentID = this._parentActor.actorID;
  // We want the newly-constructed actor to completely replace the factory
  // actor. Reusing the existing actor ID will make sure ActorPool.addActor
  // does the right thing.
  instance.actorID = this.actorID;
  this.registeredPool.addActor(instance);
  return instance;
}
exports.ObservedActorFactory = ObservedActorFactory;


/**
 * Methods shared between RootActor and BrowserTabActor.
 */

/**
 * Populate |this._extraActors| as specified by |aFactories|, reusing whatever
 * actors are already there. Add all actors in the final extra actors table to
 * |aPool|.
 *
 * The root actor and the tab actor use this to instantiate actors that other
 * parts of the browser have specified with DebuggerServer.addTabActor and
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
      // Register another factory, but this time specific to this connection.
      // It creates a fake actor that looks like an regular actor in the pool,
      // but without actually instantiating the actor.
      // It will only be instantiated on the first request made to the actor.
      actor = aFactories[name].createObservedActorFactory(this.conn, this);
      this._extraActors[name] = actor;
    }

    // If the actor already exists in the pool, it may have been instantiated,
    // so make sure not to overwrite it by a non-instantiated version.
    if (!aPool.has(actor.actorID)) {
      aPool.addActor(actor);
    }
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
      if (!prefix && typeof aActor == "function") {
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
  },

  forEach: function(callback) {
    for (let name in this._actors) {
      callback(this._actors[name]);
    }
  },
}

exports.ActorPool = ActorPool;

// TODO bug 863089: use Debugger.Script.prototype.getOffsetColumn when it is
// implemented.
exports.getOffsetColumn = function getOffsetColumn(aOffset, aScript) {
  let bestOffsetMapping = null;
  for (let offsetMapping of aScript.getAllColumnOffsets()) {
    if (!bestOffsetMapping ||
        (offsetMapping.offset <= aOffset &&
         offsetMapping.offset > bestOffsetMapping.offset)) {
      bestOffsetMapping = offsetMapping;
    }
  }

  if (!bestOffsetMapping) {
    // XXX: Try not to completely break the experience of using the debugger for
    // the user by assuming column 0. Simultaneously, report the error so that
    // there is a paper trail if the assumption is bad and the debugging
    // experience becomes wonky.
    reportError(new Error("Could not find a column for offset " + aOffset
                          + " in the script " + aScript));
    return 0;
  }

  return bestOffsetMapping.columnNumber;
}
