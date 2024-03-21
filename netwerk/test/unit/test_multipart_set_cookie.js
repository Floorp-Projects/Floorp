"use strict";

const { HttpServer } = ChromeUtils.importESModule(
  "resource://testing-common/httpd.sys.mjs"
);

var httpserver = null;

ChromeUtils.defineLazyGetter(this, "uri", function () {
  return "http://localhost:" + httpserver.identity.primaryPort + "/multipart";
});

function make_channel(url) {
  return NetUtil.newChannel({ uri: url, loadUsingSystemPrincipal: true });
}

var multipartBody =
  "--boundary\r\nSet-Cookie: foo=bar\r\n\r\nSome text\r\n--boundary--";

function contentHandler(metadata, response) {
  response.setHeader(
    "Content-Type",
    'multipart/x-mixed-replace; boundary="boundary"'
  );
  response.bodyOutputStream.write(multipartBody, multipartBody.length);
}

let first = true;

function responseHandler(request, buffer) {
  let channel = request.QueryInterface(Ci.nsIChannel);
  Assert.equal(buffer, "Some text");
  Assert.equal(channel.contentType, "text/plain");

  // two runs: pref on and off
  if (first) {
    // foo=bar should not be visible here
    Assert.equal(
      Services.cookies.getCookieStringFromHttp(channel.URI, channel),
      ""
    );
    first = false;
    Services.prefs.setBoolPref(
      "network.cookie.prevent_set_cookie_from_multipart",
      false
    );
    createConverterAndRequest();
  } else {
    // validate that the pref is working
    Assert.equal(
      Services.cookies.getCookieStringFromHttp(channel.URI, channel),
      "foo=bar"
    );
    httpserver.stop(do_test_finished);
  }
}

var multipartListener = {
  _buffer: "",

  QueryInterface: ChromeUtils.generateQI([
    "nsIStreamListener",
    "nsIRequestObserver",
  ]),

  onStartRequest() {
    this._buffer = "";
  },

  onDataAvailable(request, stream, offset, count) {
    try {
      this._buffer = this._buffer.concat(read_stream(stream, count));
      dump("BUFFEEE: " + this._buffer + "\n\n");
    } catch (ex) {
      do_throw("Error in onDataAvailable: " + ex);
    }
  },

  onStopRequest(request) {
    try {
      responseHandler(request, this._buffer);
    } catch (ex) {
      do_throw("Error in closure function: " + ex);
    }
  },
};

function createConverterAndRequest() {
  var streamConv = Cc["@mozilla.org/streamConverters;1"].getService(
    Ci.nsIStreamConverterService
  );
  var conv = streamConv.asyncConvertData(
    "multipart/x-mixed-replace",
    "*/*",
    multipartListener,
    null
  );

  var chan = make_channel(uri);
  chan.asyncOpen(conv);
}

function run_test() {
  Services.prefs.setBoolPref(
    "network.cookieJarSettings.unblocked_for_testing",
    true
  );

  httpserver = new HttpServer();
  httpserver.registerPathHandler("/multipart", contentHandler);
  httpserver.start(-1);

  createConverterAndRequest();

  do_test_pending();
}
