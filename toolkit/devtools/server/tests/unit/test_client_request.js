/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test the DebuggerClient.request API.

var gClient;

function TestActor(conn) {
  this.conn = conn;
}
TestActor.prototype = {
  actorPrefix: "test",

  hello: function () {
    return {hello: "world"};
  },

  error: function () {
    return {error: "code", message: "human message"};
  }
};
TestActor.prototype.requestTypes = {
  "hello": TestActor.prototype.hello,
  "error": TestActor.prototype.error
};

function run_test()
{
  DebuggerServer.addGlobalActor(TestActor);

  DebuggerServer.init();
  DebuggerServer.addBrowserActors();

  add_test(init);
  add_test(test_client_request_callback);
  add_test(test_client_request_promise);
  add_test(test_client_request_promise_error);
  add_test(test_client_request_event_emitter);
  add_test(close_client);
  run_next_test();
}

function init()
{
  gClient = new DebuggerClient(DebuggerServer.connectPipe());
  gClient.connect(function onConnect() {
    gClient.listTabs(function onListTabs(aResponse) {
      gActorId = aResponse.test;
      run_next_test();
    });
  });
}

function checkStack(expectedName) {
  if (!Services.prefs.getBoolPref("javascript.options.asyncstack")) {
    do_print("Async stacks are disabled.");
    return;
  }

  let stack = Components.stack;
  while (stack) {
    do_print(stack.name);
    if (stack.name == expectedName) {
      // Reached back to outer function before request
      ok(true, "Complete stack");
      return;
    }
    stack = stack.asyncCaller || stack.caller;
  }
  ok(false, "Incomplete stack");
}

function test_client_request_callback()
{
  // Test that DebuggerClient.request accepts a `onResponse` callback as 2nd argument
  gClient.request({
    to: gActorId,
    type: "hello"
  }, response => {
    do_check_eq(response.from, gActorId);
    do_check_eq(response.hello, "world");
    checkStack("test_client_request_callback");
    run_next_test();
  });
}

function test_client_request_promise()
{
  // Test that DebuggerClient.request returns a promise that resolves on response
  let request = gClient.request({
    to: gActorId,
    type: "hello"
  });

  request.then(response => {
    do_check_eq(response.from, gActorId);
    do_check_eq(response.hello, "world");
    checkStack("test_client_request_promise");
    run_next_test();
  });
}

function test_client_request_promise_error()
{
  // Test that DebuggerClient.request returns a promise that reject when server
  // returns an explicit error message
  let request = gClient.request({
    to: gActorId,
    type: "error"
  });

  request.then(() => {
    do_throw("Promise shouldn't be resolved on error");
  }, response => {
    do_check_eq(response.from, gActorId);
    do_check_eq(response.error, "code");
    do_check_eq(response.message, "human message");
    checkStack("test_client_request_promise_error");
    run_next_test();
  });
}

function test_client_request_event_emitter()
{
  // Test that DebuggerClient.request returns also an EventEmitter object
  let request = gClient.request({
    to: gActorId,
    type: "hello"
  });
  request.on("json-reply", reply => {
    do_check_eq(reply.from, gActorId);
    do_check_eq(reply.hello, "world");
    checkStack("test_client_request_event_emitter");
    run_next_test();
  });
}

function close_client() {
  gClient.close(() => {
    run_next_test()
  });
}
