const maxNestingLevel = 5;
let expectedNestingLevel = 1;
let timer;
let isInterval = false;
let stopTimer = false;
let lastCall = 0;
let testStage = "ScriptLoaded";

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

  if (!stopTimer) {
    if (expectedNestingLevel === maxNestingLevel) {
      stopTimer = true;
    } else {
      expectedNestingLevel = expectedNestingLevel + 1;
    }
    if (!isInterval) {
      setTimeout(timerCallback, 0);
    }
    lastCall = now;
    return;
  }

  await Promise.resolve(true).then(() => {
    if (WorkerTestUtils.currentTimerNestingLevel() !== expectedNestingLevel) {
      postMessage({
        stage: testStage,
        status: "FAIL",
        msg: `Timer nesting level should be in effect for immediately resolved micro-tasks`,
      });
    }
  });
  if (now - lastCall < 3) {
    postMessage({
      stage: testStage,
      status: "FAIL",
      msg: `timer nesting level reaches the max nesting level(${maxNestingLevel}), interval time should be clamped at least 3, but got ${now -
        lastCall}`,
    });
  } else {
    postMessage({ stage: testStage, status: "PASS", msg: "" });
  }
  stopTimer = false;
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
