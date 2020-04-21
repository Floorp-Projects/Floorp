/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const DOC = toDataURL("<div>foo</div>");
const DOC_IFRAMES = toDataURL(`
  <iframe src='data:text/html,foo'></iframe>
  <iframe src='data:text/html,bar'></iframe>
`);
const DOC_IFRAMES_NESTED = toDataURL(`
  <iframe src="data:text/html,<iframe src='data:text/html,foo'></iframe>">
  </iframe>
`);

add_task(async function pageWithoutFrames({ client }) {
  const { Page } = client;

  info("Navigate to a page without a frame");
  await loadURL(DOC);

  const { frameTree } = await Page.getFrameTree();
  ok(!!frameTree.frame, "Expected frame details found");

  const expectedFrames = await getFlattendFrameList();

  // Check top-level frame
  const expectedFrame = expectedFrames.get(frameTree.frame.id);
  is(frameTree.frame.id, expectedFrame.id, "Expected frame id found");
  is(frameTree.frame.parentId, undefined, "Parent frame doesn't exist");
  is(frameTree.frame.url, expectedFrame.url, "Expected url found");
  is(frameTree.childFrames, undefined, "No sub frames found");
});

add_task(async function PageWithFrames({ client }) {
  const { Page } = client;

  info("Navigate to a page with frames");
  await loadURL(DOC_IFRAMES);

  const { frameTree } = await Page.getFrameTree();
  ok(!!frameTree.frame, "Expected frame details found");

  const expectedFrames = await getFlattendFrameList();

  let frame = frameTree.frame;
  let expectedFrame = expectedFrames.get(frame.id);

  console.log(`Check top frame with id: ${frame.id}`);
  is(frame.id, expectedFrame.id, "Expected frame id found");
  is(frame.parentId, undefined, "Parent frame doesn't exist");
  is(frame.url, expectedFrame.url, "Expected URL found");

  is(frameTree.childFrames.length, 2, "Expected two sub frames");
  for (const childFrameTree of frameTree.childFrames) {
    let frame = childFrameTree.frame;
    let expectedFrame = expectedFrames.get(frame.id);

    console.log(`Check sub frame with id: ${frame.id}`);
    is(frame.id, expectedFrame.id, "Expected frame id found");
    is(frame.parentId, expectedFrame.parentId, "Expected parent id found");
    is(frame.url, expectedFrame.url, "Expected URL found");
    is(childFrameTree.childFrames, undefined, "No sub frames found");
  }
});

add_task(async function pageWithNestedFrames({ client }) {
  const { Page } = client;

  info("Navigate to a page with nested frames");
  await loadURL(DOC_IFRAMES_NESTED);

  const { frameTree } = await Page.getFrameTree();
  ok(!!frameTree.frame, "Expected frame details found");

  const expectedFrames = await getFlattendFrameList();

  let frame = frameTree.frame;
  let expectedFrame = expectedFrames.get(frame.id);

  console.log(`Check top frame with id: ${frame.id}`);
  is(frame.id, expectedFrame.id, "Expected frame id found");
  is(frame.parentId, undefined, "Parent frame doesn't exist");
  is(frame.url, expectedFrame.url, "Expected URL found");
  is(frameTree.childFrames.length, 1, "Expected a single sub frame");

  let childFrameTree = frameTree.childFrames[0];
  frame = childFrameTree.frame;
  expectedFrame = expectedFrames.get(frame.id);

  console.log(`Check sub frame with id: ${frame.id}`);
  is(frame.id, expectedFrame.id, "Expected frame id found");
  is(frame.parentId, expectedFrame.parentId, "Expected parent id found");
  is(frame.url, expectedFrame.url, "Expected URL found");
  is(frameTree.childFrames.length, 1, "Expected a single sub frame");

  childFrameTree = childFrameTree.childFrames[0];
  frame = childFrameTree.frame;
  expectedFrame = expectedFrames.get(frame.id);

  console.log(`Check nested frame with id: ${frame.id}`);
  is(frame.id, expectedFrame.id, "Expected frame id found");
  is(frame.parentId, expectedFrame.parentId, "Expected parent id found");
  is(frame.url, expectedFrame.url, "Expected URL found");
  is(childFrameTree.childFrames, undefined, "No sub frames found");
});
