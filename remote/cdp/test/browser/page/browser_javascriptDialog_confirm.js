/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for window.confirm(). Check that the dialog is correctly detected and that it can
// be rejected or accepted.
add_task(async function ({ client }) {
  const { Page } = client;

  info("Enable the page domain");
  await Page.enable();

  info("Create a confirm dialog to open");
  const { message, type } = await createConfirmDialog(Page);

  is(type, "confirm", "dialog event contains the correct type");
  is(message, "confirm-1234?", "dialog event contains the correct text");

  info("Accept the dialog");
  await Page.handleJavaScriptDialog({ accept: true });
  let isConfirmed = await getContentProperty("isConfirmed");
  ok(isConfirmed, "The confirm dialog was accepted");

  await createConfirmDialog(Page);
  info("Trigger another confirm in the test page");

  info("Reject the dialog");
  await Page.handleJavaScriptDialog({ accept: false });
  isConfirmed = await getContentProperty("isConfirmed");
  ok(!isConfirmed, "The confirm dialog was rejected");
});

function createConfirmDialog(Page) {
  const onDialogOpen = Page.javascriptDialogOpening();

  info("Trigger a confirm in the test page");
  SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    content.isConfirmed = content.confirm("confirm-1234?");
  });

  return onDialogOpen;
}
