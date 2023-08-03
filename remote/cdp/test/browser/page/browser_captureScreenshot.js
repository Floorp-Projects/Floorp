/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function documentSmallerThanViewport({ client }) {
  const { Page } = client;

  await loadURLWithElement();

  info("Check that captureScreenshot() captures the viewport by default");
  const { data } = await Page.captureScreenshot();
  ok(!!data, "Screenshot data is not empty");

  const scale = await getDevicePixelRatio();
  const viewport = await getViewportSize();
  const { mimeType, width, height } = await getImageDetails(data);

  is(mimeType, "image/png", "Screenshot has correct MIME type");
  is(width, (viewport.width - viewport.x) * scale, "Image has expected width");
  is(
    height,
    (viewport.height - viewport.y) * scale,
    "Image has expected height"
  );
});

add_task(async function documentLargerThanViewport({ client }) {
  const { Page } = client;

  await loadURL(toDataURL("<div style='margin: 100vh 100vw'>Hello world"));

  info("Check that captureScreenshot() captures the viewport by default");
  const { data } = await Page.captureScreenshot();
  ok(!!data, "Screenshot data is not empty");

  const scale = await getDevicePixelRatio();
  const scrollbarSize = await getScrollbarSize();
  const viewport = await getViewportSize();
  const { mimeType, width, height } = await getImageDetails(data);

  is(mimeType, "image/png", "Screenshot has correct MIME type");
  is(
    width,
    (viewport.width - viewport.x - scrollbarSize.width) * scale,
    "Image has expected width"
  );
  is(
    height,
    (viewport.height - viewport.y - scrollbarSize.height) * scale,
    "Image has expected height"
  );
});

add_task(async function invalidFormat({ client }) {
  const { Page } = client;
  await loadURL(toDataURL("<div>Hello world"));

  await Assert.rejects(
    Page.captureScreenshot({ format: "foo" }),
    err => err.message.includes(`Unsupported MIME type: image`),
    "captureScreenshot raised error for invalid image format"
  );
});

add_task(async function asJPEGFormat({ client }) {
  const { Page } = client;
  await loadURL(toDataURL("<div>Hello world"));

  info("Check that captureScreenshot() captures as JPEG format");
  const { data } = await Page.captureScreenshot({ format: "jpeg" });
  ok(!!data, "Screenshot data is not empty");

  const scale = await getDevicePixelRatio();
  const viewport = await getViewportSize();
  const { mimeType, height, width } = await getImageDetails(data);

  is(mimeType, "image/jpeg", "Screenshot has correct MIME type");
  is(width, (viewport.width - viewport.x) * scale);
  is(height, (viewport.height - viewport.y) * scale);
});

add_task(async function asJPEGFormatAndQuality({ client }) {
  const { Page } = client;
  await loadURL(toDataURL("<div>Hello world"));

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
  const viewport = await getViewportSize();

  // Images are all of the same dimension
  is(infoDefault.width, (viewport.width - viewport.x) * scale);
  is(infoDefault.height, (viewport.height - viewport.y) * scale);

  is(info100.width, (viewport.width - viewport.x) * scale);
  is(info100.height, (viewport.height - viewport.y) * scale);

  is(info10.width, (viewport.width - viewport.x) * scale);
  is(info10.height, (viewport.height - viewport.y) * scale);

  // Images of different quality result in different content sizes
  ok(
    info100.length > infoDefault.length,
    "Size of quality 100 is larger than default"
  );
  ok(
    info10.length < infoDefault.length,
    "Size of quality 10 is smaller than default"
  );
});

add_task(async function clipMissingProperties({ client }) {
  const { Page } = client;
  const contentSize = await getContentSize();

  for (const prop of ["x", "y", "width", "height", "scale"]) {
    console.info(`Check for missing ${prop}`);

    const clip = {
      x: 0,
      y: 0,
      width: contentSize.width,
      height: contentSize.height,
    };
    clip[prop] = undefined;

    await Assert.rejects(
      Page.captureScreenshot({ clip }),
      err => err.message.includes(`clip.${prop}: double value expected`),
      `raised error for missing clip.${prop} property`
    );
  }
});

