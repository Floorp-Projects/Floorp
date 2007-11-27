var dbservice = Cc["@mozilla.org/url-classifier/dbservice;1"].getService(Ci.nsIUrlClassifierDBService);

var checkUrls = [];
var checkExpect;

var chunk1Urls = [
  "test.com/aba",
  "test.com/foo/bar",
  "foo.bar.com/a/b/c"
];
var chunk1 = chunk1Urls.join("\n");

var chunk2Urls = [
  "blah.com/a",
  "baz.com/",
  "255.255.0.1/",
  "www.foo.com/test2?param=1"
];
var chunk2 = chunk2Urls.join("\n");

var chunk3Urls = [
  "test.com/a",
  "foo.bar.com/a",
  "blah.com/a",
  ];
var chunk3 = chunk3Urls.join("\n");

// we are going to add chunks 1 and 2 to phish-simple, and chunk 2 to
// malware-simple.  Then we'll remove the urls in chunk3 from phish-simple,
// then expire chunk 1 from phish-simple.
var phishExpected = {};
var phishUnexpected = {};
var malwareExpected = {};
for (var i = 0; i < chunk2Urls.length; i++) {
  phishExpected[chunk2Urls[i]] = true;
  malwareExpected[chunk2Urls[i]] = true;
}
for (var i = 0; i < chunk3Urls.length; i++) {
  delete phishExpected[chunk3Urls[i]];
  phishUnexpected[chunk3Urls[i]] = true;
}
for (var i = 0; i < chunk1Urls.length; i++) {
  // chunk1 urls are expired
  phishUnexpected[chunk1Urls[i]] = true;
}

// Check that the entries hit based on sub-parts
phishExpected["baz.com/foo/bar"] = true;
phishExpected["foo.bar.baz.com/foo"] = true;
phishExpected["bar.baz.com/"] = true;

var numExpecting;

function testFailure(arg) {
  do_throw(arg);
}

function checkNoHost()
{
  // Looking up a no-host uri such as a data: uri should throw an exception.
  var exception;
  try {
    dbservice.lookup("data:text/html,<b>test</b>");

    exception = false;
  } catch(e) {
    exception = true;
  }
  do_check_true(exception);

  do_test_finished();
}

function tablesCallback(tables)
{
  var parts = tables.split("\n");
  parts.sort();
  // there's a leading \n here because splitting left an empty string
  // after the trailing newline, which will sort first
  do_check_eq(parts.join("\n"),
              "\ntesting-malware-simple;a:1\ntesting-phish-simple;a:2:s:3");

  checkNoHost();
}

function checkChunks()
{
  dbservice.getTables(tablesCallback);
}

function checkDone() {
  if (--numExpecting == 0)
    checkChunks();
}

function phishExists(result) {
  dumpn("phishExists: " + result);
  try {
    do_check_true(result.indexOf("testing-phish-simple") != -1);
  } finally {
    checkDone();
  }
}

function phishDoesntExist(result) {
  dumpn("phishDoesntExist: " + result);
  try {
    do_check_true(result.indexOf("testing-phish-simple") == -1);
  } finally {
    checkDone();
  }
}

function malwareExists(result) {
  dumpn("malwareExists: " + result);

  try {
    do_check_true(result.indexOf("testing-malware-simple") != -1);
  } finally {
    checkDone();
  }
}

function checkState()
{
  numExpecting = 0;
  for (var key in phishExpected) {
    dbservice.lookup("http://" + key, phishExists, true);
    numExpecting++;
  }

  for (var key in phishUnexpected) {
    dbservice.lookup("http://" + key, phishDoesntExist, true);
    numExpecting++;
  }

  for (var key in malwareExpected) {
    dbservice.lookup("http://" + key, malwareExists, true);
    numExpecting++;
  }
}

function testSubSuccess(result)
{
  do_check_eq(result, "1000");
  checkState();
}

function do_subs() {
  var data =
    "n:1000\n" +
    "i:testing-phish-simple\n" +
    "s:3:" + chunk3.length + "\n" +
    chunk3 + "\n" +
    "ad:1\n";

  dbservice.update(data);
  dbservice.finish(testSubSuccess, testFailure);
}

function testAddSuccess(arg) {
  do_check_eq(arg, "1000");

  do_subs();
}

function do_adds() {
  // This test relies on the fact that only -regexp tables are ungzipped,
  // and only -hash tables are assumed to be pre-md5'd.  So we use
  // a 'simple' table type to get simple hostname-per-line semantics.

  var data =
    "n:1000\n" +
    "i:testing-phish-simple\n" +
    "a:1:" + chunk1.length + "\n" +
    chunk1 + "\n" +
    "a:2:" + chunk2.length + "\n" +
    chunk2 + "\n" +
    "i:testing-malware-simple\n" +
    "a:1:" + chunk2.length + "\n" +
    chunk2 + "\n";

  dbservice.update(data);
  dbservice.finish(testAddSuccess, testFailure);
}

function run_test() {
  do_adds();
  do_test_pending();
}
