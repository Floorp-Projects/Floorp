/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const TEST_PAGE_URL = "http://mochi.test:8888/browser/toolkit/components/thumbnails/test/thumbnails_background.sjs";
const TEST_CONTENT_HELPER = "chrome://mochitests/content/browser/toolkit/components/thumbnails/test/thumbnails_crash_content_helper.js";

const imports = {};
Cu.import("resource://gre/modules/BackgroundPageThumbs.jsm", imports);
Cu.import("resource://gre/modules/PageThumbs.jsm", imports);
Cu.import("resource://gre/modules/Task.jsm", imports);
Cu.import("resource://gre/modules/commonjs/sdk/core/promise.js", imports);

function test() {
  waitForExplicitFinish();
  Services.obs.addObserver(crashObserver, 'ipc:content-shutdown', false);

  spawnNextTest();
}

function spawnNextTest() {
  if (!tests.length) {
    Services.obs.removeObserver(crashObserver, 'ipc:content-shutdown');
    is(numCrashes, 2, "saw 2 crashes from this test");
    finish();
    return;
  }
  let test = tests.shift();
  info("Running sub-test " + test.name);
  imports.Task.spawn(test).then(spawnNextTest, function onError(err) {
    ok(false, err);
    spawnNextTest();
  });
}

let tests = [

  function crashDuringCapture() {
    // make a good capture first - this ensures we have the <browser>
    let goodUrl = testPageURL();
    yield capture(goodUrl);
    let goodFile = fileForURL(goodUrl);
    ok(goodFile.exists(), "Thumbnail should be cached after capture: " + goodFile.path);
    goodFile.remove(false);
    // inject our content script.
    let mm = injectContentScript();
    // queue up 2 captures - the first has a wait, so this is the one that
    // will die.  The second one should immediately capture after the crash.
    let waitUrl = testPageURL({ wait: 30000 });
    let deferred1 = capture(waitUrl);
    let deferred2 = capture(goodUrl);
    info("Crashing the thumbnail content process.");
    mm.sendAsyncMessage("thumbnails-test:crash");
    yield deferred1;
    let waitFile = fileForURL(waitUrl);
    ok(!waitFile.exists(), "Thumbnail should not have been saved due to the crash: " + waitFile.path);
    yield deferred2;
    ok(goodFile.exists(), "We should have recovered and completed the 2nd capture after the crash: " + goodFile.path);
    goodFile.remove(false);
  },

  function crashWhileIdle() {
    // make a good capture first - this ensures we have the <browser>
    let goodUrl = testPageURL();
    yield capture(goodUrl);
    let goodFile = fileForURL(goodUrl);
    ok(goodFile.exists(), "Thumbnail should be cached after capture: " + goodFile.path);
    goodFile.remove(false);
    // inject our content script.
    let mm = injectContentScript();
    // the observer for the crashing process is basically async, so it's hard
    // to know when the <browser> has actually seen it.  Easist is to just add
    // our own observer.
    let deferred = imports.Promise.defer();
    Services.obs.addObserver(function crashObserver() {
      Services.obs.removeObserver(crashObserver, "oop-frameloader-crashed");
      // spin the event loop to ensure the BPT observer was called.
      executeSoon(function() {
        // Now queue another capture and ensure it recovers.
        capture(goodUrl).then(() => {
          ok(goodFile.exists(), "We should have recovered and handled new capture requests: " + goodFile.path);
          goodFile.remove(false);
          deferred.resolve();
        });
      });
    } , "oop-frameloader-crashed", false);

    // Nothing is pending - crash the process.
    info("Crashing the thumbnail content process.");
    mm.sendAsyncMessage("thumbnails-test:crash");
    yield deferred.promise;
  },
];

function injectContentScript() {
  let thumbnailBrowser = imports.BackgroundPageThumbs._thumbBrowser;
  let mm = thumbnailBrowser.messageManager;
  mm.loadFrameScript(TEST_CONTENT_HELPER, false);
  return mm;
}

function capture(url, options) {
  let deferred = imports.Promise.defer();
  options = options || {};
  options.onDone = function onDone(capturedURL) {
    deferred.resolve(capturedURL);
  };
  imports.BackgroundPageThumbs.capture(url, options);
  return deferred.promise;
}

function fileForURL(url) {
  let path = imports.PageThumbsStorage.getFilePathForURL(url);
  let file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
  file.initWithPath(path);
  return file;
}

function testPageURL(opts) {
  return TEST_PAGE_URL + "?" + encodeURIComponent(JSON.stringify(opts || {}));
}

// This observer is needed so we can clean up all evidence of the crash so
// the testrunner thinks things are peachy.
let numCrashes = 0;
function crashObserver(subject, topic, data) {
  is(topic, 'ipc:content-shutdown', 'Received correct observer topic.');
  ok(subject instanceof Components.interfaces.nsIPropertyBag2,
     'Subject implements nsIPropertyBag2.');
  // we might see this called as the process terminates due to previous tests.
  // We are only looking for "abnormal" exits...
  if (!subject.hasKey("abnormal")) {
    info("This is a normal termination and isn't the one we are looking for...");
    return;
  }
  numCrashes++;

  var dumpID;
  if ('nsICrashReporter' in Components.interfaces) {
    dumpID = subject.getPropertyAsAString('dumpID');
    ok(dumpID, "dumpID is present and not an empty string");
  }

  if (dumpID) {
    var minidumpDirectory = getMinidumpDirectory();
    removeFile(minidumpDirectory, dumpID + '.dmp');
    removeFile(minidumpDirectory, dumpID + '.extra');
  }

}

function getMinidumpDirectory() {
  var dir = Services.dirsvc.get('ProfD', Components.interfaces.nsIFile);
  dir.append("minidumps");
  return dir;
}
function removeFile(directory, filename) {
  var file = directory.clone();
  file.append(filename);
  if (file.exists()) {
    file.remove(false);
  }
}
