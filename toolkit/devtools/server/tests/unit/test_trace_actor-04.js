/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that enteredFrame packets are sent on frame entry and
 * exitedFrame packets are sent on frame exit. Tests that the "name"
 * trace type works for function declarations.
 */

let {defer} = devtools.require("sdk/core/promise");

var gDebuggee;
var gClient;
var gTraceClient;

function run_test()
{
  initTestTracerServer();
  gDebuggee = addTestGlobal("test-tracer-actor");
  gClient = new DebuggerClient(DebuggerServer.connectPipe());
  gClient.connect(function() {
    attachTestTab(gClient, "test-tracer-actor", function(aResponse, aTabClient) {
      gClient.attachTracer(aResponse.traceActor, function(aResponse, aTraceClient) {
        gTraceClient = aTraceClient;
        test_enter_exit_frame();
      });
    });
  });
  do_test_pending();
}

function test_enter_exit_frame()
{
  let packetsSeen = 0;
  let packetNames = [];

  gTraceClient.addListener("enteredFrame", function(aEvent, aPacket) {
    packetsSeen++;
    do_check_eq(aPacket.type, "enteredFrame",
                'enteredFrame response should have type "enteredFrame"');
    do_check_eq(typeof aPacket.sequence, "number",
                'enteredFrame response should have sequence number');
    do_check_true(!isNaN(aPacket.sequence),
                  'enteredFrame sequence should be a number');
    do_check_eq(typeof aPacket.name, "string",
                'enteredFrame response should have function name');
    packetNames[aPacket.sequence] = aPacket.name;
  });

  gTraceClient.addListener("exitedFrame", function(aEvent, aPacket) {
    packetsSeen++;
    do_check_eq(aPacket.type, "exitedFrame",
                'exitedFrame response should have type "exitedFrame"');
    do_check_eq(typeof aPacket.sequence, "number",
                'exitedFrame response should have sequence number');
    do_check_true(!isNaN(aPacket.sequence),
                  'exitedFrame sequence should be a number');
  });

  start_trace()
    .then(eval_code)
    .then(stop_trace)
    .then(function() {
      do_check_eq(packetsSeen, 10,
                  'Should have seen two packets for each of 5 stack frames');
      do_check_eq(packetNames[2], "baz",
                  'Should have entered "baz" frame in third packet');
      do_check_eq(packetNames[3], "bar",
                  'Should have entered "bar" frame in fourth packet');
      do_check_eq(packetNames[4], "foo",
                  'Should have entered "foo" frame in fifth packet');
      finishClient(gClient);
    });
}

function start_trace()
{
  let deferred = defer();
  gTraceClient.startTrace(["name"], null, function() { deferred.resolve(); });
  return deferred.promise;
}

function eval_code()
{
  gDebuggee.eval("(" + function() {
    function foo() {
      return;
    }
    function bar() {
      return foo();
    }
    function baz() {
      return bar();
    }
    baz();
  } + ")()");
}

function stop_trace()
{
  let deferred = defer();
  gTraceClient.stopTrace(null, function() { deferred.resolve(); });
  return deferred.promise;
}
