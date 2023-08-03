/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const DOC_SMALL = toDataURL("<div>Hello world");
const DOC_LARGE = toDataURL("<div style='margin: 150vh 0 0 150vw'>Hello world");

const MAX_WINDOW_SIZE = 10000000;

function getContentDPR() {
  info(`Retrieve device pixel ratio in content`);
  return SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [],
    _ => content.browsingContext.overrideDPPX || content.devicePixelRatio
  );
}

add_task(async function dimensionsSmallerThanWindow({ client }) {
  const { Emulation, Page } = client;

  await loadURL(DOC_SMALL);
  const { layoutViewport } = await Page.getLayoutMetrics();

  const overrideSettings = {
    width: Math.floor(layoutViewport.clientWidth / 2),
    height: Math.floor(layoutViewport.clientHeight / 3),
    deviceScaleFactor: 1.0,
  };

  await Emulation.setDeviceMetricsOverride(overrideSettings);
  await loadURL(DOC_SMALL);

  const updatedLayoutMetrics = await Page.getLayoutMetrics();
  is(
    updatedLayoutMetrics.layoutViewport.clientWidth,
    overrideSettings.width,
    "Expected layout viewport width set"
  );
  is(
    updatedLayoutMetrics.layoutViewport.clientHeight,
    overrideSettings.height,
    "Expected layout viewport height set"
  );
  is(
    updatedLayoutMetrics.contentSize.width,
    overrideSettings.width,
    "Expected content size width set"
  );
  is(
    updatedLayoutMetrics.contentSize.height,
    overrideSettings.height,
    "Expected content size height set"
  );
});

add_task(async function dimensionsLargerThanWindow({ client }) {
  const { Emulation, Page } = client;

  await loadURL(DOC_LARGE);
  const { layoutViewport } = await Page.getLayoutMetrics();

  const overrideSettings = {
    width: layoutViewport.clientWidth * 2,
    height: layoutViewport.clientHeight * 2,
    deviceScaleFactor: 1.0,
  };

  await Emulation.setDeviceMetricsOverride(overrideSettings);
  await loadURL(DOC_LARGE);

  const scrollbarSize = await getScrollbarSize();
  const updatedLayoutMetrics = await Page.getLayoutMetrics();

  is(
    updatedLayoutMetrics.layoutViewport.clientWidth,
    overrideSettings.width - scrollbarSize.width,
    "Expected layout viewport width set"
  );
  is(
    updatedLayoutMetrics.layoutViewport.clientHeight,
    overrideSettings.height - scrollbarSize.height,
    "Expected layout viewport height set"
  );
});

add_task(async function noSizeChangeForSameWidthAndHeight({ client }) {
  const { Emulation, Page } = client;

  await loadURL(DOC_SMALL);
  const { layoutViewport } = await Page.getLayoutMetrics();

  const overrideSettings = {
    width: layoutViewport.clientWidth,
    height: layoutViewport.clientHeight,
    deviceScaleFactor: 1.0,
  };

  await Emulation.setDeviceMetricsOverride(overrideSettings);
  await loadURL(DOC_SMALL);

  const updatedLayoutMetrics = await Page.getLayoutMetrics();
  is(
    updatedLayoutMetrics.layoutViewport.clientWidth,
    layoutViewport.clientWidth,
    "Expected layout viewport width set"
  );
  is(
    updatedLayoutMetrics.layoutViewport.clientHeight,
    layoutViewport.clientHeight,
    "Expected layout viewport height set"
  );
});

add_task(async function noWidthChangeWithZeroWidth({ client }) {
  const { Emulation, Page } = client;

  await loadURL(DOC_SMALL);
  const { layoutViewport } = await Page.getLayoutMetrics();

  const overrideSettings = {
    width: 0,
    height: Math.floor(layoutViewport.clientHeight / 3),
    deviceScaleFactor: 1.0,
  };

  await Emulation.setDeviceMetricsOverride(overrideSettings);
  await loadURL(DOC_SMALL);

  const updatedLayoutMetrics = await Page.getLayoutMetrics();
  is(
    updatedLayoutMetrics.layoutViewport.clientWidth,
    layoutViewport.clientWidth,
    "Expected layout viewport width set"
  );
  is(
    updatedLayoutMetrics.layoutViewport.clientHeight,
    overrideSettings.height,
    "Expected layout viewport height set"
  );
});

