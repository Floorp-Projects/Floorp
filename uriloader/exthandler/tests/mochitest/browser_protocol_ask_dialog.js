/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_PATH = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "https://example.com"
);

const CONTENT_HANDLING_URL =
  "chrome://mozapps/content/handling/appChooser.xhtml";

add_task(setupMailHandler);

/**
 * Check that if we open the protocol handler dialog from a subframe, we close
 * it when closing the tab.
 */
add_task(async function test_closed_by_tab_closure() {
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    TEST_PATH + "file_nested_protocol_request.html"
  );

  // Wait for the window and then click the link.
  let dialogWindowPromise = waitForProtocolAppChooserDialog(
    tab.linkedBrowser,
    true
  );

  BrowserTestUtils.synthesizeMouseAtCenter(
    "a:link",
    {},
    tab.linkedBrowser.browsingContext.children[0]
  );

  let dialog = await dialogWindowPromise;

  is(
    dialog._frame.contentDocument.location.href,
    CONTENT_HANDLING_URL,
    "Dialog URL is as expected"
  );
  let dialogClosedPromise = waitForProtocolAppChooserDialog(
    tab.linkedBrowser,
    false
  );

  info("Removing tab to close the dialog.");
  gBrowser.removeTab(tab);
  await dialogClosedPromise;
  ok(!dialog._frame.contentWindow, "The dialog should have been closed.");
});

/**
 * Check that if we open the protocol handler dialog from a subframe, we close
 * it when navigating the tab to a non-same-origin URL.
 */
add_task(async function test_closed_by_tab_navigation() {
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    TEST_PATH + "file_nested_protocol_request.html"
  );

  // Wait for the window and then click the link.
  let dialogWindowPromise = waitForProtocolAppChooserDialog(
    tab.linkedBrowser,
    true
  );

  BrowserTestUtils.synthesizeMouseAtCenter(
    "a:link",
    {},
    tab.linkedBrowser.browsingContext.children[0]
  );
  let dialog = await dialogWindowPromise;

  is(
    dialog._frame.contentDocument.location.href,
    CONTENT_HANDLING_URL,
    "Dialog URL is as expected"
  );
  let dialogClosedPromise = waitForProtocolAppChooserDialog(
    tab.linkedBrowser,
    false
  );
  info(
    "Set up unload handler to ensure we don't break when the window global gets cleared"
  );
  await SpecialPowers.spawn(tab.linkedBrowser, [], async function() {
    content.addEventListener("unload", function() {});
  });

  info("Navigating tab to a different but same origin page.");
  BrowserTestUtils.loadURI(tab.linkedBrowser, TEST_PATH);
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser, false, TEST_PATH);
  ok(dialog._frame.contentWindow, "Dialog should stay open.");

  // The use of weak references in various parts of the code means that we're
  // susceptible to dropping crucial bits of our implementation on the floor,
  // if they get GC'd, and then the test hangs.
  // Do a bunch of GC/CC runs so that if we ever break, it's deterministic.
  let numCycles = 3;
  for (let i = 0; i < numCycles; i++) {
    Cu.forceGC();
    Cu.forceCC();
  }

  info("Now navigate to a cross-origin page.");
  const CROSS_ORIGIN_TEST_PATH = TEST_PATH.replace(".com", ".org");
  BrowserTestUtils.loadURI(tab.linkedBrowser, CROSS_ORIGIN_TEST_PATH);
  let loadPromise = BrowserTestUtils.browserLoaded(
    tab.linkedBrowser,
    false,
    CROSS_ORIGIN_TEST_PATH
  );
  await dialogClosedPromise;
  ok(!dialog._frame.contentWindow, "The dialog should have been closed.");

  // Avoid errors from aborted loads by waiting for it to finish.
  await loadPromise;
  gBrowser.removeTab(tab);
});

/**
 * Check that we cannot open more than one of these dialogs.
 */
