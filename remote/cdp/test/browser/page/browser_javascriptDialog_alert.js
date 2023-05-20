/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test a browser alert is detected via Page.javascriptDialogOpening and can be
// closed with Page.handleJavaScriptDialog
add_task(async function ({ client }) {
  const { Page } = client;

  info("Enable the page domain");
  await Page.enable();

  info("Set window.alertIsClosed to false in the content page");
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    // This boolean will be flipped after closing the dialog
    content.alertIsClosed = false;
  });

  info("Create an alert dialog again");
  const { message, type } = await createAlertDialog(Page);
  is(type, "alert", "dialog event contains the correct type");
  is(message, "test-1234", "dialog event contains the correct text");

  info("Close the dialog with accept:false");
  await Page.handleJavaScriptDialog({ accept: false });

  info("Retrieve the alertIsClosed boolean on the content window");
  let alertIsClosed = await getContentProperty("alertIsClosed");
  ok(alertIsClosed, "The content process is no longer blocked on the alert");

  info("Reset window.alertIsClosed to false in the content page");
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    content.alertIsClosed = false;
  });

  info("Create an alert dialog again");
  await createAlertDialog(Page);

  info("Close the dialog with accept:true");
  await Page.handleJavaScriptDialog({ accept: true });

  alertIsClosed = await getContentProperty("alertIsClosed");
  ok(alertIsClosed, "The content process is no longer blocked on the alert");
});

function createAlertDialog(Page) {
  const onDialogOpen = Page.javascriptDialogOpening();

  info("Trigger an alert in the test page");
  SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    content.alert("test-1234");
    // Flip a boolean in the content page to check if the content process resumed
    // after the alert was opened.
    content.alertIsClosed = true;
  });

  return onDialogOpen;
}
