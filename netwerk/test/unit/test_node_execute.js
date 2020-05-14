// This test checks that the interaction between NodeServer.execute defined in
// httpd.js and the node server that we're interacting with defined in
// moz-http2.js is working properly.
/* global my_defined_var */

"use strict";

const { NodeServer } = ChromeUtils.import("resource://testing-common/httpd.js");

add_task(async function test_execute() {
  function f() {
    return "bla";
  }
  let id = await NodeServer.fork();
  equal(await NodeServer.execute(id, `"hello"`), "hello");
  equal(await NodeServer.execute(id, `(() => "hello")()`), "hello");
  equal(await NodeServer.execute(id, `my_defined_var = 1;`), 1);
  equal(await NodeServer.execute(id, `(() => my_defined_var)()`), 1);
  equal(await NodeServer.execute(id, `my_defined_var`), 1);

  await NodeServer.execute(id, `not_defined_var`)
    .then(() => {
      ok(false, "should have thrown");
    })
    .catch(e => {
      equal(e.message, "ReferenceError: not_defined_var is not defined");
      ok(
        e.stack.includes("moz-http2-child.js"),
        `stack should be coming from moz-http2-child.js - ${e.stack}`
      );
    });
  await NodeServer.execute("definitely_wrong_id", `"hello"`)
    .then(() => {
      ok(false, "should have thrown");
    })
    .catch(e => {
      equal(e.message, "Error: could not find id");
      ok(
        e.stack.includes("moz-http2.js"),
        `stack should be coming from moz-http2.js - ${e.stack}`
      );
    });

  // Defines f in the context of the node server.
  // The implementation of NodeServer.execute prepends `functionName =` to the
  // body of the function we pass so it gets attached to the global context
  // in the server.
  equal(await NodeServer.execute(id, f), undefined);
  equal(await NodeServer.execute(id, `f()`), "bla");

  class myClass {
    static doStuff() {
      return my_defined_var;
    }
  }

  equal(await NodeServer.execute(id, myClass), undefined);
  equal(await NodeServer.execute(id, `myClass.doStuff()`), 1);

  equal(await NodeServer.kill(id), undefined);
  await NodeServer.execute(id, `f()`)
    .then(() => ok(false, "should throw"))
    .catch(e => equal(e.message, "Error: could not find id"));
  id = await NodeServer.fork();
  // Check that a child process dying during a call throws an error.
  await NodeServer.execute(id, `process.exit()`)
    .then(() => ok(false, "should throw"))
    .catch(e =>
      equal(e.message, "child process exit closing code: 0 signal: null")
    );

  id = await NodeServer.fork();
  equal(
    await NodeServer.execute(
      id,
      `setTimeout(function() { sendBackResponse(undefined) }, 0); 2`
    ),
    2
  );
  await new Promise(resolve => do_timeout(10, resolve));
  await NodeServer.kill(id)
    .then(() => ok(false, "should throw"))
    .catch(e =>
      equal(
        e.message,
        `forked process without handler sent: {"error":"","errorStack":""}\n`
      )
    );
});
