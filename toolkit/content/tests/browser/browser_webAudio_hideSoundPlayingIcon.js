/**
 * This test is used to ensure the 'sound-playing' icon would not disappear after
 * sites call AudioContext.resume().
 */
"use strict";

function setup_test_preference() {
  return SpecialPowers.pushPrefEnv({
    set: [
      ["media.useAudioChannelService.testing", true],
      ["browser.tabs.delayHidingAudioPlayingIconMS", 0],
    ],
  });
}

function createAudioContext() {
  content.ac = new content.AudioContext();
  const ac = content.ac;
  const dest = ac.destination;
  const osc = ac.createOscillator();
  osc.connect(dest);
  osc.start();
}

async function resumeAudioContext() {
  const ac = content.ac;
  await ac.resume();
  ok(true, "AudioContext is resumed.");
}

async function testResumeRunningAudioContext() {
  info(`- create new tab -`);
  const tab = await BrowserTestUtils.openNewForegroundTab(
    window.gBrowser,
    "about:blank"
  );
  const browser = tab.linkedBrowser;

  info(`- create audio context -`);
  // We want the same audio context to be used across different content
  // tasks, so it needs to be loaded by a frame script.
  const mm = tab.linkedBrowser.messageManager;
  mm.loadFrameScript("data:,(" + createAudioContext.toString() + ")();", false);

  info(`- wait for 'sound-playing' icon showing -`);
  await waitForTabPlayingEvent(tab, true);

  info(`- resume AudioContext -`);
  await ContentTask.spawn(browser, null, resumeAudioContext);

  info(`- 'sound-playing' icon should still exist -`);
  await waitForTabPlayingEvent(tab, true);

  info(`- remove tab -`);
  await BrowserTestUtils.removeTab(tab);
}

add_task(async function start_test() {
  info("- setup test preference -");
  await setup_test_preference();

  info("- start testing -");
  await testResumeRunningAudioContext();
});
