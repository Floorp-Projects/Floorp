/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(
  async function attachPageTarget({ Target }) {
    await getDiscoveredTargets(Target);

    const { targetInfo } = await openTab(Target);

    info("Attach new target");
    const { sessionId } = await Target.attachToTarget({
      targetId: targetInfo.targetId,
    });

    ok(!!sessionId, "attachToTarget returns a non-empty session id");
  },
  { createTab: false }
);
