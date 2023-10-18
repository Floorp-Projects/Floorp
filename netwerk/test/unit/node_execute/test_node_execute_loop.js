// This test checks that the interaction between NodeServer.execute defined in
// httpd.js and the node server that we're interacting with defined in
// moz-http2.js is working properly.
// This test spawns a node server that loops on while true and makes sure
// the the process group is killed by runxpcshelltests.py at exit.
// See bug 1855174

"use strict";

const { NodeServer } = ChromeUtils.importESModule(
  "resource://testing-common/httpd.sys.mjs"
);

add_task(async function killOnEnd() {
  let id = await NodeServer.fork();
  await NodeServer.execute(id, `console.log("hello");`);
  await NodeServer.execute(id, `console.error("hello");`);
  // Make the forked subprocess hang forever.
  NodeServer.execute(id, "while (true) {}").catch(e => {});
  await new Promise(resolve => do_timeout(10, resolve));
  // Should get killed at the end of the test by the harness.
});
