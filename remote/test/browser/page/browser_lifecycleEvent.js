/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the Page lifecycle events

const DOC = toDataURL("default-test-page");

add_task(async function noInitialEvents({ client }) {
  const { Page } = client;
  await Page.enable();
  info("Page domain has been enabled");

  const promise = recordPromises(Page, ["init", "DOMContentLoaded", "load"]);
  info("Lifecycle events are not enabled");

  let pageLoaded = Page.loadEventFired();
  const { frameId } = await Page.navigate({ url: DOC });
  await pageLoaded;
  info("A new page has been loaded");

  await assertNavigationLifecycleEvents({ promise, frameId, timeout: 1000 });
});

add_task(async function noEventsAfterDisable({ client }) {
  const { Page } = client;
  await Page.enable();
  info("Page domain has been enabled");

  await Page.setLifecycleEventsEnabled({ enabled: true });
  await Page.setLifecycleEventsEnabled({ enabled: false });
  const promise = recordPromises(Page, ["init", "DOMContentLoaded", "load"]);
  info("Lifecycle events are not enabled");

  let pageLoaded = Page.loadEventFired();
  const { frameId } = await Page.navigate({ url: DOC });
  await pageLoaded;
  info("A new page has been loaded");

  await assertNavigationLifecycleEvents({ promise, frameId, timeout: 1000 });
});

add_task(async function navigateEvents({ client }) {
  const { Page } = client;
  await Page.enable();
  info("Page domain has been enabled");

  await Page.setLifecycleEventsEnabled({ enabled: true });
  const promise = recordPromises(Page, ["init", "DOMContentLoaded", "load"]);
  info("Lifecycle events have been enabled");

  let pageLoaded = Page.loadEventFired();
  const { frameId } = await Page.navigate({ url: DOC });
  await pageLoaded;
  info("A new page has been loaded");

  await assertNavigationLifecycleEvents({ promise, frameId });
});

add_task(async function navigateEventsOnReload({ client }) {
  const { Page } = client;
  await Page.enable();
  info("Page domain has been enabled");

  let pageLoaded = Page.loadEventFired();
  const { frameId } = await Page.navigate({ url: DOC });
  await pageLoaded;
  info("Initial page has been loaded");

  await Page.setLifecycleEventsEnabled({ enabled: true });
  const promise = recordPromises(Page, ["init", "DOMContentLoaded", "load"]);
  info("Lifecycle events have been enabled");

  pageLoaded = Page.loadEventFired();
  await Page.reload();
  await pageLoaded;
  info("The page has been reloaded");

  await assertNavigationLifecycleEvents({ promise, frameId });
});

add_task(async function navigateEventsOnNavigateToSameURL({ client }) {
  const { Page } = client;
  await Page.enable();
  info("Page domain has been enabled");

  let pageLoaded = Page.loadEventFired();
  const { frameId } = await Page.navigate({ url: DOC });
  await pageLoaded;
  info("Initial page has been loaded");

  await Page.setLifecycleEventsEnabled({ enabled: true });
  const promise = recordPromises(Page, ["init", "DOMContentLoaded", "load"]);
  info("Lifecycle events have been enabled");

  pageLoaded = Page.loadEventFired();
  await Page.navigate({ url: DOC });
  await pageLoaded;
  info("The page has been reloaded");

  await assertNavigationLifecycleEvents({ promise, frameId });
});

function recordPromises(Page, names) {
  return new Promise(resolve => {
    const resolutions = new Map();

    const unsubscribe = Page.lifecycleEvent(event => {
      info(`Received Page.lifecycleEvent for ${event.name}`);
      resolutions.set(event.name, event);

      if (event.name == names[names.length - 1]) {
        unsubscribe();
        resolve(resolutions);
      }
    });
  });
}

async function assertNavigationLifecycleEvents({ promise, frameId, timeout }) {
  // Wait for all the promises to resolve
  const promises = [promise];

  if (timeout) {
    promises.push(timeoutPromise(timeout));
  }

  const resolutions = await Promise.race(promises);

  if (timeout) {
    is(resolutions, undefined, "No lifecycle events have been recorded");
    return;
  }

  // Assert the order in which they resolved
  const expectedResolutions = ["init", "DOMContentLoaded", "load"];
  Assert.deepEqual(
    [...resolutions.keys()],
    expectedResolutions,
    "Received various lifecycle events in the expected order"
  );

  // Now assert the data exposed by each of these events
  const frameStartedLoading = resolutions.get("init");
  is(frameStartedLoading.frameId, frameId, "init frameId is the same one");

  const DOMContentLoaded = resolutions.get("DOMContentLoaded");
  is(
    DOMContentLoaded.frameId,
    frameId,
    "DOMContentLoaded frameId is the same one"
  );

  const load = resolutions.get("load");
  is(load.frameId, frameId, "load frameId is the same one");
}
