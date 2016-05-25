/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const {utils: Cu} = Components;

Cu.import("chrome://marionette/content/error.js");

function notok(condition) {
  ok(!(condition));
}

add_test(function test_BuiltinErrors() {
  ok("Error" in error.BuiltinErrors);
  ok("EvalError" in error.BuiltinErrors);
  ok("InternalError" in error.BuiltinErrors);
  ok("RangeError" in error.BuiltinErrors);
  ok("ReferenceError" in error.BuiltinErrors);
  ok("SyntaxError" in error.BuiltinErrors);
  ok("TypeError" in error.BuiltinErrors);
  ok("URIError" in error.BuiltinErrors);

  run_next_test();
});

add_test(function test_isError() {
  notok(error.isError(null));
  notok(error.isError([]));
  notok(error.isError(new Date()));

  ok(error.isError(new Components.Exception()));
  ok(error.isError(new Error()));
  ok(error.isError(new EvalError()));
  ok(error.isError(new InternalError()));
  ok(error.isError(new RangeError()));
  ok(error.isError(new ReferenceError()));
  ok(error.isError(new SyntaxError()));
  ok(error.isError(new TypeError()));
  ok(error.isError(new URIError()));
  ok(error.isError(new WebDriverError()));
  ok(error.isError(new InvalidArgumentError()));

  run_next_test();
});

add_test(function test_isWebDriverError() {
  notok(error.isWebDriverError(new Components.Exception()));
  notok(error.isWebDriverError(new Error()));
  notok(error.isWebDriverError(new EvalError()));
  notok(error.isWebDriverError(new InternalError()));
  notok(error.isWebDriverError(new RangeError()));
  notok(error.isWebDriverError(new ReferenceError()));
  notok(error.isWebDriverError(new SyntaxError()));
  notok(error.isWebDriverError(new TypeError()));
  notok(error.isWebDriverError(new URIError()));

  ok(error.isWebDriverError(new WebDriverError()));
  ok(error.isWebDriverError(new InvalidArgumentError()));

  run_next_test();
});

add_test(function test_wrap() {
  equal(error.wrap(new WebDriverError()).name, "WebDriverError");
  equal(error.wrap(new InvalidArgumentError()).name, "InvalidArgumentError");
  equal(error.wrap(new Error()).name, "WebDriverError");
  equal(error.wrap(new EvalError()).name, "WebDriverError");
  equal(error.wrap(new InternalError()).name, "WebDriverError");
  equal(error.wrap(new RangeError()).name, "WebDriverError");
  equal(error.wrap(new ReferenceError()).name, "WebDriverError");
  equal(error.wrap(new SyntaxError()).name, "WebDriverError");
  equal(error.wrap(new TypeError()).name, "WebDriverError");
  equal(error.wrap(new URIError()).name, "WebDriverError");

  run_next_test();
});

add_test(function test_stringify() {
  equal("<unprintable error>", error.stringify());
  equal("<unprintable error>", error.stringify("foo"));
  equal("[object Object]", error.stringify({}));
  equal("[object Object]\nfoo", error.stringify({stack: "foo"}));
  equal("Error: foo", error.stringify(new Error("foo")).split("\n")[0]);
  equal("WebDriverError: foo",
      error.stringify(new WebDriverError("foo")).split("\n")[0]);
  equal("InvalidArgumentError: foo",
      error.stringify(new InvalidArgumentError("foo")).split("\n")[0]);

  run_next_test();
});

add_test(function test_toJson() {
  Assert.throws(() => error.toJson(new Error()),
      /Unserialisable error type: [object Error]/);

  let e0 = new WebDriverError();
  deepEqual({error: e0.status, message: "", stacktrace: ""},
      error.toJson(e0));

  let e1 = new WebDriverError("a");
  deepEqual({error: e1.status, message: "a", stacktrace: ""},
      error.toJson(e1));

  let e2 = new JavaScriptError("first", "second", "third", "fourth");
  let e2s = error.toJson(e2);
  equal(e2.status, e2s.error);
  equal(e2.message, e2s.message);
  ok(e2s.stacktrace.match(/second/));
  ok(e2s.stacktrace.match(/third/));
  ok(e2s.stacktrace.match(/fourth/));

  run_next_test();
});

