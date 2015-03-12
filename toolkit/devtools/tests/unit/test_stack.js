/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test stack.js.

function run_test() {
  let loader = new DevToolsLoader();
  let require = loader.require;

  const {StackFrameCache} = require("devtools/server/actors/utils/stack");

  let cache = new StackFrameCache();
  cache.initFrames();
  let baseFrame = {
    line: 23,
    column: 77,
    source: "nowhere",
    functionDisplayName: "nobody",
    parent: null,
    asyncParent: null,
    asyncCause: null
  };
  cache.addFrame(baseFrame);

  let event = cache.makeEvent();
  do_check_eq(event[0], null);
  do_check_eq(event[1].functionDisplayName, "nobody");
  do_check_eq(event.length, 2);

  cache.addFrame({
    line: 24,
    column: 78,
    source: "nowhere",
    functionDisplayName: "still nobody",
    parent: null,
    asyncParent: baseFrame,
    asyncCause: "async"
  });

  event = cache.makeEvent();
  do_check_eq(event[0].functionDisplayName, "still nobody");
  do_check_eq(event[0].parent, 0);
  do_check_eq(event[0].asyncParent, 1);
  do_check_eq(event.length, 1);
}
