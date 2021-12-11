/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function thumbnails_bg_no_alert() {
  let url =
    "data:text/html,<script>try { alert('yo!'); } catch (e) {}</script>";
  ok(!thumbnailExists(url), "Thumbnail file should not already exist.");

  let [capturedURL] = await bgCapture(url);
  is(capturedURL, url, "Captured URL should be URL passed to capture.");
  ok(
    thumbnailExists(url),
    "Thumbnail file should exist even though it alerted."
  );
  removeThumbnail(url);
});
