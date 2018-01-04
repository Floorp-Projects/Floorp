/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//
//  HTTP Server-Timing header test
//

Cu.import("resource://testing-common/httpd.js");
Cu.import("resource://gre/modules/NetUtil.jsm");

XPCOMUtils.defineLazyGetter(this, "URL", function() {
  return "http://localhost:" + httpServer.identity.primaryPort + "/content";
});

let httpServer = null;

function make_and_open_channel(url, callback) {
  let chan = NetUtil.newChannel({uri: url, loadUsingSystemPrincipal: true});
  chan.asyncOpen2(new ChannelListener(callback, null, CL_ALLOW_UNKNOWN_CL));
}

var respnseServerTiming = [{metric:"metric", duration:"123.4", description:"description"},
                           {metric:"metric2", duration:"456.78", description:"description1"}];
var trailerServerTiming = [{metric:"metric3", duration:"789.11", description:"description2"},
                           {metric:"metric4", duration:"1112.13", description:"description3"}];

function createServerTimingHeader(headerData) {
  var header = "";
  for (var i = 0; i < headerData.length; i++) {
    header += "Server-Timing:" + headerData[i].metric + ";" +
              "dur=" + headerData[i].duration + ";" +
              "desc=" + headerData[i].description + "\r\n";
  }
  return header;
}

function contentHandler(metadata, response)
{
  var body = "c\r\ndata reached\r\n3\r\nhej\r\n0\r\n";

  response.seizePower();
  response.write("HTTP/1.1 200 OK\r\n");
  response.write("Content-Type: text/plain\r\n");
  response.write(createServerTimingHeader(respnseServerTiming));

  response.write("Transfer-Encoding: chunked\r\n");
  response.write("\r\n");
  response.write(body);
  response.write(createServerTimingHeader(trailerServerTiming));
  response.write("\r\n");
  response.finish();
}

function run_test()
{
  httpServer = new HttpServer();
  httpServer.registerPathHandler("/content", contentHandler);
  httpServer.start(-1);

  do_test_pending();
  make_and_open_channel(URL, readServerContent);
}

function checkServerTimingContent(headers) {
  var expectedResult = respnseServerTiming.concat(trailerServerTiming);
  Assert.equal(headers.length, expectedResult.length);

  for (var i = 0; i < expectedResult.length; i++) {
    let header = headers.queryElementAt(i, Ci.nsIServerTiming);
    Assert.equal(header.name, expectedResult[i].metric);
    Assert.equal(header.description, expectedResult[i].description);
    Assert.equal(header.duration, parseFloat(expectedResult[i].duration));
  }
}

function readServerContent(request, buffer)
{
  let channel = request.QueryInterface(Ci.nsITimedChannel);
  let headers = channel.serverTiming.QueryInterface(Ci.nsIArray);
  checkServerTimingContent(headers);

  httpServer.stop(do_test_finished);
}
