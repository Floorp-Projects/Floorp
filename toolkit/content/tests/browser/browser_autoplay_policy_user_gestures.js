/* eslint-disable mozilla/no-arbitrary-setTimeout */

const VIDEO_PAGE = "https://example.com/browser/toolkit/content/tests/browser/file_video.html";

var UserGestures = {
  MOUSE_CLICK: "mouse-click",
  MOUSE_MOVE: "mouse-move",
  KEYBOARD_PRESS: "keyboard-press"
};

var UserGestureTests = [
  {type: UserGestures.MOUSE_CLICK, isActivationGesture: true},
  {type: UserGestures.MOUSE_MOVE, isActivationGesture: false},
  {type: UserGestures.KEYBOARD_PRESS, isActivationGesture: true}
];

function setup_test_preference() {
  return SpecialPowers.pushPrefEnv({"set": [
    ["media.autoplay.default", SpecialPowers.Ci.nsIAutoplay.BLOCKED],
    ["media.autoplay.enabled.user-gestures-needed", true],
    ["media.autoplay.block-webaudio", true],
    ["media.navigator.permission.fake", true]
  ]});
}

function simulateUserGesture(gesture, targetBrowser) {
  info(`- simulate ${gesture.type} event -`);
  switch (gesture.type) {
    case UserGestures.MOUSE_CLICK:
      return BrowserTestUtils.synthesizeMouseAtCenter("body", {button: 0},
                                                      targetBrowser);
    case UserGestures.MOUSE_MOVE:
      return BrowserTestUtils.synthesizeMouseAtCenter("body", {type: "mousemove"},
                                                      targetBrowser);
    case UserGestures.KEYBOARD_PRESS:
      return BrowserTestUtils.sendChar("a", targetBrowser);
    default:
      ok(false, "undefined user gesture");
      return false;
  }
}

async function test_play_without_user_gesture() {
  info("- open new tab -");
  let tab = await BrowserTestUtils.openNewForegroundTab(window.gBrowser,
                                                        "about:blank");
  tab.linkedBrowser.loadURI(VIDEO_PAGE);
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);

  async function check_autoplay_keyword() {
    info("- create an new autoplay video -");
    let video = content.document.createElement("video");
    video.src = "gizmo.mp4";
    video.autoplay = true;
    let canplayPromise = new Promise(function(resolve) {
      video.addEventListener("canplaythrough", function() {
        resolve();
      }, {once: true});
    });
    content.document.body.appendChild(video);

    info("- can't autoplay without user activation -");
    await canplayPromise;
    ok(video.paused, "video can't start without user input.");
  }
  await ContentTask.spawn(tab.linkedBrowser, null, check_autoplay_keyword);

  async function play_video() {
    let video = content.document.getElementById("v");
    info("- call play() without user activation -");
    await video.play().catch(function() {
      ok(video.paused, "video can't start play without user input.");
    });
  }
  await ContentTask.spawn(tab.linkedBrowser, null, play_video);

  info("- remove tab -");
  BrowserTestUtils.removeTab(tab);
}

async function test_play_with_user_gesture(gesture) {
  info("- open new tab -");
  let tab = await BrowserTestUtils.openNewForegroundTab(window.gBrowser,
                                                        "about:blank");
  tab.linkedBrowser.loadURI(VIDEO_PAGE);
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);

  info("- simulate user gesture -");
  await simulateUserGesture(gesture, tab.linkedBrowser);

  info("- call play() -");
  async function play_video(gesture) {
    let video = content.document.getElementById("v");
    try {
      await video.play();
      ok(gesture.isActivationGesture, "user gesture can activate the page");
      ok(!video.paused, "video starts playing.");
    } catch (e) {
      ok(!gesture.isActivationGesture, "user gesture can not activate the page");
      ok(video.paused, "video can not start playing.");
    }
  }

  await ContentTask.spawn(tab.linkedBrowser, gesture, play_video);

  info("- remove tab -");
  BrowserTestUtils.removeTab(tab);
}

function createAudioContext() {
  content.ac = new content.AudioContext();
  let ac = content.ac;
  ac.resumePromises = [];
  ac.stateChangePromise = new Promise(resolve => {
    ac.addEventListener("statechange", function() {
      resolve();
    }, {once: true});
  });
}

async function checking_audio_context_running_state() {
  let ac = content.ac;
  await new Promise(r => setTimeout(r, 2000));
  is(ac.state, "suspended", "audio context is still suspended");
}

