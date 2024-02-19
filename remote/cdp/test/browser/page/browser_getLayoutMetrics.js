/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function documentSmallerThanViewport({ client }) {
  const { Page } = client;
  await loadURL(toDataURL("<div>Hello world"));

  const { contentSize, layoutViewport } = await Page.getLayoutMetrics();
  await checkContentSize(contentSize);
  await checkLayoutViewport(layoutViewport);

  is(
    contentSize.x,
    layoutViewport.pageX,
    "X position of content is equal to layout viewport"
  );
  is(
    contentSize.y,
    layoutViewport.pageY,
    "Y position of content is equal to layout viewport"
  );
  Assert.lessOrEqual(
    contentSize.width,
    layoutViewport.clientWidth,
    "Width of content is smaller than the layout viewport"
  );
  Assert.lessOrEqual(
    contentSize.height,
    layoutViewport.clientHeight,
    "Height of content is smaller than the layout viewport"
  );
});

add_task(async function documentLargerThanViewport({ client }) {
  const { Page } = client;
  await loadURL(toDataURL("<div style='margin: 150vh 0 0 150vw'>Hello world"));

  const { contentSize, layoutViewport } = await Page.getLayoutMetrics();
  await checkContentSize(contentSize);
  await checkLayoutViewport(layoutViewport, { scrollbars: true });

  is(
    contentSize.x,
    layoutViewport.pageX,
    "X position of content is equal to layout viewport"
  );
  is(
    contentSize.y,
    layoutViewport.pageY,
    "Y position of content is equal to layout viewport"
  );
  Assert.greater(
    contentSize.width,
    layoutViewport.clientWidth,
    "Width of content is larger than the layout viewport"
  );
  Assert.greater(
    contentSize.height,
    layoutViewport.clientHeight,
    "Height of content is larger than the layout viewport"
  );
});

add_task(async function documentLargerThanViewportScrolledXY({ client }) {
  const { Page } = client;
  await loadURL(toDataURL("<div style='margin: 150vh 0 0 150vw'>Hello world"));

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    content.scrollTo(50, 100);
  });

  const { contentSize, layoutViewport } = await Page.getLayoutMetrics();
  await checkContentSize(contentSize);
  await checkLayoutViewport(layoutViewport, { scrollbars: true });

  is(
    layoutViewport.pageX,
    contentSize.x + 50,
    "X position of content is equal to layout viewport"
  );
  is(
    layoutViewport.pageY,
    contentSize.y + 100,
    "Y position of content is equal to layout viewport"
  );
  Assert.greater(
    contentSize.width,
    layoutViewport.clientWidth,
    "Width of content is larger than the layout viewport"
  );
  Assert.greater(
    contentSize.height,
    layoutViewport.clientHeight,
    "Height of content is larger than the layout viewport"
  );
});

async function checkContentSize(rect) {
  const expected = await getContentSize();

  is(rect.x, expected.x, "Expected x position returned");
  is(rect.y, expected.y, "Expected y position returned");
  is(rect.width, expected.width, "Expected width returned");
  is(rect.height, expected.height, "Expected height returned");
}

async function checkLayoutViewport(viewport, options = {}) {
  const { scrollbars = false } = options;

  const expected = await getViewportSize();

  if (scrollbars) {
    const { width, height } = await getScrollbarSize();
    expected.width -= width;
    expected.height -= height;
  }

  is(viewport.pageX, expected.x, "Expected x position returned");
  is(viewport.pageY, expected.y, "Expected y position returned");
  is(viewport.clientWidth, expected.width, "Expected width returned");
  is(viewport.clientHeight, expected.height, "Expected height returned");
}