add_task(async function test_multiple_dialogs() {
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    TEST_PATH + "file_nested_protocol_request.html"
  );

  // Wait for the window and then click the link.
  let dialogWindowPromise = waitForProtocolAppChooserDialog(
    tab.linkedBrowser,
    true
  );
  BrowserTestUtils.synthesizeMouseAtCenter(
    "a:link",
    {},
    tab.linkedBrowser.browsingContext.children[0]
  );
  let dialog = await dialogWindowPromise;

  is(
    dialog._frame.contentDocument.location.href,
    CONTENT_HANDLING_URL,
    "Dialog URL is as expected"
  );

  // Navigate the parent frame:
  await ContentTask.spawn(tab.linkedBrowser, [], () =>
    content.eval("location.href = 'mailto:help@example.com'")
  );

  // Wait for a few ticks:
  // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
  await new Promise(r => setTimeout(r, 100));
  // Check we only have one dialog

  let tabDialogBox = gBrowser.getTabDialogBox(tab.linkedBrowser);
  let dialogs = tabDialogBox
    .getTabDialogManager()
    ._dialogs.filter(d => d._openedURL == CONTENT_HANDLING_URL);

  is(dialogs.length, 1, "Should only have 1 dialog open");

  // Close the dialog:
  let dialogClosedPromise = waitForProtocolAppChooserDialog(
    tab.linkedBrowser,
    false
  );
  dialog.close();
  dialog = await dialogClosedPromise;

  ok(!dialog._openedURL, "The dialog should have been closed.");

  // Then reopen the dialog again, to make sure we don't keep blocking:
  dialogWindowPromise = waitForProtocolAppChooserDialog(
    tab.linkedBrowser,
    true
  );
  BrowserTestUtils.synthesizeMouseAtCenter(
    "a:link",
    {},
    tab.linkedBrowser.browsingContext.children[0]
  );
  dialog = await dialogWindowPromise;

  is(
    dialog._frame.contentDocument.location.href,
    CONTENT_HANDLING_URL,
    "Second dialog URL is as expected"
  );

  dialogClosedPromise = waitForProtocolAppChooserDialog(
    tab.linkedBrowser,
    false
  );
  info("Removing tab to close the dialog.");
  gBrowser.removeTab(tab);
  await dialogClosedPromise;
  ok(!dialog._frame.contentWindow, "The dialog should have been closed again.");
});

/**
 * Check that navigating invisible frames to external-proto URLs
 * is handled correctly.
 */
add_task(async function invisible_iframes() {
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "https://example.com/"
  );

  // Ensure we notice the dialog opening:
  let dialogWindowPromise = waitForProtocolAppChooserDialog(
    tab.linkedBrowser,
    true
  );
  await SpecialPowers.spawn(tab.linkedBrowser, [], function() {
    let frame = content.document.createElement("iframe");
    frame.style.display = "none";
    frame.src = "mailto:help@example.com";
    content.document.body.append(frame);
  });
  let dialog = await dialogWindowPromise;

  is(
    dialog._frame.contentDocument.location.href,
    CONTENT_HANDLING_URL,
    "Dialog opens as expected for invisible iframe"
  );
  // Close the dialog:
  let dialogClosedPromise = waitForProtocolAppChooserDialog(
    tab.linkedBrowser,
    false
  );
  dialog.close();
  await dialogClosedPromise;
  gBrowser.removeTab(tab);
});

/**
 * Check that nested iframes are handled correctly.
 */
