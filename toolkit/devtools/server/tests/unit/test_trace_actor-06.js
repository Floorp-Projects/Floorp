/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that objects, nested objects, and circular references are
 * correctly serialized and sent in exitedFrame packets.
 */

let {defer} = devtools.require("sdk/core/promise");

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
  let packetsSeen = 0;

  gTraceClient.addListener("exitedFrame", function(aEvent, aPacket) {
    if (aPacket.sequence === 3) {
      do_check_eq(typeof aPacket.return, "object",
                  'exitedFrame response should have return value');

      let objPool = aPacket.return.objectPool;
      let retval = objPool[aPacket.return.value.objectId];
      let obj = objPool[retval.ownProperties.obj.value.objectId];

      do_check_eq(retval.ownProperties.num.value, 25);
      do_check_eq(retval.ownProperties.str.value, "foo");
      do_check_eq(retval.ownProperties.bool.value, false);
      do_check_eq(retval.ownProperties.undef.value.type, "undefined");
      do_check_eq(retval.ownProperties.nil.value.type, "null");
      do_check_eq(obj.ownProperties.self.value.objectId,
                  retval.ownProperties.obj.value.objectId);
    }
  });

  start_trace()
    .then(eval_code)
    .then(stop_trace)
    .then(function() {
      finishClient(gClient);
    });
}

function start_trace()
{
  let deferred = defer();
  gTraceClient.startTrace(["return"], null, function() { deferred.resolve(); });
  return deferred.promise;
}

function eval_code()
{
  gDebuggee.eval("(" + function() {
    function foo() {
      let obj = Object.create(null);
      obj.self = obj;

      let retval = Object.create(null);
      retval.num = 25;
      retval.str = "foo";
      retval.bool = false;
      retval.undef = undefined;
      retval.nil = null;
      retval.obj = obj;

      return retval;
    }
    foo();
  } + ")()");
}

function stop_trace()
{
  let deferred = defer();
  gTraceClient.stopTrace(null, function() { deferred.resolve(); });
  return deferred.promise;
}
