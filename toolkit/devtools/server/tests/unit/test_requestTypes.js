/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

function test_requestTypes_request(aClient, anActor)
{
  var calls = [];

  calls.push(test_existent_actor(aClient, anActor));

  promise.all(calls).then(() => {
    aClient.close(() => {
      do_test_finished();
    });
  });
}

function test_existent_actor(aClient, anActor)
{
  let deferred = promise.defer();

  aClient.request({ to: anActor, type: "requestTypes" }, function (aResponse) {
    var expectedRequestTypes = Object.keys(DebuggerServer.
                                           globalActorFactories["chromeDebugger"].
                                           prototype.requestTypes);

    do_check_true(Array.isArray(aResponse.requestTypes));
    do_check_eq(JSON.stringify(aResponse.requestTypes),
                JSON.stringify(expectedRequestTypes));

    deferred.resolve();
  });

  return deferred.promise;
}

function run_test()
{
  DebuggerServer.init(function () { return true; });
  DebuggerServer.addBrowserActors();
  var client = new DebuggerClient(DebuggerServer.connectPipe());
  client.connect(function() {
    client.listTabs(function(aResponse) {
      test_requestTypes_request(client, aResponse.chromeDebugger);
    });
  });

  do_test_pending();
}
