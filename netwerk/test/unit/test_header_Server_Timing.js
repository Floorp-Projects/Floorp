/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//
//  HTTP Server-Timing header test
//


function make_and_open_channel(url, callback) {
  let chan = NetUtil.newChannel({uri: url, loadUsingSystemPrincipal: true});
  chan.asyncOpen(new ChannelListener(callback, null, CL_ALLOW_UNKNOWN_CL));
}

var responseServerTiming = [{metric:"metric", duration:"123.4", description:"description"},
                            {metric:"metric2", duration:"456.78", description:"description1"}];
var trailerServerTiming = [{metric:"metric3", duration:"789.11", description:"description2"},
                           {metric:"metric4", duration:"1112.13", description:"description3"}];

function run_test()
{
  do_test_pending();

  // Set up to allow the cert presented by the server
  do_get_profile();
  let certdb = Cc["@mozilla.org/security/x509certdb;1"]
                  .getService(Ci.nsIX509CertDB);
  addCertFromFile(certdb, "http2-ca.pem", "CTu,u,u");

  Services.prefs.setCharPref("network.dns.localDomains", "foo.example.com");
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("network.dns.localDomains");
  });

  var env = Cc["@mozilla.org/process/environment;1"].getService(Ci.nsIEnvironment);
  var serverPort = env.get("MOZHTTP2_PORT");
  make_and_open_channel("https://foo.example.com:" + serverPort + "/server-timing", readServerContent);
}

function checkServerTimingContent(headers) {
  var expectedResult = responseServerTiming.concat(trailerServerTiming);
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
  do_test_finished();
}
