/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

var tmp = {};
Cu.import("resource://gre/modules/PageThumbs.jsm", tmp);
Cu.import("resource://gre/modules/BackgroundPageThumbs.jsm", tmp);
Cu.import("resource://gre/modules/NewTabUtils.jsm", tmp);
Cu.import("resource:///modules/sessionstore/SessionStore.jsm", tmp);
Cu.import("resource://gre/modules/FileUtils.jsm", tmp);
Cu.import("resource://gre/modules/osfile.jsm", tmp);
var {PageThumbs, BackgroundPageThumbs, NewTabUtils, PageThumbsStorage, SessionStore, FileUtils, OS} = tmp;

XPCOMUtils.defineLazyModuleGetter(this, "PlacesTestUtils",
  "resource://testing-common/PlacesTestUtils.jsm");

var oldEnabledPref = Services.prefs.getBoolPref("browser.pagethumbnails.capturing_disabled");
Services.prefs.setBoolPref("browser.pagethumbnails.capturing_disabled", false);

registerCleanupFunction(function () {
  while (gBrowser.tabs.length > 1)
    gBrowser.removeTab(gBrowser.tabs[1]);
  Services.prefs.setBoolPref("browser.pagethumbnails.capturing_disabled", oldEnabledPref)
});

/**
 * Provide the default test function to start our test runner.
 */
function test() {
  TestRunner.run();
}

/**
 * The test runner that controls the execution flow of our tests.
 */
var TestRunner = {
  /**
   * Starts the test runner.
   */
  run: function () {
    waitForExplicitFinish();

    SessionStore.promiseInitialized.then(function () {
      this._iter = runTests();
      if (this._iter) {
        this.next();
      } else {
        finish();
      }
    }.bind(this));
  },

  /**
   * Runs the next available test or finishes if there's no test left.
   * @param aValue This value will be passed to the yielder via the runner's
   *               iterator.
   */
  next: function (aValue) {
    let obj = TestRunner._iter.next(aValue);
    if (obj.done) {
      finish();
      return;
    }

    let value = obj.value || obj;
    if (value && typeof value.then == "function") {
      value.then(result => {
        next(result);
      }, error => {
        ok(false, error + "\n" + error.stack);
      });
    }
  }
};

/**
 * Continues the current test execution.
 * @param aValue This value will be passed to the yielder via the runner's
 *               iterator.
 */
function next(aValue) {
  TestRunner.next(aValue);
}

/**
 * Creates a new tab with the given URI.
 * @param aURI The URI that's loaded in the tab.
 * @param aCallback The function to call when the tab has loaded.
 */
function addTab(aURI, aCallback) {
  let tab = gBrowser.selectedTab = gBrowser.addTab(aURI);
  whenLoaded(tab.linkedBrowser, aCallback);
}

/**
 * Loads a new URI into the currently selected tab.
 * @param aURI The URI to load.
 */
function navigateTo(aURI) {
  let browser = gBrowser.selectedBrowser;
  whenLoaded(browser);
  browser.loadURI(aURI);
}

/**
 * Continues the current test execution when a load event for the given element
 * has been received.
 * @param aElement The DOM element to listen on.
 * @param aCallback The function to call when the load event was dispatched.
 */
function whenLoaded(aElement, aCallback = next) {
  aElement.addEventListener("load", function onLoad() {
    aElement.removeEventListener("load", onLoad, true);
    executeSoon(aCallback);
  }, true);
}

/**
 * Captures a screenshot for the currently selected tab, stores it in the cache,
 * retrieves it from the cache and compares pixel color values.
 * @param aRed The red component's intensity.
 * @param aGreen The green component's intensity.
 * @param aBlue The blue component's intensity.
 * @param aMessage The info message to print when comparing the pixel color.
 */
function captureAndCheckColor(aRed, aGreen, aBlue, aMessage) {
  let browser = gBrowser.selectedBrowser;
  // We'll get oranges if the expiration filter removes the file during the
  // test.
  dontExpireThumbnailURLs([browser.currentURI.spec]);

  // Capture the screenshot.
  PageThumbs.captureAndStore(browser, function () {
    retrieveImageDataForURL(browser.currentURI.spec, function ([r, g, b]) {
      is("" + [r,g,b], "" + [aRed, aGreen, aBlue], aMessage);
      next();
    });
  });
}

/**
 * For a given URL, loads the corresponding thumbnail
 * to a canvas and passes its image data to the callback.
 * Note, not compat with e10s!
 * @param aURL The url associated with the thumbnail.
 * @param aCallback The function to pass the image data to.
 */
function retrieveImageDataForURL(aURL, aCallback) {
  let width = 100, height = 100;
  let thumb = PageThumbs.getThumbnailURL(aURL, width, height);

  let htmlns = "http://www.w3.org/1999/xhtml";
  let img = document.createElementNS(htmlns, "img");
  img.setAttribute("src", thumb);

  whenLoaded(img, function () {
    let canvas = document.createElementNS(htmlns, "canvas");
    canvas.setAttribute("width", width);
    canvas.setAttribute("height", height);

    // Draw the image to a canvas and compare the pixel color values.
    let ctx = canvas.getContext("2d");
    ctx.drawImage(img, 0, 0, width, height);
    let result = ctx.getImageData(0, 0, 100, 100).data;
    aCallback(result);
  });
}

/**
 * Returns the file of the thumbnail with the given URL.
 * @param aURL The URL of the thumbnail.
 */
