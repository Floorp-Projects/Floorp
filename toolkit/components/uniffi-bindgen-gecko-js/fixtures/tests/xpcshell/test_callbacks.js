/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const { logEvenNumbers, logEvenNumbersMainThread } = ChromeUtils.importESModule(
  "resource://gre/modules/RustFixtureCallbacks.sys.mjs"
);

class Logger {
  constructor() {
    this.messages = [];
    this.finishedPromise = new Promise((resolve, reject) => {
      this.finishedResolve = resolve;
      this.finishedReject = reject;
    });
  }

  log(message) {
    this.messages.push(message);
  }

  finished() {
    this.finishedResolve(true);
  }

  async waitForFinish() {
    // Set a timeout to avoid hanging the tests if the Rust code fails to call finished().
    do_timeout(2000, () =>
      this.finishedReject("Timeout waiting for finished()")
    );
    return this.finishedPromise;
  }
}

add_task(async function testLogEvenNumbers() {
  async function runTest(logEvenNumbersFunc) {
    const logger = new Logger();
    logEvenNumbersFunc(logger, [1, 1, 2, 3, 5, 8, 13]);
    await logger.waitForFinish();
    Assert.deepEqual(logger.messages, [
      "Saw even number: 2",
      "Saw even number: 8",
    ]);
  }

  await runTest(logEvenNumbers);
  await runTest(logEvenNumbersMainThread);
});
