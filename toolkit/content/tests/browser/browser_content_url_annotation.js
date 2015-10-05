/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
/*global Services, requestLongerTimeout, TestUtils, BrowserTestUtils,
 ok, info, dump, is, Ci, Cu, Components, ctypes, privateNoteIntentionalCrash,
 gBrowser, add_task, addEventListener, removeEventListener, ContentTask */

"use strict";

// Running this test in ASAN is slow.
requestLongerTimeout(2);

/**
 * Returns a Promise that resolves once a remote <xul:browser> has experienced
 * a crash. Resolves with the data from the .extra file (the crash annotations).
 *
 * @param browser
 *        The <xul:browser> that will crash
 * @return Promise
 */
function crashBrowser(browser) {
  let kv = {};
  Cu.import("resource://gre/modules/KeyValueParser.jsm", kv);
  // This frame script is injected into the remote browser, and used to
  // intentionally crash the tab. We crash by using js-ctypes and dereferencing
  // a bad pointer. The crash should happen immediately upon loading this
  // frame script.
  let frame_script = () => {
    const Cu = Components.utils;
    Cu.import("resource://gre/modules/ctypes.jsm");

    let dies = function() {
      privateNoteIntentionalCrash();
      let zero = new ctypes.intptr_t(8);
      let badptr = ctypes.cast(zero, ctypes.PointerType(ctypes.int32_t));
      let crash = badptr.contents;
    };

    dump("Et tu, Brute?");
    dies();
  };

  function checkSubject(subject, data) {
    return subject instanceof Ci.nsIPropertyBag2 &&
      subject.hasKey("abnormal");
  };
  let crashPromise = TestUtils.topicObserved('ipc:content-shutdown',
                                             checkSubject);
  let crashDataPromise = crashPromise.then(([subject, data]) => {
    ok(subject instanceof Ci.nsIPropertyBag2);

    let dumpID;
    if ('nsICrashReporter' in Ci) {
      dumpID = subject.getPropertyAsAString('dumpID');
      ok(dumpID, "dumpID is present and not an empty string");
    }

    let extra = null;
    if (dumpID) {
      let minidumpDirectory = getMinidumpDirectory();
      let extrafile = minidumpDirectory.clone();
      extrafile.append(dumpID + '.extra');
      ok(extrafile.exists(), 'found .extra file');
      extra = kv.parseKeyValuePairsFromFile(extrafile);
      removeFile(minidumpDirectory, dumpID + '.dmp');
      removeFile(minidumpDirectory, dumpID + '.extra');
    }

    return extra;
  });

  // This frame script will crash the remote browser as soon as it is
  // evaluated.
  let mm = browser.messageManager;
  mm.loadFrameScript("data:,(" + frame_script.toString() + ")();", false);
  return crashDataPromise;
}

/**
 * Removes a file from a directory. This is a no-op if the file does not
 * exist.
 *
 * @param directory
 *        The nsIFile representing the directory to remove from.
 * @param filename
 *        A string for the file to remove from the directory.
 */
function removeFile(directory, filename) {
  let file = directory.clone();
  file.append(filename);
  if (file.exists()) {
    file.remove(false);
  }
}

/**
 * Returns the directory where crash dumps are stored.
 *
 * @return nsIFile
 */
function getMinidumpDirectory() {
  let dir = Services.dirsvc.get('ProfD', Ci.nsIFile);
  dir.append("minidumps");
  return dir;
}

/**
 * Checks that the URL is correctly annotated on a content process crash.
 */
add_task(function* test_content_url_annotation() {
  let url = "https://example.com/browser/toolkit/content/tests/browser/file_redirect.html";
  let redirect_url = "https://example.com/browser/toolkit/content/tests/browser/file_redirect_to.html";

  yield BrowserTestUtils.withNewTab({
    gBrowser: gBrowser
  }, function* (browser) {
    ok(browser.isRemoteBrowser, "Should be a remote browser");

    // file_redirect.html should send us to file_redirect_to.html
    let promise = ContentTask.spawn(browser, {}, function* () {
      dump('ContentTask starting...\n');
      yield new Promise((resolve) => {
        addEventListener("RedirectDone", function listener() {
          dump('Got RedirectDone\n');
          removeEventListener("RedirectDone", listener);
          resolve();
        }, true, true);
      });
    });
    browser.loadURI(url);
    yield promise;

    // Crash the tab
    let annotations = yield crashBrowser(browser);

    ok("URL" in annotations, "annotated a URL");
    is(annotations.URL, redirect_url,
       "Should have annotated the URL after redirect");
  });
});
