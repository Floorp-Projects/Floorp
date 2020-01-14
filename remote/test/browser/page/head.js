/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from ../head.js */

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/remote/test/browser/head.js",
  this
);

async function getContentSize() {
  return SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    const docEl = content.document.documentElement;

    return {
      x: 0,
      y: 0,
      width: docEl.scrollWidth,
      height: docEl.scrollHeight,
    };
  });
}

async function getViewportSize() {
  return SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    return {
      x: content.pageXOffset,
      y: content.pageYOffset,
      width: content.innerWidth,
      height: content.innerHeight,
    };
  });
}
