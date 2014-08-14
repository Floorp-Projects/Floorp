/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Create 2 sources, A and B, B is black boxed. When calling functions A->B->A,
 * verify that only traces from source B are black boxed.
 */

var gDebuggee;
var gClient;
var gTraceClient;
var gThreadClient;

function run_test()
{
  initTestTracerServer();
  gDebuggee = addTestGlobal("test-tracer-actor");
  gClient = new DebuggerClient(DebuggerServer.connectPipe());
  gClient.connect(function() {
    attachTestThread(gClient, "test-tracer-actor",
      function(aResponse, aTabClient, aThreadClient, aTabResponse) {
      gThreadClient = aThreadClient;
      gThreadClient.resume(function (aResponse) {
        gClient.attachTracer(aTabResponse.traceActor,
          function(aResponse, aTraceClient) {
          gTraceClient = aTraceClient;
          testTraces();
        });
      });
    });
  });
  do_test_pending();
}

const BLACK_BOXED_URL = "http://example.com/blackboxme.js";
const SOURCE_URL = "http://example.com/source.js";

const testTraces = Task.async(function* () {
  // Read traces
  const tracesStopped = promise.defer();
  gClient.addListener("traces", (aEvent, { traces }) => {
    for (let t of traces) {
      check_trace(t);
    }
    tracesStopped.resolve();
  });

  yield startTrace();

  evalSetup();

  // Blackbox source
  const sourcesResponse = yield getSources(gThreadClient);
  let sourceClient = gThreadClient.source(
    sourcesResponse.sources.filter(s => s.url == BLACK_BOXED_URL)[0]);
  do_check_true(!sourceClient.isBlackBoxed,
    "By default the source is not black boxed.");
  yield blackBox(sourceClient);
  do_check_true(sourceClient.isBlackBoxed);

  evalTestCode();

  yield tracesStopped.promise;
  yield stopTrace();

  finishClient(gClient);
});

function startTrace()
{
  let deferred = promise.defer();
  gTraceClient.startTrace(["depth", "name", "location"], null,
    function() { deferred.resolve(); });
  return deferred.promise;
}

function evalSetup()
{
  Components.utils.evalInSandbox(
    "" + function fnBlackBoxed(k) {
      fnInner();
    },
    gDebuggee,
    "1.8",
    BLACK_BOXED_URL,
    1
  );

  Components.utils.evalInSandbox(
    "" + function fnOuter() {
      fnBlackBoxed();
    } + "\n" +
    "" + function fnInner() {
      [1].forEach(function noop() {});
    },
    gDebuggee,
    "1.8",
    SOURCE_URL,
    1
  );
}

function evalTestCode()
{
  Components.utils.evalInSandbox(
    "fnOuter();",
    gDebuggee,
    "1.8",
    SOURCE_URL,
    1
  );
}

function stopTrace()
{
  let deferred = promise.defer();
  gTraceClient.stopTrace(null, function() { deferred.resolve(); });
  return deferred.promise;
}

function check_trace({ type, sequence, depth, name, location, blackBoxed })
{
  switch(sequence) {
  // First two packets come from evalInSandbox in evalSetup
  // The third packet comes from evalInSandbox in evalTestCode
  case 0:
  case 2:
  case 4:
    do_check_eq(name, "(global)");
    do_check_eq(type, "enteredFrame");
    break;

  case 5:
    do_check_eq(blackBoxed, false);
    do_check_eq(name, "fnOuter");
    break;

  case 6:
    do_check_eq(blackBoxed, true);
    do_check_eq(name, "fnBlackBoxed");
    break;

  case 7:
    do_check_eq(blackBoxed, false);
    do_check_eq(name, "fnInner");
    break;

  case 8:
    do_check_eq(blackBoxed, false);
    do_check_eq(name, "noop");
    break;

  case 1: // evalInSandbox
  case 3: // evalInSandbox
  case 9: // noop
  case 10: // fnInner
  case 11: // fnBlackBoxed
  case 12: // fnOuter
  case 13: // evalInSandbox
    do_check_eq(type, "exitedFrame");
    break;

  default:
    // Should have covered all sequences.
    do_check_true(false);
  }
}
