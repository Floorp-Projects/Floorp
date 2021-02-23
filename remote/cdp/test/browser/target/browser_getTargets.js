/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const PAGE_TEST =
  "http://example.com/browser/remote/cdp/test/browser/target/doc_test.html";

add_task(
  async function getTargetsDetails({ client }) {
    const { Target, target } = client;

    await loadURL(PAGE_TEST);

    const { targetInfos } = await Target.getTargets();

    Assert.equal(targetInfos.length, 1, "Got expected amount of targets");

    const targetInfo = targetInfos[0];
    is(targetInfo.id, target.id, "Got expected target id");
    is(targetInfo.type, "page", "Got expected target type");
    is(targetInfo.title, "Test Page", "Got expected target title");
    is(targetInfo.url, PAGE_TEST, "Got expected target URL");
  },
  { createTab: false }
);

add_task(
  async function getTargetsCount({ client }) {
    const { Target, target } = client;
    const { targetInfo: newTabTargetInfo } = await openTab(Target);

    await loadURL(PAGE_TEST);

    const { targetInfos } = await Target.getTargets();

    Assert.equal(targetInfos.length, 2, "Got expected amount of targets");
    const targetIds = targetInfos.map(info => info.id);
    ok(targetIds.includes(target.id), "Got expected original target id");
    ok(targetIds.includes(newTabTargetInfo.id), "Got expected new target id");
  },
  { createTab: false }
);
