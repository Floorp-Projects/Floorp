"use strict";

/* globals browser */

XPCOMUtils.defineLazyModuleGetter(this, "AddonManager",
                                  "resource://gre/modules/AddonManager.jsm");

AddonTestUtils.init(this);

add_task(async function setup() {
  AddonTestUtils.overrideCertDB();
  await ExtensionTestUtils.startAddonManager();
});

add_task(async function test_experiments_api() {
  let apiAddonFile = Extension.generateZipFile({
    "install.rdf": `<?xml version="1.0" encoding="UTF-8"?>
      <RDF xmlns="http://www.w3.org/1999/02/22-rdf-syntax-ns#"
           xmlns:em="http://www.mozilla.org/2004/em-rdf#">
          <Description about="urn:mozilla:install-manifest"
              em:id="fooBar@experiments.addons.mozilla.org"
              em:name="FooBar Experiment"
              em:type="256"
              em:version="0.1"
              em:description="FooBar experiment"
              em:creator="Mozilla">

              <em:targetApplication>
                  <Description
                      em:id="xpcshell@tests.mozilla.org"
                      em:minVersion="48"
                      em:maxVersion="*"/>
              </em:targetApplication>
          </Description>
      </RDF>
    `,

    "api.js": String.raw`
      Components.utils.import("resource://gre/modules/Services.jsm");

      Services.obs.notifyObservers(null, "webext-api-loaded", "");

      class API extends ExtensionAPI {
        getAPI(context) {
          return {
            fooBar: {
              hello(text) {
                console.log('fooBar.hello API called', text);
                Services.obs.notifyObservers(null, "webext-api-hello", text);
              }
            }
          }
        }
      }
    `,

    "schema.json": [
      {
        "namespace": "fooBar",
        "description": "All full of fooBar.",
        "permissions": ["experiments.fooBar"],
        "functions": [
          {
            "name": "hello",
            "type": "function",
            "description": "Hates you. This is all.",
            "parameters": [
              {"type": "string", "name": "text"},
            ],
          },
        ],
      },
    ],
  });

  let addonFile = Extension.generateXPI({
    manifest: {
      applications: {gecko: {id: "fooBar@web.extension"}},
      permissions: ["experiments.fooBar"],
    },

    background() {
      // The test code below checks that hello() is called at the right
      // time with the string "Here I am".  Verify that the api schema is
      // being correctly interpreted by calling hello() with bad arguments
      // and only calling hello() with the magic string if the call with
      // bad arguments throws.
      try {
        browser.fooBar.hello("I should not see this", "since two arguments are bad");
      } catch (err) {
        browser.fooBar.hello("Here I am");
      }
    },
  });

  let boringAddonFile = Extension.generateXPI({
    manifest: {
      applications: {gecko: {id: "boring@web.extension"}},
    },
    background() {
      if (browser.fooBar) {
        browser.fooBar.hello("Here I should not be");
      }
    },
  });

  registerCleanupFunction(() => {
    for (let file of [apiAddonFile, addonFile, boringAddonFile]) {
      Services.obs.notifyObservers(file, "flush-cache-entry");
      file.remove(false);
    }
  });


  let resolveHello;
  let observer = (subject, topic, data) => {
    if (topic == "webext-api-loaded") {
      ok(!!resolveHello, "Should not see API loaded until dependent extension loads");
    } else if (topic == "webext-api-hello") {
      resolveHello(data);
    }
  };

  Services.obs.addObserver(observer, "webext-api-loaded");
  Services.obs.addObserver(observer, "webext-api-hello");
  registerCleanupFunction(() => {
    Services.obs.removeObserver(observer, "webext-api-loaded");
    Services.obs.removeObserver(observer, "webext-api-hello");
  });


  // Install API add-on.
  let apiAddon = await AddonManager.installTemporaryAddon(apiAddonFile);

  let {ExtensionAPIs} = Cu.import("resource://gre/modules/ExtensionCommon.jsm", {}).ExtensionCommon;
  ok(ExtensionAPIs.apis.has("fooBar"), "Should have fooBar API.");


  // Install boring WebExtension add-on.
  let boringAddon = await AddonManager.installTemporaryAddon(boringAddonFile);
  await AddonTestUtils.promiseWebExtensionStartup();

  // Install interesting WebExtension add-on.
  let promise = new Promise(resolve => {
    resolveHello = resolve;
  });

  let addon = await AddonManager.installTemporaryAddon(addonFile);
  await AddonTestUtils.promiseWebExtensionStartup();

  let hello = await promise;
  equal(hello, "Here I am", "Should get hello from add-on");

  // Install management test add-on.
  let managementAddon = ExtensionTestUtils.loadExtension({
    manifest: {
      applications: {gecko: {id: "management@web.extension"}},
      permissions: ["management"],
    },
    async background() {
      // Should find the simple extension.
      let normalAddon = await browser.management.get("boring@web.extension");
      browser.test.assertEq(normalAddon.id, "boring@web.extension", "Found boring addon");

      try {
        // Not allowed to get the API experiment.
        await browser.management.get("fooBar@experiments.addons.mozilla.org");
      } catch (e) {
        browser.test.sendMessage("done");
      }
    },
    useAddonManager: "temporary",
  });

  await managementAddon.startup();
  await managementAddon.awaitMessage("done");
  await managementAddon.unload();

  // Cleanup.
  apiAddon.uninstall();

  boringAddon.userDisabled = true;
  await new Promise(executeSoon);

  equal(addon.appDisabled, true, "Add-on should be app-disabled after its dependency is removed.");

  addon.uninstall();
  boringAddon.uninstall();
});

