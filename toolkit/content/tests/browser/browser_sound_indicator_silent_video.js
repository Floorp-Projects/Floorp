const SILENT_PAGE = "https://example.com/browser/toolkit/content/tests/browser/file_silentAudioTrack.html";
const ALMOST_SILENT_PAGE = "https://example.com/browser/toolkit/content/tests/browser/file_almostSilentAudioTrack.html";

function check_audio_playing_state(isPlaying) {
  let autoPlay = content.document.getElementById("autoplay");
  if (!autoPlay) {
    ok(false, "Can't get the audio element!");
  }

  is(autoPlay.paused, !isPlaying,
     "The playing state of autoplay audio is correct.");

  // wait for a while to make sure the video is playing and related event has
  // been dispatched (if any).
  let PLAYING_TIME_SEC = 0.5;
  ok(PLAYING_TIME_SEC < autoPlay.duration, "The playing time is valid.");

  return new Promise(resolve => {
    autoPlay.ontimeupdate = function() {
      if (autoPlay.currentTime > PLAYING_TIME_SEC) {
        resolve();
      }
    };
  });
}

add_task(async function should_not_show_sound_indicator_for_silent_video() {
  info("- open new foreground tab -");
  let tab = await BrowserTestUtils.openNewForegroundTab(window.gBrowser,
                                                        "about:blank");

  info("- tab should not have sound indicator before playing silent video -");
  await waitForTabPlayingEvent(tab, false);

  info("- loading autoplay silent video -");
  tab.linkedBrowser.loadURI(SILENT_PAGE);
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  await ContentTask.spawn(tab.linkedBrowser, true /* playing */,
                          check_audio_playing_state);

  info("- tab should not have sound indicator after playing silent video -");
  await waitForTabPlayingEvent(tab, false);

  info("- remove tab -");
  await BrowserTestUtils.removeTab(tab);
});

add_task(async function should_not_show_sound_indicator_for_almost_silent_video() {
  info("- open new foreground tab -");
  let tab = await BrowserTestUtils.openNewForegroundTab(window.gBrowser,
                                                        "about:blank");

  info("- tab should not have sound indicator before playing almost silent video -");
  await waitForTabPlayingEvent(tab, false);

  info("- loading autoplay almost silent video -");
  tab.linkedBrowser.loadURI(ALMOST_SILENT_PAGE);
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  await ContentTask.spawn(tab.linkedBrowser, true /* playing */,
                          check_audio_playing_state);

  info("- tab should not have sound indicator after playing almost silent video -");
  await waitForTabPlayingEvent(tab, false);

  info("- remove tab -");
  await BrowserTestUtils.removeTab(tab);
});
