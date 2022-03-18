/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const testserver = createHttpServer({ hosts: ["example.com"] });

function createTestPage(body) {
  return `<!DOCTYPE html>
    <html>
      <head>
        <meta charset="utf-8">
      </head>
      <body>
        ${body}
      </body>
    </html>
  `;
}

testserver.registerPathHandler(
  "/installtrigger_ua_detection.html",
  (request, response) => {
    response.write(
      createTestPage(`
    <button/>
    <script>
      document.querySelector("button").onclick = () => {
        typeof InstallTrigger;
      };
    </script>
  `)
    );
  }
);

testserver.registerPathHandler(
  "/installtrigger_install.html",
  (request, response) => {
    response.write(
      createTestPage(`
    <button/>
    <script>
      const install = InstallTrigger.install.bind(InstallTrigger);
      document.querySelector("button").onclick = () => {
        install({ fakeextensionurl: "http://example.com/fakeextensionurl.xpi" });
      };
    </script>
  `)
    );
  }
);

async function testDeprecationWarning(testPageURL, expectedDeprecationWarning) {
  const page = await ExtensionTestUtils.loadContentPage(testPageURL);

  const { message, messageInnerWindowID, pageInnerWindowID } = await page.spawn(
    [expectedDeprecationWarning],
    expectedWarning => {
      return new Promise(resolve => {
        const consoleListener = consoleMsg => {
          if (
            consoleMsg instanceof Ci.nsIScriptError &&
            consoleMsg.message?.includes(expectedWarning)
          ) {
            Services.console.unregisterListener(consoleListener);
            resolve({
              message: consoleMsg.message,
              messageInnerWindowID: consoleMsg.innerWindowID,
              pageInnerWindowID: this.content.windowGlobalChild.innerWindowId,
            });
          }
        };

        Services.console.registerListener(consoleListener);
        this.content.document.querySelector("button").click();
      });
    }
  );

  equal(
    typeof messageInnerWindowID,
    "number",
    `Warning message should be associated to an innerWindowID`
  );
  equal(
    messageInnerWindowID,
    pageInnerWindowID,
    `Deprecation warning "${message}" has been logged and associated to the expected window`
  );

  await page.close();

  return message;
}

add_task(function testDeprecationWarningsOnUADetection() {
  return testDeprecationWarning(
    "http://example.com/installtrigger_ua_detection.html",
    "InstallTrigger is deprecated and will be removed in the future."
  );
});

add_task(async function testDeprecationWarningsOnInstallTriggerInstall() {
  const message = await testDeprecationWarning(
    "http://example.com/installtrigger_install.html",
    "InstallTrigger.install() is deprecated and will be removed in the future."
  );

  const moreInfoURL =
    "https://extensionworkshop.com/documentation/publish/self-distribution/";

  ok(
    message.includes(moreInfoURL),
    "Deprecation warning should include an url to self-distribution documentation"
  );
});
