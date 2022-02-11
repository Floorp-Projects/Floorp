/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

const { waitForInitialNavigationCompleted } = ChromeUtils.import(
  "chrome://remote/content/shared/Navigate.jsm"
);

const CURRENT_URI = Services.io.newURI("http://foo.bar/");
const REQUESTED_URI = Services.io.newURI("http://foo.cheese/");

class MockRequest {
  constructor() {
    this.originalURI = REQUESTED_URI;
  }

  get QueryInterface() {
    return ChromeUtils.generateQI(["nsIRequest", "nsIChannel"]);
  }
}

class MockWebProgress {
  constructor(browsingContext) {
    this.browsingContext = browsingContext;

    this.documentRequest = null;
    this.isLoadingDocument = false;
    this.listener = null;
    this.progressListenerRemoved = false;
  }

  addProgressListener(listener) {
    if (this.listener) {
      throw new Error("Cannot register listener twice");
    }

    this.listener = listener;
  }

  removeProgressListener(listener) {
    if (listener === this.listener) {
      this.listener = null;
      this.progressListenerRemoved = true;
    } else {
      throw new Error("Unknown listener");
    }
  }

  sendStartState(options = {}) {
    const { coop = false, isInitial = false } = options;

    if (coop) {
      this.browsingContext = new MockTopContext(this);
    }

    if (!this.browsingContext.currentWindowGlobal) {
      this.browsingContext.currentWindowGlobal = {};
    }

    this.browsingContext.currentWindowGlobal.isInitialDocument = isInitial;
    this.isLoadingDocument = true;
    this.documentRequest = new MockRequest();

    this.listener?.onStateChange(
      this,
      this.documentRequest,
      Ci.nsIWebProgressListener.STATE_START,
      null
    );
  }

  sendStopState() {
    this.listener?.onStateChange(
      this,
      this.documentRequest,
      Ci.nsIWebProgressListener.STATE_STOP,
      null
    );

    this.browsingContext.currentURI = this.documentRequest.originalURI;
    this.isLoadingDocument = false;
    this.documentRequest = null;
  }
}

class MockTopContext {
  constructor(webProgress = null) {
    this.currentURI = CURRENT_URI;
    this.currentWindowGlobal = { isInitialDocument: true };
    this.id = 7;
    this.top = this;
    this.webProgress = webProgress || new MockWebProgress(this);
  }
}

add_test(
  async function test_waitForInitialNavigation_initialDocumentNoWindowGlobal() {
    const browsingContext = new MockTopContext();
    const webProgress = browsingContext.webProgress;

    // In some cases there might be no window global yet.
    delete browsingContext.currentWindowGlobal;

    ok(!webProgress.isLoadingDocument, "Document is not loading");

    const completed = waitForInitialNavigationCompleted(webProgress);
    webProgress.sendStartState({ isInitial: true });
    webProgress.sendStopState();
    const { currentURI, targetURI } = await completed;

    ok(!webProgress.isLoadingDocument, "Document is not loading");
    ok(
      webProgress.browsingContext.currentWindowGlobal.isInitialDocument,
      "Is initial document"
    );
    equal(currentURI.spec, REQUESTED_URI.spec, "Current URI has been set");
    equal(targetURI.spec, REQUESTED_URI.spec, "Original URI has been set");

    run_next_test();
  }
);

add_test(
  async function test_waitForInitialNavigation_initialDocumentNotLoading() {
    const browsingContext = new MockTopContext();
    const webProgress = browsingContext.webProgress;

    ok(!webProgress.isLoadingDocument, "Document is not loading");

    const completed = waitForInitialNavigationCompleted(webProgress);
    webProgress.sendStartState({ isInitial: true });
    webProgress.sendStopState();
    const { currentURI, targetURI } = await completed;

    ok(!webProgress.isLoadingDocument, "Document is not loading");
    ok(
      webProgress.browsingContext.currentWindowGlobal.isInitialDocument,
      "Is initial document"
    );
    equal(currentURI.spec, REQUESTED_URI.spec, "Current URI has been set");
    equal(targetURI.spec, REQUESTED_URI.spec, "Original URI has been set");

    run_next_test();
  }
);

add_test(
  async function test_waitForInitialNavigation_initialDocumentAlreadyLoading() {
    const browsingContext = new MockTopContext();
    const webProgress = browsingContext.webProgress;

    webProgress.sendStartState({ isInitial: true });
    ok(webProgress.isLoadingDocument, "Document is loading");

    const completed = waitForInitialNavigationCompleted(webProgress);
    webProgress.sendStopState();
    const { currentURI, targetURI } = await completed;

    ok(!webProgress.isLoadingDocument, "Document is not loading");
    ok(
      webProgress.browsingContext.currentWindowGlobal.isInitialDocument,
      "Is initial document"
    );
    equal(currentURI.spec, REQUESTED_URI.spec, "Current URI has been set");
    equal(targetURI.spec, REQUESTED_URI.spec, "Original URI has been set");

    run_next_test();
  }
);

