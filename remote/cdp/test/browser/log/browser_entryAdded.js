/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function noEventsWhenLogDomainDisabled({ client }) {
  await runEntryAddedTest(client, 0, async () => {
    await throwScriptError("foo");
  });
});

add_task(async function noEventsAfterLogDomainDisabled({ client }) {
  const { Log } = client;

  await Log.enable();
  await Log.disable();

  await runEntryAddedTest(client, 0, async () => {
    await throwScriptError("foo");
  });
});

add_task(async function noEventsForConsoleMessageWithException({ client }) {
  const { Log } = client;

  await Log.enable();

  const context = await enableRuntime(client);
  await runEntryAddedTest(client, 0, async () => {
    evaluate(client, context.id, () => {
      const foo = {};
      foo.bar();
    });
  });
});

add_task(async function eventsForScriptErrorWithoutException({ client }) {
  const { Log } = client;

  await Log.enable();

  await enableRuntime(client);
  const events = await runEntryAddedTest(client, 1, async () => {
    throwScriptError({
      text: "foo",
      sourceName: "https://foo.bar",
      lineNumber: 7,
      category: "javascript",
    });
  });

  is(events[0].source, "javascript", "Got expected source");
  is(events[0].level, "error", "Got expected level");
  is(events[0].text, "foo", "Got expected text");
  is(events[0].url, "https://foo.bar", "Got expected url");
  is(events[0].lineNumber, 7, "Got expected line number");
});

add_task(async function eventsForScriptErrorLevels({ client }) {
  const { Log } = client;

  await Log.enable();

  const flags = {
    info: Ci.nsIScriptError.infoFlag,
    warning: Ci.nsIScriptError.warningFlag,
    error: Ci.nsIScriptError.errorFlag,
  };

  await enableRuntime(client);
  for (const [level, flag] of Object.entries(flags)) {
    const events = await runEntryAddedTest(client, 1, async () => {
      throwScriptError({ text: level, flag });
    });

    is(events[0].text, level, "Got expected text");
    is(events[0].level, level, "Got expected level");
  }
});

add_task(async function eventsForScriptErrorContent({ client }) {
  const { Log } = client;

  await Log.enable();

  const context = await enableRuntime(client);
  const events = await runEntryAddedTest(client, 1, async () => {
    evaluate(client, context.id, () => {
      document.execCommand("copy");
    });
  });

  is(events[0].source, "other", "Got expected source");
  is(events[0].level, "warning", "Got expected level");
  ok(
    events[0].text.includes("document.execCommand(‘cut’/‘copy’) was denied"),
    "Got expected text"
  );
  is(events[0].url, undefined, "Got undefined url");
  is(events[0].lineNumber, 2, "Got expected line number");
});

async function runEntryAddedTest(client, eventCount, callback) {
  const { Log } = client;

  const EVENT_ENTRY_ADDED = "Log.entryAdded";

  const history = new RecordEvents(eventCount);
  history.addRecorder({
    event: Log.entryAdded,
    eventName: EVENT_ENTRY_ADDED,
    messageFn: () => `Received "${EVENT_ENTRY_ADDED}"`,
  });

  const timeBefore = Date.now();
  await callback();

  const entryAddedEvents = await history.record();
  is(entryAddedEvents.length, eventCount, "Got expected amount of events");

  if (eventCount == 0) {
    return [];
  }

  const timeAfter = Date.now();

  // Check basic details for entryAdded events
  entryAddedEvents.forEach(event => {
    const timestamp = event.payload.entry.timestamp;

    ok(
      timestamp >= timeBefore && timestamp <= timeAfter,
      "Got valid timestamp"
    );
  });

  return entryAddedEvents.map(event => event.payload.entry);
}
