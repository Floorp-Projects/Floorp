/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const { error } = ChromeUtils.importESModule(
  "chrome://remote/content/shared/webdriver/Errors.sys.mjs"
);

const errors = [
  error.WebDriverError,

  error.DetachedShadowRootError,
  error.ElementClickInterceptedError,
  error.ElementNotAccessibleError,
  error.ElementNotInteractableError,
  error.InsecureCertificateError,
  error.InvalidArgumentError,
  error.InvalidCookieDomainError,
  error.InvalidElementStateError,
  error.InvalidSelectorError,
  error.InvalidSessionIDError,
  error.JavaScriptError,
  error.MoveTargetOutOfBoundsError,
  error.NoSuchAlertError,
  error.NoSuchElementError,
  error.NoSuchFrameError,
  error.NoSuchHandleError,
  error.NoSuchInterceptError,
  error.NoSuchNodeError,
  error.NoSuchRequestError,
  error.NoSuchScriptError,
  error.NoSuchShadowRootError,
  error.NoSuchWindowError,
  error.ScriptTimeoutError,
  error.SessionNotCreatedError,
  error.StaleElementReferenceError,
  error.TimeoutError,
  error.UnableToSetCookieError,
  error.UnexpectedAlertOpenError,
  error.UnknownCommandError,
  error.UnknownError,
  error.UnsupportedOperationError,
];

function notok(condition) {
  ok(!condition);
}

add_task(function test_isError() {
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

  errors.forEach(err => ok(error.isError(new err())));
});

add_task(function test_isWebDriverError() {
  notok(error.isWebDriverError(new Components.Exception()));
  notok(error.isWebDriverError(new Error()));
  notok(error.isWebDriverError(new EvalError()));
  notok(error.isWebDriverError(new InternalError()));
  notok(error.isWebDriverError(new RangeError()));
  notok(error.isWebDriverError(new ReferenceError()));
  notok(error.isWebDriverError(new SyntaxError()));
  notok(error.isWebDriverError(new TypeError()));
  notok(error.isWebDriverError(new URIError()));

  errors.forEach(err => ok(error.isWebDriverError(new err())));
});

add_task(function test_wrap() {
  // webdriver-derived errors should not be wrapped
  errors.forEach(err => {
    const unwrappedError = new err("foo");
    const wrappedError = error.wrap(unwrappedError);

    ok(wrappedError instanceof error.WebDriverError);
    ok(wrappedError instanceof err);
    equal(wrappedError.name, unwrappedError.name);
    equal(wrappedError.status, unwrappedError.status);
    equal(wrappedError.message, "foo");
  });

  // JS errors should be wrapped in UnknownError and retain their type
  // as part of the message field.
  const jsErrors = [
    Error,
    EvalError,
    InternalError,
    RangeError,
    ReferenceError,
    SyntaxError,
    TypeError,
    URIError,
  ];

  jsErrors.forEach(err => {
    const originalError = new err("foo");
    const wrappedError = error.wrap(originalError);

    ok(wrappedError instanceof error.UnknownError);
    equal(wrappedError.name, "UnknownError");
    equal(wrappedError.status, "unknown error");
    equal(wrappedError.message, `${originalError.name}: foo`);
  });
});

add_task(function test_stringify() {
  equal("<unprintable error>", error.stringify());
  equal("<unprintable error>", error.stringify("foo"));
  equal("[object Object]", error.stringify({}));
  equal("[object Object]\nfoo", error.stringify({ stack: "foo" }));
  equal("Error: foo", error.stringify(new Error("foo")).split("\n")[0]);

  errors.forEach(err => {
    const e = new err("foo");

    equal(`${e.name}: foo`, error.stringify(e).split("\n")[0]);
  });
});

add_task(function test_constructor_from_error() {
  const data = { a: 3, b: "bar" };
  const origError = new error.WebDriverError("foo", data);

  errors.forEach(err => {
    const newError = new err(origError);

    ok(newError instanceof err);
    equal(newError.message, origError.message);
    equal(newError.stack, origError.stack);
    equal(newError.data, origError.data);
  });
});

add_task(function test_stack() {
  equal("string", typeof error.stack());
  ok(error.stack().includes("test_stack"));
  ok(!error.stack().includes("add_task"));
});

