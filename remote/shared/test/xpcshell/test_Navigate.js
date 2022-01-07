/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const { waitForInitialNavigationCompleted } = ChromeUtils.import(
  "chrome://remote/content/shared/Navigate.jsm"
);

const mockWebProgress = {
  onStateChangeCalled: false,
  isLoadingDocument: false,
  addProgressListener(listener) {
    if (!this.isLoadingDocument) {
      return;
    }

    listener.onStateChange(
      null,
      null,
      Ci.nsIWebProgressListener.STATE_STOP,
      null
    );
    this.onStateChangeCalled = true;
  },
  removeProgressListener(listener) {},
};

const mockTopContext = {
  get children() {
    return [mockNestedContext];
  },
  currentWindowGlobal: {},
  id: 7,
  get top() {
    return this;
  },
  webProgress: mockWebProgress,
};

const mockNestedContext = {
  currentWindowGlobal: {},
  id: 8,
  parent: mockTopContext,
  top: mockTopContext,
  webProgress: mockWebProgress,
};

add_test(async function test_waitForInitialNavigationCompleted_isNotLoading() {
  const browsingContext = Object.assign({}, mockTopContext);
  await waitForInitialNavigationCompleted(browsingContext);
  ok(
    !browsingContext.webProgress.onStateChangeCalled,
    "WebProgress' onStateChange hasn't been fired"
  );

  run_next_test();
});

add_test(async function test_waitForInitialNavigationCompleted_topIsLoading() {
  const browsingContext = Object.assign({}, mockTopContext);
  browsingContext.webProgress.isLoadingDocument = true;
  await waitForInitialNavigationCompleted(browsingContext);
  ok(
    browsingContext.webProgress.onStateChangeCalled,
    "WebProgress' onStateChange has been fired"
  );

  run_next_test();
});

add_test(
  async function test_waitForInitialNavigationCompleted_nestedIsLoading() {
    const browsingContext = Object.assign({}, mockNestedContext);
    const topContext = browsingContext.top;

    browsingContext.webProgress.isLoadingDocument = true;
    await waitForInitialNavigationCompleted(topContext);
    ok(
      browsingContext.webProgress.onStateChangeCalled,
      "WebProgress' onStateChange has been fired on the nested browsing context"
    );
    ok(
      topContext.webProgress.onStateChangeCalled,
      "WebProgress' onStateChange has been fired on the top browsing context"
    );

    run_next_test();
  }
);

add_test(
  async function test_waitForInitialNavigationCompleted_missingWindowGlobal() {
    const browsingContext = Object.assign({}, mockTopContext);
    delete browsingContext.currentWindowGlobal;
    await waitForInitialNavigationCompleted(browsingContext);
    ok(
      browsingContext.webProgress.onStateChangeCalled,
      "WebProgress' onStateChange has been fired"
    );

    run_next_test();
  }
);
