/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Places unit test code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Marco Bonardo <mak77@bonardo.net> (Original Author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
 
/* Tests history redirects handling */

let hs = Cc["@mozilla.org/browser/nav-history-service;1"].
         getService(Ci.nsINavHistoryService);
let bh = hs.QueryInterface(Ci.nsIBrowserHistory);
let ghist3 = hs.QueryInterface(Ci.nsIGlobalHistory3);

const PERMA_REDIR_PATH = "/permaredir";
const TEMP_REDIR_PATH = "/tempredir";
const FOUND_PATH = "/found";

const HTTPSVR = new nsHttpServer();
const PORT = 4444;
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

  HTTPSVR.start(PORT);

  var chan = NetUtil.ioService
                    .newChannelFromURI(uri("http://localhost:4444/permaredir"));
  var listener = new ChannelListener();
  chan.notificationCallbacks = listener;
  chan.asyncOpen(listener, null);
  // The test will continue on onStopRequest.
}

function continue_test() {
  let stmt = DBConn().createStatement(
    "SELECT v.id, h.url, v.from_visit, v.visit_date, v.visit_type, v.session " +
    "FROM moz_historyvisits_view v " +
    "JOIN moz_places_view h on h.id = v.place_id " +
    "ORDER BY v.id ASC");
  const EXPECTED = [
    { id: 1,
      url: PERMA_REDIR_URL,
      from_visit: 0,
      visit_type: Ci.nsINavHistoryService.TRANSITION_LINK,
      session: 1 },
    { id: 2,
      url: TEMP_REDIR_URL,
      from_visit: 1,
      visit_type: Ci.nsINavHistoryService.TRANSITION_REDIRECT_PERMANENT,
      session: 1 },
    { id: 3,
      url: FOUND_URL,
      from_visit: 2,
      visit_type: Ci.nsINavHistoryService.TRANSITION_REDIRECT_TEMPORARY,
      session: 1 },
  ];
  try {
    while(stmt.executeStep()) {
      let comparator = EXPECTED.shift();
      do_check_eq(stmt.row.id, comparator.id);
      do_check_eq(stmt.row.url, comparator.url);
      do_check_eq(stmt.row.from_visit, comparator.from_visit);
      do_check_eq(stmt.row.visit_type, comparator.visit_type);
      do_check_eq(stmt.row.session, comparator.session);
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
    print("onStartRequest");
    this._got_onstartrequest = true;
  },

  onDataAvailable: function(request, context, stream, offset, count) {
    this._buffer = this._buffer.concat(read_stream(stream, count));
  },

  onStopRequest: function(request, context, status) {
    print("onStopRequest");
    this._got_onstoprequest++;
    let success = Components.isSuccessCode(status);
    do_check_true(success);
    do_check_true(this._got_onstartrequest);
    do_check_true(this._got_onchannelredirect);
    do_check_true(this._buffer.length > 0);

    // The referrer is wrong since it's the first element in the redirects
    // chain, but this is good, since it will test a special path.
    ghist3.addURI(uri(FOUND_URL), false, true, uri(PERMA_REDIR_URL));

    // This forces a CommitLazyMessages, so we don't have to wait for LAZY_ADD.
    // Actually trying to delete visits in future.
    hs.removeVisitsByTimeframe((Date.now() * 1000) + 1, (Date.now() * 1000) + 2);

    continue_test();
  },

  // nsIChannelEventSink
  onChannelRedirect: function (aOldChannel, aNewChannel, aFlags) {
    print("onChannelRedirect");
    this._got_onchannelredirect = true;
    ghist3.addDocumentRedirect(aOldChannel, aNewChannel, aFlags, true);
  },
};