add_task(function test_toJSON() {
  errors.forEach(err => {
    const e0 = new err();
    const e0_json = e0.toJSON();
    equal(e0_json.error, e0.status);
    equal(e0_json.message, "");
    equal(e0_json.stacktrace, e0.stack);
    equal(e0_json.data, undefined);

    // message property
    const e1 = new err("a");
    const e1_json = e1.toJSON();

    equal(e1_json.message, e1.message);
    equal(e1_json.stacktrace, e1.stack);
    equal(e1_json.data, undefined);

    // message and optional data property
    const data = { a: 3, b: "bar" };
    const e2 = new err("foo", data);
    const e2_json = e2.toJSON();

    equal(e2.status, e2_json.error);
    equal(e2.message, e2_json.message);
    equal(e2_json.data, data);
  });
});

add_task(function test_fromJSON() {
  errors.forEach(err => {
    Assert.throws(
      () => err.fromJSON({ error: "foo" }),
      /Not of WebDriverError descent/
    );
    Assert.throws(
      () => err.fromJSON({ error: "Error" }),
      /Not of WebDriverError descent/
    );
    Assert.throws(() => err.fromJSON({}), /Undeserialisable error type/);
    Assert.throws(() => err.fromJSON(undefined), /TypeError/);

    // message and stack
    const e1 = new err("1");
    const e1_json = { error: e1.status, message: "3", stacktrace: "4" };
    const e1_fromJSON = error.WebDriverError.fromJSON(e1_json);

    ok(e1_fromJSON instanceof error.WebDriverError);
    ok(e1_fromJSON instanceof err);
    equal(e1_fromJSON.name, e1.name);
    equal(e1_fromJSON.status, e1_json.error);
    equal(e1_fromJSON.message, e1_json.message);
    equal(e1_fromJSON.stack, e1_json.stacktrace);

    // message and optional data
    const e2_data = { a: 3, b: "bar" };
    const e2 = new err("1", e2_data);
    const e2_json = { error: e1.status, message: "3", data: e2_data };
    const e2_fromJSON = error.WebDriverError.fromJSON(e2_json);

    ok(e2_fromJSON instanceof error.WebDriverError);
    ok(e2_fromJSON instanceof err);
    equal(e2_fromJSON.name, e2.name);
    equal(e2_fromJSON.status, e2_json.error);
    equal(e2_fromJSON.message, e2_json.message);
    equal(e2_fromJSON.data, e2_json.data);

    // parity with toJSON
    const e3_data = { a: 3, b: "bar" };
    const e3 = new err("1", e3_data);
    const e3_json = e3.toJSON();
    const e3_fromJSON = error.WebDriverError.fromJSON(e3_json);

    equal(e3_json.error, e3_fromJSON.status);
    equal(e3_json.message, e3_fromJSON.message);
    equal(e3_json.stacktrace, e3_fromJSON.stack);
  });
});

add_task(function test_WebDriverError() {
  let err = new error.WebDriverError("foo");
  equal("WebDriverError", err.name);
  equal("foo", err.message);
  equal("webdriver error", err.status);
  ok(err instanceof error.WebDriverError);
});

add_task(function test_DetachedShadowRootError() {
  let err = new error.DetachedShadowRootError("foo");
  equal("DetachedShadowRootError", err.name);
  equal("foo", err.message);
  equal("detached shadow root", err.status);
  ok(err instanceof error.WebDriverError);
});

add_task(function test_ElementClickInterceptedError() {
  let otherEl = {
    hasAttribute: attr => attr in otherEl,
    getAttribute: attr => (attr in otherEl ? otherEl[attr] : null),
    nodeType: 1,
    localName: "a",
  };
  let obscuredEl = {
    hasAttribute: attr => attr in obscuredEl,
    getAttribute: attr => (attr in obscuredEl ? obscuredEl[attr] : null),
    nodeType: 1,
    localName: "b",
    ownerDocument: {
      elementFromPoint() {
        return otherEl;
      },
    },
    style: {
      pointerEvents: "auto",
    },
  };

  let err1 = new error.ElementClickInterceptedError(
    undefined,
    undefined,
    obscuredEl,
    { x: 1, y: 2 }
  );
  equal("ElementClickInterceptedError", err1.name);
  equal(
    "Element <b> is not clickable at point (1,2) " +
      "because another element <a> obscures it",
    err1.message
  );
  equal("element click intercepted", err1.status);
  ok(err1 instanceof error.WebDriverError);

  obscuredEl.style.pointerEvents = "none";
  let err2 = new error.ElementClickInterceptedError(
    undefined,
    undefined,
    obscuredEl,
    { x: 1, y: 2 }
  );
  equal(
    "Element <b> is not clickable at point (1,2) " +
      "because it does not have pointer events enabled, " +
      "and element <a> would receive the click instead",
    err2.message
  );
});

