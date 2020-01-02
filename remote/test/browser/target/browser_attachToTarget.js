/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function raisesWithoutArguments({ Target }, _, tab) {
  let errorThrown = false;
  try {
    await Target.attachToTarget();
  } catch (e) {
    errorThrown = true;
  }
  ok(errorThrown, "attachToTarget raised error without an argument");
});

add_task(async function raisesWithUnknownTargetId({ Target }, _, tab) {
  let errorThrown = false;
  try {
    await Target.attachToTarget({ targetId: "-1" });
  } catch (e) {
    errorThrown = true;
  }
  ok(errorThrown, "attachToTarget raised error with unkown target id");
});

add_task(
  async function attachPageTarget({ Target }) {
    const { targetInfo } = await openTab(Target);

    info("Attach new target");
    const { sessionId } = await Target.attachToTarget({
      targetId: targetInfo.targetId,
    });

    ok(!!sessionId, "attachToTarget returns a non-empty session id");
  },
  { createTab: false }
);
