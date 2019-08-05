/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test beforeunload dialog events.
add_task(async function() {
  info("Allow to trigger onbeforeunload without user interaction");
  await new Promise(resolve => {
    const options = {
      set: [["dom.require_user_interaction_for_beforeunload", false]],
    };
    SpecialPowers.pushPrefEnv(options, resolve);
  });

  const { client, tab } = await setup();

  const { Page } = client;

  info("Enable the page domain");
  await Page.enable();

  info("Attach a valid onbeforeunload handler");
  await ContentTask.spawn(gBrowser.selectedBrowser, null, () => {
    content.onbeforeunload = () => true;
  });

  info("Trigger the beforeunload again but reject the prompt");
  const { type } = await triggerBeforeUnload(Page, tab, false);
  is(type, "beforeunload", "dialog event contains the correct type");

  info("Trigger the beforeunload again and accept the prompt");
  const onTabClose = BrowserTestUtils.waitForEvent(tab, "TabClose");
  await triggerBeforeUnload(Page, tab, true);

  info("Wait for the TabClose event");
  await onTabClose;

  await client.close();
  ok(true, "The client is closed");

  await RemoteAgent.close();
});

function triggerBeforeUnload(Page, tab, accept) {
  // We use then here because after clicking on the close button, nothing
  // in the main block of the function will be executed until the prompt
  // is accepted or rejected. Attaching a then to this promise still works.

  const onDialogOpen = Page.javascriptDialogOpening().then(
    async dialogEvent => {
      await Page.handleJavaScriptDialog({ accept });
      return dialogEvent;
    }
  );

  info("Click on the tab close icon");
  tab.closeButton.click();

  return onDialogOpen;
}
