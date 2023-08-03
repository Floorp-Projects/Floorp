/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const DOC_TOUCH = toDataURL("<div>Hello world");

add_task(async function invalidEnabledType({ client }) {
  const { Emulation } = client;

  for (const enabled of [null, "", 1, [], {}]) {
    await Assert.rejects(
      Emulation.setTouchEmulationEnabled({ enabled }),
      /enabled: boolean value expected/,
      `Fails with invalid type: ${enabled}`
    );
  }
});

add_task(async function enableAndDisable({ client }) {
  const { Emulation, Runtime } = client;
  const url = toDataURL("<p>foo");

  await enableRuntime(client);

  let contextCreated = Runtime.executionContextCreated();
  await loadURL(url);
  let result = await contextCreated;
  await assertTouchEnabled(client, result.context, false);

  await Emulation.setTouchEmulationEnabled({ enabled: true });
  contextCreated = Runtime.executionContextCreated();
  await loadURL(url);
  result = await contextCreated;
  await assertTouchEnabled(client, result.context, true);

  await Emulation.setTouchEmulationEnabled({ enabled: false });
  contextCreated = Runtime.executionContextCreated();
  await loadURL(url);
  result = await contextCreated;
  await assertTouchEnabled(client, result.context, false);
});

add_task(async function receiveTouchEventsWhenEnabled({ client }) {
  const { Emulation, Runtime } = client;

  await enableRuntime(client);

  await Emulation.setTouchEmulationEnabled({ enabled: true });
  const contextCreated = Runtime.executionContextCreated();
  await loadURL(DOC_TOUCH);
  const { context } = await contextCreated;

  await assertTouchEnabled(client, context, true);

  const { result } = await evaluate(client, context.id, () => {
    return new Promise(resolve => {
      window.ontouchstart = () => {
        resolve(true);
      };
      window.dispatchEvent(new Event("touchstart"));
      resolve(false);
    });
  });
  is(result.value, true, "Received touch event");
});

async function assertTouchEnabled(client, context, expectedStatus) {
  const { result } = await evaluate(client, context.id, () => {
    return "ontouchstart" in window;
  });

  if (expectedStatus) {
    ok(result.value, "Touch emulation enabled");
  } else {
    ok(!result.value, "Touch emulation disabled");
  }
}
