const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;
const CC = Components.Constructor;

const StreamCopier = CC("@mozilla.org/network/async-stream-copier;1",
                        "nsIAsyncStreamCopier",
                        "init");

const ScriptableInputStream = CC("@mozilla.org/scriptableinputstream;1",
                                 "nsIScriptableInputStream",
                                 "init");

const Pipe = CC("@mozilla.org/pipe;1",
                "nsIPipe",
                "init");

var pipe1;
var pipe2;
var copier;
var test_result;
var test_content;
var test_source_closed;
var test_sink_closed;
var test_nr;

var copyObserver =
{
  onStartRequest: function(request, context) { },

  onStopRequest: function(request, cx, statusCode)
  {
    // check status code
    do_check_eq(statusCode, test_result);

    // check number of copied bytes
    do_check_eq(pipe2.inputStream.available(), test_content.length);

    // check content
    var scinp = new ScriptableInputStream(pipe2.inputStream);
    var content = scinp.read(scinp.available());
    do_check_eq(content, test_content);

    // check closed sink
    try {
      pipe2.outputStream.write("closedSinkTest", 14);
      do_check_false(test_sink_closed);
    }
    catch (ex) {
      do_check_true(test_sink_closed);
    }

    // check closed source
    try {
      pipe1.outputStream.write("closedSourceTest", 16);
      do_check_false(test_source_closed);
    }
    catch (ex) {
      do_check_true(test_source_closed);
    }

    do_timeout(0, do_test);
  },

  QueryInterface: function(aIID)
  {
    if (aIID.equals(Ci.nsIRequestObserver) ||
        aIID.equals(Ci.nsISupports))
      return this;

    throw Cr.NS_ERROR_NO_INTERFACE;
  }
};

function startCopier(closeSource, closeSink) {
  pipe1 = new Pipe(true       /* nonBlockingInput */,
                   true       /* nonBlockingOutput */,
                   0          /* segmentSize */,
                   0xffffffff /* segmentCount */,
                   null       /* segmentAllocator */);

  pipe2 = new Pipe(true       /* nonBlockingInput */,
                   true       /* nonBlockingOutput */,
                   0          /* segmentSize */,
                   0xffffffff /* segmentCount */,
                   null       /* segmentAllocator */);

  copier = new StreamCopier(pipe1.inputStream  /* aSource */,
                            pipe2.outputStream /* aSink */,
                            null               /* aTarget */,
                            true               /* aSourceBuffered */,
                            true               /* aSinkBuffered */,
                            8192               /* aChunkSize */,
                            closeSource        /* aCloseSource */,
                            closeSink          /* aCloseSink */);

  copier.asyncCopy(copyObserver, null);
}

function do_test() {

  test_nr++;
  test_content = "test" + test_nr;

  switch (test_nr) {
  case 1:
  case 2: // close sink
  case 3: // close source
  case 4: // close both
    // test canceling transfer
    // use some undefined error code to check if it is successfully passed
    // to the request observer
    test_result = 0x87654321;

    test_source_closed = ((test_nr-1)>>1 != 0);
    test_sink_closed = ((test_nr-1)%2 != 0);

    startCopier(test_source_closed, test_sink_closed);
    pipe1.outputStream.write(test_content, test_content.length);
    pipe1.outputStream.flush();
    do_timeout(20,
		function(){
               copier.cancel(test_result);
               pipe1.outputStream.write("a", 1);});
    break;
  case 5:
  case 6: // close sink
  case 7: // close source
  case 8: // close both
    // test copying with EOF on source
    test_result = 0;

    test_source_closed = ((test_nr-5)>>1 != 0);
    test_sink_closed = ((test_nr-5)%2 != 0);

    startCopier(test_source_closed, test_sink_closed);
    pipe1.outputStream.write(test_content, test_content.length);
    // we will close the source
    test_source_closed = true;
    pipe1.outputStream.close();
    break;
  case 9:
  case 10: // close sink
  case 11: // close source
  case 12: // close both
    // test copying with error on sink
    // use some undefined error code to check if it is successfully passed
    // to the request observer
    test_result = 0x87654321;

    test_source_closed = ((test_nr-9)>>1 != 0);
    test_sink_closed = ((test_nr-9)%2 != 0);

    startCopier(test_source_closed, test_sink_closed);
    pipe1.outputStream.write(test_content, test_content.length);
    pipe1.outputStream.flush();
    // we will close the sink
    test_sink_closed = true;
    do_timeout(20,
		function()
		{
               pipe2.outputStream
                    .QueryInterface(Ci.nsIAsyncOutputStream)
                    .closeWithStatus(test_result);
               pipe1.outputStream.write("a", 1);});
    break;
  case 13:
    do_test_finished();
    break;
  }
}

function run_test() {
  test_nr = 0;
  do_timeout(0, do_test);
  do_test_pending();
}
