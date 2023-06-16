"use strict";

AddonTestUtils.init(this);
AddonTestUtils.overrideCertDB();
AddonTestUtils.createAppInfo(
  "xpcshell@tests.mozilla.org",
  "XPCShell",
  "1",
  "43"
);

let { promiseRestartManager, promiseShutdownManager, promiseStartupManager } =
  AddonTestUtils;

const server = createHttpServer({ hosts: ["example.com"] });
server.registerDirectory("/data/", do_get_file("data"));

// Test that a blocking listener that uses filterResponseData() works
// properly (i.e., that the delayed call to registerTraceableChannel
// works properly).
add_task(async function test_StreamFilter_at_restart() {
  const DATA = `<!DOCTYPE html>
<html>
<body>
  <h1>This is a modified page</h1>
</body>
</html>`;

  function background(data) {
    browser.webRequest.onBeforeRequest.addListener(
      details => {
        let filter = browser.webRequest.filterResponseData(details.requestId);
        filter.onstop = () => {
          let encoded = new TextEncoder().encode(data);
          filter.write(encoded);
          filter.close();
        };
      },
      { urls: ["http://example.com/data/file_sample.html"] },
      ["blocking"]
    );
  }

  await promiseStartupManager();

  let extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "permanent",
    manifest: {
      permissions: ["webRequest", "webRequestBlocking", "http://example.com/"],
    },

    background: `(${background})(${uneval(DATA)})`,
  });

  await extension.startup();

  await promiseRestartManager();
  await extension.awaitStartup();

  let dataPromise = ExtensionTestUtils.fetch(
    "http://example.com/",
    "http://example.com/data/file_sample.html"
  );

  Services.obs.notifyObservers(null, "browser-delayed-startup-finished");
  let data = await dataPromise;

  equal(
    data,
    DATA,
    "Stream filter was properly installed for a load during startup"
  );

  await extension.unload();
  await promiseShutdownManager();
});