add_task(async function noHeightChangeWithZeroHeight({ client }) {
  const { Emulation, Page } = client;

  await loadURL(DOC_SMALL);
  const { layoutViewport } = await Page.getLayoutMetrics();

  const overrideSettings = {
    width: Math.floor(layoutViewport.clientWidth / 2),
    height: 0,
    deviceScaleFactor: 1.0,
  };

  await Emulation.setDeviceMetricsOverride(overrideSettings);
  await loadURL(DOC_SMALL);

  const updatedLayoutMetrics = await Page.getLayoutMetrics();
  is(
    updatedLayoutMetrics.layoutViewport.clientWidth,
    overrideSettings.width,
    "Expected layout viewport width set"
  );
  is(
    updatedLayoutMetrics.layoutViewport.clientHeight,
    layoutViewport.clientHeight,
    "Expected layout viewport height set"
  );
});

add_task(async function nosizeChangeWithZeroWidthAndHeight({ client }) {
  const { Emulation, Page } = client;

  await loadURL(DOC_SMALL);
  const { layoutViewport } = await Page.getLayoutMetrics();

  const overrideSettings = {
    width: 0,
    height: 0,
    deviceScaleFactor: 1.0,
  };

  await Emulation.setDeviceMetricsOverride(overrideSettings);
  await loadURL(DOC_SMALL);

  const updatedLayoutMetrics = await Page.getLayoutMetrics();
  is(
    updatedLayoutMetrics.layoutViewport.clientWidth,
    layoutViewport.clientWidth,
    "Expected layout viewport width set"
  );
  is(
    updatedLayoutMetrics.layoutViewport.clientHeight,
    layoutViewport.clientHeight,
    "Expected layout viewport height set"
  );
});

add_task(async function failsWithNegativeWidth({ client }) {
  const { Emulation, Page } = client;

  await loadURL(DOC_SMALL);
  const { layoutViewport } = await Page.getLayoutMetrics();
  const ratio = await getContentDPR();

  const overrideSettings = {
    width: -1,
    height: Math.floor(layoutViewport.clientHeight / 3),
    deviceScaleFactor: 1.0,
  };

  await Assert.rejects(
    Emulation.setDeviceMetricsOverride(overrideSettings),
    err =>
      err.message.includes(
        "Width and height values must be positive, not greater than 10000000"
      ),
    "Negative width raised error"
  );

  await loadURL(DOC_SMALL);
  const updatedLayoutMetrics = await Page.getLayoutMetrics();

  is(
    updatedLayoutMetrics.layoutViewport.clientWidth,
    layoutViewport.clientWidth,
    "Visible layout width hasn't been changed"
  );
  is(
    updatedLayoutMetrics.layoutViewport.clientHeight,
    layoutViewport.clientHeight,
    "Visible layout height hasn't been changed"
  );
  is(await getContentDPR(), ratio, "Device pixel ratio hasn't been changed");
});

add_task(async function failsWithTooLargeWidth({ client }) {
  const { Emulation, Page } = client;

  await loadURL(DOC_SMALL);
  const { layoutViewport } = await Page.getLayoutMetrics();
  const ratio = await getContentDPR();

  const overrideSettings = {
    width: MAX_WINDOW_SIZE + 1,
    height: Math.floor(layoutViewport.clientHeight / 3),
    deviceScaleFactor: 1.0,
  };

  await Assert.rejects(
    Emulation.setDeviceMetricsOverride(overrideSettings),
    err =>
      err.message.includes(
        "Width and height values must be positive, not greater than 10000000"
      ),
    "Too large width raised error"
  );

  await loadURL(DOC_SMALL);
  const updatedLayoutMetrics = await Page.getLayoutMetrics();

  is(
    updatedLayoutMetrics.layoutViewport.clientWidth,
    layoutViewport.clientWidth,
    "Visible layout width hasn't been changed"
  );
  is(
    updatedLayoutMetrics.layoutViewport.clientHeight,
    layoutViewport.clientHeight,
    "Visible layout height hasn't been changed"
  );
  is(await getContentDPR(), ratio, "Device pixel ratio hasn't been changed");
});

