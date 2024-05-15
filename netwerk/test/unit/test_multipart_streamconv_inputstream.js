/* Any copyright is dedicated to the Public Domain.
https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

function make_channel(url) {
  return NetUtil.newChannel({ uri: url, loadUsingSystemPrincipal: true });
}

var multipartBody =
  "--boundary\r\n\r\nSome text\r\n--boundary\r\nContent-Type: text/x-test\r\n\r\n<?xml version='1.1'?>\r\n<root/>\r\n--boundary\r\n\r\n<?xml version='1.0'?><root/>\r\n--boundary--";

var testData = [
  { data: "Some text", type: "text/plain" },
  { data: "<?xml version='1.1'?>\r\n<root/>", type: "text/x-test" },
  { data: "<?xml version='1.0'?><root/>", type: "text/xml" },
];

function responseHandler(request, index, buffer) {
  Assert.equal(buffer, testData[index].data);
  Assert.equal(
    request.QueryInterface(Ci.nsIChannel).contentType,
    testData[index].type
  );
}

add_task(async function test_inputstream() {
  let uri = "http://localhost";
  let httpChan = make_channel(uri);

  let inputStream = Cc["@mozilla.org/io/string-input-stream;1"].createInstance(
    Ci.nsIStringInputStream
  );

  inputStream.setUTF8Data(multipartBody);

  let channel = Cc["@mozilla.org/network/input-stream-channel;1"]
    .createInstance(Ci.nsIInputStreamChannel)
    .QueryInterface(Ci.nsIChannel);

  channel.setURI(httpChan.URI);
  channel.loadInfo = httpChan.loadInfo;
  channel.contentStream = inputStream;
  channel.contentType = `multipart/mixed; boundary="boundary"`;

  await new Promise(resolve => {
    let streamConv = Cc["@mozilla.org/streamConverters;1"].getService(
      Ci.nsIStreamConverterService
    );
    let multipartListener = {
      _buffer: "",
      _index: 0,

      QueryInterface: ChromeUtils.generateQI([
        "nsIStreamListener",
        "nsIRequestObserver",
      ]),

      onStartRequest() {},
      onDataAvailable(request, stream, offset, count) {
        try {
          this._buffer = this._buffer.concat(read_stream(stream, count));
          dump("BUFFEEE: " + this._buffer + "\n\n");
        } catch (ex) {
          do_throw("Error in onDataAvailable: " + ex);
        }
      },

      onStopRequest(request) {
        try {
          responseHandler(request, this._index, this._buffer);
        } catch (ex) {
          do_throw("Error in closure function: " + ex);
        }

        this._index++;
        this._buffer = "";

        let isLastPart = request.QueryInterface(
          Ci.nsIMultiPartChannel
        ).isLastPart;
        Assert.equal(isLastPart, this._index == testData.length);

        if (isLastPart) {
          resolve();
        }
      },
    };
    let conv = streamConv.asyncConvertData(
      "multipart/mixed",
      "*/*",
      multipartListener,
      null
    );

    channel.asyncOpen(conv);
  });
});
