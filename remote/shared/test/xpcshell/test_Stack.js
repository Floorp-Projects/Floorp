/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const { getFramesFromStack, isChromeFrame } = ChromeUtils.importESModule(
  "chrome://remote/content/shared/Stack.sys.mjs"
);

const sourceFrames = [
  {
    column: 1,
    functionDisplayName: "foo",
    line: 2,
    source: "cheese",
    sourceId: 1,
  },
  {
    column: 3,
    functionDisplayName: null,
    line: 4,
    source: "cake",
    sourceId: 2,
  },
  {
    column: 5,
    functionDisplayName: "chrome",
    line: 6,
    source: "chrome://foo",
    sourceId: 3,
  },
];

const targetFrames = [
  {
    columnNumber: 1,
    functionName: "foo",
    lineNumber: 2,
    filename: "cheese",
    sourceId: 1,
  },
  {
    columnNumber: 3,
    functionName: "",
    lineNumber: 4,
    filename: "cake",
    sourceId: 2,
  },
  {
    columnNumber: 5,
    functionName: "chrome",
    lineNumber: 6,
    filename: "chrome://foo",
    sourceId: 3,
  },
];

add_task(async function test_getFramesFromStack() {
  const stack = buildStack(sourceFrames);
  const frames = getFramesFromStack(stack, { includeChrome: false });

  ok(Array.isArray(frames), "frames is of expected type Array");
  equal(frames.length, 3, "Got expected amount of frames");
  checkFrame(frames.at(0), targetFrames.at(0));
  checkFrame(frames.at(1), targetFrames.at(1));
  checkFrame(frames.at(2), targetFrames.at(2));
});

add_task(async function test_getFramesFromStack_asyncStack() {
  const stack = buildStack(sourceFrames, true);
  const frames = getFramesFromStack(stack);

  ok(Array.isArray(frames), "frames is of expected type Array");
  equal(frames.length, 3, "Got expected amount of frames");
  checkFrame(frames.at(0), targetFrames.at(0));
  checkFrame(frames.at(1), targetFrames.at(1));
  checkFrame(frames.at(2), targetFrames.at(2));
});

add_task(async function test_isChromeFrame() {
  for (const filename of ["chrome://foo/bar", "resource://foo/bar"]) {
    ok(isChromeFrame({ filename }), "Frame is of expected chrome scope");
  }

  for (const filename of ["http://foo.bar", "about:blank"]) {
    ok(!isChromeFrame({ filename }), "Frame is of expected content scope");
  }
});

function buildStack(frames, async = false) {
  const parent = async ? "asyncParent" : "parent";

  let currentFrame, stack;
  for (const frame of frames) {
    if (currentFrame) {
      currentFrame[parent] = Object.assign({}, frame);
      currentFrame = currentFrame[parent];
    } else {
      stack = Object.assign({}, frame);
      currentFrame = stack;
    }
  }

  return stack;
}

function checkFrame(frame, expectedFrame) {
  equal(
    frame.columnNumber,
    expectedFrame.columnNumber,
    "Got expected column number"
  );
  equal(
    frame.functionName,
    expectedFrame.functionName,
    "Got expected function name"
  );
  equal(frame.lineNumber, expectedFrame.lineNumber, "Got expected line number");
  equal(frame.filename, expectedFrame.filename, "Got expected filename");
  equal(frame.sourceId, expectedFrame.sourceId, "Got expected source id");
}
