/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests re-entrant startTrace/stopTrace calls on TraceActor.  Tests
 * that stopTrace ends the most recently started trace when not
 * provided with a name. Tests that starting a trace with the same
 * name twice results in only one trace being collected for that name.
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
        test_start_stop_reentrant();
      });
    });
  });
  do_test_pending();
}

function test_start_stop_reentrant()
{
  do_check_true(!gTraceClient.tracing, "TraceClient should start in idle state");

  start_named_trace("foo")
    .then(start_named_trace.bind(null, "foo"))
    .then(start_named_trace.bind(null, "bar"))
    .then(start_named_trace.bind(null, "baz"))
    .then(stop_trace.bind(null, "bar", "bar"))
    .then(stop_trace.bind(null, null, "baz"))
    .then(stop_trace.bind(null, null, "foo"))
    .then(function() {
      do_check_true(!gTraceClient.tracing, "TraceClient should finish in idle state");
      finishClient(gClient);
    });
}

function start_named_trace(aName)
{
  let deferred = promise.defer();
  gTraceClient.startTrace([], aName, function(aResponse) {
    do_check_true(!!gTraceClient.tracing, "TraceClient should be in tracing state");
    do_check_true(!aResponse.error,
                  'startTrace should not respond with error: ' + aResponse.error);
    do_check_eq(aResponse.type, "startedTrace",
                'startTrace response should have "type":"startedTrace" property');
    do_check_eq(aResponse.why, "requested",
                'startTrace response should have "why":"requested" property');
    do_check_eq(aResponse.name, aName,
                'startTrace response should have the given name');
    deferred.resolve();
  });
  return deferred.promise;
}

function stop_trace(aName, aExpectedName)
{
  let deferred = promise.defer();
  gTraceClient.stopTrace(aName, function(aResponse) {
    do_check_true(!aResponse.error,
                  'stopTrace should not respond with error: ' + aResponse.error);
    do_check_true(aResponse.type === "stoppedTrace",
                  'stopTrace response should have "type":"stoppedTrace" property');
    do_check_true(aResponse.why === "requested",
                  'stopTrace response should have "why":"requested" property');
    do_check_true(aResponse.name === aExpectedName,
                  'stopTrace response should have name "' + aExpectedName
                  + '", but had "' + aResponse.name + '"');
    deferred.resolve();
  });
  return deferred.promise;
}
