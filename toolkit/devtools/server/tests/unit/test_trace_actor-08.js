/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test the "depth" trace type.
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
        test_frame_depths();
      });
    });
  });
  do_test_pending();
}

function test_frame_depths()
{
  const tracesStopped = defer();
  gClient.addListener("traces", (aEvent, { traces }) => {
    for (let t of traces) {
      check_trace(t);
    }
    tracesStopped.resolve();
  });

  start_trace()
    .then(eval_code)
    .then(() => tracesStopped.promise)
    .then(stop_trace)
    .then(function() {
      finishClient(gClient);
    }).then(null, error => {
      do_check_true(false, "Should not get an error, got: " + error);
    });
}

function start_trace()
{
  let deferred = defer();
  gTraceClient.startTrace(["depth", "name"], null, function() { deferred.resolve(); });
  return deferred.promise;
}

function eval_code()
{
  gDebuggee.eval("(" + function iife() {
    function first() {
      second();
    }

    function second() {
      third();
    }

    function third() {
    }

    first();
  } + ")()");
}

function stop_trace()
{
  let deferred = defer();
  gTraceClient.stopTrace(null, function() { deferred.resolve(); });
  return deferred.promise;
}

function check_trace({ sequence, depth, name })
{
  switch(sequence) {
  case 0:
    do_check_eq(name, "(eval)");
    // Fall through
  case 9:
    do_check_eq(depth, 0);
    break;

  case 1:
    do_check_eq(name, "iife");
    // Fall through
  case 8:
    do_check_eq(depth, 1);
    break;

  case 2:
    do_check_eq(name, "first");
    // Fall through
  case 7:
    do_check_eq(depth, 2);
    break;

  case 3:
    do_check_eq(name, "second");
    // Fall through
  case 6:
    do_check_eq(depth, 3);
    break;

  case 4:
    do_check_eq(name, "third");
    // Fall through
  case 5:
    do_check_eq(depth, 4);
    break;

  default:
    // Should have covered all sequences.
    do_check_true(false);
  }
}
