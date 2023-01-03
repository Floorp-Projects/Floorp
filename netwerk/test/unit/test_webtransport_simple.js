//
//  Simple WebTransport test
//

"use strict";

var h3Port;
var host;

var { setTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs"
);

let WebTransportListener = function() {};

WebTransportListener.prototype = {
  onSessionReady(sessionId) {
    info("SessionId " + sessionId);
    this.ready();
  },
  onSessionClosed(errorCode, reason) {
    info("Error: " + errorCode + " reason: " + reason);
    if (this.closed) {
      this.closed();
    }
  },
  onIncomingBidirectionalStreamAvailable(stream) {
    info("got incoming bidirectional stream");
    this.streamAvailable(stream);
  },
  onIncomingUnidirectionalStreamAvailable(stream) {
    info("got incoming unidirectional stream");
    this.streamAvailable(stream);
  },
  onDatagramReceived(data) {
    info("got datagram");
    if (this.onDatagram) {
      this.onDatagram(data);
    }
  },
  onMaxDatagramSize(size) {
    info("max datagram size: " + size);
    if (this.onMaxDatagramSize) {
      this.onMaxDatagramSize(size);
    }
  },
  onOutgoingDatagramOutCome(id, outcome) {
    if (this.onDatagramOutcome) {
      this.onDatagramOutcome({ id, outcome });
    }
  },

  QueryInterface: ChromeUtils.generateQI(["WebTransportSessionEventListener"]),
};

registerCleanupFunction(async () => {
  Services.prefs.clearUserPref("network.dns.localDomains");
  Services.prefs.clearUserPref("network.webtransport.datagrams.enabled");
});

add_task(async function setup() {
  Services.prefs.setCharPref("network.dns.localDomains", "foo.example.com");
  Services.prefs.setBoolPref("network.webtransport.datagrams.enabled", true);

  h3Port = Services.env.get("MOZHTTP3_PORT");
  Assert.notEqual(h3Port, null);
  Assert.notEqual(h3Port, "");
  host = "foo.example.com:" + h3Port;
  do_get_profile();

  let certdb = Cc["@mozilla.org/security/x509certdb;1"].getService(
    Ci.nsIX509CertDB
  );
  // `../unit/` so that unit_ipc tests can use as well
  addCertFromFile(certdb, "../unit/http2-ca.pem", "CTu,u,u");
});

add_task(async function test_wt_datagram() {
  let webTransport = NetUtil.newWebTransport();
  let listener = new WebTransportListener().QueryInterface(
    Ci.WebTransportSessionEventListener
  );

  let pReady = new Promise(resolve => {
    listener.ready = resolve;
  });
  let pData = new Promise(resolve => {
    listener.onDatagram = resolve;
  });
  let pSize = new Promise(resolve => {
    listener.onMaxDatagramSize = resolve;
  });
  let pOutcome = new Promise(resolve => {
    listener.onDatagramOutcome = resolve;
  });

  webTransport.asyncConnect(
    NetUtil.newURI(`https://${host}/success`),
    Services.scriptSecurityManager.getSystemPrincipal(),
    Ci.nsILoadInfo.SEC_ALLOW_CROSS_ORIGIN_SEC_CONTEXT_IS_NULL,
    listener
  );

  await pReady;

  webTransport.getMaxDatagramSize();
  let size = await pSize;
  info("max size:" + size);

  let rawData = new Uint8Array(size);
  rawData.fill(42);

  webTransport.sendDatagram(rawData, 1);
  let { id, outcome } = await pOutcome;
  Assert.equal(id, 1);
  Assert.equal(outcome, Ci.WebTransportSessionEventListener.SENT);

  let received = await pData;
  Assert.deepEqual(received, rawData);

  webTransport.getMaxDatagramSize();
  size = await pSize;
  info("max size:" + size);

  rawData = new Uint8Array(size + 1);
  webTransport.sendDatagram(rawData, 2);

  pOutcome = new Promise(resolve => {
    listener.onDatagramOutcome = resolve;
  });
  ({ id, outcome } = await pOutcome);
  Assert.equal(id, 2);
  Assert.equal(
    outcome,
    Ci.WebTransportSessionEventListener.DROPPED_TOO_MUCH_DATA
  );

  webTransport.closeSession(0, "");
});

add_task(async function test_connect_wt() {
  let webTransport = NetUtil.newWebTransport();

  await new Promise(resolve => {
    let listener = new WebTransportListener().QueryInterface(
      Ci.WebTransportSessionEventListener
    );
    listener.ready = resolve;

    webTransport.asyncConnect(
      NetUtil.newURI(`https://${host}/success`),
      Services.scriptSecurityManager.getSystemPrincipal(),
      Ci.nsILoadInfo.SEC_ALLOW_CROSS_ORIGIN_SEC_CONTEXT_IS_NULL,
      listener
    );
  });

  webTransport.closeSession(0, "");
});