function resume_without_expected_success() {
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

function resume_with_expected_success() {
  let ac = content.ac;
  ac.resumePromises.push(ac.resume());
  return Promise.all(ac.resumePromises).then(() => {
    ok(ac.state == "running", "audio context starts running");
  });
}

function callGUM(testParameters) {
  info("- calling gum with " + JSON.stringify(testParameters.constraints));
  if (testParameters.shouldAllowStartingContext) {
    // Because of the prefs we've set and passed, this is going to allow the
    // window to start an AudioContext synchronously.
    testParameters.constraints.fake = true;
    return content.navigator.mediaDevices.getUserMedia(testParameters.constraints);
  }

  // Call gUM, without sucess: we've made it so that only fake requests
  // succeed without permission, and this is requesting non-fake-devices. Return
  // a resolved promise so that the test continues, but the getUserMedia Promise
  // will never be resolved.
  // We do this to check that it's not merely calling gUM that allows starting
  // an AudioContext, it's having the Promise it return resolved successfuly,
  // because of saved permissions for an origin or explicit user consent using
  // the prompt.
  content.navigator.mediaDevices.getUserMedia(testParameters.constraints);
  return Promise.resolve();
}


async function test_webaudio_with_user_gesture(gesture) {
  info("- open new tab -");
  let tab = await BrowserTestUtils.openNewForegroundTab(window.gBrowser,
                                                        "about:blank");
  info("- create audio context -");
  // We want the same audio context to be used across different content
  // tasks, so it needs to be loaded by a frame script.
  let frameScript = createAudioContext;
  let mm = tab.linkedBrowser.messageManager;
  mm.loadFrameScript("data:,(" + frameScript.toString() + ")();", false);

  info("- check whether audio context starts running -");
  try {
    await ContentTask.spawn(tab.linkedBrowser, null,
                            checking_audio_context_running_state);
  } catch (error) {
    ok(false, error.toString());
  }

  info("- calling resume() -");
  try {
    await ContentTask.spawn(tab.linkedBrowser, null,
                            resume_without_expected_success);
  } catch (error) {
    ok(false, error.toString());
  }

  info("- simulate user gesture -");
  await simulateUserGesture(gesture, tab.linkedBrowser);

  info("- calling resume() again");
  try {
    let resumeFunc = gesture.isActivationGesture ?
      resume_with_expected_success :
      resume_without_expected_success;
    await ContentTask.spawn(tab.linkedBrowser, null, resumeFunc);
  } catch (error) {
    ok(false, error.toString());
  }

  info("- remove tab -");
  await BrowserTestUtils.removeTab(tab);
}

async function test_webaudio_with_gum(testParameters) {
  info("- open new tab -");
  let tab = await BrowserTestUtils.openNewForegroundTab(window.gBrowser,
                                                        "about:blank");
  info("- create audio context -");
  // We want the same audio context be used between different content
  // tasks, so it *must* be loaded by frame script.
  let frameScript = createAudioContext;
  let mm = tab.linkedBrowser.messageManager;
  mm.loadFrameScript("data:,(" + frameScript.toString() + ")();", false);

  info("- check whether audio context starts running -");
  try {
    await ContentTask.spawn(tab.linkedBrowser, null,
                            checking_audio_context_running_state);
  } catch (error) {
    ok(false, error.toString());
  }

  try {
    await ContentTask.spawn(tab.linkedBrowser, testParameters, callGUM);
  } catch (error) {
    ok(false, error.toString());
  }

  info("- calling resume() again");
  try {
    let resumeFunc = testParameters.shouldAllowStartingContext ?
      resume_with_expected_success :
      resume_without_expected_success;
    await ContentTask.spawn(tab.linkedBrowser, null, resumeFunc);
  } catch (error) {
    ok(false, error.toString());
  }

  info("- remove tab -");
  await BrowserTestUtils.removeTab(tab);
}

add_task(async function start_test() {
  info("- setup test preference -");
  await setup_test_preference();

  info("- test play when page doesn't be activated -");
  await test_play_without_user_gesture();

  info("- test play after page got user gesture -");
  for (let idx = 0; idx < UserGestureTests.length; idx++) {
    info("- test play after page got user gesture -");
    await test_play_with_user_gesture(UserGestureTests[idx]);

    info("- test web audio with user gesture -");
    await test_webaudio_with_user_gesture(UserGestureTests[idx]);
  }

  info("- test web audio with gUM success -");

  await test_webaudio_with_gum({constraints: { audio: true },
                                shouldAllowStartingContext: true});
  await test_webaudio_with_gum({constraints: { video: true },
                                shouldAllowStartingContext: true});
  await test_webaudio_with_gum({constraints: { video: true,
                                               audio: true },
                                shouldAllowStartingContext: true});

  await SpecialPowers.pushPrefEnv({"set": [
    ["media.navigator.permission.force", true],
  ]}).then(async function() {
    info("- test web audio with gUM denied -");
    await test_webaudio_with_gum({constraints: { video: true },
                                  shouldAllowStartingContext: false});
    await test_webaudio_with_gum({constraints: { audio: true },
                                  shouldAllowStartingContext: false});
    await test_webaudio_with_gum({constraints: { video: true,
                                                 audio: true },
                                  shouldAllowStartingContext: false});
  });
});