add_task(async function clipOutOfBoundsXAndY({ client }) {
  const { Page } = client;

  const ratio = await getDevicePixelRatio();
  const size = 50;

  await loadURLWithElement();
  const contentSize = await getContentSize();

  var { data: refData } = await Page.captureScreenshot({
    clip: {
      x: 0,
      y: 0,
      width: size,
      height: size,
      scale: 1,
    },
  });

  for (const x of [-1, contentSize.width]) {
    console.info(`Check out-of-bounds x for ${x}`);
    const { data } = await Page.captureScreenshot({
      clip: {
        x,
        y: 0,
        width: size,
        height: size,
        scale: 1,
      },
    });
    const { width, height } = await getImageDetails(data);

    is(width, size * ratio, "Image has expected width");
    is(height, size * ratio, "Image has expected height");
    is(data, refData, "Image is equal");
  }

  for (const y of [-1, contentSize.height]) {
    console.info(`Check out-of-bounds y for ${y}`);
    const { data } = await Page.captureScreenshot({
      clip: {
        x: 0,
        y,
        width: size,
        height: size,
        scale: 1,
      },
    });
    const { width, height } = await getImageDetails(data);

    is(width, size * ratio, "Image has expected width");
    is(height, size * ratio, "Image has expected height");
    is(data, refData, "Image is equal");
  }
});

add_task(async function clipOutOfBoundsWidthAndHeight({ client }) {
  const { Page } = client;
  const ratio = await getDevicePixelRatio();

  await loadURL(toDataURL("<div style='margin: 100vh 100vw'>Hello world"));
  const contentSize = await getContentSize();

  var { data: refData } = await Page.captureScreenshot({
    clip: {
      x: 0,
      y: 0,
      width: contentSize.width,
      height: contentSize.height,
      scale: 1,
    },
  });

  for (const value of [-1, 0]) {
    console.info(`Check out-of-bounds width for ${value}`);
    const clip = {
      x: 0,
      y: 0,
      width: value,
      height: contentSize.height,
      scale: 1,
    };

    const { data } = await Page.captureScreenshot({ clip });
    const { width, height } = await getImageDetails(data);
    is(width, contentSize.width * ratio, "Image has expected width");
    is(height, contentSize.height * ratio, "Image has expected height");
    is(data, refData, "Image is equal");
  }

  for (const value of [-1, 0]) {
    console.info(`Check out-of-bounds height for ${value}`);
    const clip = {
      x: 0,
      y: 0,
      width: contentSize.width,
      height: value,
      scale: 1,
    };

    const { data } = await Page.captureScreenshot({ clip });
    const { width, height } = await getImageDetails(data);
    is(width, contentSize.width * ratio, "Image has expected width");
    is(height, contentSize.height * ratio, "Image has expected height");
    is(data, refData, "Image is equal");
  }
});

add_task(async function clipOutOfBoundsScale({ client }) {
  const { Page } = client;
  const ratio = await getDevicePixelRatio();

  await loadURLWithElement();
  const contentSize = await getContentSize();

  var { data: refData } = await Page.captureScreenshot({
    clip: {
      x: 0,
      y: 0,
      width: contentSize.width,
      height: contentSize.height,
      scale: 1,
    },
  });

  for (const value of [-1, 0]) {
    console.info(`Check out-of-bounds scale for ${value}`);
    var { data } = await Page.captureScreenshot({
      clip: {
        x: 0,
        y: 0,
        width: 50,
        height: 50,
        scale: value,
      },
    });

    const { width, height } = await getImageDetails(data);
    is(width, contentSize.width * ratio, "Image has expected width");
    is(height, contentSize.height * ratio, "Image has expected height");
    is(data, refData, "Image is equal");
  }
});

add_task(async function clipScale({ client }) {
  const { Page } = client;
  const ratio = await getDevicePixelRatio();

  for (const scale of [1.5, 2]) {
    console.info(`Check scale for ${scale}`);
    await loadURLWithElement({ width: 100 * scale, height: 100 * scale });
    var { data: refData } = await Page.captureScreenshot({
      clip: {
        x: 0,
        y: 0,
        width: 100 * scale,
        height: 100 * scale,
        scale: 1,
      },
    });

    await loadURLWithElement({ width: 100, height: 100 });
    var { data } = await Page.captureScreenshot({
      clip: {
        x: 0,
        y: 0,
        width: 100,
        height: 100,
        scale,
      },
    });

    const { width, height } = await getImageDetails(data);
    is(width, 100 * ratio * scale, "Image has expected width");
    is(height, 100 * ratio * scale, "Image has expected height");
    is(data, refData, "Image is equal");
  }
});

