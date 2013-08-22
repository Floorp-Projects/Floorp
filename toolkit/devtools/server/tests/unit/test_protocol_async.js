/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure we get replies in the same order that we sent their
 * requests even when earlier requests take several event ticks to
 * complete.
 */

let protocol = devtools.require("devtools/server/protocol");
let {method, Arg, Option, RetVal} = protocol;
let promise = devtools.require("sdk/core/promise");
let events = devtools.require("sdk/event/core");

function simpleHello() {
  return {
    from: "root",
    applicationType: "xpcshell-tests",
    traits: [],
  }
}

let RootActor = protocol.ActorClass({
  typeName: "root",
  initialize: function(conn) {
    protocol.Actor.prototype.initialize.call(this, conn);
    // Root actor owns itself.
    this.manage(this);
    this.actorID = "root";
    this.sequence = 0;
  },

  sayHello: simpleHello,

  simpleReturn: method(function() {
    return this.sequence++;
  }, {
    response: { value: RetVal() },
  }),

  promiseReturn: method(function(toWait) {
    // Guarantee that this resolves after simpleReturn returns.
    let deferred = promise.defer();
    let sequence = this.sequence++;

    // Wait until the number of requests specified by toWait have
    // happened, to test queuing.
    let check = () => {
      if ((this.sequence - sequence) < toWait) {
        do_execute_soon(check);
        return;
      }
      deferred.resolve(sequence);
    }
    do_execute_soon(check);

    return deferred.promise;
  }, {
    request: { toWait: Arg(0, "number") },
    response: { value: RetVal("number") },
  }),

  simpleThrow: method(function() {
    throw new Error(this.sequence++);
  }, {
    response: { value: RetVal("number") }
  }),

  promiseThrow: method(function() {
    // Guarantee that this resolves after simpleReturn returns.
    let deferred = promise.defer();
    let sequence = this.sequence++;
    // This should be enough to force a failure if the code is broken.
    do_timeout(150, () => {
      deferred.reject(sequence++);
    });
    return deferred.promise;
  }, {
    response: { value: RetVal("number") },
  })
});

let RootFront = protocol.FrontClass(RootActor, {
  initialize: function(client) {
    this.actorID = "root";
    protocol.Front.prototype.initialize.call(this, client);
    // Root owns itself.
    this.manage(this);
  }
});

function run_test()
{
  DebuggerServer.createRootActor = RootActor;
  DebuggerServer.init(() => true);

  let trace = connectPipeTracing();
  let client = new DebuggerClient(trace);
  let rootClient;

  client.connect((applicationType, traits) => {
    rootClient = RootFront(client);

    let calls = [];
    let sequence = 0;

    // Execute a call that won't finish processing until 2
    // more calls have happened
    calls.push(rootClient.promiseReturn(2).then(ret => {
      do_check_eq(sequence, 0); // Check right return order
      do_check_eq(ret, sequence++); // Check request handling order
    }));

    // Put a few requests into the backlog

    calls.push(rootClient.simpleReturn().then(ret => {
      do_check_eq(sequence, 1); // Check right return order
      do_check_eq(ret, sequence++); // Check request handling order
    }));

    calls.push(rootClient.simpleReturn().then(ret => {
      do_check_eq(sequence, 2); // Check right return order
      do_check_eq(ret, sequence++); // Check request handling order
    }));

    calls.push(rootClient.simpleThrow().then(() => {
      do_check_true(false, "simpleThrow shouldn't succeed!");
    }, error => {
      do_check_eq(sequence++, 3); // Check right return order
      return promise.resolve(null);
    }));

    calls.push(rootClient.promiseThrow().then(() => {
      do_check_true(false, "promiseThrow shouldn't succeed!");
    }, error => {
      do_check_eq(sequence++, 4); // Check right return order
      do_check_true(true, "simple throw should throw");
      return promise.resolve(null);
    }));

    calls.push(rootClient.simpleReturn().then(ret => {
      do_check_eq(sequence, 5); // Check right return order
      do_check_eq(ret, sequence++); // Check request handling order
    }));

    // Break up the backlog with a long request that waits
    // for another simpleReturn before completing
    calls.push(rootClient.promiseReturn(1).then(ret => {
      do_check_eq(sequence, 6); // Check right return order
      do_check_eq(ret, sequence++); // Check request handling order
    }));

    calls.push(rootClient.simpleReturn().then(ret => {
      do_check_eq(sequence, 7); // Check right return order
      do_check_eq(ret, sequence++); // Check request handling order
    }));

    promise.all.apply(null, calls).then(() => {
      client.close(() => {
        do_test_finished();
      });
    })
  });
  do_test_pending();
}
