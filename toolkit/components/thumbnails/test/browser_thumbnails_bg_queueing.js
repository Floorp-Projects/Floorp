/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function thumbnails_bg_queueing() {
  let urls = [
    "https://www.example.com/0",
    "https://www.example.com/1",
    // an item that will timeout to ensure timeouts work and we resume.
    bgTestPageURL({ wait: 2002 }),
    "https://www.example.com/2",
  ];
  dontExpireThumbnailURLs(urls);

  let promises = [];

  for (let url of urls) {
    ok(!thumbnailExists(url), "Thumbnail should not exist yet: " + url);
    let isTimeoutTest = url.includes("wait");

    let promise = bgCapture(url, {
      timeout: isTimeoutTest ? 100 : 30000,
      onDone: capturedURL => {
        ok(!!urls.length, "onDone called, so URLs should still remain");
        is(
          capturedURL,
          urls.shift(),
          "Captured URL should be currently expected URL (i.e., " +
            "capture() callbacks should be called in the correct order)"
        );
        if (isTimeoutTest) {
          ok(
            !thumbnailExists(capturedURL),
            "Thumbnail shouldn't exist for timed out capture"
          );
        } else {
          ok(
            thumbnailExists(capturedURL),
            "Thumbnail should be cached after capture"
          );
          removeThumbnail(url);
        }
      },
    });

    promises.push(promise);
  }

  await Promise.all(promises);
});
