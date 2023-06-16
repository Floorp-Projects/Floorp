/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for window.prompt(). Check that the dialog is correctly detected and that it can
// be rejected or accepted, with a custom prompt text.
add_task(async function ({ client }) {
  const { Page } = client;

  info("Enable the page domain");
  await Page.enable();

  info("Create a prompt dialog to open");
  const { message, type } = await createPromptDialog(Page);

  is(type, "prompt", "dialog event contains the correct type");
  is(message, "prompt-1234", "dialog event contains the correct text");

  info("Accept the prompt");
  await Page.handleJavaScriptDialog({ accept: true, promptText: "some-text" });

  let promptResult = await getContentProperty("promptResult");
  is(promptResult, "some-text", "The prompt text was correctly applied");

  await createPromptDialog(Page);
  info("Trigger another prompt in the test page");

  info("Reject the prompt");
  await Page.handleJavaScriptDialog({ accept: false, promptText: "new-text" });

  promptResult = await getContentProperty("promptResult");
  ok(!promptResult, "The prompt dialog was rejected");
});

function createPromptDialog(Page) {
  const onDialogOpen = Page.javascriptDialogOpening();

  info("Trigger a prompt in the test page");
  SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    content.promptResult = content.prompt("prompt-1234");
  });

  return onDialogOpen;
}