add_test(
  async function test_waitForInitialNavigation_initialDocumentFinishedLoading() {
    const browsingContext = new MockTopContext();
    const webProgress = browsingContext.webProgress;

    webProgress.sendStartState({ isInitial: true });
    webProgress.sendStopState();

    ok(!webProgress.isLoadingDocument, "Document is not loading");

    const { currentURI, targetURI } = await waitForInitialNavigationCompleted(
      webProgress
    );

    ok(!webProgress.isLoadingDocument, "Document is not loading");
    ok(
      webProgress.browsingContext.currentWindowGlobal.isInitialDocument,
      "Is initial document"
    );
    equal(currentURI.spec, REQUESTED_URI.spec, "Current URI has been set");
    equal(targetURI.spec, REQUESTED_URI.spec, "Original URI has been set");

    run_next_test();
  }
);

add_test(
  async function test_waitForInitialNavigation_notInitialDocumentNotLoading() {
    const browsingContext = new MockTopContext();
    const webProgress = browsingContext.webProgress;

    ok(!webProgress.isLoadingDocument, "Document is not loading");

    const completed = waitForInitialNavigationCompleted(webProgress);
    webProgress.sendStartState({ isInitial: false });
    webProgress.sendStopState();
    const { currentURI, targetURI } = await completed;

    ok(!webProgress.isLoadingDocument, "Document is not loading");
    ok(
      !browsingContext.currentWindowGlobal.isInitialDocument,
      "Is not initial document"
    );
    equal(currentURI.spec, REQUESTED_URI.spec, "Current URI has been set");
    equal(targetURI.spec, REQUESTED_URI.spec, "Original URI has been set");

    run_next_test();
  }
);

add_test(
  async function test_waitForInitialNavigation_notInitialDocumentAlreadyLoading() {
    const browsingContext = new MockTopContext();
    const webProgress = browsingContext.webProgress;

    webProgress.sendStartState({ isInitial: false });
    ok(webProgress.isLoadingDocument, "Document is loading");

    const completed = waitForInitialNavigationCompleted(webProgress);
    webProgress.sendStopState();
    const { currentURI, targetURI } = await completed;

    ok(!webProgress.isLoadingDocument, "Document is not loading");
    ok(
      !browsingContext.currentWindowGlobal.isInitialDocument,
      "Is not initial document"
    );
    equal(currentURI.spec, REQUESTED_URI.spec, "Current URI has been set");
    equal(targetURI.spec, REQUESTED_URI.spec, "Original URI has been set");

    run_next_test();
  }
);

add_test(
  async function test_waitForInitialNavigation_notInitialDocumentFinishedLoading() {
    const browsingContext = new MockTopContext();
    const webProgress = browsingContext.webProgress;

    webProgress.sendStartState({ isInitial: false });
    webProgress.sendStopState();

    ok(!webProgress.isLoadingDocument, "Document is not loading");

    const { currentURI, targetURI } = await waitForInitialNavigationCompleted(
      webProgress
    );

    ok(!webProgress.isLoadingDocument, "Document is not loading");
    ok(
      !webProgress.browsingContext.currentWindowGlobal.isInitialDocument,
      "Is not initial document"
    );
    equal(currentURI.spec, REQUESTED_URI.spec, "Current URI has been set");
    equal(targetURI.spec, REQUESTED_URI.spec, "Original URI has been set");

    run_next_test();
  }
);

add_test(async function test_waitForInitialNavigation_resolveWhenStarted() {
  const browsingContext = new MockTopContext();
  const webProgress = browsingContext.webProgress;

  webProgress.sendStartState();
  ok(webProgress.isLoadingDocument, "Document is already loading");

  const completed = waitForInitialNavigationCompleted(webProgress, {
    resolveWhenStarted: true,
  });
  const { currentURI, targetURI } = await completed;

  ok(webProgress.isLoadingDocument, "Document is still loading");
  ok(
    !webProgress.browsingContext.currentWindowGlobal.isInitialDocument,
    "Is not initial document"
  );
  equal(currentURI.spec, CURRENT_URI.spec, "Current URI has been set");
  equal(targetURI.spec, REQUESTED_URI.spec, "Original URI has been set");

  run_next_test();
});

add_test(async function test_waitForInitialNavigation_crossOrigin() {
  const browsingContext = new MockTopContext();
  const webProgress = browsingContext.webProgress;

  ok(!webProgress.isLoadingDocument, "Document is not loading");

  const completed = waitForInitialNavigationCompleted(webProgress);
  webProgress.sendStartState({ coop: true });
  webProgress.sendStopState();
  const { currentURI, targetURI } = await completed;

  notEqual(
    browsingContext,
    webProgress.browsingContext,
    "Got new browsing context"
  );
  ok(!webProgress.isLoadingDocument, "Document is not loading");
  ok(
    !webProgress.browsingContext.currentWindowGlobal.isInitialDocument,
    "Is not initial document"
  );
  equal(currentURI.spec, REQUESTED_URI.spec, "Current URI has been set");
  equal(targetURI.spec, REQUESTED_URI.spec, "Original URI has been set");

  run_next_test();
});
