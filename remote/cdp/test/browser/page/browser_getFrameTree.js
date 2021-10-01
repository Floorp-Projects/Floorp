/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function pageWithoutFrames({ client }) {
  const { Page } = client;

  info("Navigate to a page without a frame");
  await loadURL(PAGE_URL);

  const { frameTree } = await Page.getFrameTree();
  ok(!!frameTree.frame, "Expected frame details found");

  const expectedFrames = await getFlattenedFrameList();

  // Check top-level frame
  const expectedFrame = expectedFrames.get(frameTree.frame.id);
  is(frameTree.frame.id, expectedFrame.id, "Expected frame id found");
  is(frameTree.frame.parentId, undefined, "Parent frame doesn't exist");
  is(frameTree.name, undefined, "Top frame doens't contain name property");
  is(frameTree.frame.url, expectedFrame.url, "Expected url found");
  is(frameTree.childFrames, undefined, "No sub frames found");
});

add_task(async function PageWithFrames({ client }) {
  const { Page } = client;

  info("Navigate to a page with frames");
  await loadURL(FRAMESET_MULTI_URL);

  const { frameTree } = await Page.getFrameTree();
  ok(!!frameTree.frame, "Expected frame details found");

  const expectedFrames = await getFlattenedFrameList();

  let frame = frameTree.frame;
  let expectedFrame = expectedFrames.get(frame.id);

  info(`Check top frame with id: ${frame.id}`);
  is(frame.id, expectedFrame.id, "Expected frame id found");
  is(frame.parentId, undefined, "Parent frame doesn't exist");
  is(frame.name, undefined, "Top frame doesn't contain name property");
  is(frame.url, expectedFrame.url, "Expected URL found");

  is(frameTree.childFrames.length, 2, "Expected two sub frames");
  for (const childFrameTree of frameTree.childFrames) {
    let frame = childFrameTree.frame;
    let expectedFrame = expectedFrames.get(frame.id);

    info(`Check sub frame with id: ${frame.id}`);
    is(frame.id, expectedFrame.id, "Expected frame id found");
    is(frame.parentId, expectedFrame.parentId, "Expected parent id found");
    is(frame.name, expectedFrame.name, "Frame has expected name set");
    is(frame.url, expectedFrame.url, "Expected URL found");
    is(childFrameTree.childFrames, undefined, "No sub frames found");
  }
});

add_task(async function pageWithNestedFrames({ client }) {
  const { Page } = client;

  info("Navigate to a page with nested frames");
  await loadURL(FRAMESET_NESTED_URL);

  const { frameTree } = await Page.getFrameTree();
  ok(!!frameTree.frame, "Expected frame details found");

  const expectedFrames = await getFlattenedFrameList();

  let frame = frameTree.frame;
  let expectedFrame = expectedFrames.get(frame.id);

  info(`Check top frame with id: ${frame.id}`);
  is(frame.id, expectedFrame.id, "Expected frame id found");
  is(frame.parentId, undefined, "Parent frame doesn't exist");
  is(frame.name, undefined, "Top frame doesn't contain name property");
  is(frame.url, expectedFrame.url, "Expected URL found");
  is(frameTree.childFrames.length, 1, "Expected a single sub frame");

  const childFrameTree = frameTree.childFrames[0];
  frame = childFrameTree.frame;
  expectedFrame = expectedFrames.get(frame.id);

  info(`Check sub frame with id: ${frame.id}`);
  is(frame.id, expectedFrame.id, "Expected frame id found");
  is(frame.parentId, expectedFrame.parentId, "Expected parent id found");
  is(frame.name, expectedFrame.name, "Frame has expected name set");
  is(frame.url, expectedFrame.url, "Expected URL found");
  is(childFrameTree.childFrames.length, 2, "Expected two sub frames");

  let nestedChildFrameTree = childFrameTree.childFrames[0];
  frame = nestedChildFrameTree.frame;
  expectedFrame = expectedFrames.get(frame.id);

  info(`Check first nested frame with id: ${frame.id}`);
  is(frame.id, expectedFrame.id, "Expected frame id found");
  is(frame.parentId, expectedFrame.parentId, "Expected parent id found");
  is(frame.name, expectedFrame.name, "Frame has expected name set");
  is(frame.url, expectedFrame.url, "Expected URL found");
  is(nestedChildFrameTree.childFrames, undefined, "No sub frames found");

  nestedChildFrameTree = childFrameTree.childFrames[1];
  frame = nestedChildFrameTree.frame;
  expectedFrame = expectedFrames.get(frame.id);

  info(`Check second nested frame with id: ${frame.id}`);
  is(frame.id, expectedFrame.id, "Expected frame id found");
  is(frame.parentId, expectedFrame.parentId, "Expected parent id found");
  is(frame.name, expectedFrame.name, "Frame has expected name set");
  is(frame.url, expectedFrame.url, "Expected URL found");
  is(nestedChildFrameTree.childFrames, undefined, "No sub frames found");
});

/**
 * Retrieve all frames for the current tab as flattened list.
 */
function getFlattenedFrameList() {
  return SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    const frames = new Map();

    function getFrameDetails(context) {
      const frameElement = context.embedderElement;

      const frame = {
        id: context.id.toString(),
        parentId: context.parent ? context.parent.id.toString() : null,
        loaderId: null,
        name: frameElement?.id || frameElement?.name,
        url: context.docShell.domWindow.location.href,
        securityOrigin: null,
        mimeType: null,
      };

      if (context.parent) {
        frame.parentId = context.parent.id.toString();
      }

      frames.set(context.id.toString(), frame);

      for (const childContext of context.children) {
        getFrameDetails(childContext);
      }
    }

    getFrameDetails(content.docShell.browsingContext);
    return frames;
  });
}
