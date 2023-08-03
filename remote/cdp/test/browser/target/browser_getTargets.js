/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const PAGE_TEST =
  "https://example.com/browser/remote/cdp/test/browser/target/doc_test.html";

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
    ok(targetInfo.attached, "Got expected target attached status");
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

add_task(
  async function getTargetsAttached({ client }) {
    const { Target } = client;
    await openTab(Target);

    await loadURL(PAGE_TEST);

    const { targetInfos } = await Target.getTargets();

    ok(targetInfos[0].attached, "Current target is attached");
    ok(!targetInfos[1].attached, "New tab target is detached");
  },
  { createTab: false }
);

add_task(
  async function getTargets_filterAllBlank({ client }) {
    const { Target } = client;

    await loadURL(PAGE_TEST);

    // Blank/all filter so all targets are returned, including main process
    const { targetInfos } = await Target.getTargets({
      filter: [{}],
    });

    is(
      targetInfos.length,
      2,
      "Got expected amount of targets with all (blank) filter"
    );

    const pageTarget = targetInfos.find(info => info.type === "page");
    ok(!!pageTarget, "Found page target in targets with all (blank) filter");

    const mainProcessTarget = targetInfos.find(info => info.type === "browser");
    ok(
      !!mainProcessTarget,
      "Found main process target in targets with all (blank) filter"
    );
  },
  { createTab: false }
);

add_task(
  async function getTargets_filterAllExplicit({ client }) {
    const { Target } = client;

    await loadURL(PAGE_TEST);

    // Blank/all filter so all targets are returned, including main process
    const { targetInfos } = await Target.getTargets({
      filter: [{ type: "browser" }, { type: "page" }],
    });

    is(
      targetInfos.length,
      2,
      "Got expected amount of targets with all (explicit) filter"
    );

    const pageTarget = targetInfos.find(info => info.type === "page");
    ok(!!pageTarget, "Found page target in targets with all (explicit) filter");

    const mainProcessTarget = targetInfos.find(info => info.type === "browser");
    ok(
      !!mainProcessTarget,
      "Found main process target in targets with all (explicit) filter"
    );
  },
  { createTab: false }
);

add_task(
  async function getTargets_filterPage({ client }) {
    const { Target } = client;

    await loadURL(PAGE_TEST);

    // Filter so only page targets are returned
    // This returns same as default but pass our own custom filter to ensure
    const { targetInfos } = await Target.getTargets({
      filter: [{ type: "page" }],
    });

    is(
      targetInfos.length,
      1,
      "Got expected amount of targets with page filter"
    );
    is(
      targetInfos[0].type,
      "page",
      "Got expected type 'page' of target from page filter"
    );
  },
  { createTab: false }
);

add_task(
  async function getTargets_filterBrowser({ client }) {
    const { Target } = client;

    await loadURL(PAGE_TEST);

    // Filter so only main process target is returned
    const { targetInfos } = await Target.getTargets({
      filter: [{ type: "browser" }],
    });

    is(
      targetInfos.length,
      1,
      "Got expected amount of targets with browser filter"
    );
    is(
      targetInfos[0].type,
      "browser",
      "Got expected type 'browser' of target from browser filter"
    );
  },
  { createTab: false }
);

add_task(
  async function getTargets_filterExcludePage({ client }) {
    const { Target } = client;

    await loadURL(PAGE_TEST);

    // Filter so page targets are excluded (so only main process target is returned)
    // A blank object ({}) means include everything else
    const { targetInfos } = await Target.getTargets({
      filter: [{ type: "page", exclude: true }, {}],
    });

    is(
      targetInfos.length,
      1,
      "Got expected amount of targets with exclude page filter"
    );
    is(
      targetInfos[0].type,
      "browser",
      "Got expected type 'browser' of target from exclude page filter"
    );
  },
  { createTab: false }
);

add_task(
  async function getTargets_filterExcludeBrowserIncludePage({ client }) {
    const { Target } = client;

    await loadURL(PAGE_TEST);

    // Filter so main process is excluded and only page types are returned explicitly
    const { targetInfos } = await Target.getTargets({
      filter: [{ type: "browser", exclude: true }, { type: "page" }],
    });

    is(
      targetInfos.length,
      1,
      "Got expected amount of targets with exclude browser include page filter"
    );
    is(
      targetInfos[0].type,
      "page",
      "Got expected type 'page' of target from exclude browser include page filter"
    );
  },
  { createTab: false }
);

add_task(
  async function getTargets_filterInvalid({ client }) {
    const { Target } = client;

    for (const filter of [null, true, 1, "foo", {}]) {
      info(`Checking filter with invalid value: ${filter}`);

      await Assert.rejects(
        Target.getTargets({
          filter,
        }),
        /filter: array value expected/,
        `Filter fails for invalid type: ${filter}`
      );
    }

    for (const filterEntry of [null, true, 1, "foo", []]) {
      info(`Checking filter entry with invalid value: ${filterEntry}`);

      await Assert.rejects(
        Target.getTargets({
          filter: [filterEntry],
        }),
        /filter: object values expected in array/,
        `Filter entry fails for invalid type: ${filterEntry}`
      );
    }

    for (const type of [null, true, 1, [], {}]) {
      info(`Checking filter entry with type as invalid value: ${type}`);

      await Assert.rejects(
        Target.getTargets({
          filter: [{ type }],
        }),
        /filter: type: string value expected/,
        `Filter entry type fails for invalid type: ${type}`
      );
    }

    for (const exclude of [null, 1, "foo", [], {}]) {
      info(`Checking filter entry with exclude as invalid value: ${exclude}`);

      await Assert.rejects(
        Target.getTargets({
          filter: [{ exclude }],
        }),
        /filter: exclude: boolean value expected/,
        `Filter entry exclude for invalid type: ${exclude}`
      );
    }
  },
  { createTab: false }
);
