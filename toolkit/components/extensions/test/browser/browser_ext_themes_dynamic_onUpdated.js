"use strict";

// This test checks whether browser.theme.onUpdated works correctly with different
// types of dynamic theme updates.

// PNG image data for a simple red dot.
const BACKGROUND_1 = "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAUAAAAFCAYAAACNbyblAAAAHElEQVQI12P4//8/w38GIAXDIBKE0DHxgljNBAAO9TXL0Y4OHwAAAABJRU5ErkJggg==";
// PNG image data for the Mozilla dino head.
const BACKGROUND_2 = "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAYAAAAf8/9hAAAABGdBTUEAAK/INwWK6QAAABl0RVh0U29mdHdhcmUAQWRvYmUgSW1hZ2VSZWFkeXHJZTwAAAHWSURBVHjaYvz//z8DJQAggJiQOe/fv2fv7Oz8rays/N+VkfG/iYnJfyD/1+rVq7ffu3dPFpsBAAHEAHIBCJ85c8bN2Nj4vwsDw/8zQLwKiO8CcRoQu0DxqlWrdsHUwzBAAIGJmTNnPgYa9j8UqhFElwPxf2MIDeIrKSn9FwSJoRkAEEAM0DD4DzMAyPi/G+QKY4hh5WAXGf8PDQ0FGwJ22d27CjADAAIIrLmjo+MXA9R2kAHvGBA2wwx6B8W7od6CeQcggKCmCEL8bgwxYCbUIGTDVkHDBia+CuotgACCueD3TDQN75D4xmAvCoK9ARMHBzAw0AECiBHkAlC0Mdy7x9ABNA3obAZXIAa6iKEcGlMVQHwWyjYuL2d4v2cPg8vZswx7gHyAAAK7AOif7SAbOqCmn4Ha3AHFsIDtgPq/vLz8P4MSkJ2W9h8ggBjevXvHDo4FQUQg/kdypqCg4H8lUIACnQ/SOBMYI8bAsAJFPcj1AAEEjwVQqLpAbXmH5BJjqI0gi9DTAAgDBBCcAVLkgmQ7yKCZxpCQxqUZhAECCJ4XgMl493ug21ZD+aDAXH0WLM4A9MZPXJkJIIAwTAR5pQMalaCABQUULttBGCCAGCnNzgABBgAMJ5THwGvJLAAAAABJRU5ErkJggg==";

add_task(async function test_on_updated() {
  let extension = ExtensionTestUtils.loadExtension({
    async background() {
      const ACCENT_COLOR_1 = "#a14040";
      const TEXT_COLOR_1 = "#fac96e";

      const ACCENT_COLOR_2 = "#03fe03";
      const TEXT_COLOR_2 = "#0ef325";

      const theme1 = {
        "images": {
          "headerURL": "image1.png",
        },
        "colors": {
          "accentcolor": ACCENT_COLOR_1,
          "textcolor": TEXT_COLOR_1,
        },
      };

      const theme2 = {
        "images": {
          "headerURL": "image2.png",
        },
        "colors": {
          "accentcolor": ACCENT_COLOR_2,
          "textcolor": TEXT_COLOR_2,
        },
      };

      function testTheme1(returnedTheme) {
        browser.test.assertTrue(
          returnedTheme.images.headerURL.includes("image1.png"),
          "Theme 1 header URL should be applied");
        browser.test.assertEq(
          ACCENT_COLOR_1, returnedTheme.colors.accentcolor,
          "Theme 1 accent color should be applied");
        browser.test.assertEq(
          TEXT_COLOR_1, returnedTheme.colors.textcolor,
          "Theme 1 text color should be applied");
      }

      function testTheme2(returnedTheme) {
        browser.test.assertTrue(
          returnedTheme.images.headerURL.includes("image2.png"),
          "Theme 2 header URL should be applied");
        browser.test.assertEq(
          ACCENT_COLOR_2, returnedTheme.colors.accentcolor,
          "Theme 2 accent color should be applied");
        browser.test.assertEq(
          TEXT_COLOR_2, returnedTheme.colors.textcolor,
          "Theme 2 text color should be applied");
      }

      const firstWin = await browser.windows.getCurrent();
      const secondWin = await browser.windows.create();

      const onceThemeUpdated = () => new Promise(resolve => {
        const listener = updateInfo => {
          browser.theme.onUpdated.removeListener(listener);
          resolve(updateInfo);
        };
        browser.theme.onUpdated.addListener(listener);
      });

      browser.test.log("Testing update with no windowId parameter");
      let updateInfo1 = onceThemeUpdated();
      await browser.theme.update(theme1);
      updateInfo1 = await updateInfo1;
      testTheme1(updateInfo1.theme);
      browser.test.assertTrue(!updateInfo1.windowId, "No window id on first update");

      browser.test.log("Testing update with windowId parameter");
      let updateInfo2 = onceThemeUpdated();
      await browser.theme.update(secondWin.id, theme2);
      updateInfo2 = await updateInfo2;
      testTheme2(updateInfo2.theme);
      browser.test.assertEq(secondWin.id, updateInfo2.windowId,
                            "window id on second update");

      browser.test.log("Testing reset with windowId parameter");
      let updateInfo3 = onceThemeUpdated();
      await browser.theme.reset(firstWin.id);
      updateInfo3 = await updateInfo3;
      browser.test.assertEq(0, Object.keys(updateInfo3.theme).length,
                            "Empty theme given on reset");
      browser.test.assertEq(firstWin.id, updateInfo3.windowId,
                            "window id on third update");

      browser.test.log("Testing reset with no windowId parameter");
      let updateInfo4 = onceThemeUpdated();
      await browser.theme.reset();
      updateInfo4 = await updateInfo4;
      browser.test.assertEq(0, Object.keys(updateInfo4.theme).length, "Empty theme given on reset");
      browser.test.assertTrue(!updateInfo4.windowId, "no window id on fourth update");

      browser.test.log("Cleaning up test");
      await browser.windows.remove(secondWin.id);
      browser.test.notifyPass("onUpdated");
    },
    manifest: {
      permissions: ["theme"],
    },
    files: {
      "image1.png": BACKGROUND_1,
      "image2.png": BACKGROUND_2,
    },
  });

  await extension.startup();
  await extension.awaitFinish("onUpdated");
  await extension.unload();
});
