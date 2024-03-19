/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const { error } = ChromeUtils.importESModule(
  "chrome://remote/content/shared/messagehandler/Errors.sys.mjs"
);

// Note: this test file is similar to remote/shared/webdriver/test/xpcshell/test_Errors.js
// because shared/webdriver/Errors.sys.mjs and shared/messagehandler/Errors.sys.mjs share
// similar helpers.

add_task(function test_toJSON() {
  let e0 = new error.MessageHandlerError();
  let e0s = e0.toJSON();
  equal(e0s.error, "message handler error");
  equal(e0s.message, "");

  let e1 = new error.MessageHandlerError("a");
  let e1s = e1.toJSON();
  equal(e1s.message, e1.message);

  let e2 = new error.UnsupportedCommandError("foo");
  let e2s = e2.toJSON();
  equal(e2.status, e2s.error);
  equal(e2.message, e2s.message);
});

add_task(function test_fromJSON() {
  Assert.throws(
    () => error.MessageHandlerError.fromJSON({ error: "foo" }),
    /Not of MessageHandlerError descent/
  );
  Assert.throws(
    () => error.MessageHandlerError.fromJSON({ error: "Error" }),
    /Not of MessageHandlerError descent/
  );
  Assert.throws(
    () => error.MessageHandlerError.fromJSON({}),
    /Undeserialisable error type/
  );
  Assert.throws(
    () => error.MessageHandlerError.fromJSON(undefined),
    /TypeError/
  );

  let e1 = new error.MessageHandlerError("1");
  let e1r = error.MessageHandlerError.fromJSON({
    error: "message handler error",
    message: "1",
  });
  ok(e1r instanceof error.MessageHandlerError);
  equal(e1r.name, e1.name);
  equal(e1r.status, e1.status);
  equal(e1r.message, e1.message);

  let e2 = new error.UnsupportedCommandError("foo");
  let e2r = error.MessageHandlerError.fromJSON({
    error: "unsupported message handler command",
    message: "foo",
  });
  ok(e2r instanceof error.MessageHandlerError);
  ok(e2r instanceof error.UnsupportedCommandError);
  equal(e2r.name, e2.name);
  equal(e2r.status, e2.status);
  equal(e2r.message, e2.message);

  // parity with toJSON
  let e3 = new error.UnsupportedCommandError("foo");
  let e3toJSON = e3.toJSON();
  let e3fromJSON = error.MessageHandlerError.fromJSON(e3toJSON);
  equal(e3toJSON.error, e3fromJSON.status);
  equal(e3toJSON.message, e3fromJSON.message);
  equal(e3toJSON.stacktrace, e3fromJSON.stack);
});

add_task(function test_MessageHandlerError() {
  let err = new error.MessageHandlerError("foo");
  equal("MessageHandlerError", err.name);
  equal("foo", err.message);
  equal("message handler error", err.status);
  ok(err instanceof error.MessageHandlerError);
});

add_task(function test_UnsupportedCommandError() {
  let e = new error.UnsupportedCommandError("foo");
  equal("UnsupportedCommandError", e.name);
  equal("foo", e.message);
  equal("unsupported message handler command", e.status);
  ok(e instanceof error.MessageHandlerError);
});
