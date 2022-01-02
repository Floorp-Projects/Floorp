/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const PAGE_URL =
  "https://example.com/browser/remote/cdp/test/browser/input/doc_events.html";

add_task(async function testPressedReleasedAndClick({ client }) {
  const { Input } = client;

  await loadURL(PAGE_URL);
  await resetEvents();

  info("Click the 'pointers' div.");
  await Input.dispatchMouseEvent({
    type: "mousePressed",
    x: 80,
    y: 180,
  });
  await Input.dispatchMouseEvent({
    type: "mouseReleased",
    x: 80,
    y: 180,
  });

  const events = await getEvents();
  ["mousedown", "mouseup", "click"].forEach((type, index) => {
    info(`Checking properties for event ${type}`);
    checkProperties({ type, button: 0 }, events[index]);
  });
});

add_task(async function testModifiers({ client }) {
  const { Input } = client;

  await loadURL(PAGE_URL);

  const testable_modifiers = [
    [alt],
    [ctrl],
    [meta],
    [shift],
    [alt, meta],
    [ctrl, shift],
    [alt, ctrl, meta, shift],
  ];

  for (let modifiers of testable_modifiers) {
    info(`MousePressed with modifier ${modifiers} on the 'pointers' div.`);

    await resetEvents();
    await Input.dispatchMouseEvent({
      type: "mousePressed",
      x: 80,
      y: 180,
      modifiers: modifiers.reduce((previous, current) => previous | current),
    });

    const events = await getEvents();
    const expectedEvent = {
      type: "mousedown",
      button: 0,
      altKey: modifiers.includes(alt),
      ctrlKey: modifiers.includes(ctrl),
      metaKey: modifiers.includes(meta),
      shiftKey: modifiers.includes(shift),
    };

    checkProperties(expectedEvent, events[0]);
  }
});

add_task(async function testClickCount({ client }) {
  const { Input } = client;

  await loadURL(PAGE_URL);

  const testable_clickCounts = [
    { type: "click", clickCount: 1 },
    { type: "dblclick", clickCount: 2 },
  ];

  for (const { clickCount, type } of testable_clickCounts) {
    info(`MousePressed with clickCount ${clickCount} on the 'pointers' div.`);

    await resetEvents();
    await Input.dispatchMouseEvent({
      type: "mousePressed",
      x: 80,
      y: 180,
      clickCount,
    });
    await Input.dispatchMouseEvent({
      type: "mouseReleased",
      x: 80,
      y: 180,
      clickCount,
    });

    const events = await getEvents();
    checkProperties({ type, button: 0 }, events[events.length - 1]);
  }
});

add_task(async function testDispatchMouseEventAwaitClick({ client }) {
  const { Input } = client;

  await setupForInput(PAGE_URL);
  await loadURL(
    toDataURL(`
      <div onclick="setTimeout(() => result = 'clicked', 0)">foo</div>
    `)
  );

  const { x, y } = await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [],
    () => {
      const div = content.document.querySelector("div");
      return div.getBoundingClientRect();
    }
  );

  await Input.dispatchMouseEvent({ type: "mousePressed", x, y });
  await Input.dispatchMouseEvent({ type: "mouseReleased", x, y });

  const context = await enableRuntime(client);
  const { result } = await evaluate(
    client,
    context.id,
    () => globalThis.result
  );

  is(result.value, "clicked", "Awaited click event handler");
});
