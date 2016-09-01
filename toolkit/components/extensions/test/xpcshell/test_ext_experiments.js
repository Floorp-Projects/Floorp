"use strict";

/* globals browser */

XPCOMUtils.defineLazyModuleGetter(this, "AddonManager",
                                  "resource://gre/modules/AddonManager.jsm");

function promiseAddonStartup() {
  const {Management} = Cu.import("resource://gre/modules/Extension.jsm");

  return new Promise(resolve => {
    let listener = (evt, extension) => {
      Management.off("startup", listener);
      resolve(extension);
    };

    Management.on("startup", listener);
  });
}

add_task(function* setup() {
  yield ExtensionTestUtils.startAddonManager();
});

add_task(function* test_experiments_api() {
  let apiAddonFile = Extension.generateZipFile({
    "install.rdf": `<?xml version="1.0" encoding="UTF-8"?>
      <RDF xmlns="http://www.w3.org/1999/02/22-rdf-syntax-ns#"
           xmlns:em="http://www.mozilla.org/2004/em-rdf#">
          <Description about="urn:mozilla:install-manifest"
              em:id="meh@experiments.addons.mozilla.org"
              em:name="Meh Experiment"
              em:type="256"
              em:version="0.1"
              em:description="Meh experiment"
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
            meh: {
              hello(text) {
                Services.obs.notifyObservers(null, "webext-api-hello", text);
              }
            }
          }
        }
      }
    `,

    "schema.json": [
      {
        "namespace": "meh",
        "description": "All full of meh.",
        "permissions": ["experiments.meh"],
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
      applications: {gecko: {id: "meh@web.extension"}},
      permissions: ["experiments.meh"],
    },

    background() {
      // The test code below checks that hello() is called at the right
      // time with the string "Here I am".  Verify that the api schema is
      // being correctly interpreted by calling hello() with bad arguments
      // and only calling hello() with the magic string if the call with
      // bad arguments throws.
      try {
        browser.meh.hello("I should not see this", "since two arguments are bad");
      } catch (err) {
        browser.meh.hello("Here I am");
      }
    },
  });

  let boringAddonFile = Extension.generateXPI({
    manifest: {
      applications: {gecko: {id: "boring@web.extension"}},
    },
    background() {
      if (browser.meh) {
        browser.meh.hello("Here I should not be");
      }
    },
  });

  do_register_cleanup(() => {
    for (let file of [apiAddonFile, addonFile, boringAddonFile]) {
      Services.obs.notifyObservers(file, "flush-cache-entry", null);
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

  Services.obs.addObserver(observer, "webext-api-loaded", false);
  Services.obs.addObserver(observer, "webext-api-hello", false);
  do_register_cleanup(() => {
    Services.obs.removeObserver(observer, "webext-api-loaded");
    Services.obs.removeObserver(observer, "webext-api-hello");
  });


  // Install API add-on.
  let apiAddon = yield AddonManager.installTemporaryAddon(apiAddonFile);

  let {APIs} = Cu.import("resource://gre/modules/ExtensionManagement.jsm", {});
  ok(APIs.apis.has("meh"), "Should have meh API.");


  // Install boring WebExtension add-on.
  let boringAddon = yield AddonManager.installTemporaryAddon(boringAddonFile);
  yield promiseAddonStartup();


  // Install interesting WebExtension add-on.
  let promise = new Promise(resolve => {
    resolveHello = resolve;
  });

  let addon = yield AddonManager.installTemporaryAddon(addonFile);
  yield promiseAddonStartup();

  let hello = yield promise;
  equal(hello, "Here I am", "Should get hello from add-on");

  // Cleanup.
  apiAddon.uninstall();

  boringAddon.userDisabled = true;
  yield new Promise(do_execute_soon);

  equal(addon.appDisabled, true, "Add-on should be app-disabled after its dependency is removed.");

  addon.uninstall();
  boringAddon.uninstall();
});