function thumbnailFile(aURL) {
  return new FileUtils.File(PageThumbsStorage.getFilePathForURL(aURL));
}

/**
 * Checks if a thumbnail for the given URL exists.
 * @param aURL The url associated to the thumbnail.
 */
function thumbnailExists(aURL) {
  let file = thumbnailFile(aURL);
  return file.exists() && file.fileSize;
}

/**
 * Removes the thumbnail for the given URL.
 * @param aURL The URL associated with the thumbnail.
 */
function removeThumbnail(aURL) {
  let file = thumbnailFile(aURL);
  file.remove(false);
}

/**
 * Calls addVisits, and then forces the newtab module to repopulate its links.
 * See addVisits for parameter descriptions.
 */
function addVisitsAndRepopulateNewTabLinks(aPlaceInfo, aCallback) {
  PlacesTestUtils.addVisits(makeURI(aPlaceInfo)).then(() => {
    NewTabUtils.links.populateCache(aCallback, true);
  });
}
function promiseAddVisitsAndRepopulateNewTabLinks(aPlaceInfo) {
  return new Promise(resolve => addVisitsAndRepopulateNewTabLinks(aPlaceInfo, resolve));
}

/**
 * Calls a given callback when the thumbnail for a given URL has been found
 * on disk. Keeps trying until the thumbnail has been created.
 *
 * @param aURL The URL of the thumbnail's page.
 * @param [optional] aCallback
 *        Function to be invoked on completion.
 */
function whenFileExists(aURL, aCallback = next) {
  let callback = aCallback;
  if (!thumbnailExists(aURL)) {
    callback = () => whenFileExists(aURL, aCallback);
  }

  executeSoon(callback);
}

/**
 * Calls a given callback when the given file has been removed.
 * Keeps trying until the file is removed.
 *
 * @param aFile The file that is being removed
 * @param [optional] aCallback
 *        Function to be invoked on completion.
 */
function whenFileRemoved(aFile, aCallback) {
  let callback = aCallback;
  if (aFile.exists()) {
    callback = () => whenFileRemoved(aFile, aCallback);
  }

  executeSoon(callback || next);
}

function wait(aMillis) {
  setTimeout(next, aMillis);
}

/**
 * Makes sure that a given list of URLs is not implicitly expired.
 *
 * @param aURLs The list of URLs that should not be expired.
 */
function dontExpireThumbnailURLs(aURLs) {
  let dontExpireURLs = (cb) => cb(aURLs);
  PageThumbs.addExpirationFilter(dontExpireURLs);

  registerCleanupFunction(function () {
    PageThumbs.removeExpirationFilter(dontExpireURLs);
  });
}

function bgCapture(aURL, aOptions) {
  bgCaptureWithMethod("capture", aURL, aOptions);
}

function bgCaptureIfMissing(aURL, aOptions) {
  bgCaptureWithMethod("captureIfMissing", aURL, aOptions);
}

function bgCaptureWithMethod(aMethodName, aURL, aOptions = {}) {
  // We'll get oranges if the expiration filter removes the file during the
  // test.
  dontExpireThumbnailURLs([aURL]);
  if (!aOptions.onDone)
    aOptions.onDone = next;
  BackgroundPageThumbs[aMethodName](aURL, aOptions);
}

function bgTestPageURL(aOpts = {}) {
  let TEST_PAGE_URL = "http://mochi.test:8888/browser/toolkit/components/thumbnails/test/thumbnails_background.sjs";
  return TEST_PAGE_URL + "?" + encodeURIComponent(JSON.stringify(aOpts));
}

function bgAddPageThumbObserver(url) {
  return new Promise((resolve, reject) => {
    function observe(subject, topic, data) { // jshint ignore:line
      if (data === url) {
        switch (topic) {
          case "page-thumbnail:create":
            resolve();
            break;
          case "page-thumbnail:error":
            reject(new Error("page-thumbnail:error"));
            break;
        }
        Services.obs.removeObserver(observe, "page-thumbnail:create");
        Services.obs.removeObserver(observe, "page-thumbnail:error");
      }
    }
    Services.obs.addObserver(observe, "page-thumbnail:create", false);
    Services.obs.addObserver(observe, "page-thumbnail:error", false);
  });
}

function bgAddCrashObserver() {
  let crashed = false;
  Services.obs.addObserver(function crashObserver(subject, topic, data) {
    is(topic, 'ipc:content-shutdown', 'Received correct observer topic.');
    ok(subject instanceof Components.interfaces.nsIPropertyBag2,
       'Subject implements nsIPropertyBag2.');
    // we might see this called as the process terminates due to previous tests.
    // We are only looking for "abnormal" exits...
    if (!subject.hasKey("abnormal")) {
      info("This is a normal termination and isn't the one we are looking for...");
      return;
    }
    Services.obs.removeObserver(crashObserver, 'ipc:content-shutdown');
    crashed = true;

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
  }, 'ipc:content-shutdown', false);
  return {
    get crashed() {
      return crashed;
    }
  };
}

function bgInjectCrashContentScript() {
  const TEST_CONTENT_HELPER = "chrome://mochitests/content/browser/toolkit/components/thumbnails/test/thumbnails_crash_content_helper.js";
  let thumbnailBrowser = BackgroundPageThumbs._thumbBrowser;
  let mm = thumbnailBrowser.messageManager;
  mm.loadFrameScript(TEST_CONTENT_HELPER, false);
  return mm;
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
