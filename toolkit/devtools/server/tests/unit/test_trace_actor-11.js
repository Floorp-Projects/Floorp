/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that hit counts from tracer count function frames correctly, even after
 * restarting the trace.
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
        test_hit_counts();
      });
    });
  });
  do_test_pending();
}

function test_hit_counts()
{
  start_trace()
    .then(eval_code)
    .then(listen_to_traces)
    .then(stop_trace)
    .then(start_trace) // Restart tracing.
    .then(eval_code)
    .then(listen_to_traces)
    .then(stop_trace)
    .then(function() {
      finishClient(gClient);
    }).then(null, error => {
      do_check_true(false, "Should not get an error, got: " + DevToolsUtils.safeErrorString(error));
    });
}

function listen_to_traces() {
  const tracesStopped = promise.defer();
  gClient.addListener("traces", (aEvent, { traces }) => {
    for (let t of traces) {
      check_trace(t);
    }
    tracesStopped.resolve();
  });
  return tracesStopped.promise;
}

function start_trace()
{
  let deferred = promise.defer();
  gTraceClient.startTrace(["depth", "name", "location", "hitCount"], null, function() { deferred.resolve(); });
  return deferred.promise;
}

function eval_code()
{
  gDebuggee.eval("(" + function iife() {
    [1, 2, 3].forEach(function noop() {
      for (let x of [1]) {}
    });
  } + ")()");
}

function stop_trace()
{
  let deferred = promise.defer();
  gTraceClient.stopTrace(null, function() { deferred.resolve(); });
  return deferred.promise;
}

function check_trace({ type, sequence, depth, name, location, hitCount })
{
  if (location) {
    do_check_true(location.url !== "self-hosted");
  }

  switch(sequence) {
  case 0:
    do_check_eq(name, "(eval)");
    do_check_eq(hitCount, 1);
    break;

  case 1:
    do_check_eq(name, "iife");
    do_check_eq(hitCount, 1);
    break;

  case 2:
    do_check_eq(hitCount, 1);
    do_check_eq(name, "noop");
    break;

  case 4:
    do_check_eq(hitCount, 2);
    do_check_eq(name, "noop");
    break;

  case 6:
    do_check_eq(hitCount, 3);
    do_check_eq(name, "noop");
    break;

  case 3:
  case 5:
  case 7:
  case 8:
  case 9:
    do_check_eq(type, "exitedFrame");
    break;

  default:
    // Should have covered all sequences.
    do_check_true(false);
  }
}