add_task(async function test_reject() {
  let webTransport = NetUtil.newWebTransport();

  await new Promise(resolve => {
    let listener = new WebTransportListener().QueryInterface(
      Ci.WebTransportSessionEventListener
    );
    listener.closed = resolve;

    webTransport.asyncConnect(
      NetUtil.newURI(`https://${host}/reject`),
      Services.scriptSecurityManager.getSystemPrincipal(),
      Ci.nsILoadInfo.SEC_ALLOW_CROSS_ORIGIN_SEC_CONTEXT_IS_NULL,
      listener
    );
  });
});

async function test_closed(path) {
  let webTransport = NetUtil.newWebTransport();

  let listener = new WebTransportListener().QueryInterface(
    Ci.WebTransportSessionEventListener
  );

  let pReady = new Promise(resolve => {
    listener.ready = resolve;
  });
  let pClose = new Promise(resolve => {
    listener.closed = resolve;
  });
  webTransport.asyncConnect(
    NetUtil.newURI(`https://${host}${path}`),
    Services.scriptSecurityManager.getSystemPrincipal(),
    Ci.nsILoadInfo.SEC_ALLOW_CROSS_ORIGIN_SEC_CONTEXT_IS_NULL,
    listener
  );

  await pReady;
  await pClose;
}

add_task(async function test_closed_0ms() {
  test_closed("/closeafter0ms");
});

add_task(async function test_closed_100ms() {
  test_closed("/closeafter100ms");
});

function WebTransportStreamCallback() {}

WebTransportStreamCallback.prototype = {
  QueryInterface: ChromeUtils.generateQI(["nsIWebTransportStreamCallback"]),

  onBidirectionalStreamReady(aStream) {
    Assert.ok(aStream != null);
    this.finish(aStream);
  },
  onUnidirectionalStreamReady(aStream) {
    Assert.ok(aStream != null);
    this.finish(aStream);
  },
  onError(aError) {
    this.finish(aError);
  },
};

function streamCreatePromise(transport, bidi) {
  return new Promise(resolve => {
    let listener = new WebTransportStreamCallback().QueryInterface(
      Ci.nsIWebTransportStreamCallback
    );
    listener.finish = resolve;

    if (bidi) {
      transport.createOutgoingBidirectionalStream(listener);
    } else {
      transport.createOutgoingUnidirectionalStream(listener);
    }
  });
}

add_task(async function test_wt_stream_create() {
  let webTransport = NetUtil.newWebTransport().QueryInterface(
    Ci.nsIWebTransport
  );

  let error = await streamCreatePromise(webTransport, true);
  Assert.equal(error, Ci.nsIWebTransport.INVALID_STATE_ERROR);

  await new Promise(resolve => {
    let listener = new WebTransportListener().QueryInterface(
      Ci.WebTransportSessionEventListener
    );
    listener.ready = resolve;

    webTransport.asyncConnect(
      NetUtil.newURI(`https://${host}/success`),
      Services.scriptSecurityManager.getSystemPrincipal(),
      Ci.nsILoadInfo.SEC_ALLOW_CROSS_ORIGIN_SEC_CONTEXT_IS_NULL,
      listener
    );
  });

  await streamCreatePromise(webTransport, true);
  await streamCreatePromise(webTransport, false);

  webTransport.closeSession(0, "");
});

function StreamStatsCallback() {}

StreamStatsCallback.prototype = {
  QueryInterface: ChromeUtils.generateQI([
    "nsIWebTransportStreamStatsCallback",
  ]),

  onSendStatsAvailable(aStats) {
    Assert.ok(aStats != null);
    this.finish(aStats);
  },
  onReceiveStatsAvailable(aStats) {
    Assert.ok(aStats != null);
    this.finish(aStats);
  },
};

function sendStreamStatsPromise(stream) {
  return new Promise(resolve => {
    let listener = new StreamStatsCallback().QueryInterface(
      Ci.nsIWebTransportStreamStatsCallback
    );
    listener.finish = resolve;

    stream.QueryInterface(Ci.nsIWebTransportSendStream);
    stream.getSendStreamStats(listener);
  });
}

function receiveStreamStatsPromise(stream) {
  return new Promise(resolve => {
    let listener = new StreamStatsCallback().QueryInterface(
      Ci.nsIWebTransportStreamStatsCallback
    );
    listener.finish = resolve;

    stream.QueryInterface(Ci.nsIWebTransportReceiveStream);
    stream.getReceiveStreamStats(listener);
  });
}

add_task(async function test_wt_stream_send_and_stats() {
  let webTransport = NetUtil.newWebTransport().QueryInterface(
    Ci.nsIWebTransport
  );

  await new Promise(resolve => {
    let listener = new WebTransportListener().QueryInterface(
      Ci.WebTransportSessionEventListener
    );
    listener.ready = resolve;

    webTransport.asyncConnect(
      NetUtil.newURI(`https://${host}/success`),
      Services.scriptSecurityManager.getSystemPrincipal(),
      Ci.nsILoadInfo.SEC_ALLOW_CROSS_ORIGIN_SEC_CONTEXT_IS_NULL,
      listener
    );
  });

  let stream = await streamCreatePromise(webTransport, false);
  stream.QueryInterface(Ci.nsIAsyncOutputStream);

  let data = "123456";
  stream.write(data, data.length);

  // We need some time to send the packet out.
  // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
  await new Promise(resolve => setTimeout(resolve, 2000));

  let stats = await sendStreamStatsPromise(stream);
  Assert.equal(stats.bytesSent, data.length);

  webTransport.closeSession(0, "");
});

