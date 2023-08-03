/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function raisesWithoutArguments({ client, tab }) {
  const { Target } = client;

  await Assert.rejects(
    Target.closeTarget(),
    err => err.message.includes(`Unable to find target with id `),
    "closeTarget raised error without an argument"
  );
});

add_task(async function raisesWithUnknownTargetId({ client, tab }) {
  const { Target } = client;

  await Assert.rejects(
    Target.closeTarget({ targetId: "-1" }),
    err => err.message.includes(`Unable to find target with id `),
    "closeTarget raised error with unkown target id"
  );
});

add_task(async function triggersTargetDestroyed({ client, tab }) {
  const { Target } = client;
  const { targetInfo, newTab } = await openTab(Target);

  const tabClosed = BrowserTestUtils.waitForEvent(newTab, "TabClose");
  const targetDestroyed = Target.targetDestroyed();

  info("Closing the target");
  await Target.closeTarget({ targetId: targetInfo.targetId });

  await tabClosed;
  info("Tab was closed");

  await targetDestroyed;
  info("Received the Target.targetDestroyed event");
});
