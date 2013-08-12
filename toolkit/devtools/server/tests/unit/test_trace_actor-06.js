/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that objects are correctly serialized and sent in exitedFrame
 * packets.
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
  gTraceClient.addListener("exitedFrame", function(aEvent, aPacket) {
    if (aPacket.sequence === 3) {
      let obj = aPacket.return;
      do_check_eq(typeof obj, "object",
                  'exitedFrame response should have return value');
      do_check_eq(typeof obj.prototype, "object",
                  'return value should have prototype');
      do_check_eq(typeof obj.ownProperties, "object",
                  'return value should have ownProperties list');
      do_check_eq(typeof obj.safeGetterValues, "object",
                  'return value should have safeGetterValues');

      do_check_eq(typeof obj.ownProperties.num, "object",
                  'return value should have property "num"');
      do_check_eq(typeof obj.ownProperties.str, "object",
                  'return value should have property "str"');
      do_check_eq(typeof obj.ownProperties.bool, "object",
                  'return value should have property "bool"');
      do_check_eq(typeof obj.ownProperties.undef, "object",
                  'return value should have property "undef"');
      do_check_eq(typeof obj.ownProperties.undef.value, "object",
                  'return value property "undef" should be a grip');
      do_check_eq(typeof obj.ownProperties.nil, "object",
                  'return value should have property "nil"');
      do_check_eq(typeof obj.ownProperties.nil.value, "object",
                  'return value property "nil" should be a grip');
      do_check_eq(typeof obj.ownProperties.obj, "object",
                  'return value should have property "obj"');
      do_check_eq(typeof obj.ownProperties.obj.value, "object",
                  'return value property "obj" should be a grip');
      do_check_eq(typeof obj.ownProperties.arr, "object",
                  'return value should have property "arr"');
      do_check_eq(typeof obj.ownProperties.arr.value, "object",
                  'return value property "arr" should be a grip');
      do_check_eq(typeof obj.ownProperties.inf, "object",
                  'return value should have property "inf"');
      do_check_eq(typeof obj.ownProperties.inf.value, "object",
                  'return value property "inf" should be a grip');
      do_check_eq(typeof obj.ownProperties.ninf, "object",
                  'return value should have property "ninf"');
      do_check_eq(typeof obj.ownProperties.ninf.value, "object",
                  'return value property "ninf" should be a grip');
      do_check_eq(typeof obj.ownProperties.nan, "object",
                  'return value should have property "nan"');
      do_check_eq(typeof obj.ownProperties.nan.value, "object",
                  'return value property "nan" should be a grip');
      do_check_eq(typeof obj.ownProperties.nzero, "object",
                  'return value should have property "nzero"');
      do_check_eq(typeof obj.ownProperties.nzero.value, "object",
                  'return value property "nzero" should be a grip');

      do_check_eq(obj.prototype.type, "object");
      do_check_eq(obj.ownProperties.num.value, 25);
      do_check_eq(obj.ownProperties.str.value, "foo");
      do_check_eq(obj.ownProperties.bool.value, false);
      do_check_eq(obj.ownProperties.undef.value.type, "undefined");
      do_check_eq(obj.ownProperties.nil.value.type, "null");
      do_check_eq(obj.ownProperties.obj.value.type, "object");
      do_check_eq(obj.ownProperties.obj.value.class, "Object");
      do_check_eq(obj.ownProperties.arr.value.type, "object");
      do_check_eq(obj.ownProperties.arr.value.class, "Array");
      do_check_eq(obj.ownProperties.inf.value.type, "Infinity");
      do_check_eq(obj.ownProperties.ninf.value.type, "-Infinity");
      do_check_eq(obj.ownProperties.nan.value.type, "NaN");
      do_check_eq(obj.ownProperties.nzero.value.type, "-0");
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
      let obj = {};
      obj.self = obj;

      return {
        num: 25,
        str: "foo",
        bool: false,
        undef: undefined,
        nil: null,
        obj: obj,
        arr: [1,2,3,4,5],
        inf: Infinity,
        ninf: -Infinity,
        nan: NaN,
        nzero: -0
      };
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
