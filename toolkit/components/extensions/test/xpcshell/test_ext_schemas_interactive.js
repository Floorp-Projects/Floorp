"use strict";

const {ExtensionManager} = ChromeUtils.import("resource://gre/modules/ExtensionChild.jsm", {});

Cu.importGlobalProperties(["Blob", "URL"]);

let experimentAPIs = {
  userinputtest: {
    schema: "schema.json",
    parent: {
      scopes: ["addon_parent"],
      script: "parent.js",
      paths: [["userinputtest"]],
    },
    child: {
      scopes: ["addon_child"],
      script: "child.js",
      paths: [["userinputtest", "child"]],
    },
  },
};

let experimentFiles = {
  "schema.json": JSON.stringify([
    {
      namespace: "userinputtest",
      functions: [
        {
          name: "test",
          type: "function",
          async: true,
          requireUserInput: true,
          parameters: [],
        },
        {
          name: "child",
          type: "function",
          async: true,
          requireUserInput: true,
          parameters: [],
        },
      ],
    },
  ]),

  /* globals ExtensionAPI */
  "parent.js": () => {
    this.userinputtest = class extends ExtensionAPI {
      getAPI(context) {
        return {
          userinputtest: {
            test() {},
          },
        };
      }
    };
  },

  /* globals ExtensionAPI */
  "child.js": () => {
    this.userinputtest = class extends ExtensionAPI {
      getAPI(context) {
        return {
          userinputtest: {
            child() {},
          },
        };
      }
    };
  },
};

// Set the "handlingUserInput" flag for the given extension's background page.
// Returns an RAIIHelper that should be destruct()ed eventually.
function setHandlingUserInput(extension) {
  let extensionChild = ExtensionManager.extensions.get(extension.extension.id);
  let bgwin = null;
  for (let view of extensionChild.views) {
    if (view.viewType == "background") {
      bgwin = view.contentWindow;
      break;
    }
  }
  notEqual(bgwin, null, "Found background window for the test extension");
  let winutils = bgwin.QueryInterface(Ci.nsIInterfaceRequestor)
                      .getInterface(Ci.nsIDOMWindowUtils);
  return winutils.setHandlingUserInput(true);
}

// Test that the schema requireUserInput flag works correctly for
// proxied api implementations.
add_task(async function test_proxy() {
  let extension = ExtensionTestUtils.loadExtension({
    isPrivileged: true,
    background() {
      browser.test.onMessage.addListener(async () => {
        try {
          await browser.userinputtest.test();
          browser.test.sendMessage("result", null);
        } catch (err) {
          browser.test.sendMessage("result", err.message);
        }
      });
    },
    manifest: {
      permissions: ["experiments.userinputtest"],
      experiment_apis: experimentAPIs,
    },
    files: experimentFiles,
  });

  await extension.startup();

  extension.sendMessage("test");
  let result = await extension.awaitMessage("result");
  ok(/test may only be called from a user input handler/.test(result),
     `function failed when not called from a user input handler: ${result}`);

  let handle = setHandlingUserInput(extension);
  extension.sendMessage("test");
  result = await extension.awaitMessage("result");
  equal(result, null, "function succeeded when called from a user input handler");
  handle.destruct();

  await extension.unload();
});

// Test that the schema requireUserInput flag works correctly for
// non-proxied api implementations.
add_task(async function test_local() {
  let extension = ExtensionTestUtils.loadExtension({
    isPrivileged: true,
    background() {
      browser.test.onMessage.addListener(async () => {
        try {
          await browser.userinputtest.child();
          browser.test.sendMessage("result", null);
        } catch (err) {
          browser.test.sendMessage("result", err.message);
        }
      });
    },
    manifest: {
      experiment_apis: experimentAPIs,
    },
    files: experimentFiles,
  });

  await extension.startup();

  extension.sendMessage("test");
  let result = await extension.awaitMessage("result");
  ok(/child may only be called from a user input handler/.test(result),
     `function failed when not called from a user input handler: ${result}`);

  let handle = setHandlingUserInput(extension);
  extension.sendMessage("test");
  result = await extension.awaitMessage("result");
  equal(result, null, "function succeeded when called from a user input handler");
  handle.destruct();

  await extension.unload();
});
