var testStr = "This is a test. ";
for (var i = 0; i < 10; ++i) {
  testStr += testStr;
}

function run_test() {
  // Set up our stream to copy
  var inStr = Cc["@mozilla.org/io/string-input-stream;1"]
                .createInstance(Ci.nsIStringInputStream);
  inStr.setData(testStr, testStr.length);

  // Set up our destination stream.  Make sure to use segments a good
  // bit smaller than our data length.
  Assert.ok(testStr.length > 1024*10);
  var pipe = Cc["@mozilla.org/pipe;1"].createInstance(Ci.nsIPipe);
  pipe.init(true, true, 1024, 0xffffffff, null);

  var streamCopier = Cc["@mozilla.org/network/async-stream-copier;1"]
                       .createInstance(Ci.nsIAsyncStreamCopier);
  streamCopier.init(inStr, pipe.outputStream, null, true, true, 1024, true, true);

  var ctx = {
  };
  ctx.wrappedJSObject = ctx;
  
  var observer = {
    onStartRequest: function(aRequest, aContext) {
      Assert.equal(aContext.wrappedJSObject, ctx);
    },
    onStopRequest: function(aRequest, aContext, aStatusCode) {
      Assert.equal(aStatusCode, 0);
      Assert.equal(aContext.wrappedJSObject, ctx);
      var sis =
        Cc["@mozilla.org/scriptableinputstream;1"]
          .createInstance(Ci.nsIScriptableInputStream);
      sis.init(pipe.inputStream);
      var result = "";
      var temp;
      try { // Need this because read() can throw at EOF
        while ((temp = sis.read(1024))) {
          result += temp;
        }
      } catch(e) {
	Assert.equal(e.result, Components.results.NS_BASE_STREAM_CLOSED);
      }
      Assert.equal(result, testStr);
      do_test_finished();
    }
  };

  streamCopier.asyncCopy(observer, ctx);
  do_test_pending();  
}