add_task(async function clipScaleAndDevicePixelRatio({ client }) {
  const { Page } = client;

  const originalRatio = await getDevicePixelRatio();

  const ratio = 2;
  const scale = 1.5;
  const size = 100;

  const expectedSize = size * ratio * scale;

  console.info(`Create reference screenshot: ${expectedSize}x${expectedSize}`);
  await loadURLWithElement({
    width: expectedSize,
    height: expectedSize,
  });
  var { data: refData } = await Page.captureScreenshot({
    clip: {
      x: 0,
      y: 0,
      width: expectedSize,
      height: expectedSize,
      scale: 1,
    },
  });

  await setDevicePixelRatio(originalRatio * ratio);

  await loadURLWithElement({ width: size, height: size });
  var { data } = await Page.captureScreenshot({
    clip: {
      x: 0,
      y: 0,
      width: size,
      height: size,
      scale,
    },
  });

  const { width, height } = await getImageDetails(data);
  is(width, expectedSize * originalRatio, "Image has expected width");
  is(height, expectedSize * originalRatio, "Image has expected height");
  is(data, refData, "Image is equal");
});

add_task(async function clipPosition({ client }) {
  const { Page } = client;
  const ratio = await getDevicePixelRatio();

  await loadURLWithElement();
  var { data: refData } = await Page.captureScreenshot({
    clip: {
      x: 0,
      y: 0,
      width: 100,
      height: 100,
      scale: 1,
    },
  });

  for (const [x, y] of [
    [10, 20],
    [20, 10],
    [20, 20],
  ]) {
    console.info(`Check postion for ${x} and ${y}`);
    await loadURLWithElement({ x, y });
    var { data } = await Page.captureScreenshot({
      clip: {
        x,
        y,
        width: 100,
        height: 100,
        scale: 1,
      },
    });

    const { width, height } = await getImageDetails(data);
    is(width, 100 * ratio, "Image has expected width");
    is(height, 100 * ratio, "Image has expected height");
    is(data, refData, "Image is equal");
  }
});

add_task(async function clipDimension({ client }) {
  const { Page } = client;
  const ratio = await getDevicePixelRatio();

  for (const [width, height] of [
    [10, 20],
    [20, 10],
    [20, 20],
  ]) {
    console.info(`Check width and height for ${width} and ${height}`);

    // Get reference image as section from a larger image
    await loadURLWithElement({ width: 50, height: 50 });
    var { data: refData } = await Page.captureScreenshot({
      clip: {
        x: 0,
        y: 0,
        width,
        height,
        scale: 1,
      },
    });

    await loadURLWithElement({ width, height });
    var { data } = await Page.captureScreenshot({
      clip: {
        x: 0,
        y: 0,
        width,
        height,
        scale: 1,
      },
    });

    const dimension = await getImageDetails(data);
    is(dimension.width, width * ratio, "Image has expected width");
    is(dimension.height, height * ratio, "Image has expected height");
    is(data, refData, "Image is equal");
  }
});

async function loadURLWithElement(options = {}) {
  const { x = 0, y = 0, width = 100, height = 100 } = options;

  const doc = `
    <style>
      body {
        margin: 0;
      }
      div {
        margin-left: ${x}px;
        margin-top: ${y}px;
        width: ${width}px;
        height: ${height}px;
        background: green;
      }
    </style>
    <body>
    <div></div>
  `;

  await loadURL(toDataURL(doc));
}

async function getDevicePixelRatio() {
  return SpecialPowers.spawn(gBrowser.selectedBrowser, [], function () {
    return content.browsingContext.overrideDPPX || content.devicePixelRatio;
  });
}

async function setDevicePixelRatio(dppx) {
  gBrowser.selectedBrowser.browsingContext.overrideDPPX = dppx;
}

async function getImageDetails(image) {
  const mimeType = getMimeType(image);

  return SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [{ mimeType, image }],
    async function ({ mimeType, image }) {
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
    magicBytes += raw.charCodeAt(i).toString(16).toUpperCase();
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