add_test(function test_fromJson() {
  Assert.throws(() => error.fromJson({error: "foo"}),
      /Undeserialisable error type: foo/);
  Assert.throws(() => error.fromJson({error: "Error"}),
      /Undeserialisable error type: Error/);
  Assert.throws(() => error.fromJson({}),
      /Undeserialisable error type: undefined/);

  let e1 = new WebDriverError("1");
  deepEqual(e1, error.fromJson({error: "webdriver error", message: "1"}));
  let e2 = new InvalidArgumentError("2");
  deepEqual(e2, error.fromJson({error: "invalid argument", message: "2"}));

  let e3 = new JavaScriptError("first", "second", "third", "fourth");
  let e3s = error.toJson(e3);
  deepEqual(e3, error.fromJson(e3s));

  run_next_test();
});

add_test(function test_WebDriverError() {
  let err = new WebDriverError("foo");
  equal("WebDriverError", err.name);
  equal("foo", err.message);
  equal("webdriver error", err.status);
  equal(Error.prototype.toString(), Object.getPrototypeOf(err).toString());

  run_next_test();
});

add_test(function test_ElementNotAccessibleError() {
  let err = new ElementNotAccessibleError("foo");
  equal("ElementNotAccessibleError", err.name);
  equal("foo", err.message);
  equal("element not accessible", err.status);
  equal(WebDriverError.prototype.toString(), Object.getPrototypeOf(err).toString());

  run_next_test();
});

add_test(function test_ElementNotVisibleError() {
  let err = new ElementNotVisibleError("foo");
  equal("ElementNotVisibleError", err.name);
  equal("foo", err.message);
  equal("element not visible", err.status);
  equal(WebDriverError.prototype.toString(), Object.getPrototypeOf(err).toString());

  run_next_test();
});

add_test(function test_InvalidArgumentError() {
  let err = new InvalidArgumentError("foo");
  equal("InvalidArgumentError", err.name);
  equal("foo", err.message);
  equal("invalid argument", err.status);
  equal(WebDriverError.prototype.toString(), Object.getPrototypeOf(err).toString());

  run_next_test();
});

add_test(function test_InvalidElementStateError() {
  let err = new InvalidElementStateError("foo");
  equal("InvalidElementStateError", err.name);
  equal("foo", err.message);
  equal("invalid element state", err.status);
  equal(WebDriverError.prototype.toString(), Object.getPrototypeOf(err).toString());

  run_next_test();
});

add_test(function test_InvalidSelectorError() {
  let err = new InvalidSelectorError("foo");
  equal("InvalidSelectorError", err.name);
  equal("foo", err.message);
  equal("invalid selector", err.status);
  equal(WebDriverError.prototype.toString(), Object.getPrototypeOf(err).toString());

  run_next_test();
});

add_test(function test_InvalidSessionIdError() {
  let err = new InvalidSessionIdError("foo");
  equal("InvalidSessionIdError", err.name);
  equal("foo", err.message);
  equal("invalid session id", err.status);
  equal(WebDriverError.prototype.toString(), Object.getPrototypeOf(err).toString());

  run_next_test();
});

add_test(function test_JavaScriptError() {
  let err = new JavaScriptError("foo");
  equal("JavaScriptError", err.name);
  equal("foo", err.message);
  equal("javascript error", err.status);
  equal(WebDriverError.prototype.toString(), Object.getPrototypeOf(err).toString());

  equal("undefined", new JavaScriptError(undefined).message);
  // TODO(ato): Bug 1240550
  //equal("funcname @file", new JavaScriptError("message", "funcname", "file").stack);
  equal("funcname @file, line line",
      new JavaScriptError("message", "funcname", "file", "line").stack);

  // TODO(ato): More exhaustive tests for JS stack computation

  run_next_test();
});

