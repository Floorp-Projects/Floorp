/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const PAGE_CONSOLE_EVENTS =
  "http://example.com/browser/remote/cdp/test/browser/runtime/doc_console_events.html";

add_task(async function noEventsWhenRuntimeDomainDisabled({ client }) {
  await runConsoleTest(client, 0, async () => {
    ContentTask.spawn(gBrowser.selectedBrowser, {}, () => console.log("foo"));
  });
});

add_task(async function noEventsAfterRuntimeDomainDisabled({ client }) {
  const { Runtime } = client;

  await Runtime.enable();
  await Runtime.disable();

  await runConsoleTest(client, 0, async () => {
    ContentTask.spawn(gBrowser.selectedBrowser, {}, () => console.log("foo"));
  });
});

add_task(async function noEventsForJavascriptErrors({ client }) {
  await loadURL(PAGE_CONSOLE_EVENTS);
  const context = await enableRuntime(client);

  await runConsoleTest(client, 0, async () => {
    evaluate(client, context.id, () => {
      document.getElementById("js-error").click();
    });
  });
});

add_task(async function consoleAPI({ client }) {
  const context = await enableRuntime(client);

  const events = await runConsoleTest(client, 1, async () => {
    await evaluate(client, context.id, () => {
      console.log("foo");
    });
  });

  is(events[0].type, "log", "Got expected type");
  is(events[0].args[0].value, "foo", "Got expected argument value");
  is(
    events[0].executionContextId,
    context.id,
    "Got event from current execution context"
  );
});

add_task(async function consoleAPITypes({ client }) {
  const expectedEvents = ["dir", "error", "log", "timeEnd", "trace", "warning"];
  const levels = ["dir", "error", "log", "time", "timeEnd", "trace", "warn"];

  const context = await enableRuntime(client);

  const events = await runConsoleTest(
    client,
    expectedEvents.length,
    async () => {
      for (const level of levels) {
        await evaluate(
          client,
          context.id,
          level => {
            console[level]("foo");
          },
          level
        );
      }
    }
  );

  events.forEach((event, index) => {
    console.log(`Check for event type "${expectedEvents[index]}"`);
    is(event.type, expectedEvents[index], "Got expected type");
  });
});

add_task(async function consoleAPIArgumentsCount({ client }) {
  const argumentList = [[], ["foo"], ["foo", "bar", "cheese"]];

  const context = await enableRuntime(client);

  const events = await runConsoleTest(client, argumentList.length, async () => {
    for (const args of argumentList) {
      await evaluate(
        client,
        context.id,
        args => {
          console.log(...args);
        },
        args
      );
    }
  });

  events.forEach((event, index) => {
    console.log(`Check for event args "${argumentList[index]}"`);

    const argValues = event.args.map(arg => arg.value);
    Assert.deepEqual(argValues, argumentList[index], "Got expected args");
  });
});

add_task(async function consoleAPIArgumentsAsRemoteObject({ client }) {
  const context = await enableRuntime(client);

  const events = await runConsoleTest(client, 1, async () => {
    await evaluate(client, context.id, () => {
      console.log("foo", Symbol("foo"));
    });
  });

  Assert.equal(events[0].args.length, 2, "Got expected amount of arguments");

  is(events[0].args[0].type, "string", "Got expected argument type");
  is(events[0].args[0].value, "foo", "Got expected argument value");

  is(events[0].args[1].type, "symbol", "Got expected argument type");
  is(
    events[0].args[1].description,
    "Symbol(foo)",
    "Got expected argument description"
  );
  ok(!!events[0].args[1].objectId, "Got objectId for argument");
});

add_task(async function consoleAPIByContentInteraction({ client }) {
  await loadURL(PAGE_CONSOLE_EVENTS);
  const context = await enableRuntime(client);

  const events = await runConsoleTest(client, 1, async () => {
    evaluate(client, context.id, () => {
      document.getElementById("console-error").click();
    });
  });

  is(events[0].type, "error", "Got expected type");
  is(events[0].args[0].value, "foo", "Got expected argument value");
  is(
    events[0].executionContextId,
    context.id,
    "Got event from current execution context"
  );
});

async function runConsoleTest(client, eventCount, callback, options = {}) {
  const { Runtime } = client;

  const EVENT_CONSOLE_API_CALLED = "Runtime.consoleAPICalled";

  const history = new RecordEvents(eventCount);
  history.addRecorder({
    event: Runtime.consoleAPICalled,
    eventName: EVENT_CONSOLE_API_CALLED,
    messageFn: payload =>
      `Received ${EVENT_CONSOLE_API_CALLED} for ${payload.type}`,
  });

  const timeBefore = Date.now();
  await callback();

  const consoleAPIentries = await history.record();
  is(consoleAPIentries.length, eventCount, "Got expected amount of events");

  if (eventCount == 0) {
    return [];
  }

  const timeAfter = Date.now();

  // Check basic details for consoleAPICalled events
  consoleAPIentries.forEach(({ payload }) => {
    const timestamp = payload.timestamp;

    ok(
      timestamp >= timeBefore && timestamp <= timeAfter,
      `Timestamp ${timestamp} in expected range [${timeBefore} - ${timeAfter}]`
    );
  });

  return consoleAPIentries.map(event => event.payload);
}
