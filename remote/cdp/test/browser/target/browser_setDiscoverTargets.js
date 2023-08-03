/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// These tests are a near copy of the tests for Target.getTargets, but using
// the `setDiscoverTargets` method and `targetCreated` events instead.
// Calling `setDiscoverTargets` with `discover: true` will dispatch a
// `targetCreated` event for all already opened tabs and NOT the browser target
// with the default filter.

const PAGE_TEST =
  "https://example.com/browser/remote/cdp/test/browser/target/doc_test.html";

add_task(
  async function discoverInvalidTypes({ client }) {
    const { Target } = client;

    for (const discover of [null, undefined, 1, "foo", [], {}]) {
      info(`Checking discover with invalid value: ${discover}`);

      await Assert.rejects(
        Target.setDiscoverTargets({ discover }),
        /discover: boolean value expected/,
        `Discover fails for invalid type: ${discover}`
      );
    }
  },
  { createTab: false }
);

add_task(
  async function filterInvalid({ client }) {
    const { Target } = client;

    for (const filter of [null, true, 1, "foo", {}]) {
      info(`Checking filter with invalid value: ${filter}`);

      await Assert.rejects(
        Target.setDiscoverTargets({ discover: true, filter }),
        /filter: array value expected/,
        `Filter fails for invalid type: ${filter}`
      );
    }

    for (const filterEntry of [null, undefined, true, 1, "foo", []]) {
      info(`Checking filter entry with invalid value: ${filterEntry}`);

      await Assert.rejects(
        Target.setDiscoverTargets({
          discover: true,
          filter: [filterEntry],
        }),
        /filter: object values expected in array/,
        `Filter entry fails for invalid type: ${filterEntry}`
      );
    }

    for (const type of [null, true, 1, [], {}]) {
      info(`Checking filter entry with type as invalid value: ${type}`);

      await Assert.rejects(
        Target.setDiscoverTargets({
          discover: true,
          filter: [{ type }],
        }),
        /filter: type: string value expected/,
        `Filter entry type fails for invalid type: ${type}`
      );
    }

    for (const exclude of [null, 1, "foo", [], {}]) {
      info(`Checking filter entry with exclude as invalid value: ${exclude}`);

      await Assert.rejects(
        Target.setDiscoverTargets({
          discover: true,
          filter: [{ exclude }],
        }),
        /filter: exclude: boolean value expected/,
        `Filter entry exclude for invalid type: ${exclude}`
      );
    }
  },
  { createTab: false }
);

add_task(
  async function noFilterWithDiscoverFalse({ client }) {
    const { Target } = client;

    // Check filter cannot be given with discover: false

    await Assert.rejects(
      Target.setDiscoverTargets({
        discover: false,
        filter: [{}],
      }),
      /filter: should not be present when discover is false/,
      `Error throw when given filter with discover false`
    );
  },
  { createTab: false }
);

add_task(
  async function noTargetsWithDiscoverFalse({ client }) {
    const { Target } = client;

    await loadURL(PAGE_TEST);

    const targets = await getDiscoveredTargets(Target, { discover: false });
    is(targets.length, 0, "Got 0 targets with discover false");
  },
  { createTab: false }
);

add_task(
  async function noEventsWithDiscoverFalse({ client }) {
    const { Target } = client;

    await loadURL(PAGE_TEST);

    const targets = [];
    const unsubscribe = Target.targetCreated(target => {
      targets.push(target.targetInfo);
    });

    await Target.setDiscoverTargets({
      discover: false,
    });

    // Cannot use openTab() helper as it relies on the event
    await BrowserTestUtils.openNewForegroundTab(gBrowser);

    // Wait 1s for the event to possibly dispatch
    await timeoutPromise(1000);

    unsubscribe();
    is(targets.length, 0, "Got 0 target created events with discover false");
  },
  { createTab: false }
);

add_task(
  async function targetInfoValues({ client }) {
    const { Target, target } = client;

    await loadURL(PAGE_TEST);

    const targets = await getDiscoveredTargets(Target);

    Assert.equal(targets.length, 1, "Got expected amount of targets");

    const targetInfo = targets[0];
    is(targetInfo.id, target.id, "Got expected target id");
    is(targetInfo.type, "page", "Got expected target type");
    is(targetInfo.title, "Test Page", "Got expected target title");
    is(targetInfo.url, PAGE_TEST, "Got expected target URL");
  },
  { createTab: false }
);

add_task(
  async function discoverEnabledAndMultipleTabs({ client }) {
    const { Target, target } = client;
    const { targetInfo: newTabTargetInfo } = await openTab(Target);

    await loadURL(PAGE_TEST);

    const targets = await getDiscoveredTargets(Target);

    Assert.equal(targets.length, 2, "Got expected amount of targets");
    const targetIds = targets.map(info => info.id);
    ok(targetIds.includes(target.id), "Got expected original target id");
    ok(targetIds.includes(newTabTargetInfo.id), "Got expected new target id");
  },
  { createTab: false }
);

add_task(
  async function allFilters({ client }) {
    const { Target } = client;

    await loadURL(PAGE_TEST);

    for (const filter of [[{}], [{ type: "browser" }, { type: "page" }]]) {
      // Blank/all filter so all targets are returned, including main process
      const targets = await getDiscoveredTargets(Target, { filter });

      is(targets.length, 2, "Got expected amount of targets with all filter");

      const pageTarget = targets.find(info => info.type === "page");
      ok(!!pageTarget, "Found page target in targets with all filter");

      const mainProcessTarget = targets.find(info => info.type === "browser");
      ok(
        !!mainProcessTarget,
        "Found main process target in targets with all filter"
      );
    }
  },
  { createTab: false }
);

add_task(
  async function pageOnlyFilters({ client }) {
    const { Target } = client;

    await loadURL(PAGE_TEST);

    for (const filter of [
      [{ type: "page" }],
      [{ type: "browser", exclude: true }, { type: "page" }],
    ]) {
      // Filter so only page targets are returned
      // This returns same as default but pass our own custom filter to ensure
      // these filters still return what they should
      const targets = await getDiscoveredTargets(Target, { filter });

      is(targets.length, 1, "Got expected amount of targets with page filter");
      is(
        targets[0].type,
        "page",
        "Got expected type 'page' of target from page filter"
      );
    }
  },
  { createTab: false }
);

add_task(
  async function browserOnlyFilters({ client }) {
    const { Target } = client;

    await loadURL(PAGE_TEST);

    for (const filter of [
      [{ type: "browser" }],
      [{ type: "page", exclude: true }, {}],
    ]) {
      // Filter so only main process target is returned
      const targets = await getDiscoveredTargets(Target, { filter });

      is(
        targets.length,
        1,
        "Got expected amount of targets with browser only filter"
      );
      is(
        targets[0].type,
        "browser",
        "Got expected type 'browser' of target from browser only filter"
      );
    }
  },
  { createTab: false }
);
