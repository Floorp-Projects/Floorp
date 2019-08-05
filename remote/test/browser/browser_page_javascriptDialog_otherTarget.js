/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const FIRST_DOC = toDataURL("default-test-page");
const SECOND_DOC = toDataURL("other-test-page");

// Test that javascript dialog events are emitted by the page domain only if
// the dialog is created for the window of the target.
add_task(async function() {
  const { client, tab } = await setupForURL(FIRST_DOC);

  const { Page } = client;

  info("Enable the page domain");
  await Page.enable();

  // Add a listener for dialogs on the test page.
  Page.javascriptDialogOpening(() => {
    ok(false, "Should never receive this event");
  });

  info("Open another tab");
  const otherTab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    SECOND_DOC
  );
  is(gBrowser.selectedTab, otherTab, "Selected tab is now the new tab");

  // Create a promise that resolve when dialog prompt is created.
  // It will also take care of closing the dialog.
  const onOtherPageDialog = new Promise(r => {
    Services.obs.addObserver(function onDialogLoaded(promptContainer) {
      Services.obs.removeObserver(onDialogLoaded, "tabmodal-dialog-loaded");
      promptContainer.querySelector(".tabmodalprompt-button0").click();
      r();
    }, "tabmodal-dialog-loaded");
  });

  info("Trigger an alert in the second page");
  ContentTask.spawn(gBrowser.selectedBrowser, null, () => {
    content.alert("test");
  });

  info("Wait for the alert to be detected and closed");
  await onOtherPageDialog;

  info("Call bringToFront on the test page to make sure we received");
  await Page.bringToFront();

  BrowserTestUtils.removeTab(otherTab);
  await client.close();
  ok(true, "The client is closed");

  BrowserTestUtils.removeTab(tab);
  await RemoteAgent.close();
});
