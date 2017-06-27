"use strict";

// PNG image data for a simple red dot.
const BACKGROUND_1 = "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAUAAAAFCAYAAACNbyblAAAAHElEQVQI12P4//8/w38GIAXDIBKE0DHxgljNBAAO9TXL0Y4OHwAAAABJRU5ErkJggg==";
const ACCENT_COLOR_1 = "#a14040";
const TEXT_COLOR_1 = "#fac96e";

// PNG image data for the Mozilla dino head.
const BACKGROUND_2 = "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAYAAAAf8/9hAAAABGdBTUEAAK/INwWK6QAAABl0RVh0U29mdHdhcmUAQWRvYmUgSW1hZ2VSZWFkeXHJZTwAAAHWSURBVHjaYvz//z8DJQAggJiQOe/fv2fv7Oz8rays/N+VkfG/iYnJfyD/1+rVq7ffu3dPFpsBAAHEAHIBCJ85c8bN2Nj4vwsDw/8zQLwKiO8CcRoQu0DxqlWrdsHUwzBAAIGJmTNnPgYa9j8UqhFElwPxf2MIDeIrKSn9FwSJoRkAEEAM0DD4DzMAyPi/G+QKY4hh5WAXGf8PDQ0FGwJ22d27CjADAAIIrLmjo+MXA9R2kAHvGBA2wwx6B8W7od6CeQcggKCmCEL8bgwxYCbUIGTDVkHDBia+CuotgACCueD3TDQN75D4xmAvCoK9ARMHBzAw0AECiBHkAlC0Mdy7x9ABNA3obAZXIAa6iKEcGlMVQHwWyjYuL2d4v2cPg8vZswx7gHyAAAK7AOif7SAbOqCmn4Ha3AHFsIDtgPq/vLz8P4MSkJ2W9h8ggBjevXvHDo4FQUQg/kdypqCg4H8lUIACnQ/SOBMYI8bAsAJFPcj1AAEEjwVQqLpAbXmH5BJjqI0gi9DTAAgDBBCcAVLkgmQ7yKCZxpCQxqUZhAECCJ4XgMl493ug21ZD+aDAXH0WLM4A9MZPXJkJIIAwTAR5pQMalaCABQUULttBGCCAGCnNzgABBgAMJ5THwGvJLAAAAABJRU5ErkJggg==";
const ACCENT_COLOR_2 = "#03fe03";
const TEXT_COLOR_2 = "#0ef325";

function hexToRGB(hex) {
  hex = parseInt((hex.indexOf("#") > -1 ? hex.substring(1) : hex), 16);
  return [hex >> 16, (hex & 0x00FF00) >> 8, (hex & 0x0000FF)];
}

function validateTheme(backgroundImage, accentColor, textColor) {
  let docEl = window.document.documentElement;
  let style = window.getComputedStyle(docEl);

  Assert.ok(docEl.hasAttribute("lwtheme"), "LWT attribute should be set");
  Assert.equal(docEl.getAttribute("lwthemetextcolor"), "bright",
    "LWT text color attribute should be set");

  Assert.ok(style.backgroundImage.includes(backgroundImage), "Expected correct background image");
  Assert.equal(style.backgroundColor, "rgb(" + hexToRGB(accentColor).join(", ") + ")",
    "Expected correct accent color");
  Assert.equal(style.color, "rgb(" + hexToRGB(textColor).join(", ") + ")",
    "Expected correct text color");
}

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.webextensions.themes.enabled", true]],
  });
});

add_task(async function test_dynamic_theme_updates() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["theme"],
    },
    files: {
      "image1.png": BACKGROUND_1,
      "image2.png": BACKGROUND_2,
    },
    background() {
      browser.test.onMessage.addListener((msg, details) => {
        if (msg != "update-theme") {
          browser.test.fail("expected 'update-theme' message");
        }

        browser.theme.update(details);
        browser.test.sendMessage("theme-updated");
      });
    },
  });

  await extension.startup();

  extension.sendMessage("update-theme", {
    "images": {
      "headerURL": "image1.png",
    },
    "colors": {
      "accentcolor": ACCENT_COLOR_1,
      "textcolor": TEXT_COLOR_1,
    },
  });

  await extension.awaitMessage("theme-updated");

  validateTheme("image1.png", ACCENT_COLOR_1, TEXT_COLOR_1);

  extension.sendMessage("update-theme", {
    "images": {
      "headerURL": "image2.png",
    },
    "colors": {
      "accentcolor": ACCENT_COLOR_2,
      "textcolor": TEXT_COLOR_2,
    },
  });

  await extension.awaitMessage("theme-updated");

  validateTheme("image2.png", ACCENT_COLOR_2, TEXT_COLOR_2);

  await extension.unload();

  let docEl = window.document.documentElement;
  Assert.ok(!docEl.hasAttribute("lwtheme"), "LWT attribute should not be set");
});
