//
//  cleaner HTTP header test infrastructure
//
//  tests bugs:  589292, [add more here: see hg log for definitive list]
//
//  TO ADD NEW TESTS:
//  1) Increment up 'lastTest' to new number (say, "99")
//  2) Add new test 'handler99' and 'completeTest99' functions.
//  3) If your test should fail the necko channel, set 
//     test_flags[99] = CL_EXPECT_FAILURE.   
//
// TO DEBUG JUST ONE TEST: temporarily change firstTest and lastTest to equal
//                         the test # you're interested in.
//
//  For tests that need duplicate copies of headers to be sent, see
//  test_duplicate_headers.js

var firstTest = 1;   // set to test of interest when debugging
var lastTest = 4;    // set to test of interest when debugging
////////////////////////////////////////////////////////////////////////////////

// Note: sets Cc and Ci variables
do_load_httpd_js();

var httpserver = new nsHttpServer();
var index = 0;
var nextTest = firstTest; 
var test_flags = new Array();
var testPathBase = "/test_headers";

function run_test()
{
  httpserver.start(4444);

  do_test_pending();
  run_test_number(nextTest);
}

function runNextTest()
{
  if (nextTest == lastTest) {
    endTests();
    return;
  }
  nextTest++;
  // Make sure test functions exist
  if (eval("handler" + nextTest) == undefined)
    do_throw("handler" + nextTest + " undefined!");
  if (eval("completeTest" + nextTest) == undefined)
    do_throw("completeTest" + nextTest + " undefined!");
  
  run_test_number(nextTest);
}

function run_test_number(num)
{
  testPath = testPathBase + num;
  httpserver.registerPathHandler(testPath, eval("handler" + num));

  var channel = setupChannel(testPath);
  flags = test_flags[num];   // OK if flags undefined for test
  channel.asyncOpen(new ChannelListener(eval("completeTest" + num),
                                        channel, flags), null);
}

function setupChannel(url)
{
  var ios = Components.classes["@mozilla.org/network/io-service;1"].
                       getService(Ci.nsIIOService);
  var chan = ios.newChannel("http://localhost:4444" + url, "", null);
  var httpChan = chan.QueryInterface(Components.interfaces.nsIHttpChannel);
  return httpChan;
}

function endTests()
{
  httpserver.stop(do_test_finished);
}

////////////////////////////////////////////////////////////////////////////////
// Test 1: test Content-Disposition channel attributes
function handler1(metadata, response)
{
  response.setStatusLine(metadata.httpVersion, 200, "OK");
  response.setHeader("Content-Disposition", "attachment; filename=foo");
  response.setHeader("Content-Type", "text/plain", false);
  var body = "foo";
}

function completeTest1(request, data, ctx)
{
  try {
    var chan = request.QueryInterface(Ci.nsIChannel);
    do_check_eq(chan.contentDisposition, chan.DISPOSITION_ATTACHMENT);
    do_check_eq(chan.contentDispositionFilename, "foo");
    do_check_eq(chan.contentDispositionHeader, "attachment; filename=foo");
  } catch (ex) {
    do_throw("error parsing Content-Disposition: " + ex);
  }
  runNextTest();  
}

////////////////////////////////////////////////////////////////////////////////
// Test 2: no filename 
function handler2(metadata, response)
{
  response.setStatusLine(metadata.httpVersion, 200, "OK");
  response.setHeader("Content-Type", "text/plain", false);
  response.setHeader("Content-Disposition", "attachment");
  var body = "foo";
  response.bodyOutputStream.write(body, body.length);
}

function completeTest2(request, data, ctx)
{
  try {
    var chan = request.QueryInterface(Ci.nsIChannel);
    do_check_eq(chan.contentDisposition, chan.DISPOSITION_ATTACHMENT);
    do_check_eq(chan.contentDispositionHeader, "attachment");

    filename = chan.contentDispositionFilename;  // should barf
    do_throw("Should have failed getting Content-Disposition filename");
  } catch (ex) {
    do_print("correctly ate exception");    
  }
  runNextTest();  
}

////////////////////////////////////////////////////////////////////////////////
// Test 3: filename missing
function handler3(metadata, response)
{
  response.setStatusLine(metadata.httpVersion, 200, "OK");
  response.setHeader("Content-Type", "text/plain", false);
  response.setHeader("Content-Disposition", "attachment; filename=");
  var body = "foo";
  response.bodyOutputStream.write(body, body.length);
}

function completeTest3(request, data, ctx)
{
  try {
    var chan = request.QueryInterface(Ci.nsIChannel);
    do_check_eq(chan.contentDisposition, chan.DISPOSITION_ATTACHMENT);
    do_check_eq(chan.contentDispositionHeader, "attachment; filename=");

    filename = chan.contentDispositionFilename;  // should barf
    do_throw("Should have failed getting Content-Disposition filename");
  } catch (ex) {
    do_print("correctly ate exception");    
  }
  runNextTest();  
}

////////////////////////////////////////////////////////////////////////////////
// Test 4: inline
function handler4(metadata, response)
{
  response.setStatusLine(metadata.httpVersion, 200, "OK");
  response.setHeader("Content-Type", "text/plain", false);
  response.setHeader("Content-Disposition", "inline");
  var body = "foo";
  response.bodyOutputStream.write(body, body.length);
}

function completeTest4(request, data, ctx)
{
  try {
    var chan = request.QueryInterface(Ci.nsIChannel);
    do_check_eq(chan.contentDisposition, chan.DISPOSITION_INLINE);
    do_check_eq(chan.contentDispositionHeader, "inline");

    filename = chan.contentDispositionFilename;  // should barf
    do_throw("Should have failed getting Content-Disposition filename");
  } catch (ex) {
    do_print("correctly ate exception");    
  }
  runNextTest();
}
