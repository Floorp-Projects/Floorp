/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const {utils: Cu} = Components;

Cu.import("chrome://marionette/content/error.js");

function notok(condition) {
  ok(!(condition));
}

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
  ok(error.isWebDriverError(new JavaScriptError()));

  run_next_test();
});

add_test(function test_wrap() {
  // webdriver-derived errors should not be wrapped
  equal(error.wrap(new WebDriverError()).name, "WebDriverError");
  ok(error.wrap(new WebDriverError()) instanceof WebDriverError);
  equal(error.wrap(new InvalidArgumentError()).name, "InvalidArgumentError");
  ok(error.wrap(new InvalidArgumentError()) instanceof WebDriverError);
  ok(error.wrap(new InvalidArgumentError()) instanceof InvalidArgumentError);

  // JS errors should be wrapped in WebDriverError
  equal(error.wrap(new Error()).name, "WebDriverError");
  ok(error.wrap(new Error()) instanceof WebDriverError);
  equal(error.wrap(new EvalError()).name, "WebDriverError");
  equal(error.wrap(new InternalError()).name, "WebDriverError");
  equal(error.wrap(new RangeError()).name, "WebDriverError");
  equal(error.wrap(new ReferenceError()).name, "WebDriverError");
  equal(error.wrap(new SyntaxError()).name, "WebDriverError");
  equal(error.wrap(new TypeError()).name, "WebDriverError");
  equal(error.wrap(new URIError()).name, "WebDriverError");

  // wrapped JS errors should retain their type
  // as part of the message field
  equal(error.wrap(new WebDriverError("foo")).message, "foo");
  equal(error.wrap(new TypeError("foo")).message, "TypeError: foo");

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

add_test(function test_pprint() {
  equal('[object Object] {"foo":"bar"}', error.pprint`${{foo: "bar"}}`);

  equal("[object Number] 42", error.pprint`${42}`);
  equal("[object Boolean] true", error.pprint`${true}`);
  equal("[object Undefined] undefined", error.pprint`${undefined}`);
  equal("[object Null] null", error.pprint`${null}`);

  let complexObj = {toJSON: () => "foo"};
  equal('[object Object] "foo"', error.pprint`${complexObj}`);

  let cyclic = {};
  cyclic.me = cyclic;
  equal("[object Object] <cyclic object value>", error.pprint`${cyclic}`);

  let el = {
    nodeType: 1,
    localName: "input",
    id: "foo",
    classList: {length: 1},
    className: "bar baz",
  };
  equal('<input id="foo" class="bar baz">', error.pprint`${el}`);

  run_next_test();
});

add_test(function test_toJSON() {
  let e0 = new WebDriverError();
  let e0s = e0.toJSON();
  equal(e0s.error, "webdriver error");
  equal(e0s.message, "");
  equal(e0s.stacktrace, e0.stack);

  let e1 = new WebDriverError("a");
  let e1s = e1.toJSON();
  equal(e1s.message, e1.message);
  equal(e1s.stacktrace, e1.stack);

  let e2 = new JavaScriptError("first", "second", "third", "fourth");
  let e2s = e2.toJSON();
  equal(e2.status, e2s.error);
  equal(e2.message, e2s.message);
  ok(e2s.stacktrace.match(/second/));
  ok(e2s.stacktrace.match(/third/));
  ok(e2s.stacktrace.match(/fourth/));

  run_next_test();
});

add_test(function test_fromJSON() {
  Assert.throws(() => WebDriverError.fromJSON({error: "foo"}),
      /Not of WebDriverError descent/);
  Assert.throws(() => WebDriverError.fromJSON({error: "Error"}),
      /Not of WebDriverError descent/);
  Assert.throws(() => WebDriverError.fromJSON({}),
      /Undeserialisable error type/);
  Assert.throws(() => WebDriverError.fromJSON(undefined),
      /TypeError/);

  // stacks will be different
  let e1 = new WebDriverError("1");
  let e1r = WebDriverError.fromJSON({error: "webdriver error", message: "1"});
  ok(e1r instanceof WebDriverError);
  equal(e1r.name, e1.name);
  equal(e1r.status, e1.status);
  equal(e1r.message, e1.message);

  // stacks will be different
  let e2 = new InvalidArgumentError("2");
  let e2r = WebDriverError.fromJSON({error: "invalid argument", message: "2"});
  ok(e2r instanceof WebDriverError);
  ok(e2r instanceof InvalidArgumentError);
  equal(e2r.name, e2.name);
  equal(e2r.status, e2.status);
  equal(e2r.message, e2.message);

  // test stacks
  let e3j = {error: "no such element", message: "3", stacktrace: "4"};
  let e3r = WebDriverError.fromJSON(e3j);
  ok(e3r instanceof WebDriverError);
  ok(e3r instanceof NoSuchElementError);
  equal(e3r.name, "NoSuchElementError");
  equal(e3r.status, e3j.error);
  equal(e3r.message, e3j.message);
  equal(e3r.stack, e3j.stacktrace);

  // parity with toJSON
  let e4 = new JavaScriptError("first", "second", "third", "fourth");
  let e4s = e4.toJSON();
  deepEqual(e4, WebDriverError.fromJSON(e4s));

  run_next_test();
});

add_test(function test_WebDriverError() {
  let err = new WebDriverError("foo");
  equal("WebDriverError", err.name);
  equal("foo", err.message);
  equal("webdriver error", err.status);
  ok(err instanceof WebDriverError);

  run_next_test();
});

add_test(function test_ElementClickInterceptedError() {
  let otherEl = {
    nodeType: 1,
    localName: "a",
    classList: [],
  };
  let obscuredEl = {
    nodeType: 1,
    localName: "b",
    classList: [],
    ownerDocument: {
      elementFromPoint: function (x, y) {
        return otherEl;
      },
    },
    style: {
      pointerEvents: "auto",
    }
  };

  let err1 = new ElementClickInterceptedError(obscuredEl, {x: 1, y: 2});
  equal("ElementClickInterceptedError", err1.name);
  equal("Element <b> is not clickable at point (1,2) " +
      "because another element <a> obscures it",
      err1.message);
  equal("element click intercepted", err1.status);
  ok(err1 instanceof WebDriverError);

  obscuredEl.style.pointerEvents = "none";
  let err2 = new ElementClickInterceptedError(obscuredEl, {x: 1, y: 2});
  equal("Element <b> is not clickable at point (1,2) " +
      "because it does not have pointer events enabled, " +
      "and element <a> would receive the click instead",
      err2.message);

  run_next_test();
});

add_test(function test_ElementNotAccessibleError() {
  let err = new ElementNotAccessibleError("foo");
  equal("ElementNotAccessibleError", err.name);
  equal("foo", err.message);
  equal("element not accessible", err.status);
  ok(err instanceof WebDriverError);

  run_next_test();
});

add_test(function test_ElementNotInteractableError() {
  let err = new ElementNotInteractableError("foo");
  equal("ElementNotInteractableError", err.name);
  equal("foo", err.message);
  equal("element not interactable", err.status);
  ok(err instanceof WebDriverError);

  run_next_test();
});

add_test(function test_InvalidArgumentError() {
  let err = new InvalidArgumentError("foo");
  equal("InvalidArgumentError", err.name);
  equal("foo", err.message);
  equal("invalid argument", err.status);
  ok(err instanceof WebDriverError);

  run_next_test();
});

add_test(function test_InvalidElementStateError() {
  let err = new InvalidElementStateError("foo");
  equal("InvalidElementStateError", err.name);
  equal("foo", err.message);
  equal("invalid element state", err.status);
  ok(err instanceof WebDriverError);

  run_next_test();
});

add_test(function test_InvalidSelectorError() {
  let err = new InvalidSelectorError("foo");
  equal("InvalidSelectorError", err.name);
  equal("foo", err.message);
  equal("invalid selector", err.status);
  ok(err instanceof WebDriverError);

  run_next_test();
});

add_test(function test_InvalidSessionIDError() {
  let err = new InvalidSessionIDError("foo");
  equal("InvalidSessionIDError", err.name);
  equal("foo", err.message);
  equal("invalid session id", err.status);
  ok(err instanceof WebDriverError);

  run_next_test();
});

add_test(function test_JavaScriptError() {
  let err = new JavaScriptError("foo");
  equal("JavaScriptError", err.name);
  equal("foo", err.message);
  equal("javascript error", err.status);
  ok(err instanceof WebDriverError);

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
  ok(err instanceof WebDriverError);

  run_next_test();
});

add_test(function test_NoSuchElementError() {
  let err = new NoSuchElementError("foo");
  equal("NoSuchElementError", err.name);
  equal("foo", err.message);
  equal("no such element", err.status);
  ok(err instanceof WebDriverError);

  run_next_test();
});

add_test(function test_NoSuchFrameError() {
  let err = new NoSuchFrameError("foo");
  equal("NoSuchFrameError", err.name);
  equal("foo", err.message);
  equal("no such frame", err.status);
  ok(err instanceof WebDriverError);

  run_next_test();
});

add_test(function test_NoSuchWindowError() {
  let err = new NoSuchWindowError("foo");
  equal("NoSuchWindowError", err.name);
  equal("foo", err.message);
  equal("no such window", err.status);
  ok(err instanceof WebDriverError);

  run_next_test();
});

add_test(function test_ScriptTimeoutError() {
  let err = new ScriptTimeoutError("foo");
  equal("ScriptTimeoutError", err.name);
  equal("foo", err.message);
  equal("script timeout", err.status);
  ok(err instanceof WebDriverError);

  run_next_test();
});

add_test(function test_SessionNotCreatedError() {
  let err = new SessionNotCreatedError("foo");
  equal("SessionNotCreatedError", err.name);
  equal("foo", err.message);
  equal("session not created", err.status);
  ok(err instanceof WebDriverError);

  run_next_test();
});

add_test(function test_StaleElementReferenceError() {
  let err = new StaleElementReferenceError("foo");
  equal("StaleElementReferenceError", err.name);
  equal("foo", err.message);
  equal("stale element reference", err.status);
  ok(err instanceof WebDriverError);

  run_next_test();
});

add_test(function test_TimeoutError() {
  let err = new TimeoutError("foo");
  equal("TimeoutError", err.name);
  equal("foo", err.message);
  equal("timeout", err.status);
  ok(err instanceof WebDriverError);

  run_next_test();
});

add_test(function test_UnableToSetCookieError() {
  let err = new UnableToSetCookieError("foo");
  equal("UnableToSetCookieError", err.name);
  equal("foo", err.message);
  equal("unable to set cookie", err.status);
  ok(err instanceof WebDriverError);

  run_next_test();
});

add_test(function test_UnknownCommandError() {
  let err = new UnknownCommandError("foo");
  equal("UnknownCommandError", err.name);
  equal("foo", err.message);
  equal("unknown command", err.status);
  ok(err instanceof WebDriverError);

  run_next_test();
});

add_test(function test_UnknownError() {
  let err = new UnknownError("foo");
  equal("UnknownError", err.name);
  equal("foo", err.message);
  equal("unknown error", err.status);
  ok(err instanceof WebDriverError);

  run_next_test();
});

add_test(function test_UnsupportedOperationError() {
  let err = new UnsupportedOperationError("foo");
  equal("UnsupportedOperationError", err.name);
  equal("foo", err.message);
  equal("unsupported operation", err.status);
  ok(err instanceof WebDriverError);

  run_next_test();
});
