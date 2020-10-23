/**
 * Test whether the wakelock state is correct under different situations. However,
 * the lock state of power manager doesn't equal to the actual platform lock.
 * Now we don't have any way to detect whether platform lock is set correctly or
 * not, but we can at least make sure the specific topic's state in power manager
 * is correct.
 */
"use strict";

// Import this in order to use `triggerPictureInPicture()`.
/* import-globals-from ../../../../toolkit/components/pictureinpicture/tests/head.js */
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/toolkit/components/pictureinpicture/tests/head.js",
  this
);

const LOCATION = "https://example.com/browser/toolkit/content/tests/browser/";

const powerManagerService = Cc["@mozilla.org/power/powermanagerservice;1"];
const powerManager = powerManagerService.getService(Ci.nsIPowerManagerService);

function wakeLockObserved(observeTopic, checkFn) {
  return new Promise(resolve => {
    function wakeLockListener() {}
    wakeLockListener.prototype = {
      QueryInterface: ChromeUtils.generateQI(["nsIDOMMozWakeLockListener"]),
      callback(topic, state) {
        if (topic == observeTopic && checkFn(state)) {
          powerManager.removeWakeLockListener(wakeLockListener.prototype);
          resolve();
        }
      },
    };
    powerManager.addWakeLockListener(wakeLockListener.prototype);
  });
}

function getWakeLockState(topic, needLock, isTabInForeground) {
  const tabState = isTabInForeground ? "foreground" : "background";
  return {
    check: async () => {
      if (needLock) {
        const expectedLockState = `locked-${tabState}`;
        if (powerManager.getWakeLockState(topic) != expectedLockState) {
          await wakeLockObserved(topic, state => state == expectedLockState);
        }
        ok(true, `requested '${topic}' wakelock in ${tabState}`);
      } else {
        const lockState = powerManager.getWakeLockState(topic);
        info(`topic=${topic}, state=${lockState}`);
        ok(lockState == "unlocked", `doesn't request lock for '${topic}'`);
      }
    },
  };
}

