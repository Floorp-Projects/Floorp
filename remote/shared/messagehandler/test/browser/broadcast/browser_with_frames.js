/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_broadcasting_with_frames() {
  info("Navigate the initial tab to the test URL");
  const tab = gBrowser.selectedTab;

  // Create a test page with 2 iframes:
  // - one with a different eTLD+1 (example.com)
  // - one with a nested iframe on a different eTLD+1 (example.net)
  //
  // Overall the document structure should look like:
  //
  // html (example.org)
  //   iframe (example.org)
  //     iframe (example.net)
  //   iframe(example.com)
  //
  // Which means we should have 4 browsing contexts in total.

  // Create the markup for an example.net frame nested in an example.com frame.
  const NESTED_FRAME_MARKUP = createFrameForUri(
    `http://example.org/document-builder.sjs?html=${createFrame("example.net")}`
  );

  // Combine the nested frame markup created above with an example.com frame.
  const TEST_URI_MARKUP = `${NESTED_FRAME_MARKUP}${createFrame("example.com")}`;

  // Create the test page URI on example.org.
  const TEST_URI = `http://example.org/document-builder.sjs?html=${encodeURI(
    TEST_URI_MARKUP
  )}`;

  await loadURL(tab.linkedBrowser, TEST_URI);

  const contexts = tab.linkedBrowser.browsingContext.getAllBrowsingContextsInSubtree();
  is(contexts.length, 4, "Test tab has 3 children contexts");

  const rootMessageHandler = createRootMessageHandler(
    "session-id-broadcasting_with_frames"
  );
  const broadcastValue = await sendTestBroadcastCommand(
    "TestOnlyInWindowGlobalModule",
    "testBroadcast",
    {},
    rootMessageHandler
  );

  ok(
    Array.isArray(broadcastValue),
    "The broadcast returned an array of values"
  );
  is(broadcastValue.length, 4, "The broadcast returned 4 values as expected");

  for (const context of contexts) {
    ok(
      broadcastValue.includes("broadcast-" + context.id),
      "The broadcast contains the value for browsing context " + context.id
    );
  }

  rootMessageHandler.destroy();
});

/**
 * Create inline markup for a simple iframe that can be used with
 * document-builder.sjs. The iframe will be served under the provided domain.
 *
 * @param {String} domain
 *     A domain (eg "example.com"), compatible with build/pgo/server-locations.txt
 */
function createFrame(domain) {
  return createFrameForUri(
    `http://${domain}/document-builder.sjs?html=frame-${domain}`
  );
}

function createFrameForUri(uri) {
  return `<iframe src="${encodeURI(uri)}"></iframe>`;
}
