const PAGE = "https://example.com/browser/toolkit/content/tests/browser/file_plugIn.html";

function* click_icon(tab) {
  let icon = document.getAnonymousElementByAttribute(tab, "anonid", "soundplaying-icon");

  yield hover_icon(icon, document.getElementById("tabbrowser-tab-tooltip"));
  EventUtils.synthesizeMouseAtCenter(icon, {button: 0});
  leave_icon(icon);
}

function start_plugin(suspendedType) {
  var startButton = content.document.getElementById("start");
  if (!startButton) {
    ok(false, "Can't get the start button!");
  }

  startButton.click();
}

function stop_plugin(suspendedType) {
  var stopButton = content.document.getElementById("stop");
  if (!stopButton) {
    ok(false, "Can't get the stop button!");
  }

  stopButton.click();
}

add_task(function* setup_test_preference() {
  setTestPluginEnabledState(Ci.nsIPluginTag.STATE_ENABLED, "Test Plug-in");
  yield SpecialPowers.pushPrefEnv({"set": [
    ["media.useAudioChannelService.testing", true],
    ["media.block-autoplay-until-in-foreground", true]
  ]});
});

add_task(function* block_plug_in() {
  info("- open new tab -");
  let tab = yield BrowserTestUtils.openNewForegroundTab(window.gBrowser,
                                                        "about:blank");
  tab.linkedBrowser.loadURI(PAGE);
  yield BrowserTestUtils.browserLoaded(tab.linkedBrowser);

  info("- tab should be audible -");
  yield waitForTabPlayingEvent(tab, true);

  info("- mute browser -");
  ok(!tab.linkedBrowser.audioMuted, "Audio should not be muted by default");
  yield click_icon(tab);
  ok(tab.linkedBrowser.audioMuted, "Audio should be muted now");

  info("- stop plugin -");
  yield ContentTask.spawn(tab.linkedBrowser, null, stop_plugin);

  info("- start plugin -");
  yield ContentTask.spawn(tab.linkedBrowser, null, start_plugin);

  info("- unmute browser -");
  ok(tab.linkedBrowser.audioMuted, "Audio should be muted now");
  yield click_icon(tab);
  ok(!tab.linkedBrowser.audioMuted, "Audio should be unmuted now");

  info("- tab should be audible -");
  yield waitForTabPlayingEvent(tab, true);

  info("- remove tab -");
  yield BrowserTestUtils.removeTab(tab);
});
