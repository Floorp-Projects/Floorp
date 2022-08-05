"use strict";

const { createAppInfo } = AddonTestUtils;

AddonTestUtils.init(this);

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "49");

const server = createHttpServer();
server.registerDirectory("/data/", do_get_file("data"));

const BASE_URL = `http://localhost:${server.identity.primaryPort}/data`;

add_task(async function test_userscripts_register_cookieStoreId() {
  async function background() {
    const matches = ["<all_urls>"];

    await browser.test.assertRejects(
      browser.userScripts.register({
        js: [{ code: "" }],
        matches,
        cookieStoreId: "not_a_valid_cookieStoreId",
      }),
      /Invalid cookieStoreId/,
      "userScript.register with an invalid cookieStoreId"
    );

    await browser.test.assertRejects(
      browser.userScripts.register({
        js: [{ code: "" }],
        matches,
        cookieStoreId: "",
      }),
      /Invalid cookieStoreId/,
      "userScripts.register with an invalid cookieStoreId"
    );

    let cookieStoreIdJSArray = [
      {
        id: "firefox-container-1",
        code: `document.body.textContent += "1"`,
      },
      {
        id: ["firefox-container-2", "firefox-container-3"],
        code: `document.body.textContent += "2-3"`,
      },
      {
        id: "firefox-private",
        code: `document.body.textContent += "private"`,
      },
      {
        id: "firefox-default",
        code: `document.body.textContent += "default"`,
      },
    ];

    for (let { id, code } of cookieStoreIdJSArray) {
      await browser.userScripts.register({
        js: [{ code }],
        matches,
        runAt: "document_end",
        cookieStoreId: id,
      });
    }

    await browser.contentScripts.register({
      js: [
        {
          code: `browser.test.sendMessage("last-content-script");`,
        },
      ],
      matches,
      runAt: "document_end",
    });

    browser.test.sendMessage("background_ready");
  }

  const extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["<all_urls>"],
      user_scripts: {},
    },
    background,
    incognitoOverride: "spanning",
  });

  await extension.startup();
  await extension.awaitMessage("background_ready");

  registerCleanupFunction(() => extension.unload());

  let testCases = [
    {
      contentPageOptions: { userContextId: 0 },
      expectedTextContent: "default",
    },
    {
      contentPageOptions: { userContextId: 1 },
      expectedTextContent: "1",
    },
    {
      contentPageOptions: { userContextId: 2 },
      expectedTextContent: "2-3",
    },
    {
      contentPageOptions: { userContextId: 3 },
      expectedTextContent: "2-3",
    },
    {
      contentPageOptions: { userContextId: 4 },
      expectedTextContent: "",
    },
    {
      contentPageOptions: { privateBrowsing: true },
      expectedTextContent: "private",
    },
  ];

  for (let test of testCases) {
    let contentPage = await ExtensionTestUtils.loadContentPage(
      `${BASE_URL}/file_sample.html`,
      test.contentPageOptions
    );

    await extension.awaitMessage("last-content-script");

    let result = await contentPage.spawn(null, () => {
      let textContent = this.content.document.body.textContent;
      // Omit the default content from file_sample.html.
      return textContent.replace("\n\nSample text\n\n\n\n", "");
    });

    await contentPage.close();

    equal(
      result,
      test.expectedTextContent,
      `Expected textContent on content page`
    );
  }
});