add_task(function test_ElementNotAccessibleError() {
  let err = new error.ElementNotAccessibleError("foo");
  equal("ElementNotAccessibleError", err.name);
  equal("foo", err.message);
  equal("element not accessible", err.status);
  ok(err instanceof error.WebDriverError);
});

add_task(function test_ElementNotInteractableError() {
  let err = new error.ElementNotInteractableError("foo");
  equal("ElementNotInteractableError", err.name);
  equal("foo", err.message);
  equal("element not interactable", err.status);
  ok(err instanceof error.WebDriverError);
});

add_task(function test_InsecureCertificateError() {
  let err = new error.InsecureCertificateError("foo");
  equal("InsecureCertificateError", err.name);
  equal("foo", err.message);
  equal("insecure certificate", err.status);
  ok(err instanceof error.WebDriverError);
});

add_task(function test_InvalidArgumentError() {
  let err = new error.InvalidArgumentError("foo");
  equal("InvalidArgumentError", err.name);
  equal("foo", err.message);
  equal("invalid argument", err.status);
  ok(err instanceof error.WebDriverError);
});

add_task(function test_InvalidCookieDomainError() {
  let err = new error.InvalidCookieDomainError("foo");
  equal("InvalidCookieDomainError", err.name);
  equal("foo", err.message);
  equal("invalid cookie domain", err.status);
  ok(err instanceof error.WebDriverError);
});

add_task(function test_InvalidElementStateError() {
  let err = new error.InvalidElementStateError("foo");
  equal("InvalidElementStateError", err.name);
  equal("foo", err.message);
  equal("invalid element state", err.status);
  ok(err instanceof error.WebDriverError);
});

add_task(function test_InvalidSelectorError() {
  let err = new error.InvalidSelectorError("foo");
  equal("InvalidSelectorError", err.name);
  equal("foo", err.message);
  equal("invalid selector", err.status);
  ok(err instanceof error.WebDriverError);
});

add_task(function test_InvalidSessionIDError() {
  let err = new error.InvalidSessionIDError("foo");
  equal("InvalidSessionIDError", err.name);
  equal("foo", err.message);
  equal("invalid session id", err.status);
  ok(err instanceof error.WebDriverError);
});

add_task(function test_JavaScriptError() {
  let err = new error.JavaScriptError("foo");
  equal("JavaScriptError", err.name);
  equal("foo", err.message);
  equal("javascript error", err.status);
  ok(err instanceof error.WebDriverError);

  equal("", new error.JavaScriptError(undefined).message);

  let superErr = new RangeError("foo");
  let inheritedErr = new error.JavaScriptError(superErr);
  equal("RangeError: foo", inheritedErr.message);
  equal(superErr.stack, inheritedErr.stack);
});

add_task(function test_MoveTargetOutOfBoundsError() {
  let err = new error.MoveTargetOutOfBoundsError("foo");
  equal("MoveTargetOutOfBoundsError", err.name);
  equal("foo", err.message);
  equal("move target out of bounds", err.status);
  ok(err instanceof error.WebDriverError);
});

add_task(function test_NoSuchAlertError() {
  let err = new error.NoSuchAlertError("foo");
  equal("NoSuchAlertError", err.name);
  equal("foo", err.message);
  equal("no such alert", err.status);
  ok(err instanceof error.WebDriverError);
});

add_task(function test_NoSuchElementError() {
  let err = new error.NoSuchElementError("foo");
  equal("NoSuchElementError", err.name);
  equal("foo", err.message);
  equal("no such element", err.status);
  ok(err instanceof error.WebDriverError);
});

add_task(function test_NoSuchFrameError() {
  let err = new error.NoSuchFrameError("foo");
  equal("NoSuchFrameError", err.name);
  equal("foo", err.message);
  equal("no such frame", err.status);
  ok(err instanceof error.WebDriverError);
});

add_task(function test_NoSuchHandleError() {
  let err = new error.NoSuchHandleError("foo");
  equal("NoSuchHandleError", err.name);
  equal("foo", err.message);
  equal("no such handle", err.status);
  ok(err instanceof error.WebDriverError);
});

