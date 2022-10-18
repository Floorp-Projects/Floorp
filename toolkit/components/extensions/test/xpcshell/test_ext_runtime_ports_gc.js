"use strict";

let gcExperimentAPIs = {
  gcHelper: {
    schema: "schema.json",
    child: {
      scopes: ["addon_child"],
      script: "child.js",
      paths: [["gcHelper"]],
    },
  },
};

let gcExperimentFiles = {
  "schema.json": JSON.stringify([
    {
      namespace: "gcHelper",
      functions: [
        {
          name: "forceGarbageCollect",
          type: "function",
          parameters: [],
          async: true,
        },
        {
          name: "registerWitness",
          type: "function",
          parameters: [
            {
              name: "obj",
              // Expected type is "object", but using "any" here to ensure that
              // the parameter is untouched (not normalized).
              type: "any",
            },
          ],
          returns: { type: "number" },
        },
        {
          name: "isGarbageCollected",
          type: "function",
          parameters: [
            {
              name: "witnessId",
              description: "return value of registerWitness",
              type: "number",
            },
          ],
          returns: { type: "boolean" },
        },
      ],
    },
  ]),
  "child.js": () => {
    let { setTimeout } = ChromeUtils.importESModule(
      "resource://gre/modules/Timer.sys.mjs"
    );
    /* globals ExtensionAPI */
    this.gcHelper = class extends ExtensionAPI {
      getAPI(context) {
        let witnesses = new Map();
        return {
          gcHelper: {
            async forceGarbageCollect() {
              // Logic copied from test_ext_contexts_gc.js
              for (let i = 0; i < 3; ++i) {
                Cu.forceShrinkingGC();
                Cu.forceCC();
                Cu.forceGC();
                await new Promise(resolve => setTimeout(resolve, 0));
              }
            },
            registerWitness(obj) {
              let witnessId = witnesses.size;
              witnesses.set(witnessId, Cu.getWeakReference(obj));
              return witnessId;
            },
            isGarbageCollected(witnessId) {
              return witnesses.get(witnessId).get() === null;
            },
          },
        };
      }
    };
  },
};

// Verify that the experiment is working as intended before using it in tests.
add_task(async function test_gc_experiment() {
  let extension = ExtensionTestUtils.loadExtension({
    isPrivileged: true,
    manifest: {
      experiment_apis: gcExperimentAPIs,
    },
    files: gcExperimentFiles,
    async background() {
      let obj1 = {};
      let obj2 = {};
      let witness1 = browser.gcHelper.registerWitness(obj1);
      let witness2 = browser.gcHelper.registerWitness(obj2);
      obj1 = null;
      await browser.gcHelper.forceGarbageCollect();
      browser.test.assertTrue(
        browser.gcHelper.isGarbageCollected(witness1),
        "obj1 should have been garbage-collected"
      );
      browser.test.assertFalse(
        browser.gcHelper.isGarbageCollected(witness2),
        "obj2 should not have been garbage-collected"
      );

      browser.test.sendMessage("done");
    },
  });

  await extension.startup();
  await extension.awaitMessage("done");
  await extension.unload();
});

add_task(async function test_port_gc() {
  let extension = ExtensionTestUtils.loadExtension({
    isPrivileged: true,
    manifest: {
      experiment_apis: gcExperimentAPIs,
    },
    files: gcExperimentFiles,
    async background() {
      let witnessPortSender;
      let witnessPortReceiver;

      browser.runtime.onConnect.addListener(port => {
        browser.test.assertEq("daName", port.name, "expected port");
        witnessPortReceiver = browser.gcHelper.registerWitness(port);
        port.disconnect();
      });

      // runtime.connect() only triggers onConnect for different contexts,
      // so create a frame to have a different context.
      // A blank frame in a moz-extension:-document will have access to the
      // extension APIs.
      let frameWindow = await new Promise(resolve => {
        let f = document.createElement("iframe");
        f.onload = () => resolve(f.contentWindow);
        document.body.append(f);
      });
      await new Promise(resolve => {
        let port = frameWindow.browser.runtime.connect({ name: "daName" });
        witnessPortSender = browser.gcHelper.registerWitness(port);
        port.onDisconnect.addListener(() => resolve());
      });

      await browser.gcHelper.forceGarbageCollect();

      browser.test.assertTrue(
        browser.gcHelper.isGarbageCollected(witnessPortSender),
        "runtime.connect() port should have been garbage-collected"
      );
      browser.test.assertTrue(
        browser.gcHelper.isGarbageCollected(witnessPortReceiver),
        "runtime.onConnect port should have been garbage-collected"
      );

      browser.test.sendMessage("done");
    },
  });

  await extension.startup();
  await extension.awaitMessage("done");
  await extension.unload();
});
