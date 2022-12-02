"use strict";
/* global NodeHTTPServer, NodeHTTPProxyServer*/

add_task(async function run_test() {
  let proxy = new NodeHTTPProxyServer();
  await proxy.start();
  registerCleanupFunction(async () => {
    await proxy.stop();
  });

  let server = new NodeHTTPServer();

  await server.start();
  registerCleanupFunction(async () => {
    await server.stop();
  });

  await server.registerPathHandler("/test", (req, resp) => {
    let content = "content";
    resp.writeHead(200);
    resp.end(content);
  });

  do_await_remote_message("start-test").then(() => {
    do_send_remote_message(
      "start-test-done",
      `http://localhost:${server.port()}/test`
    );
  });

  run_test_in_child("child_is_proxy_used.js");
});
