/**
 * Test whether the wakelock state is correct under different situations. However,
 * the lock state of power manager doesn't equal to the actual platform lock.
 * Now we don't have any way to detect whether platform lock is set correctly or
 * not, but we can at least make sure the specific topic's state in power manager
 * is correct.
 */
"use strict";

// Import this in order to use `triggerPictureInPicture()`.
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/toolkit/components/pictureinpicture/tests/head.js",
  this
);

const LOCATION = "https://example.com/browser/toolkit/content/tests/browser/";
const AUDIO_WAKELOCK_NAME = "audio-playing";
const VIDEO_WAKELOCK_NAME = "video-playing";

add_task(async function testCheckWakelockWhenChangeTabVisibility() {
  await checkWakelockWhenChangeTabVisibility({
    description: "playing video",
    url: "file_video.html",
    lockAudio: true,
    lockVideo: true,
  });
  await checkWakelockWhenChangeTabVisibility({
    description: "playing muted video",
    url: "file_video.html",
    additionalParams: {
      muted: true,
    },
    lockAudio: false,
    lockVideo: true,
  });
  await checkWakelockWhenChangeTabVisibility({
    description: "playing volume=0 video",
    url: "file_video.html",
    additionalParams: {
      volume: 0.0,
    },
    lockAudio: false,
    lockVideo: true,
  });
  await checkWakelockWhenChangeTabVisibility({
    description: "playing video without audio in it",
    url: "file_videoWithoutAudioTrack.html",
    lockAudio: false,
    lockVideo: false,
  });
  await checkWakelockWhenChangeTabVisibility({
    description: "playing audio in video element",
    url: "file_videoWithAudioOnly.html",
    lockAudio: true,
    lockVideo: false,
  });
  await checkWakelockWhenChangeTabVisibility({
    description: "playing audio in audio element",
    url: "file_mediaPlayback2.html",
    lockAudio: true,
    lockVideo: false,
  });
  await checkWakelockWhenChangeTabVisibility({
    description: "playing video from media stream with audio and video tracks",
    url: "browser_mediaStreamPlayback.html",
    lockAudio: true,
    lockVideo: true,
  });
  await checkWakelockWhenChangeTabVisibility({
    description: "playing video from media stream without audio track",
    url: "browser_mediaStreamPlaybackWithoutAudio.html",
    lockAudio: true,
    lockVideo: true,
  });
});

/**
 * Following are helper functions.
 */
async function checkWakelockWhenChangeTabVisibility({
  description,
  url,
  additionalParams,
  lockAudio,
  lockVideo,
}) {
  const originalTab = gBrowser.selectedTab;
  info(`start a new tab for '${description}'`);
  const mediaTab = await BrowserTestUtils.openNewForegroundTab(
    window.gBrowser,
    LOCATION + url
  );

  info(`wait for media starting playing`);
  await waitUntilVideoStarted(mediaTab, additionalParams);
  await waitForExpectedWakeLockState(AUDIO_WAKELOCK_NAME, {
    needLock: lockAudio,
    isForegroundLock: true,
  });
  await waitForExpectedWakeLockState(VIDEO_WAKELOCK_NAME, {
    needLock: lockVideo,
    isForegroundLock: true,
  });

  info(`switch media tab to background`);
  await BrowserTestUtils.switchTab(window.gBrowser, originalTab);
  await waitForExpectedWakeLockState(AUDIO_WAKELOCK_NAME, {
    needLock: lockAudio,
    isForegroundLock: false,
  });
  await waitForExpectedWakeLockState(VIDEO_WAKELOCK_NAME, {
    needLock: lockVideo,
    isForegroundLock: false,
  });

  info(`switch media tab to foreground again`);
  await BrowserTestUtils.switchTab(window.gBrowser, mediaTab);
  await waitForExpectedWakeLockState(AUDIO_WAKELOCK_NAME, {
    needLock: lockAudio,
    isForegroundLock: true,
  });
  await waitForExpectedWakeLockState(VIDEO_WAKELOCK_NAME, {
    needLock: lockVideo,
    isForegroundLock: true,
  });

  info(`remove tab`);
  if (mediaTab.PIPWindow) {
    await BrowserTestUtils.closeWindow(mediaTab.PIPWindow);
  }
  BrowserTestUtils.removeTab(mediaTab);
}

async function waitUntilVideoStarted(tab, { muted, volume } = {}) {
  await SpecialPowers.spawn(
    tab.linkedBrowser,
    [muted, volume],
    async (muted, volume) => {
      const video = content.document.getElementById("v");
      if (!video) {
        ok(false, "can't get media element!");
        return;
      }
      if (muted) {
        video.muted = muted;
      }
      if (volume !== undefined) {
        video.volume = volume;
      }
      ok(
        await video.play().then(
          () => true,
          () => false
        ),
        `video started playing.`
      );
    }
  );
}