async function waitUntilVideoStarted({ muted, volume } = {}) {
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

async function initializeWebAudio({ suspend } = {}) {
  if (suspend) {
    await content.ac.suspend();
  } else {
    const ac = content.ac;
    if (ac.state == "running") {
      return;
    }
    while (ac.state != "running") {
      await new Promise(r => (ac.onstatechange = r));
    }
  }
}

function webaudioDocument() {
  content.ac = new content.AudioContext();
  const ac = content.ac;
  const dest = ac.destination;
  const source = new content.OscillatorNode(ac);
  source.start(ac.currentTime);
  source.connect(dest);
}

async function test_media_wakelock({
  description,
  urlOrFunction,
  additionalParams,
  lockAudio,
  lockVideo,
  elementIdForEnteringPIPMode,
}) {
  info(`- start a new test for '${description}' -`);
  info(`- open new foreground tab -`);
  var url;
  if (typeof urlOrFunction == "string") {
    url = LOCATION + urlOrFunction;
  } else {
    url = "about:blank";
  }
  const tab = await BrowserTestUtils.openNewForegroundTab(window.gBrowser, url);
  const browser = tab.linkedBrowser;

  if (typeof urlOrFunction == "function") {
    await SpecialPowers.spawn(browser, [], urlOrFunction);
  }

  let audioWakeLock = getWakeLockState("audio-playing", lockAudio, true);
  let videoWakeLock = getWakeLockState("video-playing", lockVideo, true);

  var initFunction = null;
  if (description.includes("web audio")) {
    initFunction = initializeWebAudio;
  } else {
    initFunction = waitUntilVideoStarted;
  }

  info(`- wait for media starting playing -`);
  let winPIP = null;
  if (elementIdForEnteringPIPMode) {
    winPIP = await triggerPictureInPicture(
      browser,
      elementIdForEnteringPIPMode
    );
  }
  await SpecialPowers.spawn(browser, [additionalParams], initFunction);
  await audioWakeLock.check();
  await videoWakeLock.check();

  info(`- switch tab to background -`);
  const isPageConsideredAsForeground = !!elementIdForEnteringPIPMode;
  audioWakeLock = getWakeLockState(
    "audio-playing",
    lockAudio,
    isPageConsideredAsForeground
  );
  videoWakeLock = getWakeLockState(
    "video-playing",
    lockVideo,
    isPageConsideredAsForeground
  );
  const tab2 = await BrowserTestUtils.openNewForegroundTab(
    window.gBrowser,
    "about:blank"
  );
  await audioWakeLock.check();
  await videoWakeLock.check();

  info(`- switch tab to foreground again -`);
  audioWakeLock = getWakeLockState("audio-playing", lockAudio, true);
  videoWakeLock = getWakeLockState("video-playing", lockVideo, true);
  await BrowserTestUtils.switchTab(window.gBrowser, tab);
  await audioWakeLock.check();
  await videoWakeLock.check();

  info(`- remove tabs -`);
  BrowserTestUtils.removeTab(tab);
  BrowserTestUtils.removeTab(tab2);
  if (winPIP) {
    await BrowserTestUtils.closeWindow(winPIP);
  }
}

add_task(async function start_tests() {
  await test_media_wakelock({
    description: "playing video",
    urlOrFunction: "file_video.html",
    lockAudio: true,
    lockVideo: true,
  });
  await test_media_wakelock({
    description: "playing muted video",
    urlOrFunction: "file_video.html",
    additionalParams: {
      muted: true,
    },
    lockAudio: false,
    lockVideo: true,
  });
  await test_media_wakelock({
    description: "playing volume=0 video",
    urlOrFunction: "file_video.html",
    additionalParams: {
      volume: 0.0,
    },
    lockAudio: false,
    lockVideo: true,
  });
  await test_media_wakelock({
    description: "playing video without audio in it",
    urlOrFunction: "file_videoWithoutAudioTrack.html",
    lockAudio: false,
    lockVideo: false,
  });
  await test_media_wakelock({
    description: "playing audio in video element",
    urlOrFunction: "file_videoWithAudioOnly.html",
    lockAudio: true,
    lockVideo: false,
  });
  await test_media_wakelock({
    description: "playing audio in audio element",
    urlOrFunction: "file_mediaPlayback2.html",
    lockAudio: true,
    lockVideo: false,
  });
  await test_media_wakelock({
    description: "playing video from media stream with audio and video tracks",
    urlOrFunction: "browser_mediaStreamPlayback.html",
    lockAudio: true,
    lockVideo: true,
  });
  await test_media_wakelock({
    description: "playing video from media stream without audio track",
    urlOrFunction: "browser_mediaStreamPlaybackWithoutAudio.html",
    lockAudio: true,
    lockVideo: true,
  });
  await test_media_wakelock({
    description: "playing audible web audio",
    urlOrFunction: webaudioDocument,
    lockAudio: true,
    lockVideo: false,
  });
  await test_media_wakelock({
    description: "suspended web audio",
    urlOrFunction: webaudioDocument,
    additionalParams: {
      suspend: true,
    },
    lockAudio: false,
    lockVideo: false,
  });
  await test_media_wakelock({
    description: "playing a PIP video",
    urlOrFunction: "file_video.html",
    elementIdForEnteringPIPMode: "v",
    lockAudio: true,
    lockVideo: true,
  });
});

async function waitUntilAudioContextStarts() {
  const ac = content.ac;
  if (ac.state == "running") {
    return;
  }

  while (ac.state != "running") {
    await new Promise(r => (ac.onstatechange = r));
  }
}

add_task(
  async function testBrieflyAudibleAudioContextReleasesAudioWakeLockWhenInaudible() {
    const tab = await BrowserTestUtils.openNewForegroundTab(
      window.gBrowser,
      "about:blank"
    );

    const browser = tab.linkedBrowser;

    let audioWakeLock = getWakeLockState("audio-playing", true, true);
    let videoWakeLock = getWakeLockState("video-playing", false, true);

    // Make a short noise
    await SpecialPowers.spawn(browser, [], () => {
      content.ac = new content.AudioContext();
      const ac = content.ac;
      const dest = ac.destination;
      const source = new content.OscillatorNode(ac);
      source.start(ac.currentTime);
      source.stop(ac.currentTime + 0.1);
      source.connect(dest);
    });

    await SpecialPowers.spawn(browser, [], waitUntilAudioContextStarts);
    info("AudioContext is running.");

    await audioWakeLock.check();
    await videoWakeLock.check();

    await waitForTabPlayingEvent(tab, false);

    audioWakeLock = getWakeLockState("audio-playing", false, true);
    videoWakeLock = getWakeLockState("video-playing", false, true);

    await audioWakeLock.check();
    await videoWakeLock.check();

    await BrowserTestUtils.removeTab(tab);
  }
);
