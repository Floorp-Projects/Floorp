/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function documentSmallerThanViewport({ Page }) {
  loadURL(toDataURL("<div>Hello world"));

  info("Check that captureScreenshot() captures the viewport by default");
  const { data } = await Page.captureScreenshot();
  ok(!!data, "Screenshot data is not empty");

  const scale = await getDevicePixelRatio();
  const viewportRect = await getViewportRect();
  const { mimeType, width, height } = await getImageDetails(data);

  is(mimeType, "image/png", "Screenshot has correct MIME type");
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
  const { mimeType, width, height } = await getImageDetails(data);

  is(mimeType, "image/png", "Screenshot has correct MIME type");
  is(width, (viewportRect.width - viewportRect.left) * scale);
  is(height, (viewportRect.height - viewportRect.top) * scale);
});

add_task(async function invalidFormat({ Page }) {
  loadURL(toDataURL("<div>Hello world"));

  let exceptionThrown = false;
  try {
    await Page.captureScreenshot({ format: "foo" });
  } catch (e) {
    exceptionThrown = true;
  }
  ok(
    exceptionThrown,
    "captureScreenshot raised error for invalid image format"
  );
});

add_task(async function asJPEGFormat({ Page }) {
  loadURL(toDataURL("<div>Hello world"));

  info("Check that captureScreenshot() captures as JPEG format");
  const { data } = await Page.captureScreenshot({ format: "jpeg" });
  ok(!!data, "Screenshot data is not empty");

  const scale = await getDevicePixelRatio();
  const viewportRect = await getViewportRect();
  const { mimeType, height, width } = await getImageDetails(data);

  is(mimeType, "image/jpeg", "Screenshot has correct MIME type");
  is(width, (viewportRect.width - viewportRect.left) * scale);
  is(height, (viewportRect.height - viewportRect.top) * scale);
});

async function getDevicePixelRatio() {
  return ContentTask.spawn(gBrowser.selectedBrowser, null, function() {
    return content.devicePixelRatio;
  });
}

async function getImageDetails(image) {
  const mimeType = getMimeType(image);

  return ContentTask.spawn(
    gBrowser.selectedBrowser,
    { mimeType, image },
    async function({ mimeType, image }) {
      return new Promise(resolve => {
        const img = new content.Image();
        img.addEventListener(
          "load",
          () => {
            resolve({
              mimeType,
              width: img.width,
              height: img.height,
            });
          },
          { once: true }
        );

        img.src = `data:${mimeType};base64,${image}`;
      });
    }
  );
}

function getMimeType(image) {
  // Decode from base64 and convert the first 4 bytes to hex
  const raw = atob(image).slice(0, 4);
  let magicBytes = "";
  for (let i = 0; i < raw.length; i++) {
    magicBytes += raw
      .charCodeAt(i)
      .toString(16)
      .toUpperCase();
  }

  switch (magicBytes) {
    case "89504E47":
      return "image/png";
    case "FFD8FFDB":
    case "FFD8FFE0":
      return "image/jpeg";
    default:
      throw new Error("Unknown MIME type");
  }
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
