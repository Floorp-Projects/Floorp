/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

async function testLinkWithoutExtension(type, expectExtension) {
  info("Checking " + type);

  let winPromise = BrowserTestUtils.domWindowOpenedAndLoaded();

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [type], mimetype => {
    let link = content.document.createElement("a");
    link.textContent = "Click me";
    link.href = "data:" + mimetype + ",hello";
    link.download = "somefile";
    content.document.body.appendChild(link);
    link.click();
  });

  let win = await winPromise;

  let actualName = win.document.getElementById("location").value;
  let expectedName = "somefile";
  if (expectExtension) {
    expectedName += "." + getMIMEInfoForType(type).primaryExtension;
    is(actualName, expectedName, `${type} should get an extension`);
  } else {
    is(actualName, expectedName, `${type} should not get an extension`);
  }

  await BrowserTestUtils.closeWindow(win);
}

/**
 * Check that for document types, images, videos and audio files,
 * we enforce a useful extension.
 */
add_task(async function test_enforce_useful_extension() {
  await BrowserTestUtils.withNewTab("data:text/html,", async browser => {
    await testLinkWithoutExtension("image/png", true);
    await testLinkWithoutExtension("audio/ogg", true);
    await testLinkWithoutExtension("video/webm", true);
    await testLinkWithoutExtension("application/msword", true);
    await testLinkWithoutExtension("application/pdf", true);

    await testLinkWithoutExtension("application/x-gobbledygook", false);
    await testLinkWithoutExtension("application/octet-stream", false);
    await testLinkWithoutExtension("binary/octet-stream", false);
    await testLinkWithoutExtension("application/x-msdownload", false);
  });
});
