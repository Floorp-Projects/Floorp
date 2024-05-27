/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const { AddonTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/AddonTestUtils.sys.mjs"
);

const TESTPAGE = `${SECURE_TESTROOT}webapi_checkavailable.html`;
const ADDON_ID = "@test-extension-to-report";

AddonTestUtils.initMochitest(this);

const server = AddonTestUtils.createHttpServer({ hosts: ["test.addons.org"] });
let apiRequestHandler;
server.registerPathHandler("/api/abuse/report/addon/", (request, response) => {
  apiRequestHandler(request, response);
});

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["extensions.webapi.testing", true],
      [
        "extensions.addonAbuseReport.url",
        // eslint-disable-next-line @microsoft/sdl/no-insecure-url
        "http://test.addons.org/api/abuse/report/addon/",
      ],
    ],
  });
});

add_task(async function test_mozAddonManager_sendAbuseReport() {
  apiRequestHandler = (req, res) => {
    res.setStatusLine(req.httpVersion, 200, "OK");
    res.setHeader("Content-Type", "application/json", false);
    res.write('{"ok":true}');
  };

  await BrowserTestUtils.withNewTab(TESTPAGE, async browser => {
    const extension = ExtensionTestUtils.loadExtension({
      manifest: {
        name: "some extension reported",
        browser_specific_settings: { gecko: { id: ADDON_ID } },
      },
      useAddonManager: "temporary",
    });
    await extension.startup();

    const response = await SpecialPowers.spawn(browser, [ADDON_ID], addonId => {
      const data = { some: "data" };
      return content.navigator.mozAddonManager.sendAbuseReport(addonId, data);
    });
    Assert.deepEqual(
      response,
      { ok: true },
      "expected API response to be returned"
    );

    await extension.unload();
  });
});

add_task(async function test_mozAddonManager_sendAbuseReport_error() {
  apiRequestHandler = (req, res) => {
    res.setStatusLine(req.httpVersion, 400, "BAD REQUEST");
  };

  await BrowserTestUtils.withNewTab(TESTPAGE, async browser => {
    const extension = ExtensionTestUtils.loadExtension({
      manifest: {
        name: "some extension reported",
        browser_specific_settings: { gecko: { id: ADDON_ID } },
      },
      useAddonManager: "temporary",
    });
    await extension.startup();

    const webApiResult = await SpecialPowers.spawn(
      browser,
      [ADDON_ID],
      addonId => {
        const data = { some: "data" };
        return content.navigator.mozAddonManager
          .sendAbuseReport(addonId, data)
          .then(
            res => ({ gotRejection: false, result: res }),
            err => ({
              gotRejection: true,
              message: err.message,
              errorName: err.name,
              isErrorInstance: err instanceof content.Error,
            })
          );
      }
    );
    Assert.deepEqual(
      webApiResult,
      {
        gotRejection: true,
        message: 'ERROR_CLIENT - {"status":400,"responseText":""}',
        errorName: "Error",
        isErrorInstance: true,
      },
      "expected rejection"
    );

    await extension.unload();
  });
});
