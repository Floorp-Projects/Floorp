const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cr = Components.results;

Cu.import("resource://testing-common/httpd.js");

var httpserver = new HttpServer();
var index = 0;
var tests = [
    {url : "/test/test",
     datalen : 16},

    // Test that the http channel fails and the response body is suppressed
    // bug 255119
    {url: "/test/test",
     responseheader: [ "Location: javascript:alert()"],
     flags : CL_EXPECT_FAILURE,
     datalen : 0},
];

function setupChannel(url) {
    var ios = Components.classes["@mozilla.org/network/io-service;1"].
                         getService(Ci.nsIIOService);
    var chan = ios.newChannel("http://localhost:" +
			      httpserver.identity.primaryPort + url, "", null);
    return chan;
}

function startIter() {
    var channel = setupChannel(tests[index].url);
    channel.asyncOpen(new ChannelListener(completeIter, channel, tests[index].flags), null);
}

function completeIter(request, data, ctx) {
    do_check_true(data.length == tests[index].datalen);
    if (++index < tests.length) {
	startIter();
    } else {
        httpserver.stop(do_test_finished);
    }
}

function run_test() {
    httpserver.registerPathHandler("/test/test", handler);
    httpserver.start(-1);

    startIter();
    do_test_pending();
}

function handler(metadata, response) {
    var body = "thequickbrownfox";
    response.setHeader("Content-Type", "text/plain", false);

    var header = tests[index].responseheader;
    if (header != undefined) {
        for (var i = 0; i < header.length; i++) {
            var splitHdr = header[i].split(": ");
            response.setHeader(splitHdr[0], splitHdr[1], false);
        }
    }
    
    response.setStatusLine(metadata.httpVersion, 302, "Redirected");
    response.bodyOutputStream.write(body, body.length);
}
 
