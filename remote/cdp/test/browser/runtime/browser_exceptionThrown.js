/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const PAGE_CONSOLE_EVENTS =
  "https://example.com/browser/remote/cdp/test/browser/runtime/doc_console_events.html";

add_task(async function noEventsWhenRuntimeDomainDisabled({ client }) {
  await runExceptionThrownTest(client, 0, async () => {
    await throwScriptError({ text: "foo" });
  });
});

add_task(async function noEventsAfterRuntimeDomainDisabled({ client }) {
  const { Runtime } = client;

  await Runtime.enable();
  await Runtime.disable();

  await runExceptionThrownTest(client, 0, async () => {
    await throwScriptError({ text: "foo" });
  });
});

add_task(async function noEventsForScriptErrorWithoutException({ client }) {
  const { Runtime } = client;

  await Runtime.enable();

  await runExceptionThrownTest(client, 0, async () => {
    await throwScriptError({ text: "foo" });
  });
});

add_task(async function eventsForScriptErrorWithException({ client }) {
  await loadURL(PAGE_CONSOLE_EVENTS);

  const context = await enableRuntime(client);

  const events = await runExceptionThrownTest(client, 1, async () => {
    evaluate(client, context.id, () => {
      document.getElementById("js-error").click();
    });
  });

  is(
    typeof events[0].exceptionId,
    "number",
    "Got expected type for exception id"
  );
  is(
    events[0].text,
    "TypeError: foo.click is not a function",
    "Got expected text"
  );
  is(events[0].lineNumber, 8, "Got expected line number");
  is(events[0].columnNumber, 10, "Got expected column number");
  is(events[0].url, PAGE_CONSOLE_EVENTS, "Got expected url");
  is(
    events[0].executionContextId,
    context.id,
    "Got event from current execution context"
  );

  const callFrames = events[0].stackTrace.callFrames;
  is(callFrames.length, 2, "Got expected amount of call frames");

  is(callFrames[0].functionName, "throwError", "Got expected function name");
  is(typeof callFrames[0].scriptId, "string", "Got scriptId as string");
  is(callFrames[0].url, PAGE_CONSOLE_EVENTS, "Got expected url");
  is(callFrames[0].lineNumber, 8, "Got expected line number");
  is(callFrames[0].columnNumber, 10, "Got expected column number");

  is(callFrames[1].functionName, "onclick", "Got expected function name");
  is(callFrames[1].url, PAGE_CONSOLE_EVENTS, "Got expected url");
});

async function runExceptionThrownTest(client, eventCount, callback) {
  const { Runtime } = client;

  const EVENT_EXCEPTION_THROWN = "Runtime.exceptionThrown";

  const history = new RecordEvents(eventCount);
  history.addRecorder({
    event: Runtime.exceptionThrown,
    eventName: EVENT_EXCEPTION_THROWN,
    messageFn: payload => `Received "${payload.name}"`,
  });

  const timeBefore = Date.now();
  await callback();

  const exceptionThrownEvents = await history.record();
  is(exceptionThrownEvents.length, eventCount, "Got expected amount of events");

  if (eventCount == 0) {
    return [];
  }

  const timeAfter = Date.now();

  // Check basic details for entryAdded events
  exceptionThrownEvents.forEach(event => {
    const details = event.payload.exceptionDetails;
    const timestamp = event.payload.timestamp;

    is(typeof details, "object", "Got expected 'exceptionDetails' property");
    ok(
      timestamp >= timeBefore && timestamp <= timeAfter,
      `Timestamp ${timestamp} in expected range [${timeBefore} - ${timeAfter}]`
    );
  });

  return exceptionThrownEvents.map(event => event.payload.exceptionDetails);
}
