/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

/**
 * Bug 1896175 - Testing canvas randomization with read canvas permission granted.
 *
 */

const emptyPage =
  getRootDirectory(gTestPath).replace(
    "chrome://mochitests/content",
    "https://example.com"
  ) + "empty.html";

const TRUE_VALUE = "255,0,0,255,0,255,0,255,0,0,255,255";

function getImageData(tab) {
  const extractCanvas = function () {
    const canvas = document.createElement("canvas");
    const ctx = canvas.getContext("2d");

    ctx.fillStyle = "rgb(255, 0, 0)";
    ctx.fillRect(0, 0, 1, 1);
    ctx.fillStyle = "rgb(0, 255, 0)";
    ctx.fillRect(1, 0, 1, 1);
    ctx.fillStyle = "rgb(0, 0, 255)";
    ctx.fillRect(2, 0, 1, 1);

    return ctx.getImageData(0, 0, 3, 1).data.join(",");
  };

  const extractCanvasExpr = `(${extractCanvas.toString()})();`;

  return SpecialPowers.spawn(
    tab.linkedBrowser,
    [extractCanvasExpr],
    async funccode => content.eval(funccode)
  );
}

add_task(async function test_no_permission() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["privacy.resistFingerprinting", true],
      ["privacy.fingerprintingProtection", false],
    ],
  });

  const tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    opening: emptyPage,
    forceNewProcess: true,
  });

  const imageData = await getImageData(tab);

  isnot(imageData, TRUE_VALUE, "Image data was randomized.");

  BrowserTestUtils.removeTab(tab);
  await SpecialPowers.popPrefEnv();
});

add_task(async function test_permission() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["privacy.resistFingerprinting", true],
      ["privacy.fingerprintingProtection", false],
    ],
  });

  const tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    opening: emptyPage,
    forceNewProcess: true,
  });

  await SpecialPowers.pushPermissions([
    {
      type: "canvas",
      allow: true,
      context: emptyPage,
    },
  ]);

  const imageData = await getImageData(tab);

  is(imageData, TRUE_VALUE, "Image data was not randomized.");

  BrowserTestUtils.removeTab(tab);
  await SpecialPowers.popPermissions();
  await SpecialPowers.popPrefEnv();
});
