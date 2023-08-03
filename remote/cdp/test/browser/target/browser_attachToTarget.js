/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function raisesWithoutArguments({ client, tab }) {
  const { Target } = client;

  await Assert.rejects(
    Target.attachToTarget(),
    err => err.message.includes(`Unable to find target with id`),
    "attachToTarget raised error without an argument"
  );
});

add_task(async function raisesWithUnknownTargetId({ client, tab }) {
  const { Target } = client;

  await Assert.rejects(
    Target.attachToTarget({ targetId: "-1" }),
    err => err.message.includes(`Unable to find target with id`),
    "attachToTarget raised error with unkown target id"
  );
});

add_task(
  async function attachPageTarget({ client }) {
    const { Target } = client;
    const { targetInfo } = await openTab(Target);

    ok(!targetInfo.attached, "New target is not attached");

    info("Attach new target");
    const { sessionId } = await Target.attachToTarget({
      targetId: targetInfo.targetId,
    });

    is(
      typeof sessionId,
      "string",
      "attachToTarget returns the session id as string"
    );

    const { targetInfos } = await Target.getTargets();
    const listedTarget = targetInfos.find(
      info => info.targetId === targetInfo.targetId
    );

    ok(listedTarget.attached, "New target is attached");
  },
  { createTab: false }
);
