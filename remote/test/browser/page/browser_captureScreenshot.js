/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

async function getDevicePixelRatio() {
  return ContentTask.spawn(gBrowser.selectedBrowser, null, function() {
    return content.devicePixelRatio;
  });
}

async function getImageDetails(client, image) {
  return ContentTask.spawn(gBrowser.selectedBrowser, image, async function(
    image
  ) {
    let infoPromise = new Promise(resolve => {
      const img = new content.Image();
      img.addEventListener(
        "load",
        () => {
          resolve({
            width: img.width,
            height: img.height,
          });
        },
        { once: true }
      );
      img.src = image;
    });
    return infoPromise;
  });
}

async function getViewportRect() {
  return ContentTask.spawn(gBrowser.selectedBrowser, null, function() {
    return {
      left: content.pageXOffset,
      top: content.pageYOffset,
      width: content.innerWidth,
      height: content.innerHeight,
    };
  });
}

add_task(async function documentSmallerThanViewport(client) {
  loadURL(toDataURL("<div>Hello world"));

  const { Page } = client;

  info("Check that captureScreenshot() captures the viewport by default");
  const screenshot = await Page.captureScreenshot();

  const scale = await getDevicePixelRatio();
  const viewportRect = await getViewportRect();
  const { width, height } = await getImageDetails(client, screenshot);

  is(width, (viewportRect.width - viewportRect.left) * scale);
  is(height, (viewportRect.height - viewportRect.top) * scale);
});

add_task(async function documentLargerThanViewport(client) {
  loadURL(toDataURL("<div style='margin: 100vh 100vw'>Hello world"));

  const { Page } = client;

  info("Check that captureScreenshot() captures the viewport by default");
  const screenshot = await Page.captureScreenshot();

  const scale = await getDevicePixelRatio();
  const viewportRect = await getViewportRect();
  const { width, height } = await getImageDetails(client, screenshot);

  is(width, (viewportRect.width - viewportRect.left) * scale);
  is(height, (viewportRect.height - viewportRect.top) * scale);
});
