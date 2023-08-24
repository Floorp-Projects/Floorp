/* global arguments */

"use strict";

var CC = Components.Constructor;

const BinaryInputStream = CC(
  "@mozilla.org/binaryinputstream;1",
  "nsIBinaryInputStream",
  "setInputStream"
);
const ProtocolProxyService = CC(
  "@mozilla.org/network/protocol-proxy-service;1",
  "nsIProtocolProxyService"
);
var sts = Cc["@mozilla.org/network/socket-transport-service;1"].getService(
  Ci.nsISocketTransportService
);

function waitForStream(stream, streamType) {
  return new Promise((resolve, reject) => {
    stream = stream.QueryInterface(streamType);
    if (!stream) {
      reject("stream didn't implement given stream type");
    }
    let currentThread =
      Cc["@mozilla.org/thread-manager;1"].getService().currentThread;
    stream.asyncWait(resolve, 0, 0, currentThread);
  });
}

async function launchConnection(
  socks_vers,
  socks_port,
  dest_host,
  dest_port,
  dns
) {
  let pi_flags = 0;
  if (dns == "remote") {
    pi_flags = Ci.nsIProxyInfo.TRANSPARENT_PROXY_RESOLVES_HOST;
  }

  let pps = new ProtocolProxyService();
  let pi = pps.newProxyInfo(
    socks_vers,
    "localhost",
    socks_port,
    "",
    "",
    pi_flags,
    -1,
    null
  );
  let trans = sts.createTransport([], dest_host, dest_port, pi, null);
  let input = trans.openInputStream(0, 0, 0);
  let output = trans.openOutputStream(0, 0, 0);
  input = await waitForStream(input, Ci.nsIAsyncInputStream);
  let bin = new BinaryInputStream(input);
  let data = bin.readBytes(5);
  let response;
  if (data == "PING!") {
    print("client: got ping, sending pong.");
    response = "PONG!";
  } else {
    print("client: wrong data from server:", data);
    response = "Error: wrong data received.";
  }
  output = await waitForStream(output, Ci.nsIAsyncOutputStream);
  output.write(response, response.length);
  output.close();
  input.close();
}

async function run(args) {
  for (let arg of args) {
    print("client: running test", arg);
    let test = arg.split("|");
    await launchConnection(
      test[0],
      parseInt(test[1]),
      test[2],
      parseInt(test[3]),
      test[4]
    );
  }
}

var satisfied = false;
run(arguments).then(() => (satisfied = true));
var mainThread = Cc["@mozilla.org/thread-manager;1"].getService().mainThread;
while (!satisfied) {
  mainThread.processNextEvent(true);
}
