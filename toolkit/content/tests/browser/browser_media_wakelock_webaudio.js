/**
 * Test if wakelock can be required correctly when we play web audio. The
 * wakelock should only be required when web audio is audible.
 */
add_task(async function testCheckWakelockWhenChangeTabVisibility() {
  await checkWakelockWhenChangeTabVisibility({
    description: "playing audible web audio",
    lockAudio: true,
    lockVideo: false,
  });
  await checkWakelockWhenChangeTabVisibility({
    description: "suspended web audio",
    additionalParams: {
      suspend: true,
    },
    lockAudio: false,
    lockVideo: false,
  });
});

add_task(
  async function testBrieflyAudibleAudioContextReleasesAudioWakeLockWhenInaudible() {
    const tab = await BrowserTestUtils.openNewForegroundTab(
      window.gBrowser,
      "about:blank"
    );

    let audioWakeLock = getWakeLockState("audio-playing", true, true);
    let videoWakeLock = getWakeLockState("video-playing", false, true);

    info(`make a short noise on web audio`);
    await createWebAudioDocument(tab, { stopTimeOffset: 0.1 });
    await audioWakeLock.check();
    await videoWakeLock.check();

    audioWakeLock = getWakeLockState("audio-playing", false, true);
    videoWakeLock = getWakeLockState("video-playing", false, true);

    await audioWakeLock.check();
    await videoWakeLock.check();

    await BrowserTestUtils.removeTab(tab);
  }
);

/**
 * Following are helper functions.
 */
async function checkWakelockWhenChangeTabVisibility({
  description,
  additionalParams,
  lockAudio,
  lockVideo,
  elementIdForEnteringPIPMode,
}) {
  const originalTab = gBrowser.selectedTab;
  info(`start a new tab for '${description}'`);
  const mediaTab = await BrowserTestUtils.openNewForegroundTab(
    window.gBrowser,
    "about:blank"
  );
  let audioWakeLock = getWakeLockState("audio-playing", lockAudio, true);
  let videoWakeLock = getWakeLockState("video-playing", lockVideo, true);
  await createWebAudioDocument(mediaTab, additionalParams);
  await audioWakeLock.check();
  await videoWakeLock.check();

  info(`switch media tab to background`);
  audioWakeLock = getWakeLockState("audio-playing", lockAudio, false);
  videoWakeLock = getWakeLockState("video-playing", lockVideo, false);
  await BrowserTestUtils.switchTab(window.gBrowser, originalTab);
  await audioWakeLock.check();
  await videoWakeLock.check();

  info(`switch media tab to foreground again`);
  audioWakeLock = getWakeLockState("audio-playing", lockAudio, true);
  videoWakeLock = getWakeLockState("video-playing", lockVideo, true);
  await BrowserTestUtils.switchTab(window.gBrowser, mediaTab);
  await audioWakeLock.check();
  await videoWakeLock.check();

  info(`remove media tab`);
  BrowserTestUtils.removeTab(mediaTab);
}

function createWebAudioDocument(tab, { stopTimeOffset, suspend } = {}) {
  return SpecialPowers.spawn(
    tab.linkedBrowser,
    [suspend, stopTimeOffset],
    async (suspend, stopTimeOffset) => {
      // Create an oscillatorNode to produce sound.
      content.ac = new content.AudioContext();
      const ac = content.ac;
      const dest = ac.destination;
      const source = new content.OscillatorNode(ac);
      source.start(ac.currentTime);
      source.connect(dest);

      if (stopTimeOffset) {
        source.stop(ac.currentTime + 0.1);
      }

      if (suspend) {
        await content.ac.suspend();
      } else {
        while (ac.state != "running") {
          info(`wait until AudioContext starts running`);
          await new Promise(r => (ac.onstatechange = r));
        }
        info("AudioContext is running");
      }
    }
  );
}
