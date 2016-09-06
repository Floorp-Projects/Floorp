function doTest(updates, assertions, expectError)
{
  if (expectError) {
    doUpdateTest(updates, assertions, updateError, runNextTest);
  } else {
    doUpdateTest(updates, assertions, runNextTest, updateError);
  }
}

// Never use the same URLs for multiple tests, because we aren't guaranteed
// to reset the database between tests.
function testFillDb() {
  var add1Urls = [ "zaz.com/a", "yxz.com/c" ];

  var update = "n:1000\n";
  update += "i:test-phish-simple\n";

  var update1 = buildBareUpdate(
    [{ "chunkNum" : 1,
       "urls" : add1Urls }]);
  update += "u:data:," + encodeURIComponent(update1) + "\n";

  var assertions = {
    "tableData" : "test-phish-simple;a:1",
    "urlsExist" : add1Urls
  };

  doTest([update], assertions, false);
}

function testSimpleForward() {
  var add1Urls = [ "foo-simple.com/a", "bar-simple.com/c" ];
  var add2Urls = [ "foo-simple.com/b" ];
  var add3Urls = [ "bar-simple.com/d" ];

  var update = "n:1000\n";
  update += "i:test-phish-simple\n";

  var update1 = buildBareUpdate(
    [{ "chunkNum" : 1,
       "urls" : add1Urls }]);
  update += "u:data:," + encodeURIComponent(update1) + "\n";

  var update2 = buildBareUpdate(
    [{ "chunkNum" : 2,
       "urls" : add2Urls }]);
  update += "u:data:," + encodeURIComponent(update2) + "\n";

  var update3 = buildBareUpdate(
    [{ "chunkNum" : 3,
       "urls" : add3Urls }]);
  update += "u:data:," + encodeURIComponent(update3) + "\n";

  var assertions = {
    "tableData" : "test-phish-simple;a:1-3",
    "urlsExist" : add1Urls.concat(add2Urls).concat(add3Urls)
  };

  doTest([update], assertions, false);
}

// Make sure that a nested forward (a forward within a forward) causes
// the update to fail.
function testNestedForward() {
  var add1Urls = [ "foo-nested.com/a", "bar-nested.com/c" ];
  var add2Urls = [ "foo-nested.com/b" ];

  var update = "n:1000\n";
  update += "i:test-phish-simple\n";

  var update1 = buildBareUpdate(
    [{ "chunkNum" : 1,
       "urls" : add1Urls }]);
  update += "u:data:," + encodeURIComponent(update1) + "\n";

  var update2 = buildBareUpdate(
    [{ "chunkNum" : 2 }]);
  var update3 = buildBareUpdate(
    [{ "chunkNum" : 3,
       "urls" : add1Urls }]);

  update2 += "u:data:," + encodeURIComponent(update3) + "\n";

  update += "u:data:," + encodeURIComponent(update2) + "\n";

  var assertions = {
    "tableData" : "",
    "urlsDontExist" : add1Urls.concat(add2Urls)
  };

  doTest([update], assertions, true);
}

// An invalid URL forward causes the update to fail.
function testInvalidUrlForward() {
  var add1Urls = [ "foo-invalid.com/a", "bar-invalid.com/c" ];

  var update = buildPhishingUpdate(
    [{ "chunkNum" : 1,
       "urls" : add1Urls }]);
  update += "u:asdf://blah/blah\n";  // invalid URL scheme

  // add1Urls is present, but that is an artifact of the way we do the test.
  var assertions = {
    "tableData" : "",
    "urlsExist" : add1Urls
  };

  doTest([update], assertions, true);
}

// A failed network request causes the update to fail.
function testErrorUrlForward() {
  var add1Urls = [ "foo-forward.com/a", "bar-forward.com/c" ];

  var update = buildPhishingUpdate(
    [{ "chunkNum" : 1,
       "urls" : add1Urls }]);
  update += "u:http://test.invalid/asdf/asdf\n";  // invalid URL scheme

  // add1Urls is present, but that is an artifact of the way we do the test.
  var assertions = {
    "tableData" : "",
    "urlsExist" : add1Urls
  };

  doTest([update], assertions, true);
}

