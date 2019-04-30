/*
 * Tests bugs 597706, 655389: prevent duplicate headers with differing values
 * for some headers like Content-Length, Location, etc.
 */

////////////////////////////////////////////////////////////////////////////////
// Test infrastructure

const {HttpServer} = ChromeUtils.import("resource://testing-common/httpd.js");

XPCOMUtils.defineLazyGetter(this, "URL", function() {
  return "http://localhost:" + httpserver.identity.primaryPort;
});

var httpserver = new HttpServer();
var index = 0;
var test_flags = new Array();
var testPathBase = "/dupe_hdrs";

function run_test()
{
  httpserver.start(-1);

  do_test_pending();
  run_test_number(1);
}

function run_test_number(num)
{
  testPath = testPathBase + num;
  httpserver.registerPathHandler(testPath, this["handler" + num]);

  var channel = setupChannel(testPath);
  flags = test_flags[num];   // OK if flags undefined for test
  channel.asyncOpen(new ChannelListener(this["completeTest" + num],
                                         channel, flags));
}

function setupChannel(url)
{
  var chan = NetUtil.newChannel({
    uri: URL + url,
    loadUsingSystemPrincipal: true
  });
  var httpChan = chan.QueryInterface(Ci.nsIHttpChannel);
  return httpChan;
}

function endTests()
{
  httpserver.stop(do_test_finished);
}

////////////////////////////////////////////////////////////////////////////////
// Test 1: FAIL because of conflicting Content-Length headers
test_flags[1] = CL_EXPECT_FAILURE;

function handler1(metadata, response)
{
  var body = "012345678901234567890123456789";
  // Comrades!  We must seize power from the petty-bourgeois running dogs of
  // httpd.js in order to reply with multiple instances of the same header!
  response.seizePower();
  response.write("HTTP/1.0 200 OK\r\n");
  response.write("Content-Type: text/plain\r\n");
  response.write("Content-Length: 30\r\n");
  response.write("Content-Length: 20\r\n");
  response.write("\r\n");
  response.write(body);
  response.finish();
}


function completeTest1(request, data, ctx)
{
  Assert.equal(request.status, Cr.NS_ERROR_CORRUPTED_CONTENT);

  run_test_number(2);
}

////////////////////////////////////////////////////////////////////////////////
// Test 2: OK to have duplicate same Content-Length headers

function handler2(metadata, response)
{
  var body = "012345678901234567890123456789";
  response.seizePower();
  response.write("HTTP/1.0 200 OK\r\n");
  response.write("Content-Type: text/plain\r\n");
  response.write("Content-Length: 30\r\n");
  response.write("Content-Length: 30\r\n");
  response.write("\r\n");
  response.write(body);
  response.finish();
}

function completeTest2(request, data, ctx)
{
  Assert.equal(request.status, 0);
  run_test_number(3);
}

////////////////////////////////////////////////////////////////////////////////
// Test 3: FAIL: 2nd Content-length is blank
test_flags[3] = CL_EXPECT_FAILURE;

function handler3(metadata, response)
{
  var body = "012345678901234567890123456789";
  response.seizePower();
  response.write("HTTP/1.0 200 OK\r\n");
  response.write("Content-Type: text/plain\r\n");
  response.write("Content-Length: 30\r\n");
  response.write("Content-Length:\r\n");
  response.write("\r\n");
  response.write(body);
  response.finish();
}

function completeTest3(request, data, ctx)
{
  Assert.equal(request.status, Cr.NS_ERROR_CORRUPTED_CONTENT);

  run_test_number(4);
}

////////////////////////////////////////////////////////////////////////////////
// Test 4: ensure that blank C-len header doesn't allow attacker to reset Clen, 
// then insert CRLF attack
test_flags[4] = CL_EXPECT_FAILURE;

function handler4(metadata, response)
{
  var body = "012345678901234567890123456789";

  response.seizePower();
  response.write("HTTP/1.0 200 OK\r\n");
  response.write("Content-Type: text/plain\r\n");
  response.write("Content-Length: 30\r\n");

  // Bad Mr Hacker!  Bad!
  var evilBody = "We are the Evil bytes, Evil bytes, Evil bytes!";
  response.write("Content-Length:\r\n");
  response.write("Content-Length: %s\r\n\r\n%s" % (evilBody.length, evilBody));
  response.write("\r\n");
  response.write(body);
  response.finish();
}

function completeTest4(request, data, ctx)
{
  Assert.equal(request.status, Cr.NS_ERROR_CORRUPTED_CONTENT);

  run_test_number(5);
}


