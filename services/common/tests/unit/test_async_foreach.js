/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const {Async} = ChromeUtils.import("resource://services-common/async.js");
const {Services} = ChromeUtils.import("resource://gre/modules/Services.jsm");
const {sinon} = ChromeUtils.import("resource://testing-common/Sinon.jsm");

function makeArray(length) {
  // Start at 1 so that we can just divide by yieldEvery to get the expected
  // call count. (we exp)
  return Array.from({ length }, (v, i) => i + 1);
}

// Adjust if we ever change the default.
const DEFAULT_YIELD_EVERY = 50;

add_task(async function testYields() {
  let spy = sinon.spy(Async, "promiseYield");
  try {
    await Async.yieldingForEach(makeArray(DEFAULT_YIELD_EVERY * 2), element => {
      // The yield will happen *after* this function is ran.
      Assert.equal(spy.callCount, Math.floor((element - 1) / DEFAULT_YIELD_EVERY));
    });
  } finally {
    spy.restore();
  }
});

add_task(async function testExistingYieldState() {
  const yieldState = Async.yieldState(DEFAULT_YIELD_EVERY);

  for (let i = 0; i < 15; i++) {
    Assert.equal(yieldState.shouldYield(), false);
  }

  let spy = sinon.spy(Async, "promiseYield");

  try {
    await Async.yieldingForEach(makeArray(DEFAULT_YIELD_EVERY * 2), element => {
      Assert.equal(spy.callCount, Math.floor((element + 15 - 1) / DEFAULT_YIELD_EVERY));
    }, yieldState);
  } finally {
    spy.restore();
  }
});

add_task(async function testEarlyReturn() {
  let lastElement = 0;
  await Async.yieldingForEach(makeArray(DEFAULT_YIELD_EVERY), element => {
    lastElement = element;
    return element === 10;
  });

  Assert.equal(lastElement, 10);
});

add_task(async function testEaryReturnAsync() {
  let lastElement = 0;
  await Async.yieldingForEach(makeArray(DEFAULT_YIELD_EVERY), async (element) => {
    lastElement = element;
    return element === 10;
  });

  Assert.equal(lastElement, 10);
});

add_task(async function testEarlyReturnPromise() {
  let lastElement = 0;
  await Async.yieldingForEach(makeArray(DEFAULT_YIELD_EVERY), element => {
    lastElement = element;
    return new Promise(resolve => resolve(element === 10));
  });

  Assert.equal(lastElement, 10);
});