function testMultipleTables() {
  var add1Urls = [ "foo-multiple.com/a", "bar-multiple.com/c" ];
  var add2Urls = [ "foo-multiple.com/b" ];
  var add3Urls = [ "bar-multiple.com/d" ];
  var add4Urls = [ "bar-multiple.com/e" ];
  var add6Urls = [ "bar-multiple.com/g" ];

  var update = "n:1000\n";
  update += "i:test-phish-simple\n";

  var update1 = buildBareUpdate(
    [{ "chunkNum" : 1,
       "urls" : add1Urls }]);
  update += "u:data:," + encodeURIComponent(update1) + "\n";

  var update2 = buildBareUpdate(
    [{ "chunkNum" : 2,
       "urls" : add2Urls }]);
  update += "u:data:," + encodeURIComponent(update2) + "\n";

  update += "i:test-malware-simple\n";

  var update3 = buildBareUpdate(
    [{ "chunkNum" : 3,
       "urls" : add3Urls }]);
  update += "u:data:," + encodeURIComponent(update3) + "\n";

  update += "i:test-unwanted-simple\n";
  var update4 = buildBareUpdate(
    [{ "chunkNum" : 4,
       "urls" : add4Urls }]);
  update += "u:data:," + encodeURIComponent(update4) + "\n";

  update += "i:test-block-simple\n";
  var update6 = buildBareUpdate(
    [{ "chunkNum" : 6,
       "urls" : add6Urls }]);
  update += "u:data:," + encodeURIComponent(update6) + "\n";

  var assertions = {
    "tableData" : "test-block-simple;a:6\ntest-malware-simple;a:3\ntest-phish-simple;a:1-2\ntest-unwanted-simple;a:4",
    "urlsExist" : add1Urls.concat(add2Urls),
    "malwareUrlsExist" : add3Urls,
    "unwantedUrlsExist" : add4Urls,
    "blockedUrlsExist" : add6Urls
  };

  doTest([update], assertions, false);
}

function testUrlInMultipleTables() {
  var add1Urls = [ "foo-forward.com/a" ];

  var update = "n:1000\n";
  update += "i:test-phish-simple\n";

  var update1 = buildBareUpdate(
    [{ "chunkNum" : 1,
       "urls" : add1Urls }]);
  update += "u:data:," + encodeURIComponent(update1) + "\n";

  update += "i:test-malware-simple\n";
  var update2 = buildBareUpdate(
    [{ "chunkNum" : 2,
       "urls" : add1Urls }]);
  update += "u:data:," + encodeURIComponent(update2) + "\n";

  update += "i:test-unwanted-simple\n";
  var update3 = buildBareUpdate(
    [{ "chunkNum" : 3,
       "urls" : add1Urls }]);
  update += "u:data:," + encodeURIComponent(update3) + "\n";

  var assertions = {
    "tableData" : "test-malware-simple;a:2\ntest-phish-simple;a:1\ntest-unwanted-simple;a:3",
    "urlExistInMultipleTables" : { url: add1Urls,
                                   tables: "test-malware-simple,test-phish-simple,test-unwanted-simple" }
  };

  doTest([update], assertions, false);
}

function Observer(callback) {
  this.observe = callback;
}

Observer.prototype =
{
QueryInterface: function(iid)
{
  if (!iid.equals(Ci.nsISupports) &&
      !iid.equals(Ci.nsIObserver)) {
    throw Cr.NS_ERROR_NO_INTERFACE;
  }
  return this;
}
};

// Tests a database reset request.
function testReset() {
  // The moz-phish-simple table is populated separately from the other update in
  // a separate update request. Therefore it should not be reset when we run the
  // updates later in this function.
  var mozAddUrls = [ "moz-reset.com/a" ];
  var mozUpdate = buildMozPhishingUpdate(
    [
      { "chunkNum" : 1,
        "urls" : mozAddUrls
      }]);

  var dataUpdate = "data:," + encodeURIComponent(mozUpdate);

  streamUpdater.downloadUpdates(mozTables, "", true,
                                dataUpdate, () => {}, updateError, updateError);

  var addUrls1 = [ "foo-reset.com/a", "foo-reset.com/b" ];
  var update1 = buildPhishingUpdate(
    [
      { "chunkNum" : 1,
        "urls" : addUrls1
      }]);

  var update2 = "n:1000\nr:pleasereset\n";

  var addUrls3 = [ "bar-reset.com/a", "bar-reset.com/b" ];
  var update3 = buildPhishingUpdate(
    [
      { "chunkNum" : 3,
        "urls" : addUrls3
      }]);

  var assertions = {
    "tableData" : "moz-phish-simple;a:1\ntest-phish-simple;a:3", // tables that should still be there.
    "mozPhishingUrlsExist" : mozAddUrls,                         // mozAddUrls added prior to the reset
                                                                 // but it should still exist after reset.
    "urlsExist" : addUrls3,                                      // addUrls3 added after the reset.
    "urlsDontExist" : addUrls1                                   // addUrls1 added prior to the reset
  };

  // Use these update responses in order. The update request only
  // contains test-*-simple tables so the reset will only apply to these.
  doTest([update1, update2, update3], assertions, false);
}


function run_test()
{
  runTests([
    testSimpleForward,
    testNestedForward,
    testInvalidUrlForward,
    testErrorUrlForward,
    testMultipleTables,
    testUrlInMultipleTables,
    testReset
  ]);
}

do_test_pending();
