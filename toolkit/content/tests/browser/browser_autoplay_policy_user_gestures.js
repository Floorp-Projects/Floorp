/* eslint-disable mozilla/no-arbitrary-setTimeout */

const VIDEO_PAGE =
  "https://example.com/browser/toolkit/content/tests/browser/file_video.html";

const UserGestures = {
  MOUSE_CLICK: "mouse-click",
  MOUSE_MOVE: "mouse-move",
  KEYBOARD_PRESS: "keyboard-press",
};

const UserGestureTests = [
  { type: UserGestures.MOUSE_CLICK, isActivationGesture: true },
  { type: UserGestures.MOUSE_MOVE, isActivationGesture: false },
  // test different keycode here. printable key, non-printable key and other
  // special keys.
  {
    type: UserGestures.KEYBOARD_PRESS,
    isActivationGesture: true,
    keyCode: "a",
  },
  {
    type: UserGestures.KEYBOARD_PRESS,
    isActivationGesture: false,
    keyCode: "VK_ESCAPE",
  },
  {
    type: UserGestures.KEYBOARD_PRESS,
    isActivationGesture: true,
    keyCode: "VK_RETURN",
  },
  {
    type: UserGestures.KEYBOARD_PRESS,
    isActivationGesture: true,
    keyCode: "VK_SPACE",
  },
];

/**
 * This test is used to ensure we would stop blocking autoplay after document
 * has been activated by user gestures. We would treat mouse clicking, key board
 * pressing (printable keys or carriage return) as valid user gesture input.
 */
add_task(async function startTestUserGestureInput() {
  info("- setup test preference -");
  await setupTestPreferences();

  info("- test play when page doesn't be activated -");
  await testPlayWithoutUserGesture();

  info("- test play after page got user gesture -");
  for (let idx = 0; idx < UserGestureTests.length; idx++) {
    info("- test play after page got user gesture -");
    await testPlayWithUserGesture(UserGestureTests[idx]);

    info("- test web audio with user gesture -");
    await testWebAudioWithUserGesture(UserGestureTests[idx]);
  }
});

/**
 * testing helper functions
 */
function setupTestPreferences() {
  return SpecialPowers.pushPrefEnv({
    set: [
      ["media.autoplay.default", SpecialPowers.Ci.nsIAutoplay.BLOCKED],
      ["media.autoplay.enabled.user-gestures-needed", true],
      ["media.autoplay.block-event.enabled", true],
      ["media.autoplay.block-webaudio", true],
      ["media.navigator.permission.fake", true],
    ],
  });
}

function simulateUserGesture(gesture, targetBrowser) {
  info(`- simulate ${gesture.type} event -`);
  switch (gesture.type) {
    case UserGestures.MOUSE_CLICK:
      return BrowserTestUtils.synthesizeMouseAtCenter(
        "body",
        { button: 0 },
        targetBrowser
      );
    case UserGestures.MOUSE_MOVE:
      return BrowserTestUtils.synthesizeMouseAtCenter(
        "body",
        { type: "mousemove" },
        targetBrowser
      );
    case UserGestures.KEYBOARD_PRESS:
      info(`- keycode=${gesture.keyCode} -`);
      return BrowserTestUtils.synthesizeKey(gesture.keyCode, {}, targetBrowser);
    default:
      ok(false, "undefined user gesture");
      return false;
  }
}

async function testPlayWithoutUserGesture() {
  info("- open new tab -");
  let tab = await BrowserTestUtils.openNewForegroundTab(
    window.gBrowser,
    "about:blank"
  );
  BrowserTestUtils.loadURI(tab.linkedBrowser, VIDEO_PAGE);
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);

  async function checkAutoplayKeyword() {
    info("- create an new autoplay video -");
    let video = content.document.createElement("video");
    video.src = "gizmo.mp4";
    video.autoplay = true;
    let canplayPromise = new Promise(function(resolve) {
      video.addEventListener(
        "canplaythrough",
        function() {
          resolve();
        },
        { once: true }
      );
    });
    content.document.body.appendChild(video);

    info("- can't autoplay without user activation -");
    await canplayPromise;
    ok(video.paused, "video can't start without user input.");
  }
  await ContentTask.spawn(tab.linkedBrowser, null, checkAutoplayKeyword);

  async function playVideo() {
    let video = content.document.getElementById("v");
    info("- call play() without user activation -");
    await video.play().catch(function() {
      ok(video.paused, "video can't start play without user input.");
    });
  }
  await ContentTask.spawn(tab.linkedBrowser, null, playVideo);

  info("- remove tab -");
  BrowserTestUtils.removeTab(tab);
}

