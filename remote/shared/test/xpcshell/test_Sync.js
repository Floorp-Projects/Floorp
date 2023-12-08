/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const { setTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs"
);

const { AnimationFramePromise, Deferred, EventPromise, PollPromise } =
  ChromeUtils.importESModule("chrome://remote/content/shared/Sync.sys.mjs");

const { Log } = ChromeUtils.importESModule(
  "resource://gre/modules/Log.sys.mjs"
);

/**
 * Mimic a DOM node for listening for events.
 */
class MockElement {
  constructor() {
    this.capture = false;
    this.eventName = null;
    this.func = null;
    this.mozSystemGroup = false;
    this.wantUntrusted = false;
    this.untrusted = false;
  }

  addEventListener(name, func, options = {}) {
    const { capture, mozSystemGroup, wantUntrusted } = options;

    this.eventName = name;
    this.func = func;
    this.capture = capture ?? false;
    this.mozSystemGroup = mozSystemGroup ?? false;
    this.wantUntrusted = wantUntrusted ?? false;
  }

  click() {
    if (this.func) {
      const event = {
        capture: this.capture,
        mozSystemGroup: this.mozSystemGroup,
        target: this,
        type: this.eventName,
        untrusted: this.untrusted,
        wantUntrusted: this.wantUntrusted,
      };
      this.func(event);
    }
  }

  dispatchEvent(event) {
    if (this.wantUntrusted) {
      this.untrusted = true;
    }
    this.click();
  }

  removeEventListener(name, func) {
    this.capture = false;
    this.eventName = null;
    this.func = null;
    this.mozSystemGroup = false;
    this.untrusted = false;
    this.wantUntrusted = false;
  }
}

class MockAppender extends Log.Appender {
  constructor(formatter) {
    super(formatter);
    this.messages = [];
  }

  append(message) {
    this.doAppend(message);
  }

  doAppend(message) {
    this.messages.push(message);
  }
}

add_task(async function test_AnimationFramePromise() {
  let called = false;
  let win = {
    requestAnimationFrame(callback) {
      called = true;
      callback();
    },
  };
  await AnimationFramePromise(win);
  ok(called);
});

add_task(async function test_AnimationFramePromiseAbortWhenWindowClosed() {
  let win = {
    closed: true,
    requestAnimationFrame() {},
  };
  await AnimationFramePromise(win);
});

add_task(async function test_DeferredPending() {
  const deferred = Deferred();
  ok(deferred.pending);

  deferred.resolve();
  await deferred.promise;
  ok(!deferred.pending);
});

add_task(async function test_DeferredRejected() {
  const deferred = Deferred();

  // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
  setTimeout(() => deferred.reject(new Error("foo")), 100);

  try {
    await deferred.promise;
    ok(false);
  } catch (e) {
    ok(!deferred.pending);

    ok(!deferred.fulfilled);
    ok(deferred.rejected);
    equal(e.message, "foo");
  }
});

add_task(async function test_DeferredResolved() {
  const deferred = Deferred();
  ok(deferred.pending);

  // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
  setTimeout(() => deferred.resolve("foo"), 100);

  const result = await deferred.promise;
  ok(!deferred.pending);

  ok(deferred.fulfilled);
  ok(!deferred.rejected);
  equal(result, "foo");
});

add_task(async function test_EventPromise_subjectTypes() {
  for (const subject of ["foo", 42, null, undefined, true, [], {}]) {
    Assert.throws(() => new EventPromise(subject, "click"), /TypeError/);
  }
});

add_task(async function test_EventPromise_eventNameTypes() {
  const element = new MockElement();

  for (const eventName of [42, null, undefined, true, [], {}]) {
    Assert.throws(() => new EventPromise(element, eventName), /TypeError/);
  }
});

add_task(async function test_EventPromise_subjectAndEventNameEvent() {
  const element = new MockElement();

  const clicked = new EventPromise(element, "click");
  element.click();
  const event = await clicked;

  equal(element, event.target);
});

add_task(async function test_EventPromise_captureTypes() {
  const element = new MockElement();

  for (const capture of [null, "foo", 42, [], {}]) {
    Assert.throws(
      () => new EventPromise(element, "click", { capture }),
      /TypeError/
    );
  }
});

add_task(async function test_EventPromise_captureEvent() {
  const element = new MockElement();

  for (const capture of [undefined, false, true]) {
    const expectedCapture = capture ?? false;

    const clicked = new EventPromise(element, "click", { capture });
    element.click();
    const event = await clicked;

    equal(element, event.target);
    equal(expectedCapture, event.capture);
  }
});

add_task(async function test_EventPromise_checkFnTypes() {
  const element = new MockElement();

  for (const checkFn of ["foo", 42, true, [], {}]) {
    Assert.throws(
      () => new EventPromise(element, "click", { checkFn }),
      /TypeError/
    );
  }
});

add_task(async function test_EventPromise_checkFnCallback() {
  const element = new MockElement();

  let count;
  const data = [
    { checkFn: null, expected_count: 0 },
    { checkFn: undefined, expected_count: 0 },
    {
      checkFn: event => {
        throw new Error("foo");
      },
      expected_count: 0,
    },
    { checkFn: event => count++ > 0, expected_count: 2 },
  ];

  for (const { checkFn, expected_count } of data) {
    count = 0;

    const clicked = new EventPromise(element, "click", { checkFn });
    element.click();
    element.click();
    const event = await clicked;

    equal(element, event.target);
    equal(expected_count, count);
  }
});

