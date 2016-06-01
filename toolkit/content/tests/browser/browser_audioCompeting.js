const PAGE = "https://example.com/browser/toolkit/content/tests/browser/file_multipleAudio.html";

function* wait_for_tab_playing_event(tab, expectPlaying) {
  if (tab.soundPlaying == expectPlaying) {
    ok(true, "The tab should " + (expectPlaying ? "" : "not ") + "be playing");
  } else {
    yield BrowserTestUtils.waitForEvent(tab, "TabAttrModified", false, (event) => {
      if (event.detail.changed.indexOf("soundplaying") >= 0) {
        is(tab.soundPlaying, expectPlaying, "The tab should " + (expectPlaying ? "" : "not ") + "be playing");
        return true;
      }
      return false;
    });
  }
}

function play_audio_from_invisible_tab () {
  return new Promise(resolve => {
    var autoPlay = content.document.getElementById('autoplay');
    if (!autoPlay) {
      ok(false, "Can't get the audio element!");
    }

    is(autoPlay.paused, true, "Audio in tab 1 was paused by audio competing.");
    autoPlay.play();
    autoPlay.onpause = function() {
      autoPlay.onpause = null;
      ok(true, "Audio in tab 1 can't playback when other tab is playing in foreground.");
      resolve();
    };
  });
}

function audio_should_keep_playing_even_go_to_background () {
  var autoPlay = content.document.getElementById('autoplay');
  if (!autoPlay) {
    ok(false, "Can't get the audio element!");
  }

  is(autoPlay.paused, false, "Audio in tab 2 is still playing in the background.");
}

function play_non_autoplay_audio () {
  return new Promise(resolve => {
    var autoPlay = content.document.getElementById('autoplay');
    var nonAutoPlay = content.document.getElementById('nonautoplay');
    if (!autoPlay || !nonAutoPlay) {
      ok(false, "Can't get the audio element!");
    }

    is(nonAutoPlay.paused, true, "Non-autoplay audio isn't started playing yet.");
    nonAutoPlay.play();

    nonAutoPlay.onplay = function() {
      nonAutoPlay.onplay = null;
      is(nonAutoPlay.paused, false, "Start Non-autoplay audio.");
      is(autoPlay.paused, false, "Autoplay audio is still playing.");
      resolve();
    };
  });
}

add_task(function* setup_test_preference() {
  yield new Promise(resolve => {
    SpecialPowers.pushPrefEnv({"set": [
      ["dom.audiochannel.audioCompeting", true]
    ]}, resolve);
  });
});

add_task(function* cross_tabs_audio_competing () {
  info("- open tab 1 in foreground -");
  let tab1 = yield BrowserTestUtils.openNewForegroundTab(window.gBrowser,
                                                         "about:blank");
  tab1.linkedBrowser.loadURI(PAGE);
  yield wait_for_tab_playing_event(tab1, true);

  info("- open tab 2 in foreground -");
  let tab2 = yield BrowserTestUtils.openNewForegroundTab(window.gBrowser,
                                                        "about:blank");
  tab2.linkedBrowser.loadURI(PAGE);
  yield wait_for_tab_playing_event(tab1, false);

  info("- open tab 3 in foreground -");
  let tab3 = yield BrowserTestUtils.openNewForegroundTab(window.gBrowser,
                                                         "about:blank");
  yield ContentTask.spawn(tab2.linkedBrowser, null,
                          audio_should_keep_playing_even_go_to_background);

  info("- play audio from background tab 1 -");
  yield ContentTask.spawn(tab1.linkedBrowser, null,
                          play_audio_from_invisible_tab);

  info("- remove tabs -");
  yield BrowserTestUtils.removeTab(tab1);
  yield BrowserTestUtils.removeTab(tab2);
  yield BrowserTestUtils.removeTab(tab3);
});

add_task(function* within_one_tab_audio_competing () {
  info("- open tab and play audio1 -");
  let tab = yield BrowserTestUtils.openNewForegroundTab(window.gBrowser,
                                                        "about:blank");
  tab.linkedBrowser.loadURI(PAGE);
  yield wait_for_tab_playing_event(tab, true);

  info("- play audio2 in the same tab -");
  yield ContentTask.spawn(tab.linkedBrowser, null,
                          play_non_autoplay_audio);

  info("- remove tab -");
  yield BrowserTestUtils.removeTab(tab);
});

