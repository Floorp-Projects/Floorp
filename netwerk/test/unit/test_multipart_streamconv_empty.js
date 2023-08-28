"use strict";

function make_channel(url) {
  return NetUtil.newChannel({ uri: url, loadUsingSystemPrincipal: true });
}

add_task(async function test_empty() {
  let uri = "http://localhost";
  let httpChan = make_channel(uri);

  let channel = Cc["@mozilla.org/network/input-stream-channel;1"]
    .createInstance(Ci.nsIInputStreamChannel)
    .QueryInterface(Ci.nsIChannel);

  channel.setURI(httpChan.URI);
  channel.loadInfo = httpChan.loadInfo;

  let inputStream = Cc["@mozilla.org/io/string-input-stream;1"].createInstance(
    Ci.nsIStringInputStream
  );
  inputStream.setUTF8Data(""); // Pass an empty string

  channel.contentStream = inputStream;

  let [status, buffer] = await new Promise(resolve => {
    let streamConv = Cc["@mozilla.org/streamConverters;1"].getService(
      Ci.nsIStreamConverterService
    );
    let multipartListener = {
      _buffer: "",

      QueryInterface: ChromeUtils.generateQI([
        "nsIStreamListener",
        "nsIRequestObserver",
      ]),

      onStartRequest(request) {},
      onDataAvailable(request, stream, offset, count) {
        try {
          this._buffer = this._buffer.concat(read_stream(stream, count));
          dump("BUFFEEE: " + this._buffer + "\n\n");
        } catch (ex) {
          do_throw("Error in onDataAvailable: " + ex);
        }
      },

      onStopRequest(request, status1) {
        resolve([status1, this._buffer]);
      },
    };
    let conv = streamConv.asyncConvertData(
      "multipart/mixed",
      "*/*",
      multipartListener,
      null
    );

    let chan = make_channel(uri);
    chan.asyncOpen(conv);
  });

  Assert.notEqual(
    status,
    Cr.NS_OK,
    "Should be an error code because content has no boundary"
  );
  Assert.equal(buffer, "", "Should have received no content");
});