add_task(async function test_bundled_experiments() {
  let extension = ExtensionTestUtils.loadExtension({
    isPrivileged: true,

    manifest: {
      experiment_apis: {
        foo: {
          schema: "schema.json",
          parent: {
            scopes: ["addon_parent"],
            script: "parent.js",
            paths: [["experiments", "foo", "parent"]],
          },
          child: {
            scopes: ["addon_child"],
            script: "child.js",
            paths: [["experiments", "foo", "child"]],
          },
        },
      },
    },

    async background() {
      browser.test.assertEq("object", typeof browser.experiments,
                            "typeof browser.experiments");

      browser.test.assertEq("object", typeof browser.experiments.foo,
                            "typeof browser.experiments.foo");

      browser.test.assertEq("function", typeof browser.experiments.foo.child,
                            "typeof browser.experiments.foo.child");

      browser.test.assertEq("function", typeof browser.experiments.foo.parent,
                            "typeof browser.experiments.foo.parent");

      browser.test.assertEq("child", browser.experiments.foo.child(),
                            "foo.child()");

      browser.test.assertEq("parent", await browser.experiments.foo.parent(),
                            "await foo.parent()");

      browser.test.notifyPass("background.experiments.foo");
    },

    files: {
      "schema.json": JSON.stringify([
        {
          "namespace": "experiments.foo",
          "types": [
            {
              "id": "Meh",
              "type": "object",
              "properties": {},
            },
          ],
          "functions": [
            {
              "name": "parent",
              "type": "function",
              "async": true,
              "parameters": [],
            },
            {
              "name": "child",
              "type": "function",
              "parameters": [],
              "returns": {"type": "string"},
            },
          ],
        },
      ]),

      /* globals ExtensionAPI */
      "parent.js": () => {
        this.foo = class extends ExtensionAPI {
          getAPI(context) {
            return {
              experiments: {
                foo: {
                  parent() {
                    return Promise.resolve("parent");
                  },
                },
              },
            };
          }
        };
      },

      "child.js": () => {
        this.foo = class extends ExtensionAPI {
          getAPI(context) {
            return {
              experiments: {
                foo: {
                  child() {
                    return "child";
                  },
                },
              },
            };
          }
        };
      },
    },
  });

  await extension.startup();

  await extension.awaitFinish("background.experiments.foo");

  await extension.unload();
});
