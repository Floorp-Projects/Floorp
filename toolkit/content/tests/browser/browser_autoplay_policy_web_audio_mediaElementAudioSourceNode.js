/**
 * This test is used to ensure blocked AudioContext would be resumed when the
 * source media element of MediaElementAudioSouceNode which has been created and
 * connected to destinationnode has started.
 */
"use strict";

const PAGE = "https://example.com/browser/toolkit/content/tests/browser/file_video.html";

function setup_test_preference() {
  return SpecialPowers.pushPrefEnv({"set": [
    ["media.autoplay.default", SpecialPowers.Ci.nsIAutoplay.BLOCKED],
    ["media.autoplay.enabled.user-gestures-needed", true],
    ["media.autoplay.block-webaudio", true],
    ["media.autoplay.block-event.enabled", true],
  ]});
}

function createAudioContext() {
  content.ac = new content.AudioContext();
  const ac = content.ac;

  ac.allowedToStart = new Promise(resolve => {
    ac.addEventListener("statechange", function() {
      if (ac.state === "running") {
        resolve();
      }
    }, {once: true});
  });

  ac.notAllowedToStart = new Promise(resolve => {
    ac.addEventListener("blocked", function() {
      resolve();
    }, {once: true});
  });
}

async function checkIfAudioContextIsAllowedToStart(isAllowedToStart) {
  const ac = content.ac;
  if (isAllowedToStart) {
    await ac.allowedToStart;
    ok(ac.state === "running", `AudioContext is running.`);
  } else {
    await ac.notAllowedToStart;
    ok(ac.state === "suspended", `AudioContext is not started yet.`);
  }
}

async function createMediaElementSourceNode() {
  const ac = content.ac;
  const video = content.document.getElementById("v");
  if (!video) {
    ok(false, `can not get video element`);
  }
  let source = ac.createMediaElementSource(video);
  source.connect(ac.destination);
}

async function playVideo() {
  const video = content.document.getElementById("v");
  if (!video) {
    ok(false, `can not get video element`);
  }
  // simulate user gesture in order to start video.
  content.document.notifyUserGestureActivation();
  ok(await video.play().then(() => true, () => false), `video started playing`);
}

async function testResumeAudioContextWhenMediaElementSourceStarted() {
  const tab = await BrowserTestUtils.openNewForegroundTab(window.gBrowser, PAGE);
  const browser = tab.linkedBrowser;

  info(`- create audio context -`);
  loadFrameScript(browser, createAudioContext);

  info(`- check AudioContext status -`);
  let isAllowedToStart = false;
  await ContentTask.spawn(browser, isAllowedToStart,
                          checkIfAudioContextIsAllowedToStart);

  info(`- create and start MediaElementAudioSourceNode -`);
  await ContentTask.spawn(browser, null, createMediaElementSourceNode);
  await ContentTask.spawn(browser, null, playVideo);

  info(`- AudioContext should be resumed after MediaElementAudioSourceNode started -`);
  isAllowedToStart = true;
  await ContentTask.spawn(browser, isAllowedToStart,
                          checkIfAudioContextIsAllowedToStart);

  info(`- remove tab -`);
  await BrowserTestUtils.removeTab(tab);
}

add_task(async function start_tests() {
  info(`- setup test preference -`);
  await setup_test_preference();

  info(`- start 'testResumeAudioContextWhenMediaElementSourceStarted' -`);
  await testResumeAudioContextWhenMediaElementSourceStarted();
});
