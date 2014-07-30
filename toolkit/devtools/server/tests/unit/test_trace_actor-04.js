/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that enteredFrame packets are sent on frame entry and
 * exitedFrame packets are sent on frame exit. Tests that the "name"
 * trace type works for function declarations.
 */

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
  let tracesSeen = 0;
  let traceNames = [];
  let traceStopped = promise.defer();

  gClient.addListener("traces", function onTraces(aEvent, { traces }) {
    for (let t of traces) {
      tracesSeen++;

      if (t.type == "enteredFrame") {
        do_check_eq(t.type, "enteredFrame",
                    'enteredFrame response should have type "enteredFrame"');
        do_check_eq(typeof t.sequence, "number",
                    'enteredFrame response should have sequence number');
        do_check_true(!isNaN(t.sequence),
                      'enteredFrame sequence should be a number');
        do_check_eq(typeof t.name, "string",
                    'enteredFrame response should have function name');
        traceNames[t.sequence] = t.name;
      } else {
        do_check_eq(t.type, "exitedFrame",
                    'exitedFrame response should have type "exitedFrame"');
        do_check_eq(typeof t.sequence, "number",
                    'exitedFrame response should have sequence number');
        do_check_true(!isNaN(t.sequence),
                      'exitedFrame sequence should be a number');
      }

      if (tracesSeen == 10) {
        gClient.removeListener("traces", onTraces);
        traceStopped.resolve();
      }
    }
  });

  start_trace()
    .then(eval_code)
    .then(() => traceStopped.promise)
    .then(stop_trace)
    .then(function() {
      do_check_eq(traceNames[2], "baz",
                  'Should have entered "baz" frame in third packet');
      do_check_eq(traceNames[3], "bar",
                  'Should have entered "bar" frame in fourth packet');
      do_check_eq(traceNames[4], "foo",
                  'Should have entered "foo" frame in fifth packet');
      finishClient(gClient);
    })
    .then(null, e => DevToolsUtils.reportException("test_trace_actor-04.js",
                                                   e));
}

function start_trace()
{
  let deferred = promise.defer();
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
  let deferred = promise.defer();
  gTraceClient.stopTrace(null, function() { deferred.resolve(); });
  return deferred.promise;
}
