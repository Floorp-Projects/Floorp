/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(
  async function attachedPageTarget({ client }) {
    const { Target } = client;
    const { targetInfo } = await openTab(Target);

    info("Attach new page target");
    const attachedToTarget = Target.attachedToTarget();
    const { sessionId } = await Target.attachToTarget({
      targetId: targetInfo.targetId,
    });
    const attachedEvent = await attachedToTarget;

    is(
      attachedEvent.sessionId,
      sessionId,
      "attachedToTarget and attachToTarget refer to the same session id"
    );
    is(
      attachedEvent.targetInfo.type,
      "page",
      "attachedToTarget refers to a tab by default"
    );
  },
  { createTab: false }
);
