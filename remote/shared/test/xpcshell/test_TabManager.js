/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const { TabManager } = ChromeUtils.import(
  "chrome://remote/content/shared/TabManager.jsm"
);

class MockTopBrowsingContext {
  constructor() {
    this.embedderElement = { permanentKey: {} };
    this.id = 1;
    this.top = this;
  }
}

class MockBrowsingContext {
  constructor() {
    this.id = 2;

    const topContext = new MockTopBrowsingContext();
    this.parent = topContext;
    this.top = topContext;
  }
}

const mockTopBrowsingContext = new MockTopBrowsingContext();
const mockBrowsingContext = new MockBrowsingContext();

add_task(async function test_getIdForBrowsingContext() {
  // Browsing context not set.
  equal(TabManager.getIdForBrowsingContext(null), null);
  equal(TabManager.getIdForBrowsingContext(undefined), null);

  // Child browsing context.
  equal(
    TabManager.getIdForBrowsingContext(mockBrowsingContext),
    mockBrowsingContext.id
  );

  // Top-level browsing context.
  equal(
    TabManager.getIdForBrowsingContext(mockTopBrowsingContext),
    TabManager.getBrowserIdForBrowsingContext(mockTopBrowsingContext)
  );
});
