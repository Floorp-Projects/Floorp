/**
 * Test if wakelock can be required correctly when we play web audio. The
 * wakelock should only be required when web audio is audible.
 */

const AUDIO_WAKELOCK_NAME = "audio-playing";
const VIDEO_WAKELOCK_NAME = "video-playing";

add_task(async function testCheckAudioWakelockWhenChangeTabVisibility() {
  await checkWakelockWhenChangeTabVisibility({
    description: "playing audible web audio",
    needLock: true,
  });
  await checkWakelockWhenChangeTabVisibility({
    description: "suspended web audio",
    additionalParams: {
      suspend: true,
    },
    needLock: false,
  });
});

add_task(
  async function testBrieflyAudibleAudioContextReleasesAudioWakeLockWhenInaudible() {
    const tab = await BrowserTestUtils.openNewForegroundTab(
      window.gBrowser,
      "about:blank"
    );

    info(`make a short noise on web audio`);
    await Promise.all([
      // As the sound would only happen for a really short period, calling
      // checking wakelock first helps to ensure that we won't miss that moment.
      waitForExpectedWakeLockState(AUDIO_WAKELOCK_NAME, {
        needLock: true,
        isForegroundLock: true,
      }),
      createWebAudioDocument(tab, { stopTimeOffset: 0.1 }),
    ]);
    await ensureNeverAcquireVideoWakelock();

    info(`audio wakelock should be released after web audio becomes silent`);
    await waitForExpectedWakeLockState(AUDIO_WAKELOCK_NAME, false, {
      needLock: false,
    });
    await ensureNeverAcquireVideoWakelock();

    await BrowserTestUtils.removeTab(tab);
  }
);

/**
 * Following are helper functions.
 */
async function checkWakelockWhenChangeTabVisibility({
  description,
  additionalParams,
  needLock,
}) {
  const originalTab = gBrowser.selectedTab;
  info(`start a new tab for '${description}'`);
  const mediaTab = await BrowserTestUtils.openNewForegroundTab(
    window.gBrowser,
    "about:blank"
  );
  await createWebAudioDocument(mediaTab, additionalParams);
  await waitForExpectedWakeLockState(AUDIO_WAKELOCK_NAME, {
    needLock,
    isForegroundLock: true,
  });
  await ensureNeverAcquireVideoWakelock();

  info(`switch media tab to background`);
  await BrowserTestUtils.switchTab(window.gBrowser, originalTab);
  await waitForExpectedWakeLockState(AUDIO_WAKELOCK_NAME, {
    needLock,
    isForegroundLock: false,
  });
  await ensureNeverAcquireVideoWakelock();

  info(`switch media tab to foreground again`);
  await BrowserTestUtils.switchTab(window.gBrowser, mediaTab);
  await waitForExpectedWakeLockState(AUDIO_WAKELOCK_NAME, {
    needLock,
    isForegroundLock: true,
  });
  await ensureNeverAcquireVideoWakelock();

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

function ensureNeverAcquireVideoWakelock() {
  // Web audio won't play any video, we never need video wakelock.
  return waitForExpectedWakeLockState(VIDEO_WAKELOCK_NAME, { needLock: false });
}
