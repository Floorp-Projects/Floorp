/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Simple tests for "location", "callsite", "time", "parameterNames",
 * "arguments", and "return" trace types.
 */

let { defer } = devtools.require("sdk/core/promise");

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

function check_number(value)
{
  do_check_eq(typeof value, "number");
  do_check_true(!isNaN(value));
}

function check_location(actual, expected)
{
  do_check_eq(typeof actual, "object");

  check_number(actual.line);
  check_number(actual.column);

  do_check_eq(actual.url, expected.url);
  do_check_eq(actual.line, expected.line);
  do_check_eq(actual.column, expected.column);

}

function test_enter_exit_frame()
{
  let traces = [];
  let traceStopped = defer();

  gClient.addListener("traces", function(aEvent, aPacket) {
    for (let t of aPacket.traces) {
      if (t.type == "enteredFrame") {
        do_check_eq(typeof t.name, "string");
        do_check_eq(typeof t.location, "object");

        check_number(t.sequence);
        check_number(t.time);
        check_number(t.location.line);
        check_number(t.location.column);
        if (t.callsite) {
          check_number(t.callsite.line);
          check_number(t.callsite.column);
        }
      } else {
        check_number(t.sequence);
        check_number(t.time);
      }

      traces[t.sequence] = t;
      if (traces.length === 4) {
        traceStopped.resolve();
      }
    }
  });

  start_trace()
    .then(eval_code)
    .then(() => traceStopped.promise)
    .then(stop_trace)
    .then(function() {
      let url = getFileUrl("tracerlocations.js");

      check_location(traces[0].location, { url: url, line: 1, column: 0 });

      do_check_eq(traces[1].name, "foo");

      // foo's definition is at tracerlocations.js:3:0, but
      // Debugger.Script does not provide complete definition
      // locations (bug 901138). tracerlocations.js:4:2 is the first
      // statement in the function (used as an approximation).
      check_location(traces[1].location, { url: url, line: 4, column: 2 });
      check_location(traces[1].callsite, { url: url, line: 8, column: 0 });

      do_check_eq(typeof traces[1].parameterNames, "object");
      do_check_eq(traces[1].parameterNames.length, 1);
      do_check_eq(traces[1].parameterNames[0], "x");

      do_check_eq(typeof traces[1].arguments, "object");
      do_check_true(Array.isArray(traces[1].arguments));
      do_check_eq(traces[1].arguments.length, 1);
      do_check_eq(traces[1].arguments[0], 42);

      do_check_eq(typeof traces[2].return, "string");
      do_check_eq(traces[2].return, "bar");

      finishClient(gClient);
    }, error => {
      DevToolsUtils.reportException("test_trace_actor-05.js", error);
      do_check_true(false);
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
