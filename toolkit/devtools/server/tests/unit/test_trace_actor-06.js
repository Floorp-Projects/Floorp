/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that values are correctly serialized and sent in enteredFrame
 * and exitedFrame packets.
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
  gTraceClient.addListener("enteredFrame", check_packet);
  gTraceClient.addListener("exitedFrame", check_packet);

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
  gTraceClient.startTrace(["arguments", "return"], null, function() { deferred.resolve(); });
  return deferred.promise;
}

function eval_code()
{
  gDebuggee.eval("(" + function() {
    function identity(x) {
      return x;
    }

    let circular = {};
    circular.self = circular;

    let obj = {
      num: 0,
      str: "foo",
      bool: false,
      undef: undefined,
      nil: null,
      inf: Infinity,
      ninf: -Infinity,
      nan: NaN,
      nzero: -0,
      obj: circular,
      arr: [1,2,3,4,5]
    };

    identity();
    identity(0);
    identity("");
    identity(false);
    identity(undefined);
    identity(null);
    identity(Infinity);
    identity(-Infinity);
    identity(NaN);
    identity(-0);
    identity(obj);
  } + ")()");
}

function stop_trace()
{
  let deferred = defer();
  gTraceClient.stopTrace(null, function() { deferred.resolve(); });
  return deferred.promise;
}

function check_packet(aEvent, aPacket)
{
  let value = (aPacket.type === "enteredFrame" && aPacket.arguments)
        ? aPacket.arguments[0]
        : aPacket.return;
  switch(aPacket.sequence) {
  case 2:
    do_check_eq(typeof aPacket.arguments, "object",
                "zero-argument function call should send arguments list");
    do_check_eq(aPacket.arguments.length, 0,
                "zero-argument function call should send zero-length arguments list");
    break;
  case 3:
    check_value(value, "object", "undefined");
    break;
  case 4:
  case 5:
    check_value(value, "number", 0);
    break;
  case 6:
  case 7:
    check_value(value, "string", "");
    break;
  case 8:
  case 9:
    check_value(value, "boolean", false);
    break;
  case 10:
  case 11:
    check_value(value, "object", "undefined");
    break;
  case 12:
  case 13:
    check_value(value, "object", "null");
    break;
  case 14:
  case 15:
    check_value(value, "object", "Infinity");
    break;
  case 16:
  case 17:
    check_value(value, "object", "-Infinity");
    break;
  case 18:
  case 19:
    check_value(value, "object", "NaN");
    break;
  case 20:
  case 21:
    check_value(value, "object", "-0");
    break;
  case 22:
  case 23:
    check_object(aPacket.type, value);
    break;
  }
}

function check_value(aActual, aExpectedType, aExpectedValue)
{
  do_check_eq(typeof aActual, aExpectedType);
  do_check_eq(aExpectedType === "object" ? aActual.type : aActual, aExpectedValue);
}

function check_object(aType, aObj) {
  do_check_eq(typeof aObj, "object",
              'serialized object should be present in packet');
  do_check_eq(typeof aObj.prototype, "object",
              'serialized object should have prototype');
  do_check_eq(typeof aObj.ownProperties, "object",
              'serialized object should have ownProperties list');
  do_check_eq(typeof aObj.safeGetterValues, "object",
              'serialized object should have safeGetterValues');

  do_check_eq(typeof aObj.ownProperties.num, "object",
              'serialized object should have property "num"');
  do_check_eq(typeof aObj.ownProperties.str, "object",
              'serialized object should have property "str"');
  do_check_eq(typeof aObj.ownProperties.bool, "object",
              'serialized object should have property "bool"');
  do_check_eq(typeof aObj.ownProperties.undef, "object",
              'serialized object should have property "undef"');
  do_check_eq(typeof aObj.ownProperties.undef.value, "object",
              'serialized object property "undef" should be a grip');
  do_check_eq(typeof aObj.ownProperties.nil, "object",
              'serialized object should have property "nil"');
  do_check_eq(typeof aObj.ownProperties.nil.value, "object",
              'serialized object property "nil" should be a grip');
  do_check_eq(typeof aObj.ownProperties.obj, "object",
              'serialized object should have property "aObj"');
  do_check_eq(typeof aObj.ownProperties.obj.value, "object",
              'serialized object property "aObj" should be a grip');
  do_check_eq(typeof aObj.ownProperties.arr, "object",
              'serialized object should have property "arr"');
  do_check_eq(typeof aObj.ownProperties.arr.value, "object",
              'serialized object property "arr" should be a grip');
  do_check_eq(typeof aObj.ownProperties.inf, "object",
              'serialized object should have property "inf"');
  do_check_eq(typeof aObj.ownProperties.inf.value, "object",
              'serialized object property "inf" should be a grip');
  do_check_eq(typeof aObj.ownProperties.ninf, "object",
              'serialized object should have property "ninf"');
  do_check_eq(typeof aObj.ownProperties.ninf.value, "object",
              'serialized object property "ninf" should be a grip');
  do_check_eq(typeof aObj.ownProperties.nan, "object",
              'serialized object should have property "nan"');
  do_check_eq(typeof aObj.ownProperties.nan.value, "object",
              'serialized object property "nan" should be a grip');
  do_check_eq(typeof aObj.ownProperties.nzero, "object",
              'serialized object should have property "nzero"');
  do_check_eq(typeof aObj.ownProperties.nzero.value, "object",
              'serialized object property "nzero" should be a grip');

  do_check_eq(aObj.prototype.type, "object");
  do_check_eq(aObj.ownProperties.num.value, 0);
  do_check_eq(aObj.ownProperties.str.value, "foo");
  do_check_eq(aObj.ownProperties.bool.value, false);
  do_check_eq(aObj.ownProperties.undef.value.type, "undefined");
  do_check_eq(aObj.ownProperties.nil.value.type, "null");
  do_check_eq(aObj.ownProperties.obj.value.type, "object");
  do_check_eq(aObj.ownProperties.obj.value.class, "Object");
  do_check_eq(aObj.ownProperties.arr.value.type, "object");
  do_check_eq(aObj.ownProperties.arr.value.class, "Array");
  do_check_eq(aObj.ownProperties.inf.value.type, "Infinity");
  do_check_eq(aObj.ownProperties.ninf.value.type, "-Infinity");
  do_check_eq(aObj.ownProperties.nan.value.type, "NaN");
  do_check_eq(aObj.ownProperties.nzero.value.type, "-0");
}