////////////////////////////////////////////////////////////////////////////////
// Test 5: ensure that we take 1st instance of duplicate, nonmerged headers that
// are permitted : (ex: Referrer)

function handler5(metadata, response)
{
  var body = "012345678901234567890123456789";
  response.seizePower();
  response.write("HTTP/1.0 200 OK\r\n");
  response.write("Content-Type: text/plain\r\n");
  response.write("Content-Length: 30\r\n");
  response.write("Referer: naive.org\r\n");
  response.write("Referer: evil.net\r\n");
  response.write("\r\n");
  response.write(body);
  response.finish();
}

function completeTest5(request, data, ctx)
{
  try {
    referer = request.getResponseHeader("Referer");
    Assert.equal(referer, "naive.org");
  } catch (ex) {
    do_throw("Referer header should be present");
  }

  run_test_number(6);
}

////////////////////////////////////////////////////////////////////////////////
// Test 5: FAIL if multiple, different Location: headers present
// - needed to prevent CRLF injection attacks
test_flags[6] = CL_EXPECT_FAILURE;

function handler6(metadata, response)
{
  var body = "012345678901234567890123456789";
  response.seizePower();
  response.write("HTTP/1.0 301 Moved\r\n");
  response.write("Content-Type: text/plain\r\n");
  response.write("Content-Length: 30\r\n");
  response.write("Location: " + URL + "/content\r\n");
  response.write("Location: http://www.microsoft.com/\r\n");
  response.write("Connection: close\r\n");
  response.write("\r\n");
  response.write(body);
  response.finish();
}

function completeTest6(request, data, ctx)
{
  Assert.equal(request.status, Cr.NS_ERROR_CORRUPTED_CONTENT);

//  run_test_number(7);   // Test 7 leaking under e10s: unrelated bug?
  run_test_number(8);
}

////////////////////////////////////////////////////////////////////////////////
// Test 7: OK to have multiple Location: headers with same value

function handler7(metadata, response)
{
  var body = "012345678901234567890123456789";
  response.seizePower();
  response.write("HTTP/1.0 301 Moved\r\n");
  response.write("Content-Type: text/plain\r\n");
  response.write("Content-Length: 30\r\n");
  // redirect to previous test handler that completes OK: test 5
  response.write("Location: " + URL + testPathBase + "5\r\n");
  response.write("Location: " + URL + testPathBase + "5\r\n");
  response.write("Connection: close\r\n");
  response.write("\r\n");
  response.write(body);
  response.finish();
}

function completeTest7(request, data, ctx)
{
  // for some reason need this here
  request.QueryInterface(Ci.nsIHttpChannel);

  try {
    referer = request.getResponseHeader("Referer");
    Assert.equal(referer, "naive.org");
  } catch (ex) {
    do_throw("Referer header should be present");
  }

  run_test_number(8);
}

////////////////////////////////////////////////////////////////////////////////
// FAIL if 2nd Location: headers blank
test_flags[8] = CL_EXPECT_FAILURE;

function handler8(metadata, response)
{
  var body = "012345678901234567890123456789";
  response.seizePower();
  response.write("HTTP/1.0 301 Moved\r\n");
  response.write("Content-Type: text/plain\r\n");
  response.write("Content-Length: 30\r\n");
  // redirect to previous test handler that completes OK: test 4
  response.write("Location: " + URL + testPathBase + "4\r\n");
  response.write("Location:\r\n");
  response.write("Connection: close\r\n");
  response.write("\r\n");
  response.write(body);
  response.finish();
}

function completeTest8(request, data, ctx)
{
  Assert.equal(request.status, Cr.NS_ERROR_CORRUPTED_CONTENT);

  run_test_number(9);
}

////////////////////////////////////////////////////////////////////////////////
// Test 9: ensure that blank Location header doesn't allow attacker to reset, 
// then insert an evil one
test_flags[9] = CL_EXPECT_FAILURE;

function handler9(metadata, response)
{
  var body = "012345678901234567890123456789";
  response.seizePower();
  response.write("HTTP/1.0 301 Moved\r\n");
  response.write("Content-Type: text/plain\r\n");
  response.write("Content-Length: 30\r\n");
  // redirect to previous test handler that completes OK: test 2 
  response.write("Location: " + URL + testPathBase + "2\r\n");
  response.write("Location:\r\n");
  // redirect to previous test handler that completes OK: test 4
  response.write("Location: " + URL + testPathBase + "4\r\n");
  response.write("Connection: close\r\n");
  response.write("\r\n");
  response.write(body);
  response.finish();
}

