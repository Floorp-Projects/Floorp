/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */

"use strict";

add_task(async () => {
  const URL_ROOT = getRootDirectory(gTestPath).replace(
    "chrome://mochitests/content/",
    "http://mochi.test:8888/"
  );

  const tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    URL_ROOT + "helper_scrollbar_colors.html"
  );

  await SpecialPowers.spawn(tab.linkedBrowser, [], () => {
    ChromeUtils.defineESModuleGetters(this, {
      WindowsVersionInfo:
        "resource://gre/modules/components-utils/WindowsVersionInfo.sys.mjs",
    });

    Services.scriptloader.loadSubScript(
      "chrome://mochikit/content/tests/SimpleTest/WindowSnapshot.js",
      this
    );

    // == Native theme ==

    const MAC_REFERENCES = [
      // Yellow background
      ["255,255,0", 7225],
      // Blue scrollbar face
      ["0,0,255", 416],
      // Cyan scrollbar track
      ["0,255,255", 1760],
    ];

    // == Non-native theme ==

    const WIN10_REFERENCES = [
      // Yellow background
      ["255,255,0", 6889],
      // Blue scrollbar face
      ["0,0,255", 612],
      // Cyan scrollbar track
      ["0,255,255", 2355],
    ];

    const WIN11_REFERENCES = [
      // Yellow background
      ["255,255,0", 6889],
      // Blue scrollbar face
      ["0,0,255", 324],
      // Cyan scrollbar track
      ["0,255,255", 2787],
    ];

    const LINUX_REFERENCES = [
      // Yellow background
      ["255,255,0", 7744],
      // Blue scrollbar face
      ["0,0,255", 368],
      // Cyan scrollbar track
      ["0,255,255", 1852],
    ];

    function countPixels(canvas) {
      let result = new Map();
      let ctx = canvas.getContext("2d");
      let image = ctx.getImageData(0, 0, canvas.width, canvas.height);
      let data = image.data;
      let size = image.width * image.height;
      for (let i = 0; i < size; i++) {
        let key = data.subarray(i * 4, i * 4 + 3).toString();
        let value = result.get(key);
        value = value ? value : 0;
        result.set(key, value + 1);
      }
      return result;
    }

    let outer = content.document.querySelector(".outer");
    let outerRect = outer.getBoundingClientRect();
    if (
      outerRect.width == outer.clientWidth &&
      outerRect.height == outer.clientHeight
    ) {
      ok(true, "Using overlay scrollbar, skip this test");
      return;
    }
    content.document.querySelector("#style").textContent = `
        .outer { scrollbar-color: blue cyan; }
      `;

    let canvas = snapshotRect(content.window, outerRect);
    let stats = countPixels(canvas);
    let references;
    if (content.navigator.platform.startsWith("Win")) {
      if (WindowsVersionInfo.get().buildNumber >= 22000) {
        // Windows 11
        references = WIN11_REFERENCES;
      } else {
        // Windows 10
        references = WIN10_REFERENCES;
      }
    } else if (content.navigator.platform.startsWith("Mac")) {
      references = MAC_REFERENCES;
    } else if (content.navigator.platform.startsWith("Linux")) {
      references = LINUX_REFERENCES;
    } else {
      ok(false, "Unsupported platform");
    }
    for (let [color, count] of references) {
      let value = stats.get(color);
      is(value, count, `Pixel count of color ${color}`);
    }
  });

  BrowserTestUtils.removeTab(tab);
});
