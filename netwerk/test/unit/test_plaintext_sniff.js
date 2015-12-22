// Test the plaintext-or-binary sniffer

Cu.import("resource://testing-common/httpd.js");
Cu.import("resource://gre/modules/NetUtil.jsm");

// List of Content-Type headers to test.  For each header we have an array.
// The first element in the array is the Content-Type header string.  The
// second element in the array is a boolean indicating whether we allow
// sniffing for that type.
var contentTypeHeaderList =
[
 [ "text/plain", true ],
 [ "text/plain; charset=ISO-8859-1", true ],
 [ "text/plain; charset=iso-8859-1", true ],
 [ "text/plain; charset=UTF-8", true ],
 [ "text/plain; charset=unknown", false ],
 [ "text/plain; param", false ],
 [ "text/plain; charset=ISO-8859-1; param", false ],
 [ "text/plain; charset=iso-8859-1; param", false ],
 [ "text/plain; charset=UTF-8; param", false ],
 [ "text/plain; charset=utf-8", false ],
 [ "text/plain; charset=utf8", false ],
 [ "text/plain; charset=UTF8", false ],
 [ "text/plain; charset=iSo-8859-1", false ]
];

// List of response bodies to test.  For each response we have an array. The
// first element in the array is the body string.  The second element in the
// array is a boolean indicating whether that string should sniff as binary.
var bodyList =
[
 [ "Plaintext", false ]
];

// List of possible BOMs
var BOMList =
[
 "\xFE\xFF",  // UTF-16BE
 "\xFF\xFE",  // UTF-16LE
 "\xEF\xBB\xBF", // UTF-8
 "\x00\x00\xFE\xFF", // UCS-4BE
 "\x00\x00\xFF\xFE" // UCS-4LE
];

// Build up bodyList.  The things we treat as binary are ASCII codes 0-8,
// 14-26, 28-31.  That is, the control char range, except for tab, newline,
// vertical tab, form feed, carriage return, and ESC (this last being used by
// Shift_JIS, apparently).
function isBinaryChar(ch) {
  return (0 <= ch && ch <= 8) || (14 <= ch && ch <= 26) ||
         (28 <= ch && ch <= 31);
}

// Test chars on their own
var i;
for (i = 0; i <= 127; ++i) {  
  bodyList.push([ String.fromCharCode(i), isBinaryChar(i) ]);
}

// Test that having a BOM prevents plaintext sniffing
var j;
for (i = 0; i <= 127; ++i) {
  for (j = 0; j < BOMList.length; ++j) {
    bodyList.push([ BOMList[j] + String.fromCharCode(i, i), false ]);
  }
}

// Test that having a BOM requires at least 4 chars to kick in
for (i = 0; i <= 127; ++i) {
  for (j = 0; j < BOMList.length; ++j) {
    bodyList.push([ BOMList[j] + String.fromCharCode(i),
                    BOMList[j].length == 2 && isBinaryChar(i) ]);
  }
}

function makeChan(headerIdx, bodyIdx) {
  var chan = NetUtil.newChannel({
    uri: "http://localhost:" + httpserv.identity.primaryPort +
         "/" + headerIdx + "/" + bodyIdx,
    loadUsingSystemPrincipal: true
  }).QueryInterface(Components.interfaces.nsIHttpChannel);

  chan.loadFlags |=
    Components.interfaces.nsIChannel.LOAD_CALL_CONTENT_SNIFFERS;

  return chan;
}

function makeListener(headerIdx, bodyIdx) {
  var listener = {
    onStartRequest : function test_onStartR(request, ctx) {
      try {
        var chan = request.QueryInterface(Components.interfaces.nsIChannel);

        do_check_eq(chan.status, Components.results.NS_OK);
        
        var type = chan.contentType;

        var expectedType =
          contentTypeHeaderList[headerIdx][1] && bodyList[bodyIdx][1] ?
            "application/x-vnd.mozilla.guess-from-ext" : "text/plain";
        if (expectedType != type) {
          do_throw("Unexpected sniffed type '" + type + "'.  " +
                   "Should be '" + expectedType + "'.  " +
                   "Header is ['" +
                     contentTypeHeaderList[headerIdx][0] + "', " +
                     contentTypeHeaderList[headerIdx][1] + "].  " +
                   "Body is ['" +
                     bodyList[bodyIdx][0].toSource() + "', " +
                     bodyList[bodyIdx][1] +
                   "].");
        }
        do_check_eq(expectedType, type);
      } catch (e) {
        do_throw("Unexpected exception: " + e);
      }

      throw Components.results.NS_ERROR_ABORT;
    },

    onDataAvailable: function test_ODA() {
      do_throw("Should not get any data!");
    },

    onStopRequest: function test_onStopR(request, ctx, status) {
      // Advance to next test
      ++headerIdx;
      if (headerIdx == contentTypeHeaderList.length) {
        headerIdx = 0;
        ++bodyIdx;
      }

      if (bodyIdx == bodyList.length) {
        do_test_pending();
        httpserv.stop(do_test_finished);
      } else {
        doTest(headerIdx, bodyIdx);
      }

      do_test_finished();
    }    
  };

  return listener;
}

function doTest(headerIdx, bodyIdx) {
  var chan = makeChan(headerIdx, bodyIdx);

  var listener = makeListener(headerIdx, bodyIdx);

  chan.asyncOpen2(listener);

  do_test_pending();    
}

function createResponse(headerIdx, bodyIdx, metadata, response) {
  response.setHeader("Content-Type", contentTypeHeaderList[headerIdx][0], false);
  response.bodyOutputStream.write(bodyList[bodyIdx][0],
                                  bodyList[bodyIdx][0].length);
}

function makeHandler(headerIdx, bodyIdx) {
  var f = 
    function handlerClosure(metadata, response) {
      return createResponse(headerIdx, bodyIdx, metadata, response);
    };
  return f;
}

var httpserv;
function run_test() {
  // disable again for everything for now (causes sporatic oranges)
  return;

  // disable on Windows for now, because it seems to leak sockets and die.
  // Silly operating system!
  // This is a really nasty way to detect Windows.  I wish we could do better.
  if (mozinfo.os == "win") {
    return;
  }

  httpserv = new HttpServer();

  for (i = 0; i < contentTypeHeaderList.length; ++i) {
    for (j = 0; j < bodyList.length; ++j) {
      httpserv.registerPathHandler("/" + i + "/" + j, makeHandler(i, j));
    }
  }

  httpserv.start(-1);

  doTest(0, 0);
}
