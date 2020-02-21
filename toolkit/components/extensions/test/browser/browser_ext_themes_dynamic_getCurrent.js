"use strict";

// This test checks whether browser.theme.getCurrent() works correctly in different
// configurations and with different parameter.

// PNG image data for a simple red dot.
const BACKGROUND_1 =
  "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAUAAAAFCAYAAACNbyblAAAAHElEQVQI12P4//8/w38GIAXDIBKE0DHxgljNBAAO9TXL0Y4OHwAAAABJRU5ErkJggg==";
// PNG image data for the Mozilla dino head.
const BACKGROUND_2 =
  "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAYAAAAf8/9hAAAABGdBTUEAAK/INwWK6QAAABl0RVh0U29mdHdhcmUAQWRvYmUgSW1hZ2VSZWFkeXHJZTwAAAHWSURBVHjaYvz//z8DJQAggJiQOe/fv2fv7Oz8rays/N+VkfG/iYnJfyD/1+rVq7ffu3dPFpsBAAHEAHIBCJ85c8bN2Nj4vwsDw/8zQLwKiO8CcRoQu0DxqlWrdsHUwzBAAIGJmTNnPgYa9j8UqhFElwPxf2MIDeIrKSn9FwSJoRkAEEAM0DD4DzMAyPi/G+QKY4hh5WAXGf8PDQ0FGwJ22d27CjADAAIIrLmjo+MXA9R2kAHvGBA2wwx6B8W7od6CeQcggKCmCEL8bgwxYCbUIGTDVkHDBia+CuotgACCueD3TDQN75D4xmAvCoK9ARMHBzAw0AECiBHkAlC0Mdy7x9ABNA3obAZXIAa6iKEcGlMVQHwWyjYuL2d4v2cPg8vZswx7gHyAAAK7AOif7SAbOqCmn4Ha3AHFsIDtgPq/vLz8P4MSkJ2W9h8ggBjevXvHDo4FQUQg/kdypqCg4H8lUIACnQ/SOBMYI8bAsAJFPcj1AAEEjwVQqLpAbXmH5BJjqI0gi9DTAAgDBBCcAVLkgmQ7yKCZxpCQxqUZhAECCJ4XgMl493ug21ZD+aDAXH0WLM4A9MZPXJkJIIAwTAR5pQMalaCABQUULttBGCCAGCnNzgABBgAMJ5THwGvJLAAAAABJRU5ErkJggg==";

