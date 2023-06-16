"use strict";

let server = createHttpServer({ hosts: ["example.com"] });
let downloadReqCount = 0;
server.registerPathHandler("/downloadtest", (req, res) => {
  ++downloadReqCount;
});

add_setup(async () => {
  let downloadDir = await IOUtils.createUniqueDirectory(
    Services.dirsvc.get("TmpD", Ci.nsIFile).path,
    "downloadDirForDnrDownloadTest"
  );
  info(`Using download directory ${downloadDir.path}`);

  Services.prefs.setIntPref("browser.download.folderList", 2);
  Services.prefs.setCharPref("browser.download.dir", downloadDir);

  registerCleanupFunction(async () => {
    Services.prefs.clearUserPref("browser.download.folderList");
    Services.prefs.clearUserPref("browser.download.dir");
    try {
      await IOUtils.remove(downloadDir);
    } catch (e) {
      info(`Failed to remove ${downloadDir} because: ${e}`);
      // Downloaded files should have been deleted by tests.
      // Clean up + report error otherwise.
      let children = await IOUtils.getChildren(downloadDir).catch(e => e);
      ok(false, `Unexpected files in downloadDir: ${children}`);
      await IOUtils.remove(downloadDir, { recursive: true });
    }
  });

  Services.prefs.setBoolPref("extensions.dnr.enabled", true);
});

// Test for Bug 1579911: Check that download requests created by the
// downloads.download API can be observed by extensions.
// The webRequest version is in test_ext_webRequest_download.js.
add_task(async function test_download_api_can_be_blocked_by_dnr() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      manifest_version: 3,
      permissions: ["declarativeNetRequest", "downloads"],
      // No host_permissions here because neither the downloads nor the DNR API
      // require host permissions to download and/or block the request.
    },
    // Not needed, but to rule out downloads being blocked by CSP:
    allowInsecureRequests: true,
    background: async function () {
      await browser.declarativeNetRequest.updateSessionRules({
        addRules: [
          {
            id: 1,
            condition: { urlFilter: "|http://example.com/downloadtest" },
            action: { type: "block" },
          },
        ],
      });

      browser.downloads.onChanged.addListener(delta => {
        browser.test.assertEq(delta.state.current, "interrupted");
        browser.test.sendMessage("done");
      });

      await browser.downloads.download({
        url: "http://example.com/downloadtest",
        filename: "example.txt",
      });
    },
  });

  await extension.startup();
  await extension.awaitMessage("done");
  await extension.unload();

  Assert.equal(downloadReqCount, 0, "Did not expect any download requests");
});

add_task(async function test_download_api_ignores_dnr_from_other_extension() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      manifest_version: 3,
      permissions: ["declarativeNetRequest"],
    },
    background: async function () {
      await browser.declarativeNetRequest.updateSessionRules({
        addRules: [
          {
            id: 1,
            condition: { urlFilter: "|http://example.com/downloadtest" },
            action: { type: "block" },
          },
        ],
      });

      browser.test.sendMessage("dnr_registered");
    },
  });

  let otherExtension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["downloads"],
    },
    background: async function () {
      let downloadDonePromise = new Promise(resolve => {
        browser.downloads.onChanged.addListener(delta => {
          if (delta.state.current === "interrupted") {
            browser.test.fail("Download was unexpectedly interrupted");
            browser.test.notifyFail("done");
          } else if (delta.state.current === "complete") {
            resolve();
          }
        });
      });

      // This download should not have been interrupted by the other extension,
      // because declarativeNetRequest cannot match requests from other
      // extensions.
      let downloadId = await browser.downloads.download({
        url: "http://example.com/downloadtest",
        filename: "example_from_other_ext.txt",
      });
      await downloadDonePromise;
      browser.test.log("Download completed, removing file...");
      // TODO bug 1654819: On Windows the file may be recreated.
      await browser.downloads.removeFile(downloadId);
      browser.test.notifyPass("done");
    },
  });

  await extension.startup();
  await extension.awaitMessage("dnr_registered");

  await otherExtension.startup();
  await otherExtension.awaitFinish("done");
  await otherExtension.unload();
  await extension.unload();

  Assert.equal(downloadReqCount, 1, "Expected one download request");
  downloadReqCount = 0;
});

add_task(
  {
    pref_set: [["extensions.dnr.match_requests_from_other_extensions", true]],
  },
  async function test_download_api_dnr_blocks_other_extension_with_pref() {
    let extension = ExtensionTestUtils.loadExtension({
      manifest: {
        manifest_version: 3,
        permissions: ["declarativeNetRequest"],
      },
      background: async function () {
        await browser.declarativeNetRequest.updateSessionRules({
          addRules: [
            {
              id: 1,
              condition: { urlFilter: "|http://example.com/downloadtest" },
              action: { type: "block" },
            },
          ],
        });

        browser.test.sendMessage("dnr_registered");
      },
    });
    let otherExtension = ExtensionTestUtils.loadExtension({
      manifest: {
        permissions: ["downloads"],
      },
      background: async function () {
        browser.downloads.onChanged.addListener(delta => {
          browser.test.assertEq(delta.state.current, "interrupted");
          browser.test.sendMessage("done");
        });
        await browser.downloads.download({
          url: "http://example.com/downloadtest",
          filename: "example_from_other_ext_with_pref.txt",
        });
      },
    });

    await extension.startup();
    await extension.awaitMessage("dnr_registered");
    await otherExtension.startup();
    await otherExtension.awaitMessage("done");
    await otherExtension.unload();
    await extension.unload();

    Assert.equal(downloadReqCount, 0, "Did not expect any download requests");
  }
);