add_task(async function nested_iframes() {
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "https://example.com/"
  );

  // Ensure we notice the dialog opening:
  let dialogWindowPromise = waitForProtocolAppChooserDialog(
    tab.linkedBrowser,
    true
  );
  let innerLoaded = BrowserTestUtils.browserLoaded(
    tab.linkedBrowser,
    true,
    "https://example.org/"
  );
  info("Constructing top frame");
  await SpecialPowers.spawn(tab.linkedBrowser, [], function() {
    let frame = content.document.createElement("iframe");
    frame.src = "https://example.org/"; // cross-origin frame.
    content.document.body.prepend(frame);

    content.eval(
      `window.addEventListener("message", e => e.source.location = "mailto:help@example.com");`
    );
  });

  await innerLoaded;
  let parentBC = tab.linkedBrowser.browsingContext;

  info("Creating innermost frame");
  await SpecialPowers.spawn(parentBC.children[0], [], async function() {
    let innerFrame = content.document.createElement("iframe");
    let frameLoaded = ContentTaskUtils.waitForEvent(innerFrame, "load", true);
    content.document.body.prepend(innerFrame);
    await frameLoaded;
  });

  info("Posting event from innermost frame");
  await SpecialPowers.spawn(
    parentBC.children[0].children[0],
    [],
    async function() {
      // Top browsing context needs reference to the innermost, which is cross origin.
      content.eval("top.postMessage('hello', '*')");
    }
  );

  let dialog = await dialogWindowPromise;

  is(
    dialog._frame.contentDocument.location.href,
    CONTENT_HANDLING_URL,
    "Dialog opens as expected for deeply nested cross-origin iframe"
  );
  // Close the dialog:
  let dialogClosedPromise = waitForProtocolAppChooserDialog(
    tab.linkedBrowser,
    false
  );
  dialog.close();
  await dialogClosedPromise;
  gBrowser.removeTab(tab);
});

add_task(async function test_oop_iframe() {
  const URI = `data:text/html,<div id="root"><iframe src="http://example.com/document-builder.sjs?html=<a href='mailto:help@example.com'>Mail it</a>"></iframe></div>`;

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, URI);

  // Wait for the window and then click the link.
  let dialogWindowPromise = waitForProtocolAppChooserDialog(
    tab.linkedBrowser,
    true
  );

  BrowserTestUtils.synthesizeMouseAtCenter(
    "a:link",
    {},
    tab.linkedBrowser.browsingContext.children[0]
  );

  let dialog = await dialogWindowPromise;

  is(
    dialog._frame.contentDocument.location.href,
    CONTENT_HANDLING_URL,
    "Dialog URL is as expected"
  );
  let dialogClosedPromise = waitForProtocolAppChooserDialog(
    tab.linkedBrowser,
    false
  );

  info("Removing tab to close the dialog.");
  gBrowser.removeTab(tab);
  await dialogClosedPromise;
  ok(!dialog._frame.contentWindow, "The dialog should have been closed.");
});

/**
 * Check that a cross-origin iframe can navigate the top frame
 * to an external protocol.
 */
add_task(async function xorigin_iframe_can_navigate_top() {
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "https://example.com/"
  );

  // Ensure we notice the dialog opening:
  let dialogWindowPromise = waitForProtocolAppChooserDialog(
    tab.linkedBrowser,
    true
  );
  let innerLoaded = BrowserTestUtils.browserLoaded(
    tab.linkedBrowser,
    true,
    "https://example.org/"
  );
  info("Constructing frame");
  await SpecialPowers.spawn(tab.linkedBrowser, [], function() {
    let frame = content.document.createElement("iframe");
    frame.src = "https://example.org/"; // cross-origin frame.
    content.document.body.prepend(frame);
  });
  await innerLoaded;

  info("Navigating top bc from frame");
  let parentBC = tab.linkedBrowser.browsingContext;
  await SpecialPowers.spawn(parentBC.children[0], [], async function() {
    content.eval("window.top.location.href = 'mailto:example@example.com';");
  });

  let dialog = await dialogWindowPromise;

  is(
    dialog._frame.contentDocument.location.href,
    CONTENT_HANDLING_URL,
    "Dialog opens as expected for navigating the top frame from an x-origin frame."
  );
  // Close the dialog:
  let dialogClosedPromise = waitForProtocolAppChooserDialog(
    tab.linkedBrowser,
    false
  );
  dialog.close();
  await dialogClosedPromise;
  gBrowser.removeTab(tab);
});
