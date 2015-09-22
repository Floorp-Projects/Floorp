Cu.import("resource://testing-common/httpd.js");
Cu.import("resource://gre/modules/Services.jsm");

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

    {url: "/test/cebrotli1",
     flags: CL_EXPECT_GZIP,
     ce: "brotli",
     body: [0x0B, 0x02, 0x80, 0x74, 0x65, 0x73, 0x74, 0x0A, 0x03],

     datalen: 5 // the data length of the uncompressed document
    },

    // this is not a brotli document
    {url: "/test/cebrotli2",
     flags: CL_EXPECT_GZIP | CL_EXPECT_FAILURE,
     ce: "brotli",
     body: [0x0B, 0x0A, 0x09],
     datalen: 3
    },

    // this is brotli but should come through as identity due to prefs
    {url: "/test/cebrotli3",
     flags: 0,
     ce: "brotli",
     body: [0x0B, 0x02, 0x80, 0x74, 0x65, 0x73, 0x74, 0x0A, 0x03],

     datalen: 9
    },
];

function setupChannel(url) {
    var ios = Components.classes["@mozilla.org/network/io-service;1"].
                         getService(Ci.nsIIOService);
    var chan = ios.newChannel2("http://localhost:" +
                               httpserver.identity.primaryPort + url,
                               "",
                               null,
                               null,      // aLoadingNode
                               Services.scriptSecurityManager.getSystemPrincipal(),
                               null,      // aTriggeringPrincipal
                               Ci.nsILoadInfo.SEC_NORMAL,
                               Ci.nsIContentPolicy.TYPE_OTHER);
    return chan;
}

function startIter() {
    if (tests[index].url === "/test/cebrotli3") {
      // this test wants to make sure we don't do brotli when not in a-e
      prefs.setCharPref("network.http.accept-encoding", "gzip, deflate");
    }
    var channel = setupChannel(tests[index].url);
    channel.asyncOpen(new ChannelListener(completeIter, channel, tests[index].flags), null);
}

function completeIter(request, data, ctx) {
    if (!(tests[index].flags & CL_EXPECT_FAILURE)) {
	do_check_eq(data.length, tests[index].datalen);
    }
    if (++index < tests.length) {
	startIter();
    } else {
        httpserver.stop(do_test_finished);
	prefs.setCharPref("network.http.accept-encoding", cePref);
    }
}

var prefs;
var cePref;
function run_test() {
    prefs = Cc["@mozilla.org/preferences-service;1"].getService(Ci.nsIPrefBranch);
    cePref = prefs.getCharPref("network.http.accept-encoding");
    prefs.setCharPref("network.http.accept-encoding", "gzip, deflate, brotli");

    httpserver.registerPathHandler("/test/cegzip1", handler);
    httpserver.registerPathHandler("/test/cegzip2", handler);
    httpserver.registerPathHandler("/test/cebrotli1", handler);
    httpserver.registerPathHandler("/test/cebrotli2", handler);
    httpserver.registerPathHandler("/test/cebrotli3", handler);
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
 
