const FILE = "https://example.com/browser/toolkit/content/tests/browser/gizmo.mp4";

function setup_test_preference(enableUserGesture) {
  let state = enableUserGesture ? "enable" : "disable";
  info(`- set pref : ${state} user gesture -`);
  return SpecialPowers.pushPrefEnv({"set": [
    ["media.autoplay.enabled", false],
    ["media.autoplay.enabled.user-gestures-needed", enableUserGesture]
  ]});
}

async function allow_play_for_played_video() {
  info("- open new tab  -");
  let tab = await BrowserTestUtils.openNewForegroundTab(window.gBrowser,
                                                        "about:blank");
  info("- create video -");
  let document = tab.linkedBrowser.contentDocument;
  let video = document.createElement("video");
  video.src = FILE;
  video.controls = true;
  document.body.appendChild(video);

  info("- simulate user-click to start video -");
  let waitForPlayEvent = once(video, "play");
  await BrowserTestUtils.synthesizeMouseAtCenter(video, {button: 0},
                                                 tab.linkedBrowser);
  info("- video starts playing -");
  await waitForPlayEvent;

  info("- call video play() again -");
  try {
    await video.play();
    ok(true, "success to resolve play promise");
  } catch (e) {
    ok(false, "promise should not be rejected");
  }

  info("- remove tab -");
  await BrowserTestUtils.removeTab(tab);
}

add_task(async function start_test() {
  await setup_test_preference(true);
  await allow_play_for_played_video();

  await setup_test_preference(false);
  await allow_play_for_played_video();
});
