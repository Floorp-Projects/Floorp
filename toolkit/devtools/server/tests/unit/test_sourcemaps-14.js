/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Check that tracer links to a correct (source-mapped) location.
 */

var gDebuggee;
var gClient;
var gTraceClient;
var gThreadClient;

Components.utils.import('resource:///modules/devtools/SourceMap.jsm');

function run_test() {
  initTestTracerServer();
  gDebuggee = addTestGlobal("test-source-map");
  gClient = new DebuggerClient(DebuggerServer.connectPipe());
  gClient.connect(function() {
    attachTestThread(gClient, "test-source-map",
      function(aResponse, aTabClient, aThreadClient, aTabResponse) {
      gThreadClient = aThreadClient;
      gThreadClient.resume(function (aResponse) {
        gClient.attachTracer(aTabResponse.traceActor, function(aResponse, aTraceClient) {
          gTraceClient = aTraceClient;
          testSourceMaps();
        });
      });
    });
  });
  do_test_pending();
}

const testSourceMaps = Task.async(function* () {
  const tracesStopped = promise.defer();
  gClient.addListener("traces", (aEvent, { traces }) => {
    for (let t of traces) {
      checkTrace(t);
    }
    tracesStopped.resolve();
  });

  yield startTrace();

  yield executeOnNextTickAndWaitForPause(evalCode, gClient);
  yield resume(gThreadClient);

  yield tracesStopped.promise;
  yield stopTrace();

  finishClient(gClient);
});

function startTrace()
{
  let deferred = promise.defer();
  gTraceClient.startTrace(["depth", "name", "location"], null, function() { deferred.resolve(); });
  return deferred.promise;
}

function evalCode()
{
  let { code, map } = (new SourceNode(null, null, null, [
    new SourceNode(10, 0, "a.js", "function fnOuter() {\n"),
    new SourceNode(20, 0, "a.js", "  fnInner();\n"),
    new SourceNode(30, 0, "a.js", "}\n"),
    new SourceNode(10, 0, "b.js", "function fnInner() {\n"),
    new SourceNode(20, 0, "b.js", "  [1].forEach(function noop() {\n"),
    new SourceNode(30, 0, "b.js", "    debugger;\n"),
    new SourceNode(40, 0, "b.js", "  });\n"),
    new SourceNode(50, 0, "b.js", "}\n"),
    new SourceNode(60, 0, "b.js", "fnOuter();\n"),
  ])).toStringWithSourceMap({
    file: "abc.js",
    sourceRoot: "http://example.com/"
  });

  code += "//# sourceMappingURL=data:text/json," + map.toString();

  Components.utils.evalInSandbox(code, gDebuggee, "1.8", "http://example.com/abc.js", 1);
}


function checkTrace({ type, sequence, depth, name, location, blackBoxed })
{
  switch(sequence) {
  // First packet comes from evalInSandbox in evalCode.
  case 0:
    do_check_eq(name, "(global)");
    break;

  case 1:
    do_check_eq(name, "fnOuter");
    do_check_eq(location.url, "http://example.com/a.js");
    do_check_eq(location.line, 10);
    break;

  case 2:
    do_check_eq(name, "fnInner");
    do_check_eq(location.url, "http://example.com/b.js");
    do_check_eq(location.line, 10);
    break;

  case 3:
    do_check_eq(name, "noop");
    do_check_eq(location.url, "http://example.com/b.js");
    do_check_eq(location.line, 20);
    break;

  case 4:
  case 5:
  case 6:
  case 7:
    do_check_eq(type, "exitedFrame");
    break;

  default:
    // Should have covered all sequences.
    do_check_true(false);
  }
}

function stopTrace()
{
  let deferred = promise.defer();
  gTraceClient.stopTrace(null, function() { deferred.resolve(); });
  return deferred.promise;
}