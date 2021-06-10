"use strict";
const { OS } = ChromeUtils.import("resource://gre/modules/osfile.jsm");

const { PromptTestUtils } = ChromeUtils.import(
  "resource://testing-common/PromptTestUtils.jsm"
);

async function createTestFile(filename, content) {
  let path = OS.Path.join(OS.Constants.Path.tmpDir, filename);
  await OS.File.writeAtomic(path, content);
  return path;
}

async function readFile(path) {
  var array = await OS.File.read(path);
  var decoder = new TextDecoder();
  return decoder.decode(array);
}

const FOLDER = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content/",
  "http://mochi.test:8888/"
);

add_task(async function() {
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:blank"
  );
  let browserLoadedPromise = BrowserTestUtils.browserLoaded(
    tab.linkedBrowser,
    true,
    `${FOLDER}post.html`
  );
  BrowserTestUtils.loadURI(tab.linkedBrowser, `${FOLDER}post.html`);
  await browserLoadedPromise;

  await SpecialPowers.spawn(tab.linkedBrowser, [], async function() {
    // eslint-disable-next-line mozilla/reject-importGlobalProperties
    Cu.importGlobalProperties(["File"]);
    let file = new File(
      [new Blob(["1234".repeat(1024 * 500)], { type: "text/plain" })],
      "test-name"
    );
    content.document.getElementById("input_file").mozSetFileArray([file]);
    content.document.getElementById("form").submit();
  });

  let promptPromise = PromptTestUtils.handleNextPrompt(
    tab.linkedBrowser,
    {
      modalType: Services.prefs.getIntPref("prompts.modalType.httpAuth"),
      promptType: "promptUserAndPass",
    },
    { buttonNumClick: 0, loginInput: "user", passwordInput: "pass" }
  );

  await promptPromise;

  // In bug 1676331 we made DoAuthRetry fail if the stream is not seekable.
  // Ideally this should always work. When bug 1696386 is fixed
  // the post should work immediately after navigation.
  await BrowserTestUtils.browserStopped(tab.linkedBrowser);
  await SpecialPowers.spawn(tab.linkedBrowser, [], async function() {
    Assert.ok(content.location.href.includes("post.html"));
  });

  let finalLoadPromise = BrowserTestUtils.browserLoaded(
    tab.linkedBrowser,
    true,
    `${FOLDER}auth_post.sjs`
  );
  await SpecialPowers.spawn(tab.linkedBrowser, [], async function() {
    content.document.getElementById("form").submit();
  });
  await finalLoadPromise;

  await SpecialPowers.spawn(tab.linkedBrowser, [], async function() {
    Assert.ok(content.location.href.includes("auth_post.sjs"));
    Assert.ok(content.document.body.innerHTML.includes("1234"));
  });

  BrowserTestUtils.removeTab(tab);
});
