/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Wraps the given object in an XPConnect wrapper and, if an interface
// is passed, queries the result to that interface.
function xpcWrap(obj, iface) {
  let ifacePointer = Cc[
    "@mozilla.org/supports-interface-pointer;1"
  ].createInstance(Ci.nsISupportsInterfacePointer);

  ifacePointer.data = obj;
  if (iface) {
    return ifacePointer.data.QueryInterface(iface);
  }
  return ifacePointer.data;
}

/**
 * Mock a (set of) service(s) as the object mockService.
 *
 * @param {[string]} serviceNames
 *                   array of services names that mockService will be
 *                   allowed to QI to.  Must include the name of the
 *                   service referenced by contractId.
 * @param {string}   contractId
 *                   the component ID that will reference the mock object
 *                   instead of the original service
 * @param {object}   interfaceObj
 *                   interface object for the component
 * @param {object}   mockService
 *                   object that satisfies the contract well
 *                   enough to use as a mock of it
 * @returns {object} The newly-mocked service
 */
function mockService(serviceNames, contractId, interfaceObj, mockService) {
  // xpcWrap allows us to mock [implicit_jscontext] methods.
  let newService = {
    ...mockService,
    QueryInterface: ChromeUtils.generateQI(serviceNames),
  };
  let o = xpcWrap(newService, interfaceObj);
  const { MockRegistrar } = ChromeUtils.importESModule(
    "resource://testing-common/MockRegistrar.sys.mjs"
  );
  let cid = MockRegistrar.register(contractId, o);
  registerCleanupFunction(() => {
    MockRegistrar.unregister(cid);
  });
  return newService;
}

/**
 * Mock the nsIContentAnalysis service with the object mockCAService.
 *
 * @param {object}    mockCAService
 *                    the service to mock for nsIContentAnalysis
 * @returns {object}  The newly-mocked service
 */
function mockContentAnalysisService(mockCAService) {
  return mockService(
    ["nsIContentAnalysis"],
    "@mozilla.org/contentanalysis;1",
    Ci.nsIContentAnalysis,
    mockCAService
  );
}

/**
 * Make an nsIContentAnalysisResponse.
 *
 * @param {number} action The action to take, from the
 *  nsIContentAnalysisResponse.Action enum.
 * @param {string} token The requestToken.
 * @returns {object} An object that conforms to nsIContentAnalysisResponse.
 */
function makeContentAnalysisResponse(action, token) {
  return {
    action,
    shouldAllowContent: action != Ci.nsIContentAnalysisResponse.eBlock,
    requestToken: token,
    acknowledge: _acknowledgement => {},
  };
}

async function waitForFileToAlmostMatchSize(filePath, expectedSize) {
  // In Cocoa the CGContext adds a hash, plus there are other minor
  // non-user-visible differences, so we need to be a bit more sloppy there.
  //
  // We see one byte difference in Windows and Linux on automation sometimes,
  // though files are consistently the same locally, that needs
  // investigation, but it's probably harmless.
  // Note that this is copied from browser_print_stream.js.
  const maxSizeDifference = AppConstants.platform == "macosx" ? 100 : 3;

  // Buffering shenanigans? Wait for sizes to match... There's no great
  // IOUtils methods to force a flush without writing anything...
  // Note that this means if this results in a timeout this is exactly
  // the same as a test failure.
  // This is taken from toolkit/components/printing/tests/browser_print_stream.js
  await TestUtils.waitForCondition(async function () {
    let fileStat = await IOUtils.stat(filePath);

    info("got size: " + fileStat.size + " expected: " + expectedSize);
    Assert.greater(
      fileStat.size,
      0,
      "File should not be empty: " + fileStat.size
    );
    return Math.abs(fileStat.size - expectedSize) <= maxSizeDifference;
  }, "Sizes should (almost) match");
}

function makeMockContentAnalysis() {
  return {
    isActive: true,
    mightBeActive: true,
    errorValue: undefined,

    setupForTest(shouldAllowRequest) {
      this.shouldAllowRequest = shouldAllowRequest;
      this.errorValue = undefined;
      this.clearCalls();
    },

    setupForTestWithError(errorValue) {
      this.errorValue = errorValue;
      this.clearCalls();
    },

    clearCalls() {
      this.calls = [];
      this.browsingContextsForURIs = [];
    },

    getAction() {
      if (this.shouldAllowRequest === undefined) {
        this.shouldAllowRequest = true;
      }
      return this.shouldAllowRequest
        ? Ci.nsIContentAnalysisResponse.eAllow
        : Ci.nsIContentAnalysisResponse.eBlock;
    },

    // nsIContentAnalysis methods
    async analyzeContentRequest(request, _autoAcknowledge) {
      info(
        "Mock ContentAnalysis service: analyzeContentRequest, this.shouldAllowRequest=" +
          this.shouldAllowRequest +
          ", this.errorValue=" +
          this.errorValue
      );
      this.calls.push(request);
      if (this.errorValue) {
        throw this.errorValue;
      }
      // Use setTimeout to simulate an async activity
      await new Promise(res => setTimeout(res, 0));
      return makeContentAnalysisResponse(
        this.getAction(),
        request.requestToken
      );
    },

    analyzeContentRequestCallback(request, autoAcknowledge, callback) {
      info(
        "Mock ContentAnalysis service: analyzeContentRequestCallback, this.shouldAllowRequest=" +
          this.shouldAllowRequest +
          ", this.errorValue=" +
          this.errorValue
      );
      this.calls.push(request);
      if (this.errorValue) {
        throw this.errorValue;
      }
      let response = makeContentAnalysisResponse(
        this.getAction(),
        request.requestToken
      );
      // Use setTimeout to simulate an async activity
      setTimeout(() => {
        callback.contentResult(response);
      }, 0);
    },

    cancelAllRequests() {
      // This is called on exit, no need to do anything
    },

    getURIForBrowsingContext(aBrowsingContext) {
      // The real implementation walks up the parent chain as long
      // as the parent principal subsumes the child one. For testing
      // purposes, just return the browsing context's URI.
      this.browsingContextsForURIs.push(aBrowsingContext);
      return aBrowsingContext.currentURI;
    },
  };
}

function whenTabLoaded(aTab, aCallback) {
  promiseTabLoadEvent(aTab).then(aCallback);
}

function promiseTabLoaded(aTab) {
  return new Promise(resolve => {
    whenTabLoaded(aTab, resolve);
  });
}

/**
 * Waits for a load (or custom) event to finish in a given tab. If provided
 * load an uri into the tab.
 *
 * @param {object} tab
 *        The tab to load into.
 * @param {string} [url]
 *        The url to load, or the current url.
 * @returns {Promise<string>} resolved when the event is handled. Rejected if
 *          a valid load event is not received within a meaningful interval
 */
function promiseTabLoadEvent(tab, url) {
  info("Wait tab event: load");

  function handle(loadedUrl) {
    if (loadedUrl === "about:blank" || (url && loadedUrl !== url)) {
      info(`Skipping spurious load event for ${loadedUrl}`);
      return false;
    }

    info("Tab event received: load");
    return true;
  }

  let loaded = BrowserTestUtils.browserLoaded(tab.linkedBrowser, false, handle);

  if (url) {
    BrowserTestUtils.startLoadingURIString(tab.linkedBrowser, url);
  }

  return loaded;
}

function promisePopupShown(popup) {
  return BrowserTestUtils.waitForPopupEvent(popup, "shown");
}

function promisePopupHidden(popup) {
  return BrowserTestUtils.waitForPopupEvent(popup, "hidden");
}
