/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function({ CDP }) {
  // Connect to the server
  const { webSocketDebuggerUrl } = await CDP.Version();
  const client = await CDP({ target: webSocketDebuggerUrl });
  info("CDP client has been instantiated");

  const { Target } = client;
  await getDiscoveredTargets(Target);

  const { browserContextId } = await Target.createBrowserContext();

  const targetCreated = Target.targetCreated();
  const { targetId } = await Target.createTarget({ browserContextId });
  ok(!!targetId, "Target.createTarget returns a non-empty target id");

  const { targetInfo } = await targetCreated;
  is(
    targetId,
    targetInfo.targetId,
    "targetCreated refers to the same target id"
  );
  is(
    browserContextId,
    targetInfo.browserContextId,
    "targetCreated refers to the same browser context"
  );
  is(targetInfo.type, "page", "The target is a page");

  // Releasing the browser context is going to remove the tab opened when calling createTarget
  await Target.disposeBrowserContext({ browserContextId });

  await client.close();
  info("The client is closed");
});
