/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(
  async function mainTarget({ client }) {
    const { Target } = client;
    const { targetInfo } = await new Promise(resolve => {
      const unsubscribe = Target.targetCreated(target => {
        if (target.targetInfo.type == "browser") {
          unsubscribe();
          resolve(target);
        }
      });

      // Calling `setDiscoverTargets` will dispatch `targetCreated` event for all
      // already opened tabs and the browser target.
      Target.setDiscoverTargets({ discover: true });
    });

    ok(!!targetInfo, "Target info for main target has been found");
    ok(!!targetInfo.targetId, "Main target has a non-empty target id");
    is(targetInfo.type, "browser", "Type of target is browser");
  },
  { createTab: false }
);

add_task(async function pageTargets({ client, tab }) {
  const { Target, target } = client;
  is(
    target.browsingContextId,
    tab.linkedBrowser.browsingContext.id,
    "Current target has expected browsing context id"
  );
  const currentTargetId = target.id;
  const url = toDataURL("pageTargets");
  await loadURL(url);

  const targets = await new Promise(resolve => {
    const targets = [];

    const unsubscribe = Target.targetCreated(target => {
      if (target.targetInfo.type == "page") {
        targets.push(target);

        // Return once all page targets have been discovered
        if (targets.length == gBrowser.tabs.length) {
          unsubscribe();
          resolve(targets);
        }
      }
    });

    // Calling `setDiscoverTargets` will dispatch `targetCreated` event for all
    // already opened tabs and the browser target.
    Target.setDiscoverTargets({ discover: true });
  });

  // Get the current target
  const filtered_targets = targets.filter(target => {
    return target.targetInfo.targetId == currentTargetId;
  });
  is(filtered_targets.length, 1, "The current target has been found");
  const { targetInfo } = filtered_targets[0];

  ok(!!targetInfo, "Target info for current tab has been found");
  ok(!!targetInfo.targetId, "Page target has a non-empty target id");
  is(targetInfo.type, "page", "Type of current target is 'page'");
  is(targetInfo.url, url, "Page target has a non-empty target id");
});
