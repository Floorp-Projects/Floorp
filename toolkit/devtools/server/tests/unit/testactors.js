/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

var gTestGlobals = [];
DebuggerServer.addTestGlobal = function(aGlobal) {
  gTestGlobals.push(aGlobal);
};

function createRootActor(aConnection)
{
  return new TestRootActor(aConnection);
}

function TestRootActor(aConnection)
{
  this.conn = aConnection;
  this.actorID = "root";

  // An array of actors for each global added with
  // DebuggerServer.addTestGlobal.
  this._tabActors = [];

  // A pool mapping those actors' names to the actors.
  this._tabActorPool = new ActorPool(aConnection);

  for (let global of gTestGlobals) {
    let actor = new TestTabActor(aConnection, global);
    this._tabActors.push(actor);
    this._tabActorPool.addActor(actor);
  }

  aConnection.addActorPool(this._tabActorPool);
}

TestRootActor.prototype = {
  constructor: TestRootActor,

  sayHello: function () {
    return { from: "root",
             applicationType: "xpcshell-tests",
             testConnectionPrefix: this.conn.prefix,
             traits: {
               sources: true
             }
           };
  },

  onListTabs: function(aRequest) {
    return { tabs:[actor.grip() for (actor of this._tabActors)], selected:0 };
  },

  onEcho: function(aRequest) { return aRequest; },
};

TestRootActor.prototype.requestTypes = {
  "listTabs": TestRootActor.prototype.onListTabs,
  "echo": TestRootActor.prototype.onEcho
};

function TestTabActor(aConnection, aGlobal)
{
  this.conn = aConnection;
  this._global = aGlobal;
  this._threadActor = new ThreadActor(this, this._global);
  this.conn.addActor(this._threadActor);
  this._attached = false;
}

TestTabActor.prototype = {
  constructor: TestTabActor,
  actorPrefix:"TestTabActor",

  grip: function() {
    return { actor: this.actorID, title: this._global.__name };
  },

  onAttach: function(aRequest) {
    this._attached = true;
    return { type: "tabAttached", threadActor: this._threadActor.actorID };
  },

  onDetach: function(aRequest) {
    if (!this._attached) {
      return { "error":"wrongState" };
    }
    return { type: "detached" };
  },

  // Hooks for use by TestTabActors.
  addToParentPool: function(aActor) {
    this.conn.addActor(aActor);
  },

  removeFromParentPool: function(aActor) {
    this.conn.removeActor(aActor);
  }
};

TestTabActor.prototype.requestTypes = {
  "attach": TestTabActor.prototype.onAttach,
  "detach": TestTabActor.prototype.onDetach
};
