/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that chained object prototypes are correctly serialized and
 * sent in exitedFrame packets.
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
      let obj = objPool[aPacket.return.value.objectId];
      let propObj = objPool[obj.ownProperties.b.value.objectId];
      let proto = objPool[obj.prototype.objectId];
      let protoProto = objPool[proto.prototype.objectId];

      do_check_eq(obj.ownProperties.a.value, 1);
      do_check_eq(propObj.ownProperties.c.value, "c");
      do_check_eq(proto.ownProperties.d.value, "foo");
      do_check_eq(proto.ownProperties.e.value, 42);
      do_check_eq(protoProto.ownProperties.f.value, 2);
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
      let protoProto = Object.create(null);
      protoProto.f = 2;

      let proto = Object.create(protoProto);
      proto.d = "foo";
      proto.e = 42;

      let obj = Object.create(proto);
      obj.a = 1;

      let propObj = Object.create(null);
      propObj.c = "c";

      obj.b = propObj;

      return obj;
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
