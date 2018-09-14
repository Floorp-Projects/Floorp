/**
 * This test is used for testing whether WebAudio can be started correctly in
 * different scenarios, such as
 * 1) site has existing 'autoplay-media' permission for allowing autoplay
 * 2) site has existing 'autoplay-media' permission for blocking autoplay
 * 3) site doesn't have permission, user clicks 'allow' button on the doorhanger
 * 4) site doesn't have permission, user clicks 'deny' button on the doorhanger
 * 5) site doesn't have permission, user ignores the doorhanger
 */
"use strict";

ChromeUtils.import("resource:///modules/SitePermissions.jsm", this);
const PAGE = "https://example.com/browser/toolkit/content/tests/browser/file_empty.html";

function setup_test_preference() {
  return SpecialPowers.pushPrefEnv({"set": [
    ["media.autoplay.default", SpecialPowers.Ci.nsIAutoplay.PROMPT],
    ["media.autoplay.enabled.user-gestures-needed", true],
    ["media.autoplay.ask-permission", true],
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
    ok(true, `AudioContext is running.`);
  } else {
    await ac.notAllowedToStart;
    ok(true, `AudioContext is not started yet.`);
  }
}

async function resumeAudioContext(isAllowedToStart) {
  const ac = content.ac;
  const resumePromise = ac.resume();
  const blockedPromise = new Promise(resolve => {
    ac.addEventListener("blocked", function() {
      resolve();
    }, {once: true});
  });

  if (isAllowedToStart) {
    await resumePromise;
    ok(ac.state === "running", `AudioContext is running.`);
  } else {
    await blockedPromise;
    ok(ac.state === "suspended", `AudioContext is suspended.`);
  }
}

function checkAudioContextState(state) {
  ok(content.ac.state === state,
     `AudioContext state is ${content.ac.state}, expected state is ${state}`);
}

function connectAudibleNodeToContext() {
  info(`- connect audible node to context graph -`);
  const ac = content.ac;
  const dest = ac.destination;
  const osc = ac.createOscillator();
  osc.connect(dest);
  osc.start();
}

async function testAutoplayExistingPermission(args) {
  info(`- starting \"${args.name}\" -`);
  const tab = await BrowserTestUtils.openNewForegroundTab(window.gBrowser, PAGE);
  const browser = tab.linkedBrowser;

  info(`- set the permission -`);
  const promptShow = () =>
    PopupNotifications.getNotification("autoplay-media", browser);
  SitePermissions.set(browser.currentURI, "autoplay-media", args.permission);
  ok(!promptShow(), `should not be showing permission prompt yet`);

  info(`- create audio context -`);
  // We want the same audio context to be used across different content
  // tasks, so it needs to be loaded by a frame script.
  const mm = tab.linkedBrowser.messageManager;
  mm.loadFrameScript("data:,(" + createAudioContext.toString() + ")();", false);

  info(`- check AudioContext status -`);
  const isAllowedToStart = args.permission === SitePermissions.ALLOW;
  await ContentTask.spawn(browser, isAllowedToStart,
                          checkIfAudioContextIsAllowedToStart);
  await ContentTask.spawn(browser, isAllowedToStart,
                          resumeAudioContext);

  info(`- remove tab -`);
  SitePermissions.remove(browser.currentURI, "autoplay-media");
  await BrowserTestUtils.removeTab(tab);
}

async function testAutoplayUnknownPermission(args) {
  info(`- starting \"${args.name}\" -`);
  const tab = await BrowserTestUtils.openNewForegroundTab(window.gBrowser, PAGE);
  const browser = tab.linkedBrowser;

  info(`- set the 'autoplay-media' permission -`);
  const promptShow = () =>
    PopupNotifications.getNotification("autoplay-media", browser);
  SitePermissions.set(browser.currentURI, "autoplay-media", SitePermissions.UNKNOWN);
  ok(!promptShow(), `should not be showing permission prompt yet`);

  info(`- create audio context -`);
  const popupShow = BrowserTestUtils.waitForEvent(PopupNotifications.panel, "popupshown");
  // We want the same audio context to be used across different content
  // tasks, so it needs to be loaded by a frame script.
  const mm = tab.linkedBrowser.messageManager;
  mm.loadFrameScript("data:,(" + createAudioContext.toString() + ")();", false);
  await popupShow;
  ok(promptShow(), `should now be showing permission prompt`);

  info(`- AudioContext should not be started before user responds to doorhanger -`);
  await ContentTask.spawn(browser, "suspended",
                          checkAudioContextState);

  if (args.ignoreDoorhanger) {
    const popupHide = BrowserTestUtils.waitForEvent(PopupNotifications.panel, "popuphidden");
    await ContentTask.spawn(browser, null, () => {
      info(`- user ingores the doorhanger and interacts with page directly -`);
      content.document.notifyUserGestureActivation();
    });

    await ContentTask.spawn(browser, true,
                            resumeAudioContext);
    ok(promptShow(), `doorhanger would only be dismissed when audible media starts`);
    await ContentTask.spawn(browser, null,
                            connectAudibleNodeToContext);
    await popupHide;
    ok(true, `doorhanger should dismiss after AudioContext starts audible`);
  } else {
    info(`- simulate clicking button on doorhanger-`);
    if (args.button == "allow") {
      PopupNotifications.panel.firstElementChild.button.click();
    } else if (args.button == "block") {
      PopupNotifications.panel.firstChild.secondaryButton.click();
    } else {
      ok(false, `Invalid button field`);
    }

    info(`- check AudioContext status -`);
    const isAllowedToStart = args.button === "allow";
    await ContentTask.spawn(browser, isAllowedToStart,
                            checkIfAudioContextIsAllowedToStart);
    await ContentTask.spawn(browser, isAllowedToStart,
                            resumeAudioContext);
  }

  info(`- remove tab -`);
  SitePermissions.remove(browser.currentURI, "autoplay-media");
  await BrowserTestUtils.removeTab(tab);
}

add_task(async function start_test() {
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
  await testAutoplayUnknownPermission({
    name: "Unknown permission and click allow button on doorhanger",
    button: "allow",
  });
  await testAutoplayUnknownPermission({
    name: "Unknown permission and click block button on doorhanger",
    button: "block",
  });
  await testAutoplayUnknownPermission({
    name: "Unknown permission and ignore doorhanger",
    ignoreDoorhanger: true,
  });
});
