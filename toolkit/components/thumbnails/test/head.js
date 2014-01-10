/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let tmp = {};
Cu.import("resource://gre/modules/PageThumbs.jsm", tmp);
Cu.import("resource://gre/modules/BackgroundPageThumbs.jsm", tmp);
Cu.import("resource://gre/modules/NewTabUtils.jsm", tmp);
Cu.import("resource:///modules/sessionstore/SessionStore.jsm", tmp);
Cu.import("resource://gre/modules/FileUtils.jsm", tmp);
Cu.import("resource://gre/modules/osfile.jsm", tmp);
let {PageThumbs, BackgroundPageThumbs, NewTabUtils, PageThumbsStorage, SessionStore, FileUtils, OS} = tmp;

Cu.import("resource://gre/modules/PlacesUtils.jsm");

let oldEnabledPref = Services.prefs.getBoolPref("browser.pagethumbnails.capturing_disabled");
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
let TestRunner = {
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
    try {
      let value = TestRunner._iter.send(aValue);
      if (value && typeof value.then == "function") {
        value.then(result => {
          next(result);
        }, error => {
          ok(false, error + "\n" + error.stack);
        });
      }
    } catch (e if e instanceof StopIteration) {
      finish();
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
  let browser = gBrowser.selectedTab.linkedBrowser;
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
 * @param aURL The url associated with the thumbnail.
 * @param aCallback The function to pass the image data to.
 */
function retrieveImageDataForURL(aURL, aCallback) {
  let width = 100, height = 100;
  let thumb = PageThumbs.getThumbnailURL(aURL, width, height);
  // create a tab with a chrome:// URL so it can host the thumbnail image.
  // Note that we tried creating the element directly in the top-level chrome
  // document, but this caused a strange problem:
  // * call this with the url of an image.
  // * immediately change the image content.
  // * call this again with the same url (now holding different content)
  // The original image data would be used.  Maybe the img hadn't been
  // collected yet and the platform noticed the same URL, so reused the
  // content?  Not sure - but this solves the problem.
  addTab("chrome://global/content/mozilla.xhtml", () => {
    let doc = gBrowser.selectedBrowser.contentDocument;
    let htmlns = "http://www.w3.org/1999/xhtml";
    let img = doc.createElementNS(htmlns, "img");
    img.setAttribute("src", thumb);

    whenLoaded(img, function () {
      let canvas = document.createElementNS(htmlns, "canvas");
      canvas.setAttribute("width", width);
      canvas.setAttribute("height", height);

      // Draw the image to a canvas and compare the pixel color values.
      let ctx = canvas.getContext("2d");
      ctx.drawImage(img, 0, 0, width, height);
      let result = ctx.getImageData(0, 0, 100, 100).data;
      gBrowser.removeTab(gBrowser.selectedTab);
      aCallback(result);
    });
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
 * Asynchronously adds visits to a page, invoking a callback function when done.
 *
 * @param aPlaceInfo
 *        One of the following: a string spec, an nsIURI, an object describing
 *        the Place as described below, or an array of any such types.  An
 *        object describing a Place must look like this:
 *          { uri: nsIURI of the page,
 *            [optional] transition: one of the TRANSITION_* from
 *                       nsINavHistoryService,
 *            [optional] title: title of the page,
 *            [optional] visitDate: visit date in microseconds from the epoch
 *            [optional] referrer: nsIURI of the referrer for this visit
 *          }
 * @param [optional] aCallback
 *        Function to be invoked on completion.
 */
function addVisits(aPlaceInfo, aCallback) {
  let places = [];
  if (aPlaceInfo instanceof Ci.nsIURI) {
    places.push({ uri: aPlaceInfo });
  }
  else if (Array.isArray(aPlaceInfo)) {
    places = places.concat(aPlaceInfo);
  } else {
    places.push(aPlaceInfo)
  }

  // Create mozIVisitInfo for each entry.
  let now = Date.now();
  for (let i = 0; i < places.length; i++) {
    if (typeof(places[i] == "string")) {
      places[i] = { uri: Services.io.newURI(places[i], "", null) };
    }
    if (!places[i].title) {
      places[i].title = "test visit for " + places[i].uri.spec;
    }
    places[i].visits = [{
      transitionType: places[i].transition === undefined ? PlacesUtils.history.TRANSITION_LINK
                                                         : places[i].transition,
      visitDate: places[i].visitDate || (now++) * 1000,
      referrerURI: places[i].referrer
    }];
  }

  PlacesUtils.asyncHistory.updatePlaces(
    places,
    {
      handleError: function AAV_handleError() {
        throw("Unexpected error in adding visit.");
      },
      handleResult: function () {},
      handleCompletion: function UP_handleCompletion() {
        if (aCallback)
          aCallback();
      }
    }
  );
}

/**
 * Calls addVisits, and then forces the newtab module to repopulate its links.
 * See addVisits for parameter descriptions.
 */
function addVisitsAndRepopulateNewTabLinks(aPlaceInfo, aCallback) {
  addVisits(aPlaceInfo, () => NewTabUtils.links.populateCache(aCallback, true));
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
    callback = function () whenFileExists(aURL, aCallback);
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
    callback = function () whenFileRemoved(aFile, aCallback);
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
  if (!aOptions.onDone)
    aOptions.onDone = next;
  BackgroundPageThumbs[aMethodName](aURL, aOptions);
}

function bgTestPageURL(aOpts = {}) {
  let TEST_PAGE_URL = "http://mochi.test:8888/browser/toolkit/components/thumbnails/test/thumbnails_background.sjs";
  return TEST_PAGE_URL + "?" + encodeURIComponent(JSON.stringify(aOpts));
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
    get crashed() crashed
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
