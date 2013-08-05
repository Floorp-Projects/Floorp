/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Simple tests for "location", "callsite", "time", "parameterNames",
 * "arguments", and "return" trace types.
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

function check_number(value, name)
{
  do_check_eq(typeof value, "number", name + ' should be a number');
  do_check_true(!isNaN(value), name + ' should be a number');
}

function check_location(actual, expected, name)
{
  do_check_eq(typeof actual, "object",
              name + ' missing expected source location');

  check_number(actual.line, name + ' line');
  check_number(actual.column, name + ' column');

  do_check_eq(actual.url, expected.url,
              name + ' location should have url ' + expected.url);
  do_check_eq(actual.line, expected.line,
              name + ' location should have source line of ' + expected.line);
  do_check_eq(actual.column, expected.column,
              name + ' location should have source column of ' + expected.line);

}

function test_enter_exit_frame()
{
  let packets = [];

  gTraceClient.addListener("enteredFrame", function(aEvent, aPacket) {
    do_check_eq(aPacket.type, "enteredFrame",
                'enteredFrame response should have type "enteredFrame"');
    do_check_eq(typeof aPacket.name, "string",
                'enteredFrame response should have function name');
    do_check_eq(typeof aPacket.location, "object",
                'enteredFrame response should have source location');

    check_number(aPacket.sequence, 'enteredFrame sequence');
    check_number(aPacket.time, 'enteredFrame time');
    check_number(aPacket.location.line, 'enteredFrame source line');
    check_number(aPacket.location.column, 'enteredFrame source column');
    if (aPacket.callsite) {
      check_number(aPacket.callsite.line, 'enteredFrame callsite line');
      check_number(aPacket.callsite.column, 'enteredFrame callsite column');
    }

    packets[aPacket.sequence] = aPacket;
  });

  gTraceClient.addListener("exitedFrame", function(aEvent, aPacket) {
    do_check_eq(aPacket.type, "exitedFrame",
                'exitedFrame response should have type "exitedFrame"');

    check_number(aPacket.sequence, 'exitedFrame sequence');
    check_number(aPacket.time, 'exitedFrame time');

    packets[aPacket.sequence] = aPacket;
  });

  start_trace()
    .then(eval_code)
    .then(stop_trace)
    .then(function() {
      let url = getFileUrl("tracerlocations.js");

      check_location(packets[0].location, { url: url, line: 1, column: 0 },
                    'global entry packet');

      do_check_eq(packets[1].name, "foo",
                  'Second packet in sequence should be entry to "foo" frame');

      // foo's definition is at tracerlocations.js:3:0, but
      // Debugger.Script does not provide complete definition
      // locations (bug 901138). tracerlocations.js:4:2 is the first
      // statement in the function (used as an approximation).
      check_location(packets[1].location, { url: url, line: 4, column: 2 },
                     'foo source');
      check_location(packets[1].callsite, { url: url, line: 8, column: 0 },
                     'foo callsite');

      do_check_eq(typeof packets[1].parameterNames, "object",
                  'foo entry packet should have parameterNames');
      do_check_eq(packets[1].parameterNames.length, 1,
                  'foo should have only one formal parameter');
      do_check_eq(packets[1].parameterNames[0], "x",
                  'foo should have formal parameter "x"');

      do_check_eq(typeof packets[1].arguments, "object",
                  'foo entry packet should have arguments');
      do_check_true(Array.isArray(packets[1].arguments),
                    'foo entry packet arguments should be an array');
      do_check_eq(packets[1].arguments.length, 1,
                  'foo should have only one actual parameter');
      do_check_eq(packets[1].arguments[0], 42,
                  'foo should have actual parameter 42');

      do_check_eq(typeof packets[2].return, "string",
                  'Fourth packet in sequence should be exit from "foo" frame');
      do_check_eq(packets[2].return, "bar",
                  'foo should return "bar"');

      finishClient(gClient);
    });
}

function start_trace()
{
  let deferred = defer();
  gTraceClient.startTrace(
    ["name", "location", "callsite", "time", "parameterNames", "arguments", "return"],
    null,
    function() { deferred.resolve(); });
  return deferred.promise;
}

function eval_code()
{
  let code = readFile("tracerlocations.js");
  Components.utils.evalInSandbox(code, gDebuggee, "1.8",
                                getFileUrl("tracerlocations.js"), 1);
}

function stop_trace()
{
  let deferred = defer();
  gTraceClient.stopTrace(null, function() { deferred.resolve(); });
  return deferred.promise;
}