async function testPlayWithUserGesture(gesture) {
  info("- open new tab -");
  let tab = await BrowserTestUtils.openNewForegroundTab(
    window.gBrowser,
    "about:blank"
  );
  BrowserTestUtils.loadURI(tab.linkedBrowser, VIDEO_PAGE);
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);

  info("- simulate user gesture -");
  await simulateUserGesture(gesture, tab.linkedBrowser);

  info("- call play() -");
  async function playVideo(gesture) {
    let video = content.document.getElementById("v");
    try {
      await video.play();
      ok(gesture.isActivationGesture, "user gesture can activate the page");
      ok(!video.paused, "video starts playing.");
    } catch (e) {
      ok(
        !gesture.isActivationGesture,
        "user gesture can not activate the page"
      );
      ok(video.paused, "video can not start playing.");
    }
  }

  await ContentTask.spawn(tab.linkedBrowser, gesture, playVideo);

  info("- remove tab -");
  BrowserTestUtils.removeTab(tab);
}

function createAudioContext() {
  content.ac = new content.AudioContext();
  let ac = content.ac;
  ac.resumePromises = [];
  ac.stateChangePromise = new Promise(resolve => {
    ac.addEventListener(
      "statechange",
      function() {
        resolve();
      },
      { once: true }
    );
  });
  ac.notAllowedToStart = new Promise(resolve => {
    ac.addEventListener(
      "blocked",
      function() {
        resolve();
      },
      { once: true }
    );
  });
}

async function checkingAudioContextRunningState() {
  let ac = content.ac;
  await ac.notAllowedToStart;
  ok(ac.state === "suspended", `AudioContext is not started yet.`);
}

function resumeWithoutExpectedSuccess() {
  let ac = content.ac;
  let promise = ac.resume();
  ac.resumePromises.push(promise);
  return new Promise((resolve, reject) => {
    setTimeout(() => {
      if (ac.state == "suspended") {
        ok(true, "audio context is still suspended");
        resolve();
      } else {
        reject("audio context should not be allowed to start");
      }
    }, 2000);
  });
}

function resumeWithExpectedSuccess() {
  let ac = content.ac;
  ac.resumePromises.push(ac.resume());
  return Promise.all(ac.resumePromises).then(() => {
    ok(ac.state == "running", "audio context starts running");
  });
}

async function testWebAudioWithUserGesture(gesture) {
  info("- open new tab -");
  let tab = await BrowserTestUtils.openNewForegroundTab(
    window.gBrowser,
    "about:blank"
  );
  info("- create audio context -");
  // We want the same audio context to be used across different content
  // tasks, so it needs to be loaded by a frame script.
  const mm = tab.linkedBrowser.messageManager;
  mm.loadFrameScript("data:,(" + createAudioContext.toString() + ")();", false);

  info("- check whether audio context starts running -");
  try {
    await ContentTask.spawn(
      tab.linkedBrowser,
      null,
      checkingAudioContextRunningState
    );
  } catch (error) {
    ok(false, error.toString());
  }

  info("- calling resume() -");
  try {
    await ContentTask.spawn(
      tab.linkedBrowser,
      null,
      resumeWithoutExpectedSuccess
    );
  } catch (error) {
    ok(false, error.toString());
  }

  info("- simulate user gesture -");
  await simulateUserGesture(gesture, tab.linkedBrowser);

  info("- calling resume() again");
  try {
    let resumeFunc = gesture.isActivationGesture
      ? resumeWithExpectedSuccess
      : resumeWithoutExpectedSuccess;
    await ContentTask.spawn(tab.linkedBrowser, null, resumeFunc);
  } catch (error) {
    ok(false, error.toString());
  }

  info("- remove tab -");
  await BrowserTestUtils.removeTab(tab);
}
