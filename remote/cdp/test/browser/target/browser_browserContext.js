/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function ({ CDP }) {
  // Connect to the server
  const { webSocketDebuggerUrl } = await CDP.Version();
  const client = await CDP({ target: webSocketDebuggerUrl });
  info("CDP client has been instantiated");

  const { Target } = client;
  await getDiscoveredTargets(Target);

  // Test if Target.getBrowserContexts is empty before creatinga ny
  const { browserContextIds: browserContextIdsBefore } =
    await Target.getBrowserContexts();

  is(
    browserContextIdsBefore.length,
    0,
    "No browser context is open by default"
  );

  const { browserContextId } = await Target.createBrowserContext();

  // Test if Target.getBrowserContexts includes the context we just created
  const { browserContextIds } = await Target.getBrowserContexts();

  is(browserContextIds.length, 1, "Got expected length of browser contexts");
  is(
    browserContextIds[0],
    browserContextId,
    "Got expected browser context id from getBrowserContexts"
  );

  const targetCreated = Target.targetCreated();
  const { targetId } = await Target.createTarget({
    url: "about:blank",
    browserContextId,
  });
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

  // Test if Target.getBrowserContexts now is empty
  const { browserContextIds: browserContextIdsAfter } =
    await Target.getBrowserContexts();

  is(
    browserContextIdsAfter.length,
    0,
    "After closing all browser contexts none is available anymore"
  );

  await client.close();
  info("The client is closed");
});