add_test(function test_NoAlertOpenError() {
  let err = new NoAlertOpenError("foo");
  equal("NoAlertOpenError", err.name);
  equal("foo", err.message);
  equal("no such alert", err.status);
  equal(WebDriverError.prototype.toString(), Object.getPrototypeOf(err).toString());

  run_next_test();
});

add_test(function test_NoSuchElementError() {
  let err = new NoSuchElementError("foo");
  equal("NoSuchElementError", err.name);
  equal("foo", err.message);
  equal("no such element", err.status);
  equal(WebDriverError.prototype.toString(), Object.getPrototypeOf(err).toString());

  run_next_test();
});

add_test(function test_NoSuchFrameError() {
  let err = new NoSuchFrameError("foo");
  equal("NoSuchFrameError", err.name);
  equal("foo", err.message);
  equal("no such frame", err.status);
  equal(WebDriverError.prototype.toString(), Object.getPrototypeOf(err).toString());

  run_next_test();
});

add_test(function test_NoSuchWindowError() {
  let err = new NoSuchWindowError("foo");
  equal("NoSuchWindowError", err.name);
  equal("foo", err.message);
  equal("no such window", err.status);
  equal(WebDriverError.prototype.toString(), Object.getPrototypeOf(err).toString());

  run_next_test();
});

add_test(function test_ScriptTimeoutError() {
  let err = new ScriptTimeoutError("foo");
  equal("ScriptTimeoutError", err.name);
  equal("foo", err.message);
  equal("script timeout", err.status);
  equal(WebDriverError.prototype.toString(), Object.getPrototypeOf(err).toString());

  run_next_test();
});

add_test(function test_SessionNotCreatedError() {
  let err = new SessionNotCreatedError("foo");
  equal("SessionNotCreatedError", err.name);
  equal("foo", err.message);
  equal("session not created", err.status);
  equal(WebDriverError.prototype.toString(), Object.getPrototypeOf(err).toString());

  run_next_test();
});

add_test(function test_StaleElementReferenceError() {
  let err = new StaleElementReferenceError("foo");
  equal("StaleElementReferenceError", err.name);
  equal("foo", err.message);
  equal("stale element reference", err.status);
  equal(WebDriverError.prototype.toString(), Object.getPrototypeOf(err).toString());

  run_next_test();
});

add_test(function test_TimeoutError() {
  let err = new TimeoutError("foo");
  equal("TimeoutError", err.name);
  equal("foo", err.message);
  equal("timeout", err.status);
  equal(WebDriverError.prototype.toString(), Object.getPrototypeOf(err).toString());

  run_next_test();
});

add_test(function test_UnableToSetCookieError() {
  let err = new UnableToSetCookieError("foo");
  equal("UnableToSetCookieError", err.name);
  equal("foo", err.message);
  equal("unable to set cookie", err.status);
  equal(WebDriverError.prototype.toString(), Object.getPrototypeOf(err).toString());

  run_next_test();
});

add_test(function test_UnknownCommandError() {
  let err = new UnknownCommandError("foo");
  equal("UnknownCommandError", err.name);
  equal("foo", err.message);
  equal("unknown command", err.status);
  equal(WebDriverError.prototype.toString(), Object.getPrototypeOf(err).toString());

  run_next_test();
});

add_test(function test_UnknownError() {
  let err = new UnknownError("foo");
  equal("UnknownError", err.name);
  equal("foo", err.message);
  equal("unknown error", err.status);
  equal(WebDriverError.prototype.toString(), Object.getPrototypeOf(err).toString());

  run_next_test();
});

add_test(function test_UnsupportedOperationError() {
  let err = new UnsupportedOperationError("foo");
  equal("UnsupportedOperationError", err.name);
  equal("foo", err.message);
  equal("unsupported operation", err.status);
  equal(WebDriverError.prototype.toString(), Object.getPrototypeOf(err).toString());

  run_next_test();
});
