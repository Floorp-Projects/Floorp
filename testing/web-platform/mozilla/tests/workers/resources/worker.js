const maxNestingLevel = 5;
let expectedNestingLevel = 1;
let timer;
let isInterval = false;
let testStage = "ScriptLoaded";
let stopIncreaseExpectedLevel = false;
let startClampedTimeStamp = 0;
let startRepeatingClamped = false;
let repeatCount = 0;
let maxRepeatTimes = 10;

let timerCallback = async () => {
  let now = Date.now();
  if (WorkerTestUtils.currentTimerNestingLevel() !== expectedNestingLevel) {
    postMessage({
      stage: testStage,
      status: "FAIL",
      msg: `current timer nesting level is ${WorkerTestUtils.currentTimerNestingLevel()}, expected ${expectedNestingLevel}`,
    });
    if (isInterval) {
      clearInterval(timer);
    }
    return;
  }

  if (!stopIncreaseExpectedLevel) {
    if (expectedNestingLevel === maxNestingLevel) {
      stopIncreaseExpectedLevel = true;
      startClampedTimeStamp = now;
    } else {
      expectedNestingLevel = expectedNestingLevel + 1;
    }
    if (!isInterval) {
      setTimeout(timerCallback, 0);
    }
    return;
  }

  // This is the first time the timeout is clamped, checking if it is clamped
  // to at least 2ms.
  if (repeatCount === 0) {
    await Promise.resolve(true).then(() => {
      if (WorkerTestUtils.currentTimerNestingLevel() !== expectedNestingLevel) {
        postMessage({
          stage: testStage,
          status: "FAIL",
          msg: `Timer nesting level should be in effect for immediately resolved micro-tasks`,
        });
      }
    });
    if (now - startClampedTimeStamp < 2 ) {
      startRepeatingClamped = true;
    } else {
      postMessage({ stage: testStage, status: "PASS", msg: "" });
    }
  }

  // If the first clamped timeout is less than 2ms, start to repeat the clamped
  // timeout for 10 times. Then checking if total clamped time should be at least
  // 25ms.
  if (startRepeatingClamped) {
    if (repeatCount === 10) {
      if (now - startClampedTimeStamp < 25) {
        postMessage({
          stage: testStage,
          status: "FAIL",
          msg: `total clamped time of repeating ten times should be at least 25ms(${now - startClampedTimeStamp})`,
        });
      } else {
        postMessage({ stage: testStage, status: "PASS", msg: "" });
      }
    } else {
      repeatCount = repeatCount + 1;
      if (!isInterval) {
        setTimeout(timerCallback, 0);
      }
      return;
    }
  }

  // reset testing variables
  repeatCount = 0;
  startRepeatingClamped = false;
  stopIncreaseExpectedLevel = false;
  if (isInterval) {
    clearInterval(timer);
  }
};

onmessage = async e => {
  testStage = e.data;
  switch (e.data) {
    case "CheckInitialValue":
      if (WorkerTestUtils.currentTimerNestingLevel() === 0) {
        postMessage({ stage: testStage, status: "PASS", msg: "" });
      } else {
        postMessage({
          stage: testStage,
          status: "FAIL",
          msg: `current timer nesting level should be 0(${WorkerTestUtils.currentTimerNestingLevel()}) after top level script loaded.`,
        });
      }
      break;
    case "TestSetInterval":
      expectedNestingLevel = 1;
      isInterval = true;
      timer = setInterval(timerCallback, 0);
      break;
    case "TestSetTimeout":
      expectedNestingLevel = 1;
      isInterval = false;
      setTimeout(timerCallback, 0);
      break;
    case "CheckNoTimer":
      if (WorkerTestUtils.currentTimerNestingLevel() === 0) {
        postMessage({ stage: testStage, status: "PASS", msg: "" });
      } else {
        postMessage({
          stage: testStage,
          status: "FAIL",
          msg: `current timer nesting level should be 0(${WorkerTestUtils.currentTimerNestingLevel()}) when there is no timer in queue.`,
        });
      }

      break;
  }
};

postMessage({ stage: testStage, status: "PASS" });
