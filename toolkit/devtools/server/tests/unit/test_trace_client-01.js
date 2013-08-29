/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that TraceClient emits enteredFrame and exitedFrame events in
 * order when receiving packets out of order.
 */

let {defer, resolve} = devtools.require("sdk/core/promise");

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
        test_packet_order();
      });
    });
  });
  do_test_pending();
}

function test_packet_order()
{
  let sequence = 0;

  function check_packet(aEvent, aPacket) {
    do_check_eq(aPacket.sequence, sequence,
                'packet should have sequence number ' + sequence);
    sequence++;
  }

  gTraceClient.addListener("enteredFrame", check_packet);
  gTraceClient.addListener("exitedFrame", check_packet);

  start_trace()
    .then(mock_packets)
    .then(start_trace)
    .then(mock_packets.bind(null, 14))
    .then(stop_trace)
    .then(stop_trace)
    .then(function() {
      // All traces were stopped, so the sequence number resets
      sequence = 0;
      return resolve();
    })
    .then(start_trace)
    .then(mock_packets)
    .then(stop_trace)
    .then(function() {
      finishClient(gClient);
    });
}

function start_trace()
{
  let deferred = defer();
  gTraceClient.startTrace([], null, function() { deferred.resolve(); });
  return deferred.promise;
}

function stop_trace()
{
  let deferred = defer();
  gTraceClient.stopTrace(null, function() { deferred.resolve(); });
  return deferred.promise;
}

function mock_packets(s = 0)
{
  gTraceClient.onPacket("", { type: "exitedFrame",  sequence: s + 5 });
  gTraceClient.onPacket("", { type: "enteredFrame", sequence: s + 3 });
  gTraceClient.onPacket("", { type: "exitedFrame",  sequence: s + 2 });
  gTraceClient.onPacket("", { type: "enteredFrame", sequence: s + 4 });
  gTraceClient.onPacket("", { type: "enteredFrame", sequence: s + 1 });

  gTraceClient.onPacket("", { type: "enteredFrame", sequence: s + 7 });
  gTraceClient.onPacket("", { type: "enteredFrame", sequence: s + 8 });

  // Triggers 0-5
  gTraceClient.onPacket("", { type: "enteredFrame", sequence: s + 0 });

  gTraceClient.onPacket("", { type: "exitedFrame",  sequence: s + 9 });
  gTraceClient.onPacket("", { type: "exitedFrame",  sequence: s + 10 });

  // Triggers 6-10
  gTraceClient.onPacket("", { type: "exitedFrame",  sequence: s + 6 });

  // Each following packet is expected; event is fired immediately
  gTraceClient.onPacket("", { type: "enteredFrame", sequence: s + 11 });
  gTraceClient.onPacket("", { type: "exitedFrame",  sequence: s + 12 });
  gTraceClient.onPacket("", { type: "exitedFrame",  sequence: s + 13 });
}
