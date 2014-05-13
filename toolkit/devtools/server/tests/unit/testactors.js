/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const { ActorPool, appendExtraActors, createExtraActors } = require("devtools/server/actors/common");
const { RootActor } = require("devtools/server/actors/root");
const { ThreadActor } = require("devtools/server/actors/script");
const { DebuggerServer } = require("devtools/server/main");

var gTestGlobals = [];
DebuggerServer.addTestGlobal = function(aGlobal) {
  gTestGlobals.push(aGlobal);
};

// A mock tab list, for use by tests. This simply presents each global in
// gTestGlobals as a tab, and the list is fixed: it never calls its
// onListChanged handler.
//
// As implemented now, we consult gTestGlobals when we're constructed, not
// when we're iterated over, so tests have to add their globals before the
// root actor is created.
function TestTabList(aConnection) {
  this.conn = aConnection;

  // An array of actors for each global added with
  // DebuggerServer.addTestGlobal.
  this._tabActors = [];

  // A pool mapping those actors' names to the actors.
  this._tabActorPool = new ActorPool(aConnection);

  for (let global of gTestGlobals) {
    let actor = new TestTabActor(aConnection, global);
    actor.selected = false;
    this._tabActors.push(actor);
    this._tabActorPool.addActor(actor);
  }
  if (this._tabActors.length > 0) {
    this._tabActors[0].selected = true;
  }

  aConnection.addActorPool(this._tabActorPool);
}

TestTabList.prototype = {
  constructor: TestTabList,
  getList: function () {
    return promise.resolve([tabActor for (tabActor of this._tabActors)]);
  }
};

function createRootActor(aConnection)
{
  let root = new RootActor(aConnection,
                           { tabList: new TestTabList(aConnection) });
  root.applicationType = "xpcshell-tests";
  return root;
}

function TestTabActor(aConnection, aGlobal)
{
  this.conn = aConnection;
  this._global = aGlobal;
  this._global.wrappedJSObject = aGlobal;
  this._threadActor = new ThreadActor(this, this._global);
  this.conn.addActor(this._threadActor);
  this._attached = false;
  this._extraActors = {};
}

TestTabActor.prototype = {
  constructor: TestTabActor,
  actorPrefix: "TestTabActor",

  get window() {
    return { wrappedJSObject: this._global };
  },

  get url() {
    return this._global.__name;
  },

  form: function() {
    let response = { actor: this.actorID, title: this._global.__name };

    // Walk over tab actors added by extensions and add them to a new ActorPool.
    let actorPool = new ActorPool(this.conn);
    this._createExtraActors(DebuggerServer.tabActorFactories, actorPool);
    if (!actorPool.isEmpty()) {
      this._tabActorPool = actorPool;
      this.conn.addActorPool(this._tabActorPool);
    }

    this._appendExtraActors(response);

    return response;
  },

  onAttach: function(aRequest) {
    this._attached = true;

    let response = { type: "tabAttached", threadActor: this._threadActor.actorID };
    this._appendExtraActors(response);

    return response;
  },

  onDetach: function(aRequest) {
    if (!this._attached) {
      return { "error":"wrongState" };
    }
    return { type: "detached" };
  },

  /* Support for DebuggerServer.addTabActor. */
  _createExtraActors: createExtraActors,
  _appendExtraActors: appendExtraActors
};

TestTabActor.prototype.requestTypes = {
  "attach": TestTabActor.prototype.onAttach,
  "detach": TestTabActor.prototype.onDetach
};

exports.register = function(handle) {
  handle.setRootActor(createRootActor);
};

exports.unregister = function(handle) {
  handle.setRootActor(null);
};
