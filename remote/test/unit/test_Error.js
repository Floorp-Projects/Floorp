/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";
/* eslint-disable no-tabs */

const {
  FatalError,
  RemoteAgentError,
  UnknownMethodError,
  UnsupportedError,
} = ChromeUtils.import("chrome://remote/content/Error.jsm");

add_test(function test_RemoteAgentError_ctor() {
  const e1 = new RemoteAgentError();
  equal(e1.name, "RemoteAgentError");
  equal(e1.message, "");
  equal(e1.cause, e1.message);

  const e2 = new RemoteAgentError("message");
  equal(e2.message, "message");
  equal(e2.cause, e2.message);

  const e3 = new RemoteAgentError("message", "cause");
  equal(e3.message, "message");
  equal(e3.cause, "cause");

  run_next_test();
});

add_test(function test_RemoteAgentError_notify() {
  // nothing much we can test, except test that it doesn't throw
  new RemoteAgentError().notify();

  run_next_test();
});

add_test(function test_RemoteAgentError_toString() {
  const e = new RemoteAgentError("message");
  equal(e.toString(), RemoteAgentError.format(e));
  equal(e.toString({stack: true}), RemoteAgentError.format(e, {stack: true}));

  run_next_test();
});

add_test(function test_RemoteAgentError_format() {
  const {format} = RemoteAgentError;

  equal(format({name: "HippoError"}), "HippoError");
  equal(format({name: "HorseError", message: "neigh"}), "HorseError: neigh");

  const dog = {
    name: "DogError",
    message: "woof",
    stack: "  one\ntwo\nthree  ",
  };
  equal(format(dog), "DogError: woof");
  equal(format(dog, {stack: true}),
`DogError: woof:
	one
	two
	three`);

  const cat = {
    name: "CatError",
    message: "meow",
    stack: "four\nfive\nsix",
    cause: dog,
  };
  equal(format(cat), "CatError: meow");
  equal(format(cat, {stack: true}),
`CatError: meow:
	four
	five
	six
caused by: DogError: woof:
	one
	two
	three`);

  run_next_test();
});

// FatalError calls nsIAppStartup.quit(), but it's OK because this is an xpcshell test:
add_test(function test_FatalError() {
  const e = new FatalError();
  ok(e instanceof RemoteAgentError);
  e.notify();
  equal(e.toString(), RemoteAgentError.format(e));
  e.quit();

  run_next_test();
});

add_test(function test_UnsupportedError() {
  ok(new UnsupportedError() instanceof RemoteAgentError);
  run_next_test();
});

add_test(function test_UnknownMethodError() {
  ok(new UnknownMethodError() instanceof RemoteAgentError);
  run_next_test();
});
