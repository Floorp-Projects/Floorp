/* run some tests on the data: protocol handler */

// The behaviour wrt spaces is:
// - Textual content keeps all spaces
// - Other content strips unescaped spaces
// - Base64 content strips escaped and unescaped spaces

"use strict";

var urls = [
  ["data:,", "text/plain", ""],
  ["data:,foo", "text/plain", "foo"],
  [
    "data:application/octet-stream,foo bar",
    "application/octet-stream",
    "foobar",
  ],
  [
    "data:application/octet-stream,foo%20bar",
    "application/octet-stream",
    "foo bar",
  ],
  ["data:application/xhtml+xml,foo bar", "application/xhtml+xml", "foo bar"],
  ["data:application/xhtml+xml,foo%20bar", "application/xhtml+xml", "foo bar"],
  ["data:text/plain,foo%00 bar", "text/plain", "foo\x00 bar"],
  ["data:text/plain;x=y,foo%00 bar", "text/plain", "foo\x00 bar"],
  ["data:;x=y,foo%00 bar", "text/plain", "foo\x00 bar"],
  ["data:text/plain;base64,Zm9 vI%20GJ%0Dhc%0Ag==", "text/plain", "foo bar"],
  ["DATA:TEXT/PLAIN;BASE64,Zm9 vI%20GJ%0Dhc%0Ag==", "text/plain", "foo bar"],
  ["DaTa:;BaSe64,Zm9 vI%20GJ%0Dhc%0Ag==", "text/plain", "foo bar"],
  ["data:;x=y;base64,Zm9 vI%20GJ%0Dhc%0Ag==", "text/plain", "foo bar"],
  // Bug 774240
  [
    "data:application/octet-stream;base64=y,foobar",
    "application/octet-stream",
    "foobar",
  ],
  // Bug 781693
  ["data:text/plain;base64;x=y,dGVzdA==", "text/plain", "test"],
  ["data:text/plain;x=y;base64,dGVzdA==", "text/plain", "test"],
  ["data:text/plain;x=y;base64,", "text/plain", ""],
  ["data:  ;charset=x   ;  base64,WA", "text/plain", "X", "x"],
  ["data:base64,WA", "text/plain", "WA", "US-ASCII"],
];

function run_test() {
  dump("*** run_test\n");

  function on_read_complete(request, data, idx) {
    dump("*** run_test.on_read_complete\n");

    if (request.nsIChannel.contentType != urls[idx][1]) {
      do_throw(
        "Type mismatch! Is <" +
          chan.contentType +
          ">, should be <" +
          urls[idx][1] +
          ">"
      );
    }

    if (urls[idx][3] && request.nsIChannel.contentCharset !== urls[idx][3]) {
      do_throw(
        `Charset mismatch! Test <${urls[idx][0]}> - Is <${request.nsIChannel.contentCharset}>, should be <${urls[idx][3]}>`
      );
    }

    /* read completed successfully.  now compare the data. */
    if (data != urls[idx][2]) {
      do_throw(
        "Stream contents do not match with direct read! Is <" +
          data +
          ">, should be <" +
          urls[idx][2] +
          ">"
      );
    }
    do_test_finished();
  }

  var ios = Cc["@mozilla.org/network/io-service;1"].getService(Ci.nsIIOService);
  for (var i = 0; i < urls.length; ++i) {
    dump("*** opening channel " + i + "\n");
    do_test_pending();
    var chan = NetUtil.newChannel({
      uri: urls[i][0],
      loadUsingSystemPrincipal: true,
    });
    chan.contentType = "foo/bar"; // should be ignored
    chan.asyncOpen(new ChannelListener(on_read_complete, i));
  }
}
