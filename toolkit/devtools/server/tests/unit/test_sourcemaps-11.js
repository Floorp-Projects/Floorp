/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Check that we source map frame locations returned by "frames" requests.
 */

var gDebuggee;
var gClient;
var gThreadClient;

const promise = devtools.require("sdk/core/promise");
Components.utils.import('resource:///modules/devtools/SourceMap.jsm');

function run_test() {
  initTestDebuggerServer();
  gDebuggee = addTestGlobal("test-source-map");
  gClient = new DebuggerClient(DebuggerServer.connectPipe());
  gClient.connect(function() {
    attachTestTabAndResume(gClient, "test-source-map", function(aResponse, aTabClient, aThreadClient) {
      gThreadClient = aThreadClient;
      promise.resolve(define_code())
        .then(run_code)
        .then(test_frames)
        .then(null, error => {
          dump(error + "\n");
          dump(error.stack);
          do_check_true(false);
        })
        .then(() => {
          finishClient(gClient);
        });
    });
  });
  do_test_pending();
}

function define_code() {
  let { code, map } = (new SourceNode(null, null, null, [
    new SourceNode(1, 0, "a.js", "function a() {\n"),
    new SourceNode(2, 0, "a.js", "  b();\n"),
    new SourceNode(3, 0, "a.js", "}\n"),
    new SourceNode(1, 0, "b.js", "function b() {\n"),
    new SourceNode(2, 0, "b.js", "  c();\n"),
    new SourceNode(3, 0, "b.js", "}\n"),
    new SourceNode(1, 0, "c.js", "function c() {\n"),
    new SourceNode(2, 0, "c.js", "  debugger;\n"),
    new SourceNode(3, 0, "c.js", "}\n"),
  ])).toStringWithSourceMap({
    file: "abc.js",
    sourceRoot: "http://example.com/www/js/"
  });

  code += "//# sourceMappingURL=data:text/json," + map.toString();

  Components.utils.evalInSandbox(code, gDebuggee, "1.8",
                                 "http://example.com/www/js/abc.js", 1);
}

function run_code() {
  const d = promise.defer();
  gClient.addOneTimeListener("paused", function () {
    gThreadClient.getFrames(0, 3, function (aResponse) {
      d.resolve(aResponse);
      gThreadClient.resume();
    })
  });
  gDebuggee.a();
  return d.promise;
}

function test_frames({ error, frames }) {
  do_check_true(!error);
  do_check_eq(frames.length, 3);
  check_frame(frames[0], "http://example.com/www/js/c.js");
  check_frame(frames[1], "http://example.com/www/js/b.js");
  check_frame(frames[2], "http://example.com/www/js/a.js");
}

function check_frame({ where: { url, line, column } }, aExpectedUrl) {
  do_check_eq(url, aExpectedUrl);
  do_check_eq(line, 2);
  do_check_eq(column, 0);
}
