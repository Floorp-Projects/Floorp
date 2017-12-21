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

var chunk3SubUrls = [
  "1:test.com/a",
  "1:foo.bar.com/a",
  "2:blah.com/a" ];
var chunk3Sub = chunk3SubUrls.join("\n");

var chunk4Urls = [
  "a.com/b",
  "b.com/c",
  ];
var chunk4 = chunk4Urls.join("\n");

var chunk5Urls = [
  "d.com/e",
  "f.com/g",
  ];
var chunk5 = chunk5Urls.join("\n");

var chunk6Urls = [
  "h.com/i",
  "j.com/k",
  ];
var chunk6 = chunk6Urls.join("\n");

var chunk7Urls = [
  "l.com/m",
  "n.com/o",
  ];
var chunk7 = chunk7Urls.join("\n");

// we are going to add chunks 1, 2, 4, 5, and 6 to phish-simple,
// chunk 2 to malware-simple, and chunk 3 to unwanted-simple,
// and chunk 7 to block-simple.
// Then we'll remove the urls in chunk3 from phish-simple, then
// expire chunk 1 and chunks 4-7 from phish-simple.
var phishExpected = {};
var phishUnexpected = {};
var malwareExpected = {};
var unwantedExpected = {};
var blockedExpected = {};
for (let i = 0; i < chunk2Urls.length; i++) {
  phishExpected[chunk2Urls[i]] = true;
  malwareExpected[chunk2Urls[i]] = true;
}
for (let i = 0; i < chunk3Urls.length; i++) {
  unwantedExpected[chunk3Urls[i]] = true;
  delete phishExpected[chunk3Urls[i]];
  phishUnexpected[chunk3Urls[i]] = true;
}
for (let i = 0; i < chunk1Urls.length; i++) {
  // chunk1 urls are expired
  phishUnexpected[chunk1Urls[i]] = true;
}
for (let i = 0; i < chunk4Urls.length; i++) {
  // chunk4 urls are expired
  phishUnexpected[chunk4Urls[i]] = true;
}
for (let i = 0; i < chunk5Urls.length; i++) {
  // chunk5 urls are expired
  phishUnexpected[chunk5Urls[i]] = true;
}
for (let i = 0; i < chunk6Urls.length; i++) {
  // chunk6 urls are expired
  phishUnexpected[chunk6Urls[i]] = true;
}
for (let i = 0; i < chunk7Urls.length; i++) {
  blockedExpected[chunk7Urls[i]] = true;
  // chunk7 urls are expired
  phishUnexpected[chunk7Urls[i]] = true;
}

// Check that the entries hit based on sub-parts
phishExpected["baz.com/foo/bar"] = true;
phishExpected["foo.bar.baz.com/foo"] = true;
phishExpected["bar.baz.com/"] = true;

var numExpecting;

function testFailure(arg) {
  do_throw(arg);
}

function checkNoHost() {
  // Looking up a no-host uri such as a data: uri should throw an exception.
  var exception;
  try {
    let principal = Services.scriptSecurityManager.createCodebasePrincipal(
      Services.io.newURI("data:text/html,<b>test</b>"), {});
    dbservice.lookup(principal, allTables);

    exception = false;
  } catch (e) {
    exception = true;
  }
  Assert.ok(exception);

  do_test_finished();
}

function tablesCallbackWithoutSub(tables) {
  var parts = tables.split("\n");
  parts.sort();

  // there's a leading \n here because splitting left an empty string
  // after the trailing newline, which will sort first
  Assert.equal(parts.join("\n"),
               "\ntest-block-simple;a:1\ntest-malware-simple;a:1\ntest-phish-simple;a:2\ntest-unwanted-simple;a:1");

  checkNoHost();
}


function expireSubSuccess(result) {
  dbservice.getTables(tablesCallbackWithoutSub);
}

function tablesCallbackWithSub(tables) {
  var parts = tables.split("\n");
  parts.sort();

  // there's a leading \n here because splitting left an empty string
  // after the trailing newline, which will sort first
  Assert.equal(parts.join("\n"),
               "\ntest-block-simple;a:1\ntest-malware-simple;a:1\ntest-phish-simple;a:2:s:3\ntest-unwanted-simple;a:1");

  // verify that expiring a sub chunk removes its name from the list
  var data =
    "n:1000\n" +
    "i:test-phish-simple\n" +
    "sd:3\n";

  doSimpleUpdate(data, expireSubSuccess, testFailure);
}

