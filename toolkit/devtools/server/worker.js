"use strict"

loadSubScript("resource://gre/modules/devtools/worker-loader.js");

let { ActorPool } = worker.require("devtools/server/actors/common");
let { ThreadActor } = worker.require("devtools/server/actors/script");
let { TabSources } = worker.require("devtools/server/actors/utils/TabSources");
let makeDebugger = worker.require("devtools/server/actors/utils/make-debugger");
let { DebuggerServer } = worker.require("devtools/server/main");

DebuggerServer.init();
DebuggerServer.createRootActor = function () {
  throw new Error("Should never get here!");
};

let connections = Object.create(null);

this.addEventListener("message",  function (event) {
  let packet = JSON.parse(event.data);
  switch (packet.type) {
  case "connect":
    // Step 3: Create a connection to the parent.
    let connection = DebuggerServer.connectToParent(packet.id, this);
    connections[packet.id] = connection;

    // Step 4: Create a thread actor for the connection to the parent.
    let pool = new ActorPool(connection);
    connection.addActorPool(pool);

    let sources = null;

    let actor = new ThreadActor({
      makeDebugger: makeDebugger.bind(null, {
        findDebuggees: () => {
          return [this.global];
        },

        shouldAddNewGlobalAsDebuggee: () => {
          return true;
        },
      }),

      get sources() {
        if (sources === null) {
          sources = new TabSources(actor);
        }
        return sources;
      }
    }, global);

    pool.addActor(actor);

    // Step 5: Attach to the thread actor.
    //
    // This will cause a packet to be sent over the connection to the parent.
    // Because this connection uses WorkerDebuggerTransport internally, this
    // packet will be sent using WorkerDebuggerGlobalScope.postMessage, causing
    // an onMessage event to be fired on the WorkerDebugger in the main thread.
    actor.onAttach({});
    break;

  case "disconnect":
    connections[packet.id].close();
    break;
  };
});
