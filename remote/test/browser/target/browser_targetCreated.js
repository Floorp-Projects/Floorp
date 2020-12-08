/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function eventFiredWhenTabIsCreated({ client }) {
  const { Target } = client;

  const targetCreated = Target.targetCreated();
  await BrowserTestUtils.openNewForegroundTab(gBrowser);
  const { targetInfo } = await targetCreated;

  is(typeof targetInfo.targetId, "string", "Got expected type for target id");
  is(targetInfo.type, "page", "Got expected target type");
  is(targetInfo.title, "", "Got expected target title");
  is(targetInfo.url, "about:blank", "Got expected target URL");
  is(targetInfo.attached, false, "Got expected attached status");
});