function checkChunksWithSub() {
  dbservice.getTables(tablesCallbackWithSub);
}

function checkDone() {
  if (--numExpecting == 0)
    checkChunksWithSub();
}

function phishExists(result) {
  dumpn("phishExists: " + result);
  try {
    Assert.ok(result.indexOf("test-phish-simple") != -1);
  } finally {
    checkDone();
  }
}

function phishDoesntExist(result) {
  dumpn("phishDoesntExist: " + result);
  try {
    Assert.ok(result.indexOf("test-phish-simple") == -1);
  } finally {
    checkDone();
  }
}

function malwareExists(result) {
  dumpn("malwareExists: " + result);

  try {
    Assert.ok(result.indexOf("test-malware-simple") != -1);
  } finally {
    checkDone();
  }
}

function unwantedExists(result) {
  dumpn("unwantedExists: " + result);

  try {
    Assert.ok(result.indexOf("test-unwanted-simple") != -1);
  } finally {
    checkDone();
  }
}

function blockedExists(result) {
  dumpn("blockedExists: " + result);

  try {
    Assert.ok(result.indexOf("test-block-simple") != -1);
  } finally {
    checkDone();
  }
}

function checkState() {
  numExpecting = 0;


  for (let key in phishExpected) {
    let principal = Services.scriptSecurityManager.createCodebasePrincipal(
      Services.io.newURI("http://" + key), {});
    dbservice.lookup(principal, allTables, phishExists, true);
    numExpecting++;
  }

  for (let key in phishUnexpected) {
    let principal = Services.scriptSecurityManager.createCodebasePrincipal(
      Services.io.newURI("http://" + key), {});
    dbservice.lookup(principal, allTables, phishDoesntExist, true);
    numExpecting++;
  }

  for (let key in malwareExpected) {
    let principal = Services.scriptSecurityManager.createCodebasePrincipal(
      Services.io.newURI("http://" + key), {});
    dbservice.lookup(principal, allTables, malwareExists, true);
    numExpecting++;
  }

  for (let key in unwantedExpected) {
    let principal = Services.scriptSecurityManager.createCodebasePrincipal(
      Services.io.newURI("http://" + key), {});
    dbservice.lookup(principal, allTables, unwantedExists, true);
    numExpecting++;
  }

  for (let key in blockedExpected) {
    let principal = Services.scriptSecurityManager.createCodebasePrincipal(
      Services.io.newURI("http://" + key), {});
    dbservice.lookup(principal, allTables, blockedExists, true);
    numExpecting++;
  }
}

function testSubSuccess(result) {
  Assert.equal(result, "1000");
  checkState();
}

function do_subs() {
  var data =
    "n:1000\n" +
    "i:test-phish-simple\n" +
    "s:3:32:" + chunk3Sub.length + "\n" +
    chunk3Sub + "\n" +
    "ad:1\n" +
    "ad:4-6\n";

  doSimpleUpdate(data, testSubSuccess, testFailure);
}

function testAddSuccess(arg) {
  Assert.equal(arg, "1000");

  do_subs();
}

function do_adds() {
  // This test relies on the fact that only -regexp tables are ungzipped,
  // and only -hash tables are assumed to be pre-md5'd.  So we use
  // a 'simple' table type to get simple hostname-per-line semantics.

  var data =
    "n:1000\n" +
    "i:test-phish-simple\n" +
    "a:1:32:" + chunk1.length + "\n" +
    chunk1 + "\n" +
    "a:2:32:" + chunk2.length + "\n" +
    chunk2 + "\n" +
    "a:4:32:" + chunk4.length + "\n" +
    chunk4 + "\n" +
    "a:5:32:" + chunk5.length + "\n" +
    chunk5 + "\n" +
    "a:6:32:" + chunk6.length + "\n" +
    chunk6 + "\n" +
    "i:test-malware-simple\n" +
    "a:1:32:" + chunk2.length + "\n" +
    chunk2 + "\n" +
    "i:test-unwanted-simple\n" +
    "a:1:32:" + chunk3.length + "\n" +
    chunk3 + "\n" +
    "i:test-block-simple\n" +
    "a:1:32:" + chunk7.length + "\n" +
    chunk7 + "\n";

  doSimpleUpdate(data, testAddSuccess, testFailure);
}

function run_test() {
  do_adds();
  do_test_pending();
}
