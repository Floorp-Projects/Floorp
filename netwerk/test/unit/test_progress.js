
do_load_httpd_js();

var httpserver = new nsHttpServer();
var testpath = "/simple";
var httpbody = "0123456789";

var last = 0, max = 0;

const STATUS_RECEIVING_FROM = 0x804b0006;
const LOOPS = 50000;

var progressCallback = {
  QueryInterface: function (iid) {
    if (iid.equals(Ci.nsISupports) || iid.equals(Ci.nsIProgressEventSink))
      return this;
    throw Cr.NS_ERROR_NO_INTERFACE;
  },

  getInterface: function (iid) {
    if (iid.equals(Ci.nsIProgressEventSink))
      return this;
    throw Cr.NS_ERROR_NO_INTERFACE;
  },

  onProgress: function (request, context, progress, progressMax) {
    do_check_eq(mStatus, STATUS_RECEIVING_FROM);
    last = progress;
    max = progressMax;
  },

  onStatus: function (request, context, status, statusArg) {
    do_check_eq(statusArg, "localhost");
    mStatus = status;
  },

  mStatus: 0,
};

function run_test() {
  httpserver.registerPathHandler(testpath, serverHandler);
  httpserver.start(4444);
  var channel = setupChannel(testpath);
  channel.asyncOpen(new ChannelListener(checkRequest, channel), null);
  do_test_pending();
}

function setupChannel(path) {
  var ios = Cc["@mozilla.org/network/io-service;1"].
                       getService(Ci.nsIIOService);
  var chan = ios.newChannel("http://localhost:4444" + path, "", null);
  chan.QueryInterface(Ci.nsIHttpChannel);
  chan.requestMethod = "GET";
  chan.notificationCallbacks = progressCallback;
  return chan;
}

function serverHandler(metadata, response) {
  response.setHeader("Content-Type", "text/plain", false);
  for (let i = 0; i < LOOPS; i++)
    response.bodyOutputStream.write(httpbody, httpbody.length);
}

function checkRequest(request, data, context) {
  do_check_eq(last, httpbody.length*LOOPS);
  do_check_eq(max, httpbody.length*LOOPS);
  httpserver.stop(do_test_finished);
}
