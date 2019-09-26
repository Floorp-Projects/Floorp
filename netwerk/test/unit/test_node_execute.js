// This test checks that the interaction between NodeServer.execute defined in
// httpd.js and the node server that we're interacting with defined in
// moz-http2.js is working properly.

"use strict";

const { NodeServer } = ChromeUtils.import("resource://testing-common/httpd.js");

add_task(async function test_execute() {
  function f() {
    return "bla";
  }
  equal(await NodeServer.execute(`"hello"`), "hello");
  equal(await NodeServer.execute(`(() => "hello")()`), "hello");
  equal(await NodeServer.execute(`my_defined_var = 0;`), 0);
  equal(await NodeServer.execute(`(() => my_defined_var)()`), 0);
  equal(await NodeServer.execute(`my_defined_var`), 0);

  await NodeServer.execute(`not_defined_var`)
    .then(() => {
      ok(false, "should have thrown");
    })
    .catch(e => {
      equal(e.message, "ReferenceError: not_defined_var is not defined");
      ok(
        e.stack.includes("moz-http2.js"),
        `stack should ve coming from moz-http2.js - ${e.stack}`
      );
    });
  equal(await NodeServer.execute(f), undefined);
  equal(await NodeServer.execute(`f()`), "bla");
  let result = await NodeServer.execute(`startNewProxy()`);
  equal(result.success, true);
  info(JSON.stringify(result));
  equal(await NodeServer.execute(`closeProxy("${result.name}")`), undefined);
});
