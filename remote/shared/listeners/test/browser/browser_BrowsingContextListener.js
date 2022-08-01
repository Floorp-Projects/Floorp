/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const { BrowsingContextListener } = ChromeUtils.import(
  "chrome://remote/content/shared/listeners/BrowsingContextListener.jsm"
);

add_task(async function test_attachedOnNewTab() {
  const listener = new BrowsingContextListener();
  const attached = listener.once("attached");

  listener.startListening();

  const tab = BrowserTestUtils.addTab(gBrowser, "about:blank");
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);

  const { browsingContext, why } = await attached;

  is(
    browsingContext.id,
    tab.linkedBrowser.browsingContext.id,
    "Received expected browsing context"
  );
  is(why, "attach", "Browsing context has been attached");

  listener.stopListening();
  gBrowser.removeTab(tab);
});

add_task(async function test_attachedValidEmbedderElement() {
  const listener = new BrowsingContextListener();

  let hasEmbedderElement = false;
  listener.on(
    "attached",
    (evtName, { browsingContext }) => {
      hasEmbedderElement = !!browsingContext.embedderElement;
    },
    { once: true }
  );

  listener.startListening();

  const tab = BrowserTestUtils.addTab(gBrowser, "about:blank");
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);

  ok(
    hasEmbedderElement,
    "Attached browsing context has a valid embedder element"
  );

  listener.stopListening();
  gBrowser.removeTab(tab);
});

add_task(async function test_discardedOnCloseTab() {
  const listener = new BrowsingContextListener();
  const discarded = listener.once("discarded");

  const tab = BrowserTestUtils.addTab(gBrowser, "about:blank");
  const browsingContext = tab.linkedBrowser.browsingContext;
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);

  listener.startListening();
  gBrowser.removeTab(tab);
  const { browsingContext: discardedBrowsingContext, why } = await discarded;

  is(
    discardedBrowsingContext.id,
    browsingContext.id,
    "Received expected browsing context"
  );
  is(why, "discard", "Browsing context has been discarded");

  listener.stopListening();
});

add_task(async function test_replaceTopLevelOnNavigation() {
  const listener = new BrowsingContextListener();
  const attached = listener.once("attached");
  const discarded = listener.once("discarded");

  const tab = BrowserTestUtils.addTab(gBrowser, "about:blank");
  const browsingContext = tab.linkedBrowser.browsingContext;
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);

  listener.startListening();

  await loadURL(tab.linkedBrowser, "about:mozilla");

  const discardEvent = await discarded;
  const attachEvent = await attached;

  is(
    discardEvent.browsingContext.id,
    browsingContext.id,
    "Received expected browsing context for discarded"
  );
  is(discardEvent.why, "replace", "Browsing context has been replaced");

  is(
    attachEvent.browsingContext.id,
    tab.linkedBrowser.browsingContext.id,
    "Received expected browsing context for attached"
  );
  is(discardEvent.why, "replace", "Browsing context has been replaced");

  isnot(
    discardEvent.browsingContext,
    attachEvent.browsingContext,
    "Got different browsing contexts"
  );

  listener.stopListening();
  gBrowser.removeTab(gBrowser.selectedTab);
});
