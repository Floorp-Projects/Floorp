"use strict";

ChromeUtils.import("resource://gre/modules/Services.jsm", this);
ChromeUtils.import("resource://normandy/lib/EventEmitter.jsm", this);

const evidence = {
  a: 0,
  b: 0,
  c: 0,
  log: "",
};

function listenerA(x) {
  evidence.a += x;
  evidence.log += "a";
}

function listenerB(x) {
  evidence.b += x;
  evidence.log += "b";
}

function listenerC(x) {
  evidence.c += x;
  evidence.log += "c";
}

decorate_task(async function() {
  const eventEmitter = new EventEmitter();

  // Fire an unrelated event, to make sure nothing goes wrong
  eventEmitter.on("nothing");

  // bind listeners
  eventEmitter.on("event", listenerA);
  eventEmitter.on("event", listenerB);
  eventEmitter.once("event", listenerC);

  // one event for all listeners
  eventEmitter.emit("event", 1);
  // another event for a and b, since c should have turned off already
  eventEmitter.emit("event", 10);

  // make sure events haven't actually fired yet, just queued
  Assert.deepEqual(
    evidence,
    {
      a: 0,
      b: 0,
      c: 0,
      log: "",
    },
    "events are fired async"
  );

  // Spin the event loop to run events, so we can safely "off"
  await Promise.resolve();

  // Check intermediate event results
  Assert.deepEqual(
    evidence,
    {
      a: 11,
      b: 11,
      c: 1,
      log: "abcab",
    },
    "intermediate events are fired"
  );

  // one more event for a
  eventEmitter.off("event", listenerB);
  eventEmitter.emit("event", 100);

  // And another unrelated event
  eventEmitter.on("nothing");

  // Spin the event loop to run events
  await Promise.resolve();

  Assert.deepEqual(
    evidence,
    {
      a: 111,
      b: 11,
      c: 1,
      log: "abcaba", // events are in order
    },
    "events fired as expected"
  );

  // Test that mutating the data passed to the event doesn't actually
  // mutate it for other events.
  let handlerRunCount = 0;
  const mutationHandler = data => {
    handlerRunCount++;
    data.count++;
    is(data.count, 1, "Event data is not mutated between handlers.");
  };
  eventEmitter.on("mutationTest", mutationHandler);
  eventEmitter.on("mutationTest", mutationHandler);

  const data = { count: 0 };
  eventEmitter.emit("mutationTest", data);
  await Promise.resolve();

  is(handlerRunCount, 2, "Mutation handler was executed twice.");
  is(data.count, 0, "Event data cannot be mutated by handlers.");
});
