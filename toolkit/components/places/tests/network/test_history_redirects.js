/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/* Tests history redirects handling */

let hs = Cc["@mozilla.org/browser/nav-history-service;1"].
         getService(Ci.nsINavHistoryService);
let bh = hs.QueryInterface(Ci.nsIBrowserHistory);

const PERMA_REDIR_PATH = "/permaredir";
const TEMP_REDIR_PATH = "/tempredir";
const FOUND_PATH = "/found";

const HTTPSVR = new HttpServer();
HTTPSVR.start(-1);
const PORT = HTTPSVR.identity.primaryPort;
HTTPSVR.registerPathHandler(PERMA_REDIR_PATH, permaRedirHandler);
HTTPSVR.registerPathHandler(TEMP_REDIR_PATH, tempRedirHandler);
HTTPSVR.registerPathHandler(FOUND_PATH, foundHandler);

const STATUS = {
  REDIRECT_PERMANENT: [301, "Moved Permanently"],
  REDIRECT_TEMPORARY: [302, "Moved"],
  FOUND: [200, "Found"],
}

const PERMA_REDIR_URL = "http://localhost:" + PORT + PERMA_REDIR_PATH;
const TEMP_REDIR_URL = "http://localhost:" + PORT + TEMP_REDIR_PATH;
const FOUND_URL = "http://localhost:" + PORT + FOUND_PATH;

// PERMANENT REDIRECT
function permaRedirHandler(aMeta, aResponse) {
  // Redirect permanently to TEMP_REDIR_URL
  PathHandler(aMeta, aResponse, "REDIRECT_PERMANENT", TEMP_REDIR_URL);
}

// TEMPORARY REDIRECT
function tempRedirHandler(aMeta, aResponse) {
  // Redirect temporarily to FOUND_URL
  PathHandler(aMeta, aResponse, "REDIRECT_TEMPORARY", FOUND_URL);
}

// FOUND
function foundHandler(aMeta, aResponse) {
  PathHandler(aMeta, aResponse, "FOUND");
}

function PathHandler(aMeta, aResponse, aChannelEvent, aRedirURL) {
  aResponse.setStatusLine(aMeta.httpVersion,
                          STATUS[aChannelEvent][0],   // Code
                          STATUS[aChannelEvent][1]);  // Text
  if (aRedirURL)
    aResponse.setHeader("Location", aRedirURL, false);

  //aResponse.setHeader("Content-Type", "text/html", false);
  let body = STATUS[aChannelEvent][1] + "\r\n";
  aResponse.bodyOutputStream.write(body, body.length);
}

function run_test() {
  do_test_pending();

  var chan = NetUtil.ioService.newChannelFromURI(uri(PERMA_REDIR_URL));
  var listener = new ChannelListener();
  chan.notificationCallbacks = listener;
  chan.asyncOpen(listener, null);
  // The test will continue on onStopRequest.
}

function continue_test() {
  let stmt = DBConn().createStatement(
    "SELECT v.id, h.url, v.from_visit, v.visit_date, v.visit_type " +
    "FROM moz_historyvisits v " +
    "JOIN moz_places h on h.id = v.place_id " +
    "ORDER BY v.id ASC");
  const EXPECTED = [
    { id: 1,
      url: PERMA_REDIR_URL,
      from_visit: 0,
      visit_type: Ci.nsINavHistoryService.TRANSITION_LINK },
    { id: 2,
      url: TEMP_REDIR_URL,
      from_visit: 1,
      visit_type: Ci.nsINavHistoryService.TRANSITION_REDIRECT_PERMANENT },
    { id: 3,
      url: FOUND_URL,
      from_visit: 2,
      visit_type: Ci.nsINavHistoryService.TRANSITION_REDIRECT_TEMPORARY },
  ];
  try {
    while(stmt.executeStep()) {
      let comparator = EXPECTED.shift();
      do_log_info("Checking that '" + comparator.url +
                  "' was entered into the DB correctly");
      do_check_eq(stmt.row.id, comparator.id);
      do_check_eq(stmt.row.url, comparator.url);
      do_check_eq(stmt.row.from_visit, comparator.from_visit);
      do_check_eq(stmt.row.visit_type, comparator.visit_type);
    }
  }
  finally {
    stmt.finalize();
  }

  HTTPSVR.stop(do_test_finished);
}

/**
 * Read count bytes from stream and return as a String object
 */
function read_stream(stream, count) {
  /* assume stream has non-ASCII data */
  var wrapper =
      Components.classes["@mozilla.org/binaryinputstream;1"]
                .createInstance(Components.interfaces.nsIBinaryInputStream);
  wrapper.setInputStream(stream);
  /* JS methods can be called with a maximum of 65535 arguments, and input
     streams don't have to return all the data they make .available() when
     asked to .read() that number of bytes. */
  var data = [];
  while (count > 0) {
    var bytes = wrapper.readByteArray(Math.min(65535, count));
    data.push(String.fromCharCode.apply(null, bytes));
    count -= bytes.length;
    if (bytes.length == 0)
      do_throw("Nothing read from input stream!");
  }
  return data.join('');
}

function ChannelListener() {

}
ChannelListener.prototype = {
  _buffer: "",
  _got_onstartrequest: false,
  _got_onchannelredirect: false,

  QueryInterface: XPCOMUtils.generateQI([
    Ci.nsIStreamListener,
    Ci.nsIRequestObserver,
    Ci.nsIInterfaceRequestor,
    Ci.nsIChannelEventSink,
  ]),

  // nsIInterfaceRequestor
  getInterface: function (aIID) {
    try {
      return this.QueryInterface(aIID);
    } catch (e) {
      throw Components.results.NS_NOINTERFACE;
    }
  },

  onStartRequest: function(request, context) {
    do_log_info("onStartRequest");
    this._got_onstartrequest = true;
  },

  onDataAvailable: function(request, context, stream, offset, count) {
    this._buffer = this._buffer.concat(read_stream(stream, count));
  },

  onStopRequest: function(request, context, status) {
    do_log_info("onStopRequest");
    this._got_onstoprequest++;
    let success = Components.isSuccessCode(status);
    do_check_true(success);
    do_check_true(this._got_onstartrequest);
    do_check_true(this._got_onchannelredirect);
    do_check_true(this._buffer.length > 0);

    continue_test();
  },

  // nsIChannelEventSink
  asyncOnChannelRedirect: function (aOldChannel, aNewChannel, aFlags, callback) {
    do_log_info("onChannelRedirect");
    this._got_onchannelredirect = true;
    callback.onRedirectVerifyCallback(Components.results.NS_OK);
  },
};
