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
  is(
    width,
    (viewportRect.width - viewportRect.left) * scale,
    "Image has expected width"
  );
  is(
    height,
    (viewportRect.height - viewportRect.top) * scale,
    "Image has expected height"
  );
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
  is(
    width,
    (viewportRect.width - viewportRect.left) * scale,
    "Image has expected width"
  );
  is(
    height,
    (viewportRect.height - viewportRect.top) * scale,
    "Image has expected height"
  );
});

add_task(async function invalidFormat({ Page }) {
  loadURL(toDataURL("<div>Hello world"));

  let errorThrown = false;
  try {
    await Page.captureScreenshot({ format: "foo" });
  } catch (e) {
    errorThrown = true;
  }
  ok(errorThrown, "captureScreenshot raised error for invalid image format");
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

add_task(async function asJPEGFormatAndQuality({ Page }) {
  loadURL(toDataURL("<div>Hello world"));

  info("Check that captureScreenshot() captures as JPEG format");
  const imageDefault = await Page.captureScreenshot({ format: "jpeg" });
  ok(!!imageDefault, "Screenshot data with default quality is not empty");

  const image100 = await Page.captureScreenshot({
    format: "jpeg",
    quality: 100,
  });
  ok(!!image100, "Screenshot data with quality 100 is not empty");

  const image10 = await Page.captureScreenshot({
    format: "jpeg",
    quality: 10,
  });
  ok(!!image10, "Screenshot data with quality 10 is not empty");

  const infoDefault = await getImageDetails(imageDefault.data);
  const info100 = await getImageDetails(image100.data);
  const info10 = await getImageDetails(image10.data);

  // All screenshots are of mimeType JPEG
  is(
    infoDefault.mimeType,
    "image/jpeg",
    "Screenshot with default quality has correct MIME type"
  );
  is(
    info100.mimeType,
    "image/jpeg",
    "Screenshot with quality 100 has correct MIME type"
  );
  is(
    info10.mimeType,
    "image/jpeg",
    "Screenshot with quality 10 has correct MIME type"
  );

  const scale = await getDevicePixelRatio();
  const viewportRect = await getViewportRect();

  // Images are all of the same dimension
  is(infoDefault.width, (viewportRect.width - viewportRect.left) * scale);
  is(infoDefault.height, (viewportRect.height - viewportRect.top) * scale);

  is(info100.width, (viewportRect.width - viewportRect.left) * scale);
  is(info100.height, (viewportRect.height - viewportRect.top) * scale);

  is(info10.width, (viewportRect.width - viewportRect.left) * scale);
  is(info10.height, (viewportRect.height - viewportRect.top) * scale);

  // Images of different quality result in different content sizes
  ok(
    info100.length > infoDefault.length,
    "Size of quality 100 is larger than default"
  );
  // Disabled due to bug 1594871
  // ok(
  //   info10.length < infoDefault.length,
  //   "Size of quality 10 is smaller than default"
  // );
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
              length: image.length,
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
