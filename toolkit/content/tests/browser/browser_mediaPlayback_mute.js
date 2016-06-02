const PAGE = "https://example.com/browser/toolkit/content/tests/browser/file_mediaPlayback2.html";
const FRAME = "https://example.com/browser/toolkit/content/tests/browser/file_mediaPlaybackFrame2.html";

function wait_for_event(browser, event) {
  return BrowserTestUtils.waitForEvent(browser, event, false, (event) => {
    is(event.originalTarget, browser, "Event must be dispatched to correct browser.");
    return true;
  });
}

function* test_audio_in_browser() {
  function get_audio_element() {
    var doc = content.document;
    var list = doc.getElementsByTagName('audio');
    if (list.length == 1) {
      return list[0];
    }

    // iframe?
    list = doc.getElementsByTagName('iframe');

    var iframe = list[0];
    list = iframe.contentDocument.getElementsByTagName('audio');
    return list[0];
  }

  var audio = get_audio_element();
  return {
    computedVolume: audio.computedVolume,
    computedMuted: audio.computedMuted
  }
}

function* test_on_browser(url, browser) {
  browser.loadURI(url);
  yield wait_for_event(browser, "DOMAudioPlaybackStarted");

  var result = yield ContentTask.spawn(browser, null, test_audio_in_browser);
  is(result.computedVolume, 1, "Audio volume is 1");
  is(result.computedMuted, false, "Audio is not muted");

  ok(!browser.audioMuted, "Audio should not be muted by default");
  browser.mute();
  ok(browser.audioMuted, "Audio should be muted now");

  yield wait_for_event(browser, "DOMAudioPlaybackStopped");

  result = yield ContentTask.spawn(browser, null, test_audio_in_browser);
  is(result.computedVolume, 0, "Audio volume is 0 when muted");
  is(result.computedMuted, true, "Audio is muted");
}

function* test_visibility(url, browser) {
  browser.loadURI(url);
  yield wait_for_event(browser, "DOMAudioPlaybackStarted");

  var result = yield ContentTask.spawn(browser, null, test_audio_in_browser);
  is(result.computedVolume, 1, "Audio volume is 1");
  is(result.computedMuted, false, "Audio is not muted");

  yield BrowserTestUtils.withNewTab({
    gBrowser,
    url: "about:blank",
  }, function() {});

  ok(!browser.audioMuted, "Audio should not be muted by default");
  browser.mute();
  ok(browser.audioMuted, "Audio should be muted now");

  yield wait_for_event(browser, "DOMAudioPlaybackStopped");

  result = yield ContentTask.spawn(browser, null, test_audio_in_browser);
  is(result.computedVolume, 0, "Audio volume is 0 when muted");
  is(result.computedMuted, true, "Audio is muted");
}

add_task(function*() {
  yield new Promise((resolve) => {
    SpecialPowers.pushPrefEnv({"set": [
      ["media.useAudioChannelService.testing", true]
    ]}, resolve);
  });
});

add_task(function* test_page() {
  yield BrowserTestUtils.withNewTab({
    gBrowser,
    url: "about:blank",
  }, test_on_browser.bind(undefined, PAGE));
});

add_task(function* test_frame() {
  yield BrowserTestUtils.withNewTab({
    gBrowser,
    url: "about:blank",
  }, test_on_browser.bind(undefined, FRAME));
});

add_task(function* test_frame() {
  yield BrowserTestUtils.withNewTab({
    gBrowser,
    url: "about:blank",
  }, test_visibility.bind(undefined, PAGE));
});
