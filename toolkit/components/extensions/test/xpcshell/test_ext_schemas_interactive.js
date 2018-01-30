"use strict";

Components.utils.import("resource://gre/modules/ExtensionCommon.jsm");

const {ExtensionAPI, ExtensionAPIs} = ExtensionCommon;

const {ExtensionManager} = Components.utils.import("resource://gre/modules/ExtensionChild.jsm", {});

Components.utils.importGlobalProperties(["Blob", "URL"]);

let schema = [
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
    ],
  },
];

class API extends ExtensionAPI {
  getAPI(context) {
    return {
      userinputtest: {
        test() {},
      },
    };
  }
}

let schemaUrl = `data:,${JSON.stringify(schema)}`;

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
  let apiUrl = URL.createObjectURL(new Blob([API.toString()]));
  ExtensionAPIs.register("userinputtest", schemaUrl, apiUrl);

  let extension = ExtensionTestUtils.loadExtension({
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
    },
  });

  await extension.startup();

  extension.sendMessage("test");
  let result = await extension.awaitMessage("result");
  ok(/test may only be called from a user input handler/.test(result),
     "function failed when not called from a user input handler");

  let handle = setHandlingUserInput(extension);
  extension.sendMessage("test");
  result = await extension.awaitMessage("result");
  equal(result, null, "function succeeded when called from a user input handler");
  handle.destruct();

  await extension.unload();
  ExtensionAPIs.unregister("userinputtest");
});

// Test that the schema requireUserInput flag works correctly for
// non-proxied api implementations.
add_task(async function test_local() {
  let apiString = `this.userinputtest = ${API.toString()};`;
  let apiUrl = URL.createObjectURL(new Blob([apiString]));
  await Schemas.load(schemaUrl);
  const {apiManager} = Components.utils.import("resource://gre/modules/ExtensionPageChild.jsm", {});
  apiManager.registerModules({
    userinputtest: {
      url: apiUrl,
      scopes: ["addon_child"],
      paths: [["userinputtest"]],
    },
  });

  let extension = ExtensionTestUtils.loadExtension({
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
    manifest: {},
  });

  await extension.startup();

  extension.sendMessage("test");
  let result = await extension.awaitMessage("result");
  ok(/test may only be called from a user input handler/.test(result),
     "function failed when not called from a user input handler");

  let handle = setHandlingUserInput(extension);
  extension.sendMessage("test");
  result = await extension.awaitMessage("result");
  equal(result, null, "function succeeded when called from a user input handler");
  handle.destruct();

  await extension.unload();
});
