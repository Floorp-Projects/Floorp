// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
/* eslint-env node */

/*
Ensure the `--firefox.preference=network.http.http3.enabled:true` is
set for this test.
*/

async function test(context, commands) {
  let rootUrl = "https://www.youtube.com/watch?v=COU5T-Wafa4";
  let waitTime = 20000;

  if (
    (typeof context.options.browsertime !== "undefined") &
    (typeof context.options.browsertime.waitTime !== "undefined")
  ) {
    waitTime = context.options.browsertime.waitTime;
  }

  // Make firefox learn of HTTP/3 server
  await commands.navigate(rootUrl);

  let cycles = 1;
  for (let cycle = 0; cycle < cycles; cycle++) {
    await commands.measure.start("pageload");
    await commands.navigate(rootUrl);

    // Make sure the video is running
    // XXX: Should we start the video ourself?
    if (
      await commands.js.run(`return document.querySelector("video").paused;`)
    ) {
      throw new Error("Video should be running but it's paused");
    }

    // Disable youtube autoplay
    await commands.click.byIdAndWait("toggleButton");

    // Start playback quality measurements
    const start = await commands.js.run(`return performance.now();`);
    let counter = 1;
    let direction = 0;
    while (
      !(await commands.js.run(`
          return document.querySelector("video").ended;
      `)) &
      !(await commands.js.run(`
          return document.querySelector("video").paused;
      `)) &
      ((await commands.js.run(`return performance.now();`)) - start < waitTime)
    ) {
      // Reset the scroll after going down 10 times
      direction = counter * 1000;
      if (direction > 10000) {
        counter = -1;
      }
      counter++;

      await commands.js.runAndWait(
        `window.scrollTo({ top: ${direction}, behavior: 'smooth' })`
      );
      context.log.info("playing while scrolling...");
    }

    // Video done, now gather metrics
    const playbackQuality = await commands.js.run(
      `return document.querySelector("video").getVideoPlaybackQuality();`
    );
    await commands.measure.stop();

    commands.measure.result[0].browserScripts.pageinfo.droppedFrames =
      playbackQuality.droppedVideoFrames;
    commands.measure.result[0].browserScripts.pageinfo.decodedFrames =
      playbackQuality.totalVideoFrames;
  }
}

module.exports = {
  test,
  owner: "Network Team",
  component: "netwerk",
  name: "youtube-scroll",
  description: "Measures quality of the video being played.",
};
