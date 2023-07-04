/**
 * This test is used to ensure we suspend video decoding if video is not in the
 * viewport.
 */
"use strict";

const PAGE =
  "https://example.com/browser/toolkit/content/tests/browser/file_outside_viewport_videos.html";

async function test_suspend_video_decoding() {
  let videos = content.document.getElementsByTagName("video");
  for (let video of videos) {
    info(`- start video on the ${video.id} side and outside the viewport -`);
    await video.play();
    ok(true, `video started playing`);
    ok(video.isVideoDecodingSuspended, `video decoding is suspended`);
  }
}

add_task(async function setup_test_preference() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["media.suspend-background-video.enabled", true],
      ["media.suspend-background-video.delay-ms", 0],
    ],
  });
});

add_task(async function start_test() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: PAGE,
    },
    async browser => {
      await SpecialPowers.spawn(browser, [], test_suspend_video_decoding);
    }
  );
});
