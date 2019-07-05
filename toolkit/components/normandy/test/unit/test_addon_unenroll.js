const { AddonTestUtils } = ChromeUtils.import(
  "resource://testing-common/AddonTestUtils.jsm"
);
const { ExtensionTestUtils } = ChromeUtils.import(
  "resource://testing-common/ExtensionXPCShellUtils.jsm"
);
const { AddonStudyAction } = ChromeUtils.import(
  "resource://normandy/actions/AddonStudyAction.jsm"
);
const { TelemetryEvents } = ChromeUtils.import(
  "resource://normandy/lib/TelemetryEvents.jsm"
);
const { AddonManager } = ChromeUtils.import(
  "resource://gre/modules/AddonManager.jsm"
);

const global = this;

load("utils.js"); /* globals withMockApiServer, CryptoUtils */

add_task(
  withMockApiServer(async function test_addon_unenroll(...args) {
    const apiServer = args[args.length - 1];

    ExtensionTestUtils.init(global);
    AddonTestUtils.init(global);
    AddonTestUtils.createAppInfo(
      "xpcshell@tests.mozilla.org",
      "XPCShell",
      "1",
      "1.9.2"
    );
    AddonTestUtils.overrideCertDB();
    await AddonTestUtils.promiseStartupManager();

    TelemetryEvents.init();

    const ID = "study@tests.mozilla.org";

    // Create a test extension that uses webextension experiments to install
    // an unenroll listener.
    let xpi = AddonTestUtils.createTempWebExtensionFile({
      manifest: {
        version: "1.0",

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
          },
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
          },
        ]),

        // The code below is serialized into a file embedded in an extension.
        // But by including it here as code, eslint can analyze it.  However,
        // this code runs in a different environment with different globals,
        // the following line avoids false eslint warnings:
        /* globals browser, ExtensionAPI */
        "api.js": () => {
          const { AddonStudies } = ChromeUtils.import(
            "resource://normandy/lib/AddonStudies.jsm"
          );
          const { ExtensionCommon } = ChromeUtils.import(
            "resource://gre/modules/ExtensionCommon.jsm"
          );
          this.study = class extends ExtensionAPI {
            getAPI(context) {
              return {
                study: {
                  onStudyEnded: new ExtensionCommon.EventManager({
                    context,
                    name: "study.onStudyEnded",
                    register: fire => {
                      AddonStudies.addUnenrollListener(
                        this.extension.id,
                        reason => fire.sync(reason)
                      );
                      return () => {};
                    },
                  }).api(),
                },
              };
            }
          };
        },
      },

      background() {
        browser.study.onStudyEnded.addListener(reason => {
          browser.test.sendMessage("got-event", reason);
          return new Promise(resolve => {
            browser.test.onMessage.addListener(resolve);
          });
        });
      },
    });

    const server = AddonTestUtils.createHttpServer({ hosts: ["example.com"] });
    server.registerFile("/study.xpi", xpi);

    const API_ID = 999;
    apiServer.registerPathHandler(
      `/api/v1/extension/${API_ID}/`,
      (request, response) => {
        response.setStatusLine(request.httpVersion, 200, "OK");
        response.write(
          JSON.stringify({
            id: API_ID,
            name: "Addon Unenroll Fixture",
            xpi: "http://example.com/study.xpi",
            extension_id: ID,
            version: "1.0",
            hash: CryptoUtils.getFileHash(xpi, "sha256"),
            hash_algorithm: "sha256",
          })
        );
      }
    );

    // Begin by telling Normandy to install the test extension above
    // that uses a webextension experiment to register a blocking callback
    // to be invoked when the study ends.
    let extension = ExtensionTestUtils.expectExtension(ID);

    const RECIPE_ID = 1;
    const UNENROLL_REASON = "test-ending";
    let action = new AddonStudyAction();
    await action.runRecipe({
      id: RECIPE_ID,
      type: "addon-study",
      arguments: {
        name: "addon unenroll test",
        description: "testing",
        addonUrl: "http://example.com/study.xpi",
        extensionApiId: API_ID,
      },
    });

    await extension.awaitStartup();

    let addon = await AddonManager.getAddonByID(ID);
    ok(addon, "Extension is installed");

    // Tell Normandy to end the study, the extension event should be fired.
    let unenrollPromise = action.unenroll(RECIPE_ID, UNENROLL_REASON);

    let receivedReason = await extension.awaitMessage("got-event");
    info("Got onStudyEnded event in extension");
    equal(receivedReason, UNENROLL_REASON, "Unenroll reason should be passed");

    // The extension has not yet finished its unenrollment tasks, so it
    // should not yet be uninstalled.
    addon = await AddonManager.getAddonByID(ID);
    ok(addon, "Extension has not yet been uninstalled");

    // Once the extension does resolve the promise returned from the
    // event listener, the uninstall can proceed.
    extension.sendMessage("resolve");
    await unenrollPromise;

    addon = await AddonManager.getAddonByID(ID);
    equal(
      addon,
      null,
      "After resolving studyEnded promise, extension is uninstalled"
    );
  })
);
