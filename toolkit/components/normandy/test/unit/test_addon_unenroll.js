const { AddonTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/AddonTestUtils.sys.mjs"
);
const { ExtensionTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/ExtensionXPCShellUtils.sys.mjs"
);
const { BranchedAddonStudyAction } = ChromeUtils.importESModule(
  "resource://normandy/actions/BranchedAddonStudyAction.sys.mjs"
);
const { BaseAction } = ChromeUtils.importESModule(
  "resource://normandy/actions/BaseAction.sys.mjs"
);
const { TelemetryEvents } = ChromeUtils.importESModule(
  "resource://normandy/lib/TelemetryEvents.sys.mjs"
);
const { AddonManager } = ChromeUtils.importESModule(
  "resource://gre/modules/AddonManager.sys.mjs"
);
const { AddonStudies } = ChromeUtils.importESModule(
  "resource://normandy/lib/AddonStudies.sys.mjs"
);

/* import-globals-from utils.js */
load("utils.js");

NormandyTestUtils.init({ add_task });
const { decorate_task } = NormandyTestUtils;

const global = this;

add_task(async () => {
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
});

decorate_task(
  withMockApiServer(),
  AddonStudies.withStudies([]),
  async function test_addon_unenroll({ server: apiServer }) {
    const ID = "study@tests.mozilla.org";

    // Create a test extension that uses webextension experiments to install
    // an unenroll listener.
    let xpi = AddonTestUtils.createTempWebExtensionFile({
      manifest: {
        version: "1.0",

        browser_specific_settings: {
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

        "api.js": () => {
          // The code below is serialized into a file embedded in an extension.
          // But by including it here as code, eslint can analyze it.  However,
          // this code runs in a different environment with different globals,
          // the following two lines avoid false eslint warnings:
          /* globals browser, ExtensionAPI */
          /* eslint-disable-next-line no-shadow */
          const { AddonStudies } = ChromeUtils.importESModule(
            "resource://normandy/lib/AddonStudies.sys.mjs"
          );
          const { ExtensionCommon } = ChromeUtils.importESModule(
            "resource://gre/modules/ExtensionCommon.sys.mjs"
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
    let action = new BranchedAddonStudyAction();
    await action.processRecipe(
      {
        id: RECIPE_ID,
        type: "addon-study",
        arguments: {
          slug: "addon-unenroll-test",
          userFacingDescription: "A recipe to test add-on unenrollment",
          userFacingName: "Add-on Unenroll Test",
          isEnrollmentPaused: false,
          branches: [
            {
              ratio: 1,
              slug: "only",
              extensionApiId: API_ID,
            },
          ],
        },
      },
      BaseAction.suitability.FILTER_MATCH
    );

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
  }
);

/* Test that a broken unenroll listener doesn't stop the add-on from being removed */
decorate_task(
  withMockApiServer(),
  AddonStudies.withStudies([]),
  async function test_addon_unenroll({ server: apiServer }) {
    const ID = "study@tests.mozilla.org";

    // Create a dummy webextension
    // an unenroll listener that throws an error.
    let xpi = AddonTestUtils.createTempWebExtensionFile({
      manifest: {
        version: "1.0",

        browser_specific_settings: {
          gecko: { id: ID },
        },
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
            name: "Addon Fixture",
            xpi: "http://example.com/study.xpi",
            extension_id: ID,
            version: "1.0",
            hash: CryptoUtils.getFileHash(xpi, "sha256"),
            hash_algorithm: "sha256",
          })
        );
      }
    );

    // Begin by telling Normandy to install the test extension above that uses a
    // webextension experiment to register a callback when the study ends that
    // throws an error.
    let extension = ExtensionTestUtils.expectExtension(ID);

    const RECIPE_ID = 1;
    const UNENROLL_REASON = "test-ending";
    let action = new BranchedAddonStudyAction();
    await action.processRecipe(
      {
        id: RECIPE_ID,
        type: "addon-study",
        arguments: {
          slug: "addon-unenroll-test",
          userFacingDescription: "A recipe to test add-on unenrollment",
          userFacingName: "Add-on Unenroll Test",
          isEnrollmentPaused: false,
          branches: [
            {
              ratio: 1,
              slug: "only",
              extensionApiId: API_ID,
            },
          ],
        },
      },
      BaseAction.suitability.FILTER_MATCH
    );

    await extension.startupPromise;

    let addon = await AddonManager.getAddonByID(ID);
    ok(addon, "Extension is installed");

    let listenerDeferred = Promise.withResolvers();

    AddonStudies.addUnenrollListener(ID, () => {
      listenerDeferred.resolve();
      throw new Error("This listener is busted");
    });

    // Tell Normandy to end the study, the extension event should be fired.
    await action.unenroll(RECIPE_ID, UNENROLL_REASON);
    await listenerDeferred;

    addon = await AddonManager.getAddonByID(ID);
    equal(
      addon,
      null,
      "Extension is uninstalled even though it threw an exception in the callback"
    );
  }
);
