/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test stack.js.

function run_test() {
  let loader = new DevToolsLoader();
  let require = loader.require;

  const {StackFrameCache} = require("devtools/server/actors/utils/stack");

  let cache = new StackFrameCache();
  cache.initFrames();
  cache.addFrame({
    line: 23,
    column: 77,
    source: "nowhere",
    functionDisplayName: "nobody",
    parent: null
  });

  let event = cache.makeEvent();
  do_check_eq(event[0], null);
  do_check_eq(event[1].functionDisplayName, "nobody");
  do_check_eq(event.length, 2);
}