add_task(async function test_get_current() {
  let extension = ExtensionTestUtils.loadExtension({
    async background() {
      const ACCENT_COLOR_1 = "#a14040";
      const TEXT_COLOR_1 = "#fac96e";

      const ACCENT_COLOR_2 = "#03fe03";
      const TEXT_COLOR_2 = "#0ef325";

      const theme1 = {
        images: {
          theme_frame: "image1.png",
        },
        colors: {
          frame: ACCENT_COLOR_1,
          tab_background_text: TEXT_COLOR_1,
        },
      };

      const theme2 = {
        images: {
          theme_frame: "image2.png",
        },
        colors: {
          frame: ACCENT_COLOR_2,
          tab_background_text: TEXT_COLOR_2,
        },
      };

      function ensureWindowFocused(winId) {
        browser.test.log("Waiting for focused window to be " + winId);
        // eslint-disable-next-line no-async-promise-executor
        return new Promise(async resolve => {
          let listener = windowId => {
            if (windowId === winId) {
              browser.windows.onFocusChanged.removeListener(listener);
              resolve();
            }
          };
          // We first add a listener and then check whether the window is
          // focused using .get(), because the .get() Promise resolving
          // could race with the listener running, in which case we'd
          // never be notified.
          browser.windows.onFocusChanged.addListener(listener);
          let { focused } = await browser.windows.get(winId);
          if (focused) {
            browser.windows.onFocusChanged.removeListener(listener);
            resolve();
          }
        });
      }

      function testTheme1(returnedTheme) {
        browser.test.assertTrue(
          returnedTheme.images.theme_frame.includes("image1.png"),
          "Theme 1 theme_frame image should be applied"
        );
        browser.test.assertEq(
          ACCENT_COLOR_1,
          returnedTheme.colors.frame,
          "Theme 1 frame color should be applied"
        );
        browser.test.assertEq(
          TEXT_COLOR_1,
          returnedTheme.colors.tab_background_text,
          "Theme 1 tab_background_text color should be applied"
        );
      }

      function testTheme2(returnedTheme) {
        browser.test.assertTrue(
          returnedTheme.images.theme_frame.includes("image2.png"),
          "Theme 2 theme_frame image should be applied"
        );
        browser.test.assertEq(
          ACCENT_COLOR_2,
          returnedTheme.colors.frame,
          "Theme 2 frame color should be applied"
        );
        browser.test.assertEq(
          TEXT_COLOR_2,
          returnedTheme.colors.tab_background_text,
          "Theme 2 tab_background_text color should be applied"
        );
      }

      function testEmptyTheme(returnedTheme) {
        browser.test.assertEq(
          0,
          Object.keys(returnedTheme).length,
          JSON.stringify(returnedTheme, null, 2)
        );
      }

      browser.test.log("Testing getCurrent() with initial unthemed window");
      const firstWin = await browser.windows.getCurrent();
      testEmptyTheme(await browser.theme.getCurrent());
      testEmptyTheme(await browser.theme.getCurrent(firstWin.id));

      browser.test.log("Testing getCurrent() with after theme.update()");
      await browser.theme.update(theme1);
      testTheme1(await browser.theme.getCurrent());
      testTheme1(await browser.theme.getCurrent(firstWin.id));

      browser.test.log(
        "Testing getCurrent() with after theme.update(windowId)"
      );
      const secondWin = await browser.windows.create();
      await ensureWindowFocused(secondWin.id);
      await browser.theme.update(secondWin.id, theme2);
      testTheme2(await browser.theme.getCurrent());
      testTheme1(await browser.theme.getCurrent(firstWin.id));
      testTheme2(await browser.theme.getCurrent(secondWin.id));

      browser.test.log("Testing getCurrent() after window focus change");
      let focusChanged = ensureWindowFocused(firstWin.id);
      await browser.windows.update(firstWin.id, { focused: true });
      await focusChanged;
      testTheme1(await browser.theme.getCurrent());
      testTheme1(await browser.theme.getCurrent(firstWin.id));
      testTheme2(await browser.theme.getCurrent(secondWin.id));

      browser.test.log(
        "Testing getCurrent() after another window focus change"
      );
      focusChanged = ensureWindowFocused(secondWin.id);
      await browser.windows.update(secondWin.id, { focused: true });
      await focusChanged;
      testTheme2(await browser.theme.getCurrent());
      testTheme1(await browser.theme.getCurrent(firstWin.id));
      testTheme2(await browser.theme.getCurrent(secondWin.id));

      browser.test.log("Testing getCurrent() after theme.reset(windowId)");
      await browser.theme.reset(firstWin.id);
      testTheme2(await browser.theme.getCurrent());
      testEmptyTheme(await browser.theme.getCurrent(firstWin.id));
      testTheme2(await browser.theme.getCurrent(secondWin.id));

      browser.test.log(
        "Testing getCurrent() after reset and window focus change"
      );
      focusChanged = ensureWindowFocused(firstWin.id);
      await browser.windows.update(firstWin.id, { focused: true });
      await focusChanged;
      testEmptyTheme(await browser.theme.getCurrent());
      testEmptyTheme(await browser.theme.getCurrent(firstWin.id));
      testTheme2(await browser.theme.getCurrent(secondWin.id));

      browser.test.log("Testing getCurrent() after theme.update(windowId)");
      await browser.theme.update(firstWin.id, theme1);
      testTheme1(await browser.theme.getCurrent());
      testTheme1(await browser.theme.getCurrent(firstWin.id));
      testTheme2(await browser.theme.getCurrent(secondWin.id));

      browser.test.log("Testing getCurrent() after theme.reset()");
      await browser.theme.reset();
      testEmptyTheme(await browser.theme.getCurrent());
      testEmptyTheme(await browser.theme.getCurrent(firstWin.id));
      testEmptyTheme(await browser.theme.getCurrent(secondWin.id));

      browser.test.log("Testing getCurrent() after closing a window");
      await browser.windows.remove(secondWin.id);
      testEmptyTheme(await browser.theme.getCurrent());
      testEmptyTheme(await browser.theme.getCurrent(firstWin.id));

      browser.test.log("Testing update calls with invalid window ID");
      await browser.test.assertRejects(
        browser.theme.reset(secondWin.id),
        /Invalid window/,
        "Invalid window should throw"
      );
      await browser.test.assertRejects(
        browser.theme.update(secondWin.id, theme2),
        /Invalid window/,
        "Invalid window should throw"
      );
      browser.test.notifyPass("get_current");
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
  await extension.awaitFinish("get_current");
  await extension.unload();
});
