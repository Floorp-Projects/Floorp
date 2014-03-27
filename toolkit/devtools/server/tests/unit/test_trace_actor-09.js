/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that self-hosted functions aren't traced and don't add depth.
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
        test_frame_depths();
      });
    });
  });
  do_test_pending();
}

function test_frame_depths()
{
  const tracesStopped = promise.defer();
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
      do_check_true(false, "Should not get an error, got: " + DevToolsUtils.safeErrorString(error));
    });
}

function start_trace()
{
  let deferred = promise.defer();
  gTraceClient.startTrace(["depth", "name", "location"], null, function() { deferred.resolve(); });
  return deferred.promise;
}

function eval_code()
{
  gDebuggee.eval("(" + function iife() {
    [1].forEach(function noop() {});
    for (let x of [1]) {}
  } + ")()");
}

function stop_trace()
{
  let deferred = promise.defer();
  gTraceClient.stopTrace(null, function() { deferred.resolve(); });
  return deferred.promise;
}

function check_trace({ sequence, depth, name, location })
{
  if (location) {
    do_check_true(location.url !== "self-hosted");
  }

  switch(sequence) {
  case 0:
    do_check_eq(name, "(eval)");
  case 5:
    do_check_eq(depth, 0);
    break;

  case 1:
    do_check_eq(name, "iife");
  case 4:
    do_check_eq(depth, 1);
    break;

  case 2:
    do_check_eq(name, "noop");
  case 3:
    do_check_eq(depth, 2);
    break;

  default:
    // Should have covered all sequences.
    do_check_true(false);
  }
}
