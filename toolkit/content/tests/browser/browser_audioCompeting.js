const PAGE =
  "https://example.com/browser/toolkit/content/tests/browser/file_multipleAudio.html";

function play_audio_from_invisible_tab() {
  return new Promise(resolve => {
    var autoPlay = content.document.getElementById("autoplay");
    if (!autoPlay) {
      ok(false, "Can't get the audio element!");
    }

    is(autoPlay.paused, true, "Audio in tab 1 was paused by audio competing.");
    autoPlay.play();
    autoPlay.onpause = function() {
      autoPlay.onpause = null;
      ok(
        true,
        "Audio in tab 1 can't playback when other tab is playing in foreground."
      );
      resolve();
    };
  });
}

function audio_should_keep_playing_even_go_to_background() {
  var autoPlay = content.document.getElementById("autoplay");
  if (!autoPlay) {
    ok(false, "Can't get the audio element!");
  }

  is(
    autoPlay.paused,
    false,
    "Audio in tab 2 is still playing in the background."
  );
}

function play_non_autoplay_audio() {
  return new Promise(resolve => {
    var autoPlay = content.document.getElementById("autoplay");
    var nonAutoPlay = content.document.getElementById("nonautoplay");
    if (!autoPlay || !nonAutoPlay) {
      ok(false, "Can't get the audio element!");
    }

    is(
      nonAutoPlay.paused,
      true,
      "Non-autoplay audio isn't started playing yet."
    );
    nonAutoPlay.play();

    nonAutoPlay.onplay = function() {
      nonAutoPlay.onplay = null;
      is(nonAutoPlay.paused, false, "Start Non-autoplay audio.");
      is(autoPlay.paused, false, "Autoplay audio is still playing.");
      resolve();
    };
  });
}

add_task(async function setup_test_preference() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["dom.audiochannel.audioCompeting", true],
      ["dom.ipc.processCount", 1],
    ],
  });
});

add_task(async function cross_tabs_audio_competing() {
  info("- open tab 1 in foreground -");
  let tab1 = await BrowserTestUtils.openNewForegroundTab(
    window.gBrowser,
    "about:blank"
  );
  BrowserTestUtils.loadURI(tab1.linkedBrowser, PAGE);
  await waitForTabPlayingEvent(tab1, true);

  info("- open tab 2 in foreground -");
  let tab2 = await BrowserTestUtils.openNewForegroundTab(
    window.gBrowser,
    "about:blank"
  );
  BrowserTestUtils.loadURI(tab2.linkedBrowser, PAGE);
  await waitForTabPlayingEvent(tab1, false);

  info("- open tab 3 in foreground -");
  let tab3 = await BrowserTestUtils.openNewForegroundTab(
    window.gBrowser,
    "about:blank"
  );
  await ContentTask.spawn(
    tab2.linkedBrowser,
    null,
    audio_should_keep_playing_even_go_to_background
  );

  info("- play audio from background tab 1 -");
  await ContentTask.spawn(
    tab1.linkedBrowser,
    null,
    play_audio_from_invisible_tab
  );

  info("- remove tabs -");
  BrowserTestUtils.removeTab(tab1);
  BrowserTestUtils.removeTab(tab2);
  BrowserTestUtils.removeTab(tab3);
});

add_task(async function within_one_tab_audio_competing() {
  info("- open tab and play audio1 -");
  let tab = await BrowserTestUtils.openNewForegroundTab(
    window.gBrowser,
    "about:blank"
  );
  BrowserTestUtils.loadURI(tab.linkedBrowser, PAGE);
  await waitForTabPlayingEvent(tab, true);

  info("- play audio2 in the same tab -");
  await ContentTask.spawn(tab.linkedBrowser, null, play_non_autoplay_audio);

  info("- remove tab -");
  BrowserTestUtils.removeTab(tab);
});