add_task(async function test_EventPromise_mozSystemGroupTypes() {
  const element = new MockElement();

  for (const mozSystemGroup of [null, "foo", 42, [], {}]) {
    Assert.throws(
      () => new EventPromise(element, "click", { mozSystemGroup }),
      /TypeError/
    );
  }
});

add_task(async function test_EventPromise_mozSystemGroupEvent() {
  const element = new MockElement();

  for (const mozSystemGroup of [undefined, false, true]) {
    const expectedMozSystemGroup = mozSystemGroup ?? false;

    const clicked = new EventPromise(element, "click", { mozSystemGroup });
    element.click();
    const event = await clicked;

    equal(element, event.target);
    equal(expectedMozSystemGroup, event.mozSystemGroup);
  }
});

add_task(async function test_EventPromise_wantUntrustedTypes() {
  const element = new MockElement();

  for (let wantUntrusted of [null, "foo", 42, [], {}]) {
    Assert.throws(
      () => new EventPromise(element, "click", { wantUntrusted }),
      /TypeError/
    );
  }
});

add_task(async function test_EventPromise_wantUntrustedEvent() {
  for (const wantUntrusted of [undefined, false, true]) {
    let expected_untrusted = wantUntrusted ?? false;

    const element = new MockElement();

    const clicked = new EventPromise(element, "click", { wantUntrusted });
    element.dispatchEvent(new CustomEvent("click", {}));
    const event = await clicked;

    equal(element, event.target);
    equal(expected_untrusted, event.untrusted);
  }
});

add_task(function test_executeSoon_callback() {
  // executeSoon() is already defined for xpcshell in head.js. As such import
  // our implementation into a custom namespace.
  let sync = ChromeUtils.importESModule(
    "chrome://remote/content/shared/Sync.sys.mjs"
  );

  for (let func of ["foo", null, true, [], {}]) {
    Assert.throws(() => sync.executeSoon(func), /TypeError/);
  }

  let a;
  sync.executeSoon(() => {
    a = 1;
  });
  executeSoon(() => equal(1, a));
});

add_task(function test_PollPromise_funcTypes() {
  for (let type of ["foo", 42, null, undefined, true, [], {}]) {
    Assert.throws(() => new PollPromise(type), /TypeError/);
  }
  new PollPromise(() => {});
  new PollPromise(function () {});
});

add_task(function test_PollPromise_timeoutTypes() {
  for (let timeout of ["foo", true, [], {}]) {
    Assert.throws(() => new PollPromise(() => {}, { timeout }), /TypeError/);
  }
  for (let timeout of [1.2, -1]) {
    Assert.throws(() => new PollPromise(() => {}, { timeout }), /RangeError/);
  }
  for (let timeout of [null, undefined, 42]) {
    new PollPromise(resolve => resolve(1), { timeout });
  }
});

add_task(function test_PollPromise_intervalTypes() {
  for (let interval of ["foo", null, true, [], {}]) {
    Assert.throws(() => new PollPromise(() => {}, { interval }), /TypeError/);
  }
  for (let interval of [1.2, -1]) {
    Assert.throws(() => new PollPromise(() => {}, { interval }), /RangeError/);
  }
  new PollPromise(() => {}, { interval: 42 });
});

add_task(async function test_PollPromise_retvalTypes() {
  for (let typ of [true, false, "foo", 42, [], {}]) {
    strictEqual(typ, await new PollPromise(resolve => resolve(typ)));
  }
});

add_task(async function test_PollPromise_rethrowError() {
  let nevals = 0;
  let err;
  try {
    await PollPromise(() => {
      ++nevals;
      throw new Error();
    });
  } catch (e) {
    err = e;
  }
  equal(1, nevals);
  ok(err instanceof Error);
});

add_task(async function test_PollPromise_noTimeout() {
  let nevals = 0;
  await new PollPromise((resolve, reject) => {
    ++nevals;
    nevals < 100 ? reject() : resolve();
  });
  equal(100, nevals);
});

add_task(async function test_PollPromise_zeroTimeout() {
  // run at least once when timeout is 0
  let nevals = 0;
  let start = new Date().getTime();
  await new PollPromise(
    (resolve, reject) => {
      ++nevals;
      reject();
    },
    { timeout: 0 }
  );
  let end = new Date().getTime();
  equal(1, nevals);
  less(end - start, 500);
});

add_task(async function test_PollPromise_timeoutElapse() {
  let nevals = 0;
  let start = new Date().getTime();
  await new PollPromise(
    (resolve, reject) => {
      ++nevals;
      reject();
    },
    { timeout: 100 }
  );
  let end = new Date().getTime();
  lessOrEqual(nevals, 11);
  greaterOrEqual(end - start, 100);
});

add_task(async function test_PollPromise_interval() {
  let nevals = 0;
  await new PollPromise(
    (resolve, reject) => {
      ++nevals;
      reject();
    },
    { timeout: 100, interval: 100 }
  );
  equal(2, nevals);
});

add_task(async function test_PollPromise_resolve() {
  const log = Log.repository.getLogger("RemoteAgent");
  const appender = new MockAppender(new Log.BasicFormatter());
  appender.level = Log.Level.Info;
  log.addAppender(appender);

  const errorMessage = "PollingFailed";
  const timeout = 100;

  await new PollPromise(
    (resolve, reject) => {
      resolve();
    },
    { timeout, errorMessage }
  );
  Assert.equal(appender.messages.length, 0);

  await new PollPromise(
    (resolve, reject) => {
      reject();
    },
    { timeout, errorMessage: "PollingFailed" }
  );
  Assert.equal(appender.messages.length, 1);
  Assert.equal(appender.messages[0].level, Log.Level.Warn);
  Assert.equal(appender.messages[0].message, "PollingFailed after 100 ms");
});