add_task(function test_NoSuchInterceptError() {
  let err = new error.NoSuchInterceptError("foo");
  equal("NoSuchInterceptError", err.name);
  equal("foo", err.message);
  equal("no such intercept", err.status);
  ok(err instanceof error.WebDriverError);
});

add_task(function test_NoSuchNodeError() {
  let err = new error.NoSuchNodeError("foo");
  equal("NoSuchNodeError", err.name);
  equal("foo", err.message);
  equal("no such node", err.status);
  ok(err instanceof error.WebDriverError);
});

add_task(function test_NoSuchRequestError() {
  let err = new error.NoSuchRequestError("foo");
  equal("NoSuchRequestError", err.name);
  equal("foo", err.message);
  equal("no such request", err.status);
  ok(err instanceof error.WebDriverError);
});

add_task(function test_NoSuchScriptError() {
  let err = new error.NoSuchScriptError("foo");
  equal("NoSuchScriptError", err.name);
  equal("foo", err.message);
  equal("no such script", err.status);
  ok(err instanceof error.WebDriverError);
});

add_task(function test_NoSuchShadowRootError() {
  let err = new error.NoSuchShadowRootError("foo");
  equal("NoSuchShadowRootError", err.name);
  equal("foo", err.message);
  equal("no such shadow root", err.status);
  ok(err instanceof error.WebDriverError);
});

add_task(function test_NoSuchUserContextError() {
  let err = new error.NoSuchUserContextError("foo");
  equal("NoSuchUserContextError", err.name);
  equal("foo", err.message);
  equal("no such user context", err.status);
  ok(err instanceof error.WebDriverError);
});

add_task(function test_NoSuchWindowError() {
  let err = new error.NoSuchWindowError("foo");
  equal("NoSuchWindowError", err.name);
  equal("foo", err.message);
  equal("no such window", err.status);
  ok(err instanceof error.WebDriverError);
});

add_task(function test_ScriptTimeoutError() {
  let err = new error.ScriptTimeoutError("foo");
  equal("ScriptTimeoutError", err.name);
  equal("foo", err.message);
  equal("script timeout", err.status);
  ok(err instanceof error.WebDriverError);
});

add_task(function test_SessionNotCreatedError() {
  let err = new error.SessionNotCreatedError("foo");
  equal("SessionNotCreatedError", err.name);
  equal("foo", err.message);
  equal("session not created", err.status);
  ok(err instanceof error.WebDriverError);
});

add_task(function test_StaleElementReferenceError() {
  let err = new error.StaleElementReferenceError("foo");
  equal("StaleElementReferenceError", err.name);
  equal("foo", err.message);
  equal("stale element reference", err.status);
  ok(err instanceof error.WebDriverError);
});

add_task(function test_TimeoutError() {
  let err = new error.TimeoutError("foo");
  equal("TimeoutError", err.name);
  equal("foo", err.message);
  equal("timeout", err.status);
  ok(err instanceof error.WebDriverError);
});

add_task(function test_UnableToSetCookieError() {
  let err = new error.UnableToSetCookieError("foo");
  equal("UnableToSetCookieError", err.name);
  equal("foo", err.message);
  equal("unable to set cookie", err.status);
  ok(err instanceof error.WebDriverError);
});

add_task(function test_UnexpectedAlertOpenError() {
  let err = new error.UnexpectedAlertOpenError("foo");
  equal("UnexpectedAlertOpenError", err.name);
  equal("foo", err.message);
  equal("unexpected alert open", err.status);
  ok(err instanceof error.WebDriverError);
});

add_task(function test_UnknownCommandError() {
  let err = new error.UnknownCommandError("foo");
  equal("UnknownCommandError", err.name);
  equal("foo", err.message);
  equal("unknown command", err.status);
  ok(err instanceof error.WebDriverError);
});

add_task(function test_UnknownError() {
  let err = new error.UnknownError("foo");
  equal("UnknownError", err.name);
  equal("foo", err.message);
  equal("unknown error", err.status);
  ok(err instanceof error.WebDriverError);
});

add_task(function test_UnsupportedOperationError() {
  let err = new error.UnsupportedOperationError("foo");
  equal("UnsupportedOperationError", err.name);
  equal("foo", err.message);
  equal("unsupported operation", err.status);
  ok(err instanceof error.WebDriverError);
});
