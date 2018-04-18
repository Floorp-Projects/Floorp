"use strict";

ChromeUtils.import("resource://normandy/lib/SandboxManager.jsm");

// wrapAsync should wrap privileged Promises with Promises that are usable by
// the sandbox.
add_task(async function() {
  const manager = new SandboxManager();
  manager.addHold("testing");

  manager.cloneIntoGlobal("driver", {
    async privileged() {
      return "privileged";
    },
    wrapped: manager.wrapAsync(async function() {
      return "wrapped";
    }),
    aValue: "aValue",
    wrappedThis: manager.wrapAsync(async function() {
      return this.aValue;
    }),
  }, {cloneFunctions: true});

  // Assertion helpers
  manager.addGlobal("ok", ok);
  manager.addGlobal("equal", equal);

  const sandboxResult = await new Promise(resolve => {
    manager.addGlobal("resolve", result => resolve(result));
    manager.evalInSandbox(`
      // Unwrapped privileged promises are not accessible in the sandbox
      try {
        const privilegedResult = driver.privileged().then(() => false);
        ok(false, "The sandbox could not use a privileged Promise");
      } catch (err) { }

      // Wrapped functions return promises that the sandbox can access.
      const wrappedResult = driver.wrapped();
      ok("then" in wrappedResult);

      // Resolve the Promise around the sandbox with the wrapped result to test
      // that the Promise in the sandbox works.
      wrappedResult.then(resolve);
    `);
  });
  equal(sandboxResult, "wrapped", "wrapAsync methods return Promises that work in the sandbox");

  await manager.evalInSandbox(`
    (async function sandboxTest() {
      equal(
        await driver.wrappedThis(),
        "aValue",
        "wrapAsync preserves the behavior of the this keyword",
      );
    })();
  `);

  manager.removeHold("testing");
});

// wrapAsync cloning options
add_task(async function() {
  const manager = new SandboxManager();
  manager.addHold("testing");

  // clonedArgument stores the argument passed to cloned(), which we use to test
  // that arguments from within the sandbox are cloned outside.
  let clonedArgument = null;
  manager.cloneIntoGlobal("driver", {
    uncloned: manager.wrapAsync(async function() {
      return {value: "uncloned"};
    }),
    cloned: manager.wrapAsync(async function(argument) {
      clonedArgument = argument;
      return {value: "cloned"};
    }, {cloneInto: true, cloneArguments: true}),
  }, {cloneFunctions: true});

  // Assertion helpers
  manager.addGlobal("ok", ok);
  manager.addGlobal("deepEqual", deepEqual);

  await new Promise(resolve => {
    manager.addGlobal("resolve", resolve);
    manager.evalInSandbox(`
      (async function() {
        // The uncloned return value should be privileged and inaccesible.
        const uncloned = await driver.uncloned();
        ok(!("value" in uncloned), "The sandbox could not use an uncloned return value");

        // The cloned return value should be usable.
        deepEqual(
          await driver.cloned({value: "insidesandbox"}),
          {value: "cloned"},
          "The sandbox could use the cloned return value",
        );
      })().then(resolve);
    `);
  });

  // Removing the hold nukes the sandbox. Afterwards, because cloned() has the
  // cloneArguments option, the clonedArgument variable should still be
  // accessible.
  manager.removeHold("testing");
  deepEqual(
    clonedArgument,
    {value: "insidesandbox"},
    "cloneArguments allowed an argument from within the sandbox to persist after it was nuked",
  );
});
