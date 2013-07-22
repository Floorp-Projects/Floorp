const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cr = Components.results;

Cu.import("resource://testing-common/httpd.js");

var httpserver = new HttpServer();
var index = 0;
var tests = [
    {url: "/test/cegzip1",
     flags: CL_EXPECT_GZIP,
     ce: "gzip",
     body: [
	 0x1f, 0x8b, 0x08, 0x08, 0x5a, 0xa0, 0x31, 0x4f, 0x00, 0x03, 0x74, 0x78, 0x74, 0x00, 0x2b, 0xc9,
	 0xc8, 0x2c, 0x56, 0x00, 0xa2, 0x92, 0xd4, 0xe2, 0x12, 0x43, 0x2e, 0x00, 0xb9, 0x23, 0xd7, 0x3b,
	 0x0e, 0x00, 0x00, 0x00],
     datalen: 14 // the data length of the uncompressed document
    },

    {url: "/test/cegzip2", 
     flags: CL_EXPECT_GZIP,
     ce: "gzip, gzip",
     body: [
	 0x1f, 0x8b, 0x08, 0x00, 0x72, 0xa1, 0x31, 0x4f, 0x00, 0x03, 0x93, 0xef, 0xe6, 0xe0, 0x88, 0x5a, 
	 0x60, 0xe8, 0xcf, 0xc0, 0x5c, 0x52, 0x51, 0xc2, 0xa0, 0x7d, 0xf2, 0x84, 0x4e, 0x18, 0xc3, 0xa2, 
	 0x49, 0x57, 0x1e, 0x09, 0x39, 0xeb, 0x31, 0xec, 0x54, 0xbe, 0x6e, 0xcd, 0xc7, 0xc0, 0xc0, 0x00, 
	 0x00, 0x6e, 0x90, 0x7a, 0x85, 0x24, 0x00, 0x00, 0x00],
     datalen: 14 // the data length of the uncompressed document
    },
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
    httpserver.registerPathHandler("/test/cegzip1", handler);
    httpserver.registerPathHandler("/test/cegzip2", handler);
    httpserver.start(-1);

    startIter();
    do_test_pending();
}

function handler(metadata, response) {
    response.setStatusLine(metadata.httpVersion, 200, "OK");
    response.setHeader("Content-Type", "text/plain", false);
    response.setHeader("Content-Encoding", tests[index].ce, false);
    response.setHeader("Content-Length", "" + tests[index].body.length, false);
  
    var bos = Components.classes["@mozilla.org/binaryoutputstream;1"]
	.createInstance(Components.interfaces.nsIBinaryOutputStream);
    bos.setOutputStream(response.bodyOutputStream);

    response.processAsync();
    bos.writeByteArray(tests[index].body, tests[index].body.length);
    response.finish();
}
 
