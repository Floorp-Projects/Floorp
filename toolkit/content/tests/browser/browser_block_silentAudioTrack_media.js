const PAGE = "https://example.com/browser/toolkit/content/tests/browser/file_silentAudioTrack.html";

var SuspendedType = {
  NONE_SUSPENDED             : 0,
  SUSPENDED_PAUSE            : 1,
  SUSPENDED_BLOCK            : 2,
  SUSPENDED_PAUSE_DISPOSABLE : 3
};

function* wait_for_tab_playing_event(tab, expectPlaying) {
  if (tab.soundPlaying == expectPlaying) {
    ok(true, "The tab should " + (expectPlaying ? "" : "not ") + "be playing");
  } else {
    info("Playing state doens't match, wait for attributes changes.");
    yield BrowserTestUtils.waitForEvent(tab, "TabAttrModified", false, (event) => {
      if (event.detail.changed.indexOf("soundplaying") >= 0) {
        is(tab.soundPlaying, expectPlaying, "The tab should " + (expectPlaying ? "" : "not ") + "be playing");
        return true;
      }
      return false;
    });
  }
}

function disable_non_test_mouse(disable) {
  let utils = window.QueryInterface(Ci.nsIInterfaceRequestor)
                    .getInterface(Ci.nsIDOMWindowUtils);
  utils.disableNonTestMouseEvents(disable);
}

function* hover_icon(icon, tooltip) {
  disable_non_test_mouse(true);

  let popupShownPromise = BrowserTestUtils.waitForEvent(tooltip, "popupshown");
  EventUtils.synthesizeMouse(icon, 1, 1, {type: "mouseover"});
  EventUtils.synthesizeMouse(icon, 2, 2, {type: "mousemove"});
  EventUtils.synthesizeMouse(icon, 3, 3, {type: "mousemove"});
  EventUtils.synthesizeMouse(icon, 4, 4, {type: "mousemove"});
  return popupShownPromise;
}

function leave_icon(icon) {
  EventUtils.synthesizeMouse(icon, 0, 0, {type: "mouseout"});
  EventUtils.synthesizeMouseAtCenter(document.documentElement, {type: "mousemove"});
  EventUtils.synthesizeMouseAtCenter(document.documentElement, {type: "mousemove"});
  EventUtils.synthesizeMouseAtCenter(document.documentElement, {type: "mousemove"});

  disable_non_test_mouse(false);
}

function* click_unblock_icon(tab) {
  let icon = document.getAnonymousElementByAttribute(tab, "anonid", "soundplaying-icon");

  yield hover_icon(icon, document.getElementById("tabbrowser-tab-tooltip"));
  EventUtils.synthesizeMouseAtCenter(icon, {button: 0});
  leave_icon(icon);
}

function check_audio_suspended(suspendedType) {
  var autoPlay = content.document.getElementById("autoplay");
  if (!autoPlay) {
    ok(false, "Can't get the audio element!");
  }

  is(autoPlay.computedSuspended, suspendedType,
     "The suspeded state of autoplay audio is correct.");
}

add_task(function* setup_test_preference() {
  yield SpecialPowers.pushPrefEnv({"set": [
    ["media.useAudioChannelService.testing", true],
    ["media.block-autoplay-until-in-foreground", true]
  ]});
});

add_task(function* unblock_icon_should_disapear_after_resume_tab() {
  info("- open new background tab -");
  let tab = window.gBrowser.addTab("about:blank");
  tab.linkedBrowser.loadURI(PAGE);
  yield BrowserTestUtils.browserLoaded(tab.linkedBrowser);

  info("- the suspend state of tab should be suspend-block -");
  yield ContentTask.spawn(tab.linkedBrowser, SuspendedType.SUSPENDED_BLOCK,
                          check_audio_suspended);

  info("- tab should display unblocking icon -");
  yield waitForTabBlockEvent(tab, true);

  info("- select tab as foreground tab -");
  yield BrowserTestUtils.switchTab(window.gBrowser, tab);

  info("- the suspend state of tab should be none-suspend -");
  yield ContentTask.spawn(tab.linkedBrowser, SuspendedType.NONE_SUSPENDED,
                          check_audio_suspended);

  info("- should not display unblocking icon -");
  yield waitForTabBlockEvent(tab, false);

  info("- should not display sound indicator icon -");
  yield wait_for_tab_playing_event(tab, false);

  info("- remove tab -");
  yield BrowserTestUtils.removeTab(tab);
});

add_task(function* should_not_show_sound_indicator_after_resume_tab() {
  info("- open new background tab -");
  let tab = window.gBrowser.addTab("about:blank");
  tab.linkedBrowser.loadURI(PAGE);
  yield BrowserTestUtils.browserLoaded(tab.linkedBrowser);

  info("- the suspend state of tab should be suspend-block -");
  yield ContentTask.spawn(tab.linkedBrowser, SuspendedType.SUSPENDED_BLOCK,
                          check_audio_suspended);

  info("- tab should display unblocking icon -");
  yield waitForTabBlockEvent(tab, true);

  info("- click play tab icon -");
  yield click_unblock_icon(tab);

  info("- the suspend state of tab should be none-suspend -");
  yield ContentTask.spawn(tab.linkedBrowser, SuspendedType.NONE_SUSPENDED,
                          check_audio_suspended);

  info("- should not display unblocking icon -");
  yield waitForTabBlockEvent(tab, false);

  info("- should not display sound indicator icon -");
  yield wait_for_tab_playing_event(tab, false);

  info("- remove tab -");
  yield BrowserTestUtils.removeTab(tab);
});
