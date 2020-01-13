/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function eventFiredWhenTabIsClosed({ client, tab }) {
  const { Target } = client;
  const { newTab } = await openTab(Target);

  const tabClosed = BrowserTestUtils.waitForEvent(newTab, "TabClose");
  const targetDestroyed = Target.targetDestroyed();

  info("Closing the tab");
  BrowserTestUtils.removeTab(newTab);

  await tabClosed;
  info("Tab was closed");

  await targetDestroyed;
  info("Received the Target.targetDestroyed event");
});
