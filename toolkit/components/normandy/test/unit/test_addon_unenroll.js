
ChromeUtils.import("resource://testing-common/AddonTestUtils.jsm");
ChromeUtils.import("resource://testing-common/ExtensionXPCShellUtils.jsm");
ChromeUtils.import("resource://normandy/actions/AddonStudyAction.jsm");
ChromeUtils.import("resource://normandy/lib/TelemetryEvents.jsm");
ChromeUtils.import("resource://gre/modules/AddonManager.jsm");

const global = this;

add_task(async function test_addon_unenroll() {
  ExtensionTestUtils.init(global);
  AddonTestUtils.init(global);
  AddonTestUtils.createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");
  AddonTestUtils.overrideCertDB();
  await AddonTestUtils.promiseStartupManager();

  TelemetryEvents.init();

  const ID = "study@tests.mozilla.org";

  // Create a test extension that uses webextension experiments to install
  // an unenroll listener.
  let xpi = AddonTestUtils.createTempWebExtensionFile({
    manifest: {
      applications: {
        gecko: { id: ID },
      },

      experiment_apis: {
        study: {
          schema: "schema.json",
          parent: {
            scopes: ["addon_parent"],
            script: "api.js",
            paths: [["study"]],
          },
        }
      },
    },

    files: {
      "schema.json": JSON.stringify([
        {
          namespace: "study",
          events: [
            {
              name: "onStudyEnded",
              type: "function",
            },
          ],
        }
      ]),

      // The code below is serialized into a file embedded in an extension.
      // But by including it here as code, eslint can analyze it.  However,
      // this code runs in a different environment with different globals,
      // the following line avoids false eslint warnings:
      /* globals browser, ExtensionAPI */
      "api.js": () => {
        const {AddonStudies} = ChromeUtils.import("resource://normandy/lib/AddonStudies.jsm", {});
        const {ExtensionCommon} = ChromeUtils.import("resource://gre/modules/ExtensionCommon.jsm", {});
        this.study = class extends ExtensionAPI {
          getAPI(context) {
            return {
              study: {
                onStudyEnded: new ExtensionCommon.EventManager({
                  context,
                  name: "study.onStudyEnded",
                  register: fire => {
                    AddonStudies.addUnenrollListener(this.extension.id,
                                                     () => fire.sync());
                    return () => {};
                  },
                }).api(),
              }
            };
          }
        };
      },
    },

    background() {
      browser.study.onStudyEnded.addListener(() => {
        browser.test.sendMessage("got-event");
        return new Promise(resolve => {
          browser.test.onMessage.addListener(resolve);
        });
      });
    }
  });

  const server = AddonTestUtils.createHttpServer({hosts: ["example.com"]});
  server.registerFile("/study.xpi", xpi);

  // Begin by telling Normandy to install the test extension above
  // that uses a webextension experiment to register a blocking callback
  // to be invoked when the study ends.
  let extension = ExtensionTestUtils.expectExtension(ID);

  const RECIPE_ID = 1;
  let action = new AddonStudyAction();
  await action.runRecipe({
    id: RECIPE_ID,
    type: "addon-study",
    arguments: {
      name: "addon unenroll test",
      description: "testing",
      addonUrl: "http://example.com/study.xpi",
    },
  });

  await extension.awaitStartup();

  let addon = await AddonManager.getAddonByID(ID);
  ok(addon, "Extension is installed");

  // Tell Normandy to end the study, the extension event should be fired.
  let unenrollPromise = action.unenroll(RECIPE_ID);

  await extension.awaitMessage("got-event");
  info("Got onStudyEnded event in extension");

  // The extension has not yet finished its unenrollment tasks, so it
  // should not yet be uninstalled.
  addon = await AddonManager.getAddonByID(ID);
  ok(addon, "Extension has not yet been uninstalled");

  // Once the extension does resolve the promise returned from the
  // event listener, the uninstall can proceed.
  extension.sendMessage("resolve");
  await unenrollPromise;

  addon = await AddonManager.getAddonByID(ID);
  equal(addon, null, "Afer resolving studyEnded promise, extension is uninstalled");
});
