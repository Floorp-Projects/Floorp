/**
 * Test whether the autoplay permission can be transfer well through different
 * frame hierarchy. When the target frame has been interacted with the user
 * gesture, it would has the autoplay permission, then the permission would be
 * propagated to its parent and child frames.
 *
 * In this test, I use A/B/C to indicate different domain frames, and the number
 * after the name is which layer the frame is in.
 * Ex. A1 -> B2 -> A3,
 * Top frame and grandchild frame is in the domain A, and child frame is in the
 * domain B.
 *
 * Once any document in a tab is activated, all other documents in that tab
 * should be considered activated, even if they're cross origin from the
 * originally activated document.
 */
const PAGE_A1_A2 = "https://example.com/browser/toolkit/content/tests/browser/file_autoplay_two_layers_frame1.html";
const PAGE_A1_B2 = "https://example.com/browser/toolkit/content/tests/browser/file_autoplay_two_layers_frame2.html";
const PAGE_A1_B2_C3 = "https://test1.example.org/browser/toolkit/content/tests/browser/file_autoplay_three_layers_frame1.html";
const PAGE_A1_B2_A3 = "https://example.org/browser/toolkit/content/tests/browser/file_autoplay_three_layers_frame1.html";
const PAGE_A1_B2_B3 = "https://example.org/browser/toolkit/content/tests/browser/file_autoplay_three_layers_frame2.html";
const PAGE_A1_A2_A3 = "https://example.com/browser/toolkit/content/tests/browser/file_autoplay_three_layers_frame2.html";
const PAGE_A1_A2_B3 = "https://example.com/browser/toolkit/content/tests/browser/file_autoplay_three_layers_frame1.html";

function setup_test_preference() {
  return SpecialPowers.pushPrefEnv({"set": [
    ["media.autoplay.enabled", false],
    ["media.autoplay.enabled.user-gestures-needed", true]
  ]});
}

var frameTestArray = [
  { name: "A1_A2",    layersNum: 2, src: PAGE_A1_A2 },
  { name: "A1_B2",    layersNum: 2, src: PAGE_A1_B2 },
  { name: "A1_B2_C3", layersNum: 3, src: PAGE_A1_B2_C3 },
  { name: "A1_B2_A3", layersNum: 3, src: PAGE_A1_B2_A3 },
  { name: "A1_B2_B3", layersNum: 3, src: PAGE_A1_B2_B3 },
  { name: "A1_A2_A3", layersNum: 3, src: PAGE_A1_A2_A3 },
  { name: "A1_A2_B3", layersNum: 3, src: PAGE_A1_A2_B3 }
];

async function test_permission_propagation(testName, testSrc, layersNum) {
  info(`- start test for ${testName} -`);
  for (let layerIdx = 1; layerIdx <= layersNum; layerIdx++) {
    info("- open new tab -");
    let tab = await BrowserTestUtils.openNewForegroundTab(window.gBrowser,
                                                          "about:blank");
    tab.linkedBrowser.loadURI(testSrc);
    await BrowserTestUtils.browserLoaded(tab.linkedBrowser);

    // If the frame isn't activated, the video play will fail.
    async function playing_video_should_fail(layersNum) {
      for (let layerIdx = 1; layerIdx <= layersNum; layerIdx++) {
        let doc;
        if (layerIdx == 1) {
          doc = content.document;
        } else {
          doc = layerIdx == 2 ? content.frames[0].document :
                                content.frames[0].frames[0].document;
        }
        await doc.getElementById("v").play().catch(function() {
          ok(true, `video in layer ${layerIdx} can't start play without user input.`);
        });
      }
    }
    await ContentTask.spawn(tab.linkedBrowser, layersNum,
                            playing_video_should_fail);

    function activate_frame(testInfo) {
      let layerIdx = testInfo[0];
      info(`- activate frame in layer ${layerIdx} (${testInfo[1]}) -`);
      let doc;
      if (layerIdx == 1) {
        doc = content.document;
      } else {
        doc = layerIdx == 2 ? content.frames[0].document :
                              content.frames[0].frames[0].document;
      }
      doc.notifyUserGestureActivation();
    }
    await ContentTask.spawn(tab.linkedBrowser, [layerIdx, testName],
                            activate_frame);

    // If frame is activated, the video play will succeed, as interaction
    // anywhere in the frame activates the entire doctree, irrespective
    // of whether the documents are cross origin or not.
    async function playing_video_may_success(testInfo) {
      let layersNum = testInfo[2];
      for (let layerIdx = 1; layerIdx <= layersNum; layerIdx++) {
        let doc;
        if (layerIdx == 1) {
          doc = content.document;
        } else {
          doc = layerIdx == 2 ? content.frames[0].document :
                                content.frames[0].frames[0].document;
        }
        let video = doc.getElementById("v");
        try {
          await video.play();
          ok(true, `video in layer ${layerIdx} starts playing.`);
        } catch (e) {
          ok(false, `video in layer ${layerIdx} fails to start.`);
        }
      }
    }
    await ContentTask.spawn(tab.linkedBrowser,
                            [layerIdx, testName, layersNum],
                            playing_video_may_success);

    info("- remove tab -");
    BrowserTestUtils.removeTab(tab);
  }
}

add_task(async function start_test() {
  info("- setup test preference -");
  await setup_test_preference();
  requestLongerTimeout(2);

  info("- test permission propagation in different frame hierarchy -");
  for (let testIdx = 0; testIdx < frameTestArray.length; testIdx++) {
    let testInfo = frameTestArray[testIdx];
    let testName = testInfo.name;
    let testSrc = testInfo.src;
    let layersNum = testInfo.layersNum;
    if (layersNum > 3) {
      ok(false, "Not support more than 3 layers frame yet.");
    } else {
      await test_permission_propagation(testName, testSrc, layersNum);
    }
  }
});
