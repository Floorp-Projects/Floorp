/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that values are correctly serialized and sent in enteredFrame
 * and exitedFrame packets.
 */

let { defer } = devtools.require("sdk/core/promise");

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
  const traceStopped = defer();

  gClient.addListener("traces", (aEvent, { traces }) => {
    for (let t of traces) {
      check_trace(t);
      if (t.sequence === 27) {
        traceStopped.resolve();
      }
    }
  });

  start_trace()
    .then(eval_code)
    .then(() => traceStopped.promise)
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

    // Make sure there is only 5 properties per object because that is the value
    // of MAX_PROPERTIES in the server.
    let obj = {
      num: 0,
      str: "foo",
      bool: false,
      undef: undefined,
      nil: null
    };
    let obj2 = {
      inf: Infinity,
      ninf: -Infinity,
      nan: NaN,
      nzero: -0,
      obj: circular
    };
    let obj3 = {
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
    identity(obj2);
    identity(obj3);
  } + ")()");
}

function stop_trace()
{
  let deferred = defer();
  gTraceClient.stopTrace(null, function() { deferred.resolve(); });
  return deferred.promise;
}

function check_trace(aTrace)
{
  let value = (aTrace.type === "enteredFrame" && aTrace.arguments)
        ? aTrace.arguments[0]
        : aTrace.return;
  switch(aTrace.sequence) {
  case 2:
    do_check_eq(typeof aTrace.arguments, "object");
    do_check_eq(aTrace.arguments.length, 0);
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
    check_obj(aTrace.type, value);
    break;
  case 24:
  case 25:
    check_obj2(aTrace.type, value);
    break;
  case 26:
  case 27:
    check_obj3(aTrace.type, value);
  }
}

function check_value(aActual, aExpectedType, aExpectedValue)
{
  do_check_eq(typeof aActual, aExpectedType);
  do_check_eq(aExpectedType === "object" ? aActual.type : aActual, aExpectedValue);
}

function check_obj(aType, aObj) {
  do_check_eq(typeof aObj, "object");
  do_check_eq(typeof aObj.ownProperties, "object");

  do_check_eq(typeof aObj.ownProperties.num, "object");
  do_check_eq(aObj.ownProperties.num.value, 0);

  do_check_eq(typeof aObj.ownProperties.str, "object");
  do_check_eq(aObj.ownProperties.str.value, "foo");

  do_check_eq(typeof aObj.ownProperties.bool, "object");
  do_check_eq(aObj.ownProperties.bool.value, false);

  do_check_eq(typeof aObj.ownProperties.undef, "object");
  do_check_eq(typeof aObj.ownProperties.undef.value, "object");
  do_check_eq(aObj.ownProperties.undef.value.type, "undefined");

  do_check_eq(typeof aObj.ownProperties.nil, "object");
  do_check_eq(typeof aObj.ownProperties.nil.value, "object");
  do_check_eq(aObj.ownProperties.nil.value.type, "null");
}

function check_obj2(aType, aObj) {
  do_check_eq(typeof aObj.ownProperties.inf, "object");
  do_check_eq(typeof aObj.ownProperties.inf.value, "object");
  do_check_eq(aObj.ownProperties.inf.value.type, "Infinity");

  do_check_eq(typeof aObj.ownProperties.ninf, "object");
  do_check_eq(typeof aObj.ownProperties.ninf.value, "object");
  do_check_eq(aObj.ownProperties.ninf.value.type, "-Infinity");

  do_check_eq(typeof aObj.ownProperties.nan, "object");
  do_check_eq(typeof aObj.ownProperties.nan.value, "object");
  do_check_eq(aObj.ownProperties.nan.value.type, "NaN");

  do_check_eq(typeof aObj.ownProperties.nzero, "object");
  do_check_eq(typeof aObj.ownProperties.nzero.value, "object");
  do_check_eq(aObj.ownProperties.nzero.value.type, "-0");

  // Sub-objects aren't added.
  do_check_eq(typeof aObj.ownProperties.obj, "undefined");
}

function check_obj3(aType, aObj) {
  // Sub-objects aren't added.
  do_check_eq(typeof aObj.ownProperties.arr, "undefined");
}