function completeTest9(request, data, ctx)
{
  // All redirection should fail:
  Assert.equal(request.status, Cr.NS_ERROR_CORRUPTED_CONTENT);

  run_test_number(10);
}

////////////////////////////////////////////////////////////////////////////////
// Test 10: FAIL:  if conflicting values for Content-Dispo 
test_flags[10] = CL_EXPECT_FAILURE;

function handler10(metadata, response)
{
  var body = "012345678901234567890123456789";
  response.seizePower();
  response.write("HTTP/1.0 200 OK\r\n");
  response.write("Content-Type: text/plain\r\n");
  response.write("Content-Length: 30\r\n");
  response.write("Content-Disposition: attachment; filename=foo\r\n");
  response.write("Content-Disposition: attachment; filename=bar\r\n");
  response.write("Content-Disposition: attachment; filename=baz\r\n");
  response.write("\r\n");
  response.write(body);
  response.finish();
}


function completeTest10(request, data, ctx)
{
  Assert.equal(request.status, Cr.NS_ERROR_CORRUPTED_CONTENT);

  run_test_number(11);
}

////////////////////////////////////////////////////////////////////////////////
// Test 11: OK to have duplicate same Content-Disposition headers

function handler11(metadata, response)
{
  var body = "012345678901234567890123456789";
  response.seizePower();
  response.write("HTTP/1.0 200 OK\r\n");
  response.write("Content-Type: text/plain\r\n");
  response.write("Content-Length: 30\r\n");
  response.write("Content-Disposition: attachment; filename=foo\r\n");
  response.write("Content-Disposition: attachment; filename=foo\r\n");
  response.write("\r\n");
  response.write(body);
  response.finish();
}

function completeTest11(request, data, ctx)
{
  Assert.equal(request.status, 0);

  try {
    var chan = request.QueryInterface(Ci.nsIChannel);
    Assert.equal(chan.contentDisposition, chan.DISPOSITION_ATTACHMENT);
    Assert.equal(chan.contentDispositionFilename, "foo");
    Assert.equal(chan.contentDispositionHeader, "attachment; filename=foo");
  } catch (ex) {
    do_throw("error parsing Content-Disposition: " + ex);
  }

  run_test_number(12);
}

////////////////////////////////////////////////////////////////////////////////
// Bug 716801 OK for Location: header to be blank

function handler12(metadata, response)
{
  var body = "012345678901234567890123456789";
  response.seizePower();
  response.write("HTTP/1.0 200 OK\r\n");
  response.write("Content-Type: text/plain\r\n");
  response.write("Content-Length: 30\r\n");
  response.write("Location:\r\n");
  response.write("Connection: close\r\n");
  response.write("\r\n");
  response.write(body);
  response.finish();
}

function completeTest12(request, data, ctx)
{
  Assert.equal(request.status, Cr.NS_OK);
  Assert.equal(30, data.length);

  run_test_number(13);
}

////////////////////////////////////////////////////////////////////////////////
// Negative content length is ok
test_flags[13] = CL_ALLOW_UNKNOWN_CL;

function handler13(metadata, response)
{
  var body = "012345678901234567890123456789";
  response.seizePower();
  response.write("HTTP/1.0 200 OK\r\n");
  response.write("Content-Type: text/plain\r\n");
  response.write("Content-Length: -1\r\n");
  response.write("Connection: close\r\n");
  response.write("\r\n");
  response.write(body);
  response.finish();
}

function completeTest13(request, data, ctx)
{
  Assert.equal(request.status, Cr.NS_OK);
  Assert.equal(30, data.length);

  run_test_number(14);
}

////////////////////////////////////////////////////////////////////////////////
// leading negative content length is not ok if paired with positive one

test_flags[14] = CL_EXPECT_FAILURE | CL_ALLOW_UNKNOWN_CL;

function handler14(metadata, response)
{
  var body = "012345678901234567890123456789";
  response.seizePower();
  response.write("HTTP/1.0 200 OK\r\n");
  response.write("Content-Type: text/plain\r\n");
  response.write("Content-Length: -1\r\n");
  response.write("Content-Length: 30\r\n");
  response.write("Connection: close\r\n");
  response.write("\r\n");
  response.write(body);
  response.finish();
}

function completeTest14(request, data, ctx)
{
  Assert.equal(request.status, Cr.NS_ERROR_CORRUPTED_CONTENT);

  run_test_number(15);
}

////////////////////////////////////////////////////////////////////////////////
// trailing negative content length is not ok if paired with positive one

test_flags[15] = CL_EXPECT_FAILURE | CL_ALLOW_UNKNOWN_CL;

