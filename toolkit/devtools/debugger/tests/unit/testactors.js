/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

var gTestGlobals = [];

function createRootActor()
{
  let actor = {
    sayHello: function() {
      this._globalActors = [];
      for each (let g in gTestGlobals) {
        let addBreakpoint = function _addBreakpoint(aActor) {
          this.conn.addActor(aActor);
        }.bind(this);
        let removeBreakpoint = function _removeBreakpoint(aActor) {
          this.conn.removeActor(aActor);
        }.bind(this);
        let hooks = {
          addToBreakpointPool: addBreakpoint,
          removeFromBreakpointPool: removeBreakpoint
        };
        let actor = new ThreadActor(hooks);
        actor.addDebuggee(g);
        actor._global = g;
        actor.json = function() {
          return { actor: actor.actorID,
                   threadActor: actor.actorID,
                   global: actor._global.__name };
        };
        this.conn.addActor(actor);
        this._globalActors.push(actor);
      }

      return { from: "root",
               applicationType: "xpcshell-tests",
               testConnectionPrefix: this.conn.prefix };
    },

    listGlobals: function(aRequest) {
      return { from: "root", contexts: [ g.json() for each (g in this._globalActors) ]};
    },
  };

  actor.requestTypes = {
    "listContexts": actor.listGlobals,
    "echo": function(aRequest) { return aRequest; },
  };
  return actor;
}

DebuggerServer.addTestGlobal = function addTestGlobal(aGlobal)
{
  gTestGlobals.push(aGlobal);
}
