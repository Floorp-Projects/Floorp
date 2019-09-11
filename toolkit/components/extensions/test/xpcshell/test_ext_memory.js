"use strict";

AddonTestUtils.init(this);

add_task(async function test_memory_getInfo() {
  // Background script used for the test extension.
  //
  // Adds a listener that proxies commands, for instance you can send:
  //    'getInfo.request'
  // and that will run the `memory.getInfo` command and responde with a
  //    'getInfo.done'
  // message including an error message or the results.
  //
  // Broadcasts a 'ready' message when loaded.
  function background() {
    const onLowMem = data => {
      browser.test.sendMessage("onLowMemory", { data });
    };

    browser.test.onMessage.addListener(async (msg, args) => {
      let match = msg.match(/^(\w+)\.request$/);
      if (!match) {
        return;
      }
      let cmd = match[1];

      if (cmd === "registerLowMem") {
        browser.memory.onLowMemory.addListener(onLowMem);
        browser.test.sendMessage(`${cmd}.done`, {});
        return;
      } else if (cmd === "unregisterLowMem") {
        browser.memory.onLowMemory.removeListener(onLowMem);
        browser.test.sendMessage(`${cmd}.done`, {});
        return;
      }

      try {
        let results = await browser.memory[cmd](...args);
        browser.test.sendMessage(`${cmd}.done`, { results });
      } catch (e) {
        browser.test.sendMessage(`${cmd}.done`, { errmsg: e.message });
      }
    });
  }

  function run(ext, cmd, ...args) {
    let promise = ext.awaitMessage(`${cmd}.done`);
    ext.sendMessage(`${cmd}.request`, args);
    return promise;
  }

  // Create a temporary extension that uses the memory api.
  let privilegedExtension = ExtensionTestUtils.loadExtension({
    background,
    manifest: {
      permissions: ["memory"],
    },
    isPrivileged: true,
  });

  await privilegedExtension.startup();

  // Now we can actually invoke the extension api.
  let response = await run(privilegedExtension, "getInfo");

  // We expect to get back an object similar to:
  // { "results": { "Parent": { "uss": 123, "rss": 123 } } }
  // Since this is just a test we don't expect child processes.
  let results = response.results;
  ok(
    results.hasOwnProperty("Parent"),
    "Should have memory object for the parent process."
  );
  ok(results.Parent.uss > 0, "uss value must be nonzero.");
  ok(results.Parent.rss > 0, "rss value must be nonzero.");

  response = await run(privilegedExtension, "registerLowMem");
  equal(
    Object.keys(response).length,
    0,
    "Low memory listeners should register."
  );

  // Trigger a low memory event
  let p = privilegedExtension.awaitMessage("onLowMemory");
  Services.obs.notifyObservers(null, "memory-pressure", "heap-minimize");
  response = await p;
  ok(response.hasOwnProperty("data"), "Response data should exist.");
  equal(
    response.data,
    "heap-minimize",
    "Heap-minimize low memory event should trigger."
  );

  response = await run(privilegedExtension, "unregisterLowMem");
  equal(
    Object.keys(response).length,
    0,
    "Low memory listeners should unregister."
  );

  // Try minimizing memory.
  response = await run(privilegedExtension, "minimizeMemoryUsage");

  await privilegedExtension.unload();
});

add_task(async function test_memory_permission() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["memory"],
    },
    background() {
      browser.test.assertEq(browser.memory, undefined, "memory is privileged");
    },
  });
  await extension.startup();
  await extension.unload();
});
