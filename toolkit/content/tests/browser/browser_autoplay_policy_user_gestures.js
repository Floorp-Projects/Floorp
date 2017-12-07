const FILE = "https://example.com/browser/toolkit/content/tests/browser/gizmo.mp4";

var UserGestures = {
  MOUSE_CLICK: "mouse-click",
  MOUSE_MOVE: "mouse-move",
  KEYBOARD_PRESS: "keyboard-press"
};

var UserGestureTests = [
  {type: UserGestures.MOUSE_CLICK, isActivationGesture: true},
  {type: UserGestures.MOUSE_MOVE, isActivationGesture: false},
  {type: UserGestures.KEYBOARD_PRESS, isActivationGesture: true}
];

function setup_test_preference() {
  return SpecialPowers.pushPrefEnv({"set": [
    ["media.autoplay.enabled", false],
    ["media.autoplay.enabled.user-gestures-needed", true]
  ]});
}

function simulateUserGesture(gesture, targetBrowser) {
  let targetElement = targetBrowser.contentDocument.documentElement;
  info(`- simulate ${gesture.type} event -`);
  switch (gesture.type) {
    case UserGestures.MOUSE_CLICK:
      return BrowserTestUtils.synthesizeMouseAtCenter(targetElement, {button: 0},
                                                      targetBrowser);
    case UserGestures.MOUSE_MOVE:
      return BrowserTestUtils.synthesizeMouseAtCenter(targetElement, {type: "mousemove"},
                                                      targetBrowser);
    case UserGestures.KEYBOARD_PRESS:
      return BrowserTestUtils.sendChar("a", targetBrowser);
    default:
      ok(false, "undefined user gesture");
      return false;
  }
}

async function test_play_without_user_gesture() {
  info("- open new tab -");
  let tab = await BrowserTestUtils.openNewForegroundTab(window.gBrowser,
                                                        "about:blank");
  info("- create autoplay video -");
  let document = tab.linkedBrowser.contentDocument;
  let video = document.createElement("video");
  video.src = FILE;
  video.autoplay = true;
  let canplayPromise = once(video, "canplaythrough");
  document.body.appendChild(video);

  info("- can't autoplay without user activation -");
  await canplayPromise;
  ok(video.paused, "video can't start without user input.");

  info("- call play() without user activation -");
  await video.play().catch(function() {
      ok(video.paused, "video can't start play without user input.");
  });

  info("- remove tab -");
  await BrowserTestUtils.removeTab(tab);
}

async function test_play_with_user_gesture(gesture) {
  info("- open new tab -");
  let tab = await BrowserTestUtils.openNewForegroundTab(window.gBrowser,
                                                        "about:blank");
  info("- create autoplay video -");
  let document = tab.linkedBrowser.contentDocument;
  let video = document.createElement("video");
  video.src = FILE;
  document.body.appendChild(video);

  info("- simulate user gesture -");
  await simulateUserGesture(gesture, tab.linkedBrowser);

  info("- call play() -");
  try {
    await video.play();
    ok(gesture.isActivationGesture, "user gesture can activate the page");
    ok(!video.paused, "video starts playing.");
  } catch (e) {
    ok(!gesture.isActivationGesture, "user gesture can not activate the page");
    ok(video.paused, "video can not start playing.");
  }

  info("- remove tab -");
  await BrowserTestUtils.removeTab(tab);
}

add_task(async function start_test() {
  info("- setup test preference -");
  await setup_test_preference();

  info("- test play when page doesn't be activated -");
  await test_play_without_user_gesture();

  info("- test play after page got user gesture -");
  for (let idx = 0; idx < UserGestureTests.length; idx++) {
    await test_play_with_user_gesture(UserGestureTests[idx]);
  }
});
