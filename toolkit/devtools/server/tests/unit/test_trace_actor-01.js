/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that TraceActor is available and returns correct responses to
 * startTrace and stopTrace requests.
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
      do_check_true(!!aResponse.traceActor, "TraceActor should be visible in tab");
      gClient.attachTracer(aResponse.traceActor, function(aResponse, aTraceClient) {
        gTraceClient = aTraceClient;
        test_start_stop_response();
      });
    });
  });
  do_test_pending();
}

function test_start_stop_response()
{
  do_check_true(!gTraceClient.tracing, "TraceClient should start in idle state");
  gTraceClient.startTrace([], null, function(aResponse) {
    do_check_true(!!gTraceClient.tracing, "TraceClient should be in tracing state");
    do_check_true(!aResponse.error,
                  'startTrace should not respond with error: ' + aResponse.error);
    do_check_eq(aResponse.type, "startedTrace",
                'startTrace response should have "type":"startedTrace" property');
    do_check_eq(aResponse.why, "requested",
                'startTrace response should have "why":"requested" property');

    gTraceClient.stopTrace(null, function(aResponse) {
      do_check_true(!gTraceClient.tracing, "TraceClient should be in idle state");
      do_check_true(!aResponse.error,
                   'stopTrace should not respond with error: ' + aResponse.error);
      do_check_eq(aResponse.type, "stoppedTrace",
                  'stopTrace response should have "type":"stoppedTrace" property');
      do_check_eq(aResponse.why, "requested",
                  'stopTrace response should have "why":"requested" property');

      finishClient(gClient);
    });
  });
}