function handler15(metadata, response)
{
  var body = "012345678901234567890123456789";
  response.seizePower();
  response.write("HTTP/1.0 200 OK\r\n");
  response.write("Content-Type: text/plain\r\n");
  response.write("Content-Length: 30\r\n");
  response.write("Content-Length: -1\r\n");
  response.write("Connection: close\r\n");
  response.write("\r\n");
  response.write(body);
  response.finish();
}

function completeTest15(request, data, ctx)
{
  Assert.equal(request.status, Cr.NS_ERROR_CORRUPTED_CONTENT);

  run_test_number(16);
}

////////////////////////////////////////////////////////////////////////////////
// empty content length is ok
test_flags[16] = CL_ALLOW_UNKNOWN_CL;
reran16 = false;

function handler16(metadata, response)
{
  var body = "012345678901234567890123456789";
  response.seizePower();
  response.write("HTTP/1.0 200 OK\r\n");
  response.write("Content-Type: text/plain\r\n");
  response.write("Content-Length: \r\n");
  response.write("Cache-Control: max-age=600\r\n");
  response.write("Connection: close\r\n");
  response.write("\r\n");
  response.write(body);
  response.finish();
}

function completeTest16(request, data, ctx)
{
  Assert.equal(request.status, Cr.NS_OK);
  Assert.equal(30, data.length);

  if (!reran16) {
    reran16 = true;
    run_test_number(16);
  }
 else {
   run_test_number(17);
 }
}

////////////////////////////////////////////////////////////////////////////////
// empty content length paired with non empty is not ok
test_flags[17] = CL_EXPECT_FAILURE | CL_ALLOW_UNKNOWN_CL;

function handler17(metadata, response)
{
  var body = "012345678901234567890123456789";
  response.seizePower();
  response.write("HTTP/1.0 200 OK\r\n");
  response.write("Content-Type: text/plain\r\n");
  response.write("Content-Length: \r\n");
  response.write("Content-Length: 30\r\n");
  response.write("Connection: close\r\n");
  response.write("\r\n");
  response.write(body);
  response.finish();
}

function completeTest17(request, data, ctx)
{
  Assert.equal(request.status, Cr.NS_ERROR_CORRUPTED_CONTENT);

  run_test_number(18);
}

////////////////////////////////////////////////////////////////////////////////
// alpha content-length is just like -1
test_flags[18] = CL_ALLOW_UNKNOWN_CL;

function handler18(metadata, response)
{
  var body = "012345678901234567890123456789";
  response.seizePower();
  response.write("HTTP/1.0 200 OK\r\n");
  response.write("Content-Type: text/plain\r\n");
  response.write("Content-Length: seventeen\r\n");
  response.write("Connection: close\r\n");
  response.write("\r\n");
  response.write(body);
  response.finish();
}

function completeTest18(request, data, ctx)
{
  Assert.equal(request.status, Cr.NS_OK);
  Assert.equal(30, data.length);

  run_test_number(19);
}

////////////////////////////////////////////////////////////////////////////////
// semi-colons are ok too in the content-length
test_flags[19] = CL_ALLOW_UNKNOWN_CL;

function handler19(metadata, response)
{
  var body = "012345678901234567890123456789";
  response.seizePower();
  response.write("HTTP/1.0 200 OK\r\n");
  response.write("Content-Type: text/plain\r\n");
  response.write("Content-Length: 30;\r\n");
  response.write("Connection: close\r\n");
  response.write("\r\n");
  response.write(body);
  response.finish();
}

function completeTest19(request, data, ctx)
{
  Assert.equal(request.status, Cr.NS_OK);
  Assert.equal(30, data.length);

  run_test_number(20);
}

////////////////////////////////////////////////////////////////////////////////
// FAIL if 1st Location: header is blank, followed by non-blank
test_flags[20] = CL_EXPECT_FAILURE;

function handler20(metadata, response)
{
  var body = "012345678901234567890123456789";
  response.seizePower();
  response.write("HTTP/1.0 301 Moved\r\n");
  response.write("Content-Type: text/plain\r\n");
  response.write("Content-Length: 30\r\n");
  // redirect to previous test handler that completes OK: test 4
  response.write("Location:\r\n");
  response.write("Location: " + URL + testPathBase + "4\r\n");
  response.write("Connection: close\r\n");
  response.write("\r\n");
  response.write(body);
  response.finish();
}

function completeTest20(request, data, ctx)
{
  Assert.equal(request.status, Cr.NS_ERROR_CORRUPTED_CONTENT);

  endTests();
}

