/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that automatically generated names for traces are unique.
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
        test_unique_generated_trace_names();
      });
    });
  });
  do_test_pending();
}

function test_unique_generated_trace_names()
{
  let deferred = promise.defer();
  deferred.resolve([]);

  let p = deferred.promise, traces = 50;
  for (let i = 0; i < traces; i++)
    p = p.then(start_trace);
  for (let i = 0; i < traces; i++)
    p = p.then(stop_trace);

  p = p.then(function() {
    finishClient(gClient);
  });
}

function start_trace(aTraceNames)
{
  let deferred = promise.defer();
  gTraceClient.startTrace([], null, function(aResponse) {
    let hasDuplicates = aTraceNames.some(name => name === aResponse.name);
    do_check_true(!hasDuplicates, "Generated trace names should be unique");
    aTraceNames.push(aResponse.name);
    deferred.resolve(aTraceNames);
  });
  return deferred.promise;
}

function stop_trace(aTraceNames)
{
  let deferred = promise.defer();
  gTraceClient.stopTrace(null, function(aResponse) {
    do_check_eq(aTraceNames.pop(), aResponse.name,
                "Stopped trace should be most recently started trace");
    let hasDuplicates = aTraceNames.some(name => name === aResponse.name);
    do_check_true(!hasDuplicates, "Generated trace names should be unique");
    deferred.resolve(aTraceNames);
  });
  return deferred.promise;
}
