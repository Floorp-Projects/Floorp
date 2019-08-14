/**
 * This test is used for testing whether WebAudio can be started correctly in
 * different scenarios, such as
 * 1) site has existing 'autoplay-media' permission for allowing autoplay
 * 2) site has existing 'autoplay-media' permission for blocking autoplay
 * 3) site doesn't have permission, start audio context by calling resume() or
 *    AudioScheduledNode.start() after granting user activation.
 */
"use strict";

ChromeUtils.import("resource:///modules/SitePermissions.jsm", this);
const PAGE =
  "https://example.com/browser/toolkit/content/tests/browser/file_empty.html";

function setup_test_preference() {
  return SpecialPowers.pushPrefEnv({
    set: [
      ["media.autoplay.default", SpecialPowers.Ci.nsIAutoplay.BLOCKED],
      ["media.autoplay.enabled.user-gestures-needed", true],
      ["media.autoplay.block-webaudio", true],
      ["media.autoplay.block-event.enabled", true],
    ],
  });
}

function createAudioContext() {
  content.ac = new content.AudioContext();
  const ac = content.ac;

  ac.allowedToStart = new Promise(resolve => {
    ac.addEventListener(
      "statechange",
      function() {
        if (ac.state === "running") {
          resolve();
        }
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

async function resumeAudioContext(isAllowedToStart) {
  const ac = content.ac;
  const resumePromise = ac.resume();
  const blockedPromise = new Promise(resolve => {
    ac.addEventListener(
      "blocked",
      function() {
        resolve();
      },
      { once: true }
    );
  });

  if (isAllowedToStart) {
    await resumePromise;
    ok(true, `successfully resume AudioContext.`);
  } else {
    await blockedPromise;
    ok(true, `resume is blocked because AudioContext is not allowed to start.`);
  }
}

function startAudioContext(method) {
  const ac = content.ac;
  if (method == "AudioContext") {
    info(`using AudioContext.resume() to start AudioContext`);
    ac.resume();
    return;
  }
  info(`using ${method}.start() to start AudioContext`);
  let node;
  switch (method) {
    case "AudioBufferSourceNode":
      node = ac.createBufferSource();
      break;
    case "ConstantSourceNode":
      node = ac.createConstantSource();
      break;
    case "OscillatorNode":
      node = ac.createOscillator();
      break;
    default:
      ok(false, "undefined AudioScheduledSourceNode type");
      return;
  }
  node.connect(ac.destination);
  node.start();
}

async function testAutoplayExistingPermission({ name, permission }) {
  info(`- starting \"${name}\" -`);
  const tab = await BrowserTestUtils.openNewForegroundTab(
    window.gBrowser,
    PAGE
  );
  const browser = tab.linkedBrowser;

  info(`- set the 'autoplay-media' permission -`);
  const promptShow = () =>
    PopupNotifications.getNotification("autoplay-media", browser);
  SitePermissions.set(browser.currentURI, "autoplay-media", permission);
  ok(!promptShow(), `should not be showing permission prompt yet`);

  info(`- create audio context -`);
  loadFrameScript(browser, createAudioContext);

  info(`- check AudioContext status -`);
  const isAllowedToStart = permission === SitePermissions.ALLOW;
  await ContentTask.spawn(
    browser,
    isAllowedToStart,
    checkIfAudioContextIsAllowedToStart
  );
  await ContentTask.spawn(browser, isAllowedToStart, resumeAudioContext);

  info(`- remove tab -`);
  SitePermissions.remove(browser.currentURI, "autoplay-media");
  await BrowserTestUtils.removeTab(tab);
}

async function testAutoplayUnknownPermission({ name, method }) {
  info(`- starting \"${name}\" -`);
  const tab = await BrowserTestUtils.openNewForegroundTab(
    window.gBrowser,
    PAGE
  );
  const browser = tab.linkedBrowser;

  info(`- set the 'autoplay-media' permission to UNKNOWN -`);
  const promptShow = () =>
    PopupNotifications.getNotification("autoplay-media", browser);
  SitePermissions.set(
    browser.currentURI,
    "autoplay-media",
    SitePermissions.UNKNOWN
  );
  ok(!promptShow(), `should not be showing permission prompt yet`);

  info(`- create AudioContext which should not start -`);
  loadFrameScript(browser, createAudioContext);
  await ContentTask.spawn(browser, false, checkIfAudioContextIsAllowedToStart);

  info(`- simulate user activate the page -`);
  await ContentTask.spawn(browser, null, () => {
    content.document.notifyUserGestureActivation();
  });

  info(`- try to start AudioContext -`);
  await ContentTask.spawn(browser, method, startAudioContext);

  info(`- check AudioContext status -`);
  await ContentTask.spawn(
    browser,
    true /* allow to start */,
    checkIfAudioContextIsAllowedToStart
  );
  await ContentTask.spawn(
    browser,
    true /* allow to start */,
    resumeAudioContext
  );

  info(`- remove tab -`);
  SitePermissions.remove(browser.currentURI, "autoplay-media");
  await BrowserTestUtils.removeTab(tab);
}

add_task(async function start_tests() {
  info("- setup test preference -");
  await setup_test_preference();

  await testAutoplayExistingPermission({
    name: "Prexisting allow permission",
    permission: SitePermissions.ALLOW,
  });
  await testAutoplayExistingPermission({
    name: "Prexisting block permission",
    permission: SitePermissions.BLOCK,
  });
  const startMethods = [
    "AudioContext",
    "AudioBufferSourceNode",
    "ConstantSourceNode",
    "OscillatorNode",
  ];
  for (let method of startMethods) {
    await testAutoplayUnknownPermission({
      name:
        "Unknown permission and start AudioContext after granting user activation",
      method,
    });
  }
});
