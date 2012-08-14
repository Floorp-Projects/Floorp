const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cr = Components.results;

Cu.import("resource://testing-common/httpd.js");

const SERVER_PORT = 8080;
const baseURL = "http://localhost:" + SERVER_PORT + "/";

var cookie = "";
for (let i =0; i < 10000; i++) {
    cookie += " big cookie";
}

var listener = {
  onStartRequest: function (request, ctx) {
  },

  onDataAvailable: function (request, ctx, stream) {
  },

  onStopRequest: function (request, ctx, status) {
      do_check_eq(status, Components.results.NS_OK);
      do_test_finished();
  },

};

function run_test() {
    var server = new HttpServer();
    server.start(SERVER_PORT);
    server.registerPathHandler('/', function(metadata, response) {
        response.setStatusLine(metadata.httpVersion, 200, "OK");
        response.setHeader("Set-Cookie", "BigCookie=" + cookie, false);
        response.write("Hello world");
    });

    var ios = Components.classes["@mozilla.org/network/io-service;1"]
                         .getService(Components.interfaces.nsIIOService);
    var chan = ios.newChannel(baseURL, null, null)
                  .QueryInterface(Components.interfaces.nsIHttpChannel);
    chan.asyncOpen(listener, null);
    do_test_pending();
}