function inputStreamReader() {}

inputStreamReader.prototype = {
  QueryInterface: ChromeUtils.generateQI(["nsIInputStreamCallback"]),

  onInputStreamReady(input) {
    let data = NetUtil.readInputStreamToString(input, input.available());
    this.finish(data);
  },
};

add_task(async function test_wt_receive_stream_and_stats() {
  let webTransport = NetUtil.newWebTransport().QueryInterface(
    Ci.nsIWebTransport
  );

  let listener = new WebTransportListener().QueryInterface(
    Ci.WebTransportSessionEventListener
  );

  let pReady = new Promise(resolve => {
    listener.ready = resolve;
  });
  let pStreamReady = new Promise(resolve => {
    listener.streamAvailable = resolve;
  });
  webTransport.asyncConnect(
    NetUtil.newURI(`https://${host}/create_unidi_stream`),
    Services.scriptSecurityManager.getSystemPrincipal(),
    Ci.nsILoadInfo.SEC_ALLOW_CROSS_ORIGIN_SEC_CONTEXT_IS_NULL,
    listener
  );

  await pReady;
  let stream = await pStreamReady;

  let data = await new Promise(resolve => {
    let handler = new inputStreamReader().QueryInterface(
      Ci.nsIInputStreamCallback
    );
    handler.finish = resolve;
    stream.QueryInterface(Ci.nsIAsyncInputStream);
    stream.asyncWait(handler, 0, 0, Services.tm.currentThread);
  });

  info("data: " + data);
  Assert.equal(data, "0123456789");

  let stats = await receiveStreamStatsPromise(stream);
  Assert.equal(stats.bytesReceived, data.length);

  stream.sendStopSending(0);

  webTransport.closeSession(0, "");
});

add_task(async function test_wt_outgoing_bidi_stream() {
  let webTransport = NetUtil.newWebTransport().QueryInterface(
    Ci.nsIWebTransport
  );

  await new Promise(resolve => {
    let listener = new WebTransportListener().QueryInterface(
      Ci.WebTransportSessionEventListener
    );
    listener.ready = resolve;

    webTransport.asyncConnect(
      NetUtil.newURI(`https://${host}/success`),
      Services.scriptSecurityManager.getSystemPrincipal(),
      Ci.nsILoadInfo.SEC_ALLOW_CROSS_ORIGIN_SEC_CONTEXT_IS_NULL,
      listener
    );
  });

  let stream = await streamCreatePromise(webTransport, true);
  stream.QueryInterface(Ci.nsIAsyncOutputStream);

  let data = "1234567";
  stream.write(data, data.length);

  let received = await new Promise(resolve => {
    let handler = new inputStreamReader().QueryInterface(
      Ci.nsIInputStreamCallback
    );
    handler.finish = resolve;
    stream.QueryInterface(Ci.nsIAsyncInputStream);
    stream.asyncWaitForRead(handler, 0, 0, Services.tm.currentThread);
  });

  info("received: " + received);
  Assert.equal(received, data);

  let stats = await sendStreamStatsPromise(stream);
  Assert.equal(stats.bytesSent, data.length);

  stats = await receiveStreamStatsPromise(stream);
  Assert.equal(stats.bytesReceived, data.length);

  webTransport.closeSession(0, "");
});

add_task(async function test_wt_incoming_bidi_stream() {
  let webTransport = NetUtil.newWebTransport().QueryInterface(
    Ci.nsIWebTransport
  );

  let listener = new WebTransportListener().QueryInterface(
    Ci.WebTransportSessionEventListener
  );

  let pReady = new Promise(resolve => {
    listener.ready = resolve;
  });
  let pStreamReady = new Promise(resolve => {
    listener.streamAvailable = resolve;
  });
  webTransport.asyncConnect(
    NetUtil.newURI(`https://${host}/create_bidi_stream`),
    Services.scriptSecurityManager.getSystemPrincipal(),
    Ci.nsILoadInfo.SEC_ALLOW_CROSS_ORIGIN_SEC_CONTEXT_IS_NULL,
    listener
  );

  await pReady;
  let stream = await pStreamReady;

  stream.QueryInterface(Ci.nsIAsyncOutputStream);

  let data = "12345678";
  stream.write(data, data.length);

  let received = await new Promise(resolve => {
    let handler = new inputStreamReader().QueryInterface(
      Ci.nsIInputStreamCallback
    );
    handler.finish = resolve;
    stream.QueryInterface(Ci.nsIAsyncInputStream);
    stream.asyncWaitForRead(handler, 0, 0, Services.tm.currentThread);
  });

  info("received: " + received);
  Assert.equal(received, data);

  let stats = await sendStreamStatsPromise(stream);
  Assert.equal(stats.bytesSent, data.length);

  stats = await receiveStreamStatsPromise(stream);
  Assert.equal(stats.bytesReceived, data.length);

  webTransport.closeSession(0, "");
});