add_task(async function failsWithNegativeHeight({ client }) {
  const { Emulation, Page } = client;

  await loadURL(DOC_SMALL);
  const { layoutViewport } = await Page.getLayoutMetrics();
  const ratio = await getContentDPR();

  const overrideSettings = {
    width: Math.floor(layoutViewport.clientWidth / 2),
    height: -1,
    deviceScaleFactor: 1.0,
  };

  await Assert.rejects(
    Emulation.setDeviceMetricsOverride(overrideSettings),
    err =>
      err.message.includes(
        "Width and height values must be positive, not greater than 10000000"
      ),
    "Negative height raised error"
  );
  await loadURL(DOC_SMALL);
  const updatedLayoutMetrics = await Page.getLayoutMetrics();

  is(
    updatedLayoutMetrics.layoutViewport.clientWidth,
    layoutViewport.clientWidth,
    "Visible layout width hasn't been changed"
  );
  is(
    updatedLayoutMetrics.layoutViewport.clientHeight,
    layoutViewport.clientHeight,
    "Visible layout height hasn't been changed"
  );
  is(await getContentDPR(), ratio, "Device pixel ratio hasn't been changed");
});

add_task(async function failsWithTooLargeHeight({ client }) {
  const { Emulation, Page } = client;

  await loadURL(DOC_SMALL);
  const { layoutViewport } = await Page.getLayoutMetrics();
  const ratio = await getContentDPR();

  const overrideSettings = {
    width: Math.floor(layoutViewport.clientWidth / 2),
    height: MAX_WINDOW_SIZE + 1,
    deviceScaleFactor: 1.0,
  };
  await Assert.rejects(
    Emulation.setDeviceMetricsOverride(overrideSettings),
    err =>
      err.message.includes(
        "Width and height values must be positive, not greater than 10000000"
      ),
    "Too large height raised error"
  );

  await loadURL(DOC_SMALL);
  const updatedLayoutMetrics = await Page.getLayoutMetrics();

  is(
    updatedLayoutMetrics.layoutViewport.clientWidth,
    layoutViewport.clientWidth,
    "Visible layout width hasn't been changed"
  );
  is(
    updatedLayoutMetrics.layoutViewport.clientHeight,
    layoutViewport.clientHeight,
    "Visible layout height hasn't been changed"
  );
  is(await getContentDPR(), ratio, "Device pixel ratio hasn't been changed");
});

add_task(async function setDevicePixelRatio({ client }) {
  const { Emulation, Page } = client;

  await loadURL(DOC_SMALL);
  const { layoutViewport } = await Page.getLayoutMetrics();
  const ratio_orig = await getContentDPR();

  const overrideSettings = {
    width: layoutViewport.clientWidth,
    height: layoutViewport.clientHeight,
    deviceScaleFactor: ratio_orig * 2,
  };

  // Set a custom pixel ratio
  await Emulation.setDeviceMetricsOverride(overrideSettings);
  await loadURL(DOC_SMALL);

  is(await getContentDPR(), ratio_orig * 2, "Expected device pixel ratio set");
});

add_task(async function failsWithNegativeRatio({ client }) {
  const { Emulation, Page } = client;

  await loadURL(DOC_SMALL);
  const { layoutViewport } = await Page.getLayoutMetrics();
  const ratio = await getContentDPR();

  const overrideSettings = {
    width: Math.floor(layoutViewport.clientHeight / 2),
    height: Math.floor(layoutViewport.clientHeight / 3),
    deviceScaleFactor: -1,
  };

  await Assert.rejects(
    Emulation.setDeviceMetricsOverride(overrideSettings),
    err => err.message.includes("deviceScaleFactor: must be positive"),
    "Negative device scale factor raised error"
  );

  await loadURL(DOC_SMALL);
  const updatedLayoutMetrics = await Page.getLayoutMetrics();

  is(
    updatedLayoutMetrics.layoutViewport.clientWidth,
    layoutViewport.clientWidth,
    "Visible layout width hasn't been changed"
  );
  is(
    updatedLayoutMetrics.layoutViewport.clientHeight,
    layoutViewport.clientHeight,
    "Visible layout height hasn't been changed"
  );
  is(await getContentDPR(), ratio, "Device pixel ratio hasn't been changed");
});
