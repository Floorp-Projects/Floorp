/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

async function getDevicePixelRatio() {
  return ContentTask.spawn(gBrowser.selectedBrowser, null, function() {
    return content.devicePixelRatio;
  });
}

async function getImageDetails(format, data) {
  return ContentTask.spawn(
    gBrowser.selectedBrowser,
    { format, data },
    async function({ format, data }) {
      return new Promise(resolve => {
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

        img.src = `data:image/${format};base64,${data}`;
      });
    }
  );
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

add_task(async function documentSmallerThanViewport({ Page }) {
  loadURL(toDataURL("<div>Hello world"));

  info("Check that captureScreenshot() captures the viewport by default");
  const { data } = await Page.captureScreenshot();
  ok(!!data, "Screenshot data is not empty");

  const scale = await getDevicePixelRatio();
  const viewportRect = await getViewportRect();
  const { width, height } = await getImageDetails("png", data);
  is(width, (viewportRect.width - viewportRect.left) * scale);
  is(height, (viewportRect.height - viewportRect.top) * scale);
});

add_task(async function documentLargerThanViewport({ Page }) {
  loadURL(toDataURL("<div style='margin: 100vh 100vw'>Hello world"));

  info("Check that captureScreenshot() captures the viewport by default");
  const { data } = await Page.captureScreenshot();
  ok(!!data, "Screenshot data is not empty");

  const scale = await getDevicePixelRatio();
  const viewportRect = await getViewportRect();
  const { width, height } = await getImageDetails("png", data);

  is(width, (viewportRect.width - viewportRect.left) * scale);
  is(height, (viewportRect.height - viewportRect.top) * scale);
});
