"use strict";

const convService = Cc["@mozilla.org/streamConverters;1"]
  .getService(Ci.nsIStreamConverterService);

const UUID = "72b61ee3-aceb-476c-be1b-0822b036c9f1";
const ADDON_ID = "test@web.extension";
const URI = NetUtil.newURI(`moz-extension://${UUID}/file.css`);

const FROM_TYPE = "application/vnd.mozilla.webext.unlocalized";
const TO_TYPE = "text/css";


function StringStream(string) {
  let stream = Cc["@mozilla.org/io/string-input-stream;1"]
    .createInstance(Ci.nsIStringInputStream);

  stream.data = string;
  return stream;
}


// Initialize the policy service with a stub localizer for our
// add-on ID.
add_task(async function init() {
  let policy = new WebExtensionPolicy({
    id: ADDON_ID,
    mozExtensionHostname: UUID,
    baseURL: "file:///",

    allowedOrigins: new MatchPatternSet([]),

    localizeCallback(string) {
      return string.replace(/__MSG_(.*?)__/g, "<localized-$1>");
    },
  });

  policy.active = true;

  registerCleanupFunction(() => {
    policy.active = false;
  });
});


// Test that the synchronous converter works as expected with a
// simple string.
add_task(async function testSynchronousConvert() {
  let stream = StringStream("Foo __MSG_xxx__ bar __MSG_yyy__ baz");

  let resultStream = convService.convert(stream, FROM_TYPE, TO_TYPE, URI);

  let result = NetUtil.readInputStreamToString(resultStream, resultStream.available());

  equal(result, "Foo <localized-xxx> bar <localized-yyy> baz");
});


// Test that the asynchronous converter works as expected with input
// split into multiple chunks, and a boundary in the middle of a
// replacement token.
add_task(async function testAsyncConvert() {
  let listener;
  let awaitResult = new Promise((resolve, reject) => {
    listener = {
      QueryInterface: ChromeUtils.generateQI([Ci.nsIStreamListener]),

      onDataAvailable(request, context, inputStream, offset, count) {
        this.resultParts.push(NetUtil.readInputStreamToString(inputStream, count));
      },

      onStartRequest() {
        ok(!("resultParts" in this));
        this.resultParts = [];
      },

      onStopRequest(request, context, statusCode) {
        if (!Components.isSuccessCode(statusCode)) {
          reject(new Error(statusCode));
        }

        resolve(this.resultParts.join("\n"));
      },
    };
  });

  let parts = ["Foo __MSG_x", "xx__ bar __MSG_yyy__ baz"];

  let converter = convService.asyncConvertData(FROM_TYPE, TO_TYPE, listener, URI);
  converter.onStartRequest(null, null);

  for (let part of parts) {
    converter.onDataAvailable(null, null, StringStream(part), 0, part.length);
  }

  converter.onStopRequest(null, null, Cr.NS_OK);


  let result = await awaitResult;
  equal(result, "Foo <localized-xxx> bar <localized-yyy> baz");
});


// Test that attempting to initialize a converter with the URI of a
// nonexistent WebExtension fails.
add_task(async function testInvalidUUID() {
  let uri = NetUtil.newURI("moz-extension://eb4f3be8-41c9-4970-aa6d-b84d1ecc02b2/file.css");
  let stream = StringStream("Foo __MSG_xxx__ bar __MSG_yyy__ baz");

  // Assert.throws raise a TypeError exception when the expected param
  // is an arrow function. (See Bug 1237961 for rationale)
  let expectInvalidContextException = function(e) {
    return e.result === Cr.NS_ERROR_INVALID_ARG && /Invalid context/.test(e);
  };

  Assert.throws(() => {
    convService.convert(stream, FROM_TYPE, TO_TYPE, uri);
  }, expectInvalidContextException);

  Assert.throws(() => {
    let listener = {QueryInterface: ChromeUtils.generateQI([Ci.nsIStreamListener])};

    convService.asyncConvertData(FROM_TYPE, TO_TYPE, listener, uri);
  }, expectInvalidContextException);
});


// Test that an empty stream does not throw an NS_ERROR_ILLEGAL_VALUE.
add_task(async function testEmptyStream() {
  let stream = StringStream("");
  let resultStream = convService.convert(stream, FROM_TYPE, TO_TYPE, URI);
  equal(resultStream.available(), 0, "Size of output stream should match size of input stream");
});
