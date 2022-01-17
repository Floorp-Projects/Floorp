/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const { waitForInitialNavigationCompleted } = ChromeUtils.import(
  "chrome://remote/content/shared/Navigate.jsm"
);

class MockWebProgress {
  constructor(browsingContext) {
    this.browsingContext = browsingContext;

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

    this.isLoadingDocument = true;

    if (coop) {
      this.browsingContext = new MockTopContext(this);
    }

    if (!this.browsingContext.currentWindowGlobal) {
      this.browsingContext.currentWindowGlobal = {};
    }
    this.browsingContext.currentWindowGlobal.isInitialDocument = isInitial;

    this.listener?.onStateChange(
      this,
      null,
      Ci.nsIWebProgressListener.STATE_START,
      null
    );
  }

  sendStopState() {
    this.isLoadingDocument = false;
    this.listener?.onStateChange(
      this,
      null,
      Ci.nsIWebProgressListener.STATE_STOP,
      null
    );
  }
}

class MockTopContext {
  constructor(webProgress = null) {
    this.currentWindowGlobal = { isInitialDocument: true };
    this.id = 7;
    this.top = this;
    this.webProgress = webProgress || new MockWebProgress(this);
  }
}

add_test(
  async function test_waitForInitialNavigation_initialDocumentFinishedLoading() {
    const browsingContext = new MockTopContext();
    await waitForInitialNavigationCompleted(browsingContext);

    ok(
      !browsingContext.webProgress.isLoadingDocument,
      "Document is not loading"
    );
    ok(
      browsingContext.currentWindowGlobal.isInitialDocument,
      "Is initial document"
    );

    run_next_test();
  }
);

add_test(
  async function test_waitForInitialNavigation_initialDocumentAlreadyLoading() {
    const browsingContext = new MockTopContext();
    browsingContext.webProgress.sendStartState({ isInitial: true });

    const completed = waitForInitialNavigationCompleted(browsingContext);
    browsingContext.webProgress.sendStopState();
    await completed;

    ok(
      !browsingContext.webProgress.isLoadingDocument,
      "Document is not loading"
    );
    ok(
      browsingContext.currentWindowGlobal.isInitialDocument,
      "Is initial document"
    );

    run_next_test();
  }
);

add_test(
  async function test_waitForInitialNavigation_initialDocumentNoWindowGlobal() {
    const browsingContext = new MockTopContext();
    delete browsingContext.currentWindowGlobal;

    const completed = waitForInitialNavigationCompleted(browsingContext);
    browsingContext.webProgress.sendStartState({ isInitial: true });
    browsingContext.webProgress.sendStopState();
    await completed;

    ok(
      !browsingContext.webProgress.isLoadingDocument,
      "Document is not loading"
    );
    ok(
      browsingContext.currentWindowGlobal.isInitialDocument,
      "Is initial document"
    );

    run_next_test();
  }
);

add_test(
  async function test_waitForInitialNavigation_notInitialDocumentFinishedLoading() {
    const browsingContext = new MockTopContext();
    browsingContext.webProgress.sendStartState({ isInitial: false });
    browsingContext.webProgress.sendStopState();

    await waitForInitialNavigationCompleted(browsingContext);

    ok(
      !browsingContext.webProgress.isLoadingDocument,
      "Document is not loading"
    );
    ok(
      !browsingContext.currentWindowGlobal.isInitialDocument,
      "Is not initial document"
    );

    run_next_test();
  }
);

add_test(
  async function test_waitForInitialNavigation_notInitialDocumentAlreadyLoading() {
    const browsingContext = new MockTopContext();
    browsingContext.webProgress.sendStartState({ isInitial: false });

    const completed = waitForInitialNavigationCompleted(browsingContext);
    browsingContext.webProgress.sendStopState();
    await completed;

    ok(
      !browsingContext.webProgress.isLoadingDocument,
      "Document is not loading"
    );
    ok(
      !browsingContext.currentWindowGlobal.isInitialDocument,
      "Is not initial document"
    );

    run_next_test();
  }
);

add_test(
  async function test_waitForInitialNavigation_crossOriginAlreadyLoading() {
    const browsingContext = new MockTopContext();
    const webProgress = browsingContext.webProgress;

    browsingContext.webProgress.sendStartState({ coop: true });

    const completed = waitForInitialNavigationCompleted(browsingContext);
    browsingContext.webProgress.sendStopState();
    await completed;

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

    run_next_test();
  }
);
