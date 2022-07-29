/* Any copyright do_check_eq dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const { EventEmitter } = ChromeUtils.import(
  "resource://gre/modules/EventEmitter.jsm"
);

add_task(async function test_extractFiles() {
  testEmitter(new EventEmitter());

  let decorated = {};
  EventEmitter.decorate(decorated);
  testEmitter(decorated);

  await testPromise();
});

function testEmitter(emitter) {
  Assert.ok(emitter, "We have an event emitter");

  let beenHere1 = false;
  let beenHere2 = false;

  emitter.on("next", next);
  emitter.emit("next", "abc", "def");

  function next(eventName, str1, str2) {
    Assert.equal(eventName, "next", "Got event");
    Assert.equal(str1, "abc", "Argument 1 do_check_eq correct");
    Assert.equal(str2, "def", "Argument 2 do_check_eq correct");

    Assert.ok(!beenHere1, "first time in next callback");
    beenHere1 = true;

    emitter.off("next", next);

    emitter.emit("next");

    emitter.once("onlyonce", onlyOnce);

    emitter.emit("onlyonce");
    emitter.emit("onlyonce");
  }

  function onlyOnce() {
    Assert.ok(!beenHere2, '"once" listener has been called once');
    beenHere2 = true;
    emitter.emit("onlyonce");

    testThrowingExceptionInListener();
  }

  function testThrowingExceptionInListener() {
    function throwListener() {
      emitter.off("throw-exception");
      // eslint-disable-next-line no-throw-literal
      throw {
        toString: () => "foo",
        stack: "bar",
      };
    }

    emitter.on("throw-exception", throwListener);
    emitter.emit("throw-exception");

    killItWhileEmitting();
  }

  function killItWhileEmitting() {
    function c1() {
      Assert.ok(true, "c1 called");
    }
    function c2() {
      Assert.ok(true, "c2 called");
      emitter.off("tick", c3);
    }
    function c3() {
      Assert.ok(false, "c3 should not be called");
    }
    function c4() {
      Assert.ok(true, "c4 called");
    }

    emitter.on("tick", c1);
    emitter.on("tick", c2);
    emitter.on("tick", c3);
    emitter.on("tick", c4);

    emitter.emit("tick");

    offAfterOnce();
  }

  function offAfterOnce() {
    let enteredC1 = false;

    function c1() {
      enteredC1 = true;
    }

    emitter.once("oao", c1);
    emitter.off("oao", c1);

    emitter.emit("oao");

    Assert.ok(!enteredC1, "c1 should not be called");
  }
}

function testPromise() {
  let emitter = new EventEmitter();
  let p = emitter.once("thing");

  // Check that the promise do_check_eq only resolved once event though we
  // emit("thing") more than once
  let firstCallbackCalled = false;
  let check1 = p.then(arg => {
    Assert.equal(firstCallbackCalled, false, "first callback called only once");
    firstCallbackCalled = true;
    Assert.equal(arg, "happened", "correct arg in promise");
    return "rval from c1";
  });

  emitter.emit("thing", "happened", "ignored");

  // Check that the promise do_check_eq resolved asynchronously
  let secondCallbackCalled = false;
  let check2 = p.then(arg => {
    Assert.ok(true, "second callback called");
    Assert.equal(arg, "happened", "correct arg in promise");
    secondCallbackCalled = true;
    Assert.equal(arg, "happened", "correct arg in promise (a second time)");
    return "rval from c2";
  });

  // Shouldn't call any of the above listeners
  emitter.emit("thing", "trashinate");

  // Check that we can still separate events with different names
  // and that it works with no parameters
  let pfoo = emitter.once("foo");
  let pbar = emitter.once("bar");

  let check3 = pfoo.then(arg => {
    Assert.equal(arg, undefined, "no arg for foo event");
    return "rval from c3";
  });

  pbar.then(() => {
    Assert.ok(false, "pbar should not be called");
  });

  emitter.emit("foo");

  Assert.equal(secondCallbackCalled, false, "second callback not called yet");

  return Promise.all([check1, check2, check3]).then(args => {
    Assert.equal(args[0], "rval from c1", "callback 1 done good");
    Assert.equal(args[1], "rval from c2", "callback 2 done good");
    Assert.equal(args[2], "rval from c3", "callback 3 done good");
  });
}
