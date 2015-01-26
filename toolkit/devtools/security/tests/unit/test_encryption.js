/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test basic functionality of DevTools client and server TLS encryption mode
function run_test() {
  // Need profile dir to store the key / cert
  do_get_profile();
  // Ensure PSM is initialized
  Cc["@mozilla.org/psm;1"].getService(Ci.nsISupports);
  run_next_test();
}

function connectClient(client) {
  let deferred = promise.defer();
  client.connect(() => {
    client.listTabs(deferred.resolve);
  });
  return deferred.promise;
}

add_task(function*() {
  initTestDebuggerServer();
});

// Client w/ encryption connects successfully to server w/ encryption
add_task(function*() {
  equal(DebuggerServer.listeningSockets, 0, "0 listening sockets");

  let AuthenticatorType = DebuggerServer.Authenticators.get("PROMPT");
  let authenticator = new AuthenticatorType.Server();
  authenticator.allowConnection = () => {
    return DebuggerServer.AuthenticationResult.ALLOW;
  };

  let listener = DebuggerServer.createListener();
  ok(listener, "Socket listener created");
  listener.portOrPath = -1 /* any available port */;
  listener.authenticator = authenticator;
  listener.encryption = true;
  yield listener.open();
  equal(DebuggerServer.listeningSockets, 1, "1 listening socket");

  let transport = yield DebuggerClient.socketConnect({
    host: "127.0.0.1",
    port: listener.port,
    encryption: true
  });
  ok(transport, "Client transport created");

  let client = new DebuggerClient(transport);
  let onUnexpectedClose = () => {
    do_throw("Closed unexpectedly");
  };
  client.addListener("closed", onUnexpectedClose);
  yield connectClient(client);

  // Send a message the server will echo back
  let message = "secrets";
  let reply = yield client.request({
    to: "root",
    type: "echo",
    message
  });
  equal(reply.message, message, "Encrypted echo matches");

  client.removeListener("closed", onUnexpectedClose);
  transport.close();
  listener.close();
  equal(DebuggerServer.listeningSockets, 0, "0 listening sockets");
});

// Client w/o encryption fails to connect to server w/ encryption
add_task(function*() {
  equal(DebuggerServer.listeningSockets, 0, "0 listening sockets");

  let AuthenticatorType = DebuggerServer.Authenticators.get("PROMPT");
  let authenticator = new AuthenticatorType.Server();
  authenticator.allowConnection = () => {
    return DebuggerServer.AuthenticationResult.ALLOW;
  };

  let listener = DebuggerServer.createListener();
  ok(listener, "Socket listener created");
  listener.portOrPath = -1 /* any available port */;
  listener.authenticator = authenticator;
  listener.encryption = true;
  yield listener.open();
  equal(DebuggerServer.listeningSockets, 1, "1 listening socket");

  try {
    yield DebuggerClient.socketConnect({
      host: "127.0.0.1",
      port: listener.port
      // encryption: false is the default
    });
  } catch(e) {
    ok(true, "Client failed to connect as expected");
    listener.close();
    equal(DebuggerServer.listeningSockets, 0, "0 listening sockets");
    return;
  }

  do_throw("Connection unexpectedly succeeded");
});

add_task(function*() {
  DebuggerServer.destroy();
});
