/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Request a longer timeout as we have many tests which are longer
requestLongerTimeout(2);

const PAGE_CONSOLE_EVENTS =
  "https://example.com/browser/remote/cdp/test/browser/runtime/doc_console_events.html";
const PAGE_CONSOLE_EVENTS_ONLOAD =
  "https://example.com/browser/remote/cdp/test/browser/runtime/doc_console_events_onload.html";

add_task(async function noEventsWhenRuntimeDomainDisabled({ client }) {
  await runConsoleTest(client, 0, async () => {
    SpecialPowers.spawn(gBrowser.selectedBrowser, [], () =>
      content.console.log("foo")
    );
  });
});

add_task(async function noEventsAfterRuntimeDomainDisabled({ client }) {
  const { Runtime } = client;

  await Runtime.enable();
  await Runtime.disable();

  await runConsoleTest(client, 0, async () => {
    SpecialPowers.spawn(gBrowser.selectedBrowser, [], () =>
      content.console.log("foo")
    );
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

add_task(async function consoleAPIBeforeEnable({ client }) {
  const { Runtime } = client;
  const timeBefore = Date.now();

  const check = async () => {
    const events = await runConsoleTest(
      client,
      1,
      async () => {
        await Runtime.enable();
      },
      // Set custom before timestamp as the event is before our callback
      { timeBefore }
    );

    is(events[0].type, "log", "Got expected type");
    is(events[0].args[0].value, "foo", "Got expected argument value");
  };

  // Load the page which runs a log on load
  await loadURL(PAGE_CONSOLE_EVENTS_ONLOAD);
  await check();

  // Disable and re-enable Runtime domain, should send event again
  await Runtime.disable();
  await check();
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

  const { callFrames } = events[0].stackTrace;
  is(callFrames.length, 1, "Got expected amount of call frames");

  is(callFrames[0].functionName, "", "Got expected call frame function name");
  is(callFrames[0].lineNumber, 0, "Got expected call frame line number");
  is(callFrames[0].columnNumber, 8, "Got expected call frame column number");
  is(
    callFrames[0].url,
    "javascript:console.error('foo')",
    "Got expected call frame URL"
  );
});

add_task(async function consoleAPIByScript({ client }) {
  const context = await enableRuntime(client);

  const events = await runConsoleTest(client, 1, async () => {
    await evaluate(client, context.id, function runLog() {
      console.trace("foo");
    });
  });

  const { callFrames } = events[0].stackTrace;
  is(callFrames.length, 1, "Got expected amount of call frames");

  is(
    callFrames[0].functionName,
    "runLog",
    "Got expected call frame function name"
  );
  is(callFrames[0].lineNumber, 1, "Got expected call frame line number");
  is(callFrames[0].columnNumber, 14, "Got expected call frame column number");
  is(callFrames[0].url, "debugger eval code", "Got expected call frame URL");
});

add_task(async function consoleAPIByScriptSubstack({ client }) {
  await loadURL(PAGE_CONSOLE_EVENTS);
  const context = await enableRuntime(client);

  const events = await runConsoleTest(client, 1, async () => {
    await evaluate(client, context.id, () => {
      document.getElementById("log-wrapper").click();
    });
  });

  const { callFrames } = events[0].stackTrace;
  is(callFrames.length, 5, "Got expected amount of call frames");

  is(
    callFrames[0].functionName,
    "runLogCaller",
    "Got expected call frame function name (frame 1)"
  );
  is(
    callFrames[0].lineNumber,
    13,
    "Got expected call frame line number (frame 1)"
  );
  is(
    callFrames[0].columnNumber,
    16,
    "Got expected call frame column number (frame 1)"
  );
  is(
    callFrames[0].url,
    PAGE_CONSOLE_EVENTS,
    "Got expected call frame UR (frame 1)"
  );

  is(
    callFrames[1].functionName,
    "runLogChild",
    "Got expected call frame function name (frame 2)"
  );
  is(
    callFrames[1].lineNumber,
    17,
    "Got expected call frame line number (frame 2)"
  );
  is(
    callFrames[1].columnNumber,
    8,
    "Got expected call frame column number (frame 2)"
  );
  is(
    callFrames[1].url,
    PAGE_CONSOLE_EVENTS,
    "Got expected call frame URL (frame 2)"
  );

  is(
    callFrames[2].functionName,
    "runLogParent",
    "Got expected call frame function name (frame 3)"
  );
  is(
    callFrames[2].lineNumber,
    20,
    "Got expected call frame line number (frame 3)"
  );
  is(
    callFrames[2].columnNumber,
    6,
    "Got expected call frame column number (frame 3)"
  );
  is(
    callFrames[2].url,
    PAGE_CONSOLE_EVENTS,
    "Got expected call frame URL (frame 3)"
  );

  is(
    callFrames[3].functionName,
    "onclick",
    "Got expected call frame function name (frame 4)"
  );
  is(
    callFrames[3].lineNumber,
    0,
    "Got expected call frame line number (frame 4)"
  );
  is(
    callFrames[3].columnNumber,
    0,
    "Got expected call frame column number (frame 4)"
  );
  is(
    callFrames[3].url,
    PAGE_CONSOLE_EVENTS,
    "Got expected call frame URL (frame 4)"
  );

  is(
    callFrames[4].functionName,
    "",
    "Got expected call frame function name (frame 5)"
  );
  is(
    callFrames[4].lineNumber,
    1,
    "Got expected call frame line number (frame 5)"
  );
  is(
    callFrames[4].columnNumber,
    45,
    "Got expected call frame column number (frame 5)"
  );
  is(
    callFrames[4].url,
    "debugger eval code",
    "Got expected call frame URL (frame 5)"
  );
});

async function runConsoleTest(client, eventCount, callback, options = {}) {
  let { timeBefore } = options;

  const { Runtime } = client;

  const EVENT_CONSOLE_API_CALLED = "Runtime.consoleAPICalled";

  const history = new RecordEvents(eventCount);
  history.addRecorder({
    event: Runtime.consoleAPICalled,
    eventName: EVENT_CONSOLE_API_CALLED,
    messageFn: payload =>
      `Received ${EVENT_CONSOLE_API_CALLED} for ${payload.type}`,
  });

  timeBefore ??= Date.now();
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
