/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

function cookiesToMime(cookies) {
  return `dummy/${encodeURIComponent(cookies)}`.toLowerCase();
}

function mimeToCookies(mime) {
  return decodeURIComponent(mime.replace("dummy/", ""));
}

const server = createHttpServer({ hosts: ["example.net"] });

server.registerPathHandler("/download", (request, response) => {
  response.setStatusLine(request.httpVersion, 200, "OK");
  let cookies = request.hasHeader("Cookie") ? request.getHeader("Cookie") : "";
  // Assign the result through the MIME-type, to make it easier to read the
  // result via the downloads API.
  response.setHeader("Content-Type", cookiesToMime(cookies));
  // Response of length 7.
  response.write("1234567");
});

const DOWNLOAD_URL = "http://example.net/download";

async function setUpCookies() {
  Services.cookies.removeAll();
  let extension = ExtensionTestUtils.loadExtension({
    incognitoOverride: "spanning",
    manifest: {
      permissions: ["cookies", "http://example.net/download"],
    },
    async background() {
      let url = "http://example.net/download";
      // Add default cookie
      await browser.cookies.set({
        url,
        name: "cookie_normal",
        value: "1",
      });

      // Add private cookie
      await browser.cookies.set({
        url,
        storeId: "firefox-private",
        name: "cookie_private",
        value: "1",
      });

      // Add container cookie
      await browser.cookies.set({
        url,
        storeId: "firefox-container-1",
        name: "cookie_container",
        value: "1",
      });
      browser.test.sendMessage("cookies set");
    },
  });
  await extension.startup();
  await extension.awaitMessage("cookies set");
  await extension.unload();
}

function createDownloadTestExtension(extraPermissions = [], incognito = false) {
  let extensionOptions = {
    manifest: {
      permissions: ["downloads", ...extraPermissions],
    },
    background() {
      browser.test.onMessage.addListener(async (method, data) => {
        async function getDownload(data) {
          let donePromise = new Promise(resolve => {
            browser.downloads.onChanged.addListener(async delta => {
              if (delta.state?.current === "complete") {
                resolve(delta.id);
              }
            });
          });
          let downloadId = await browser.downloads.download(data);
          browser.test.assertEq(await donePromise, downloadId, "got download");
          let [download] = await browser.downloads.search({ id: downloadId });
          browser.test.log(`Download results: ${JSON.stringify(download)}`);
          // Delete the file since we aren't interested in it.
          // TODO bug 1654819: On Windows the file may be recreated.
          await browser.downloads.removeFile(download.id);
          // Sanity check to verify that we got the result from /download.
          browser.test.assertEq(7, download.fileSize, "download succeeded");
          return download;
        }
        function checkDownloadError(data) {
          return browser.test.assertRejects(
            browser.downloads.download(data.downloadData),
            data.exceptionRe
          );
        }
        function search(data) {
          return browser.downloads.search(data);
        }
        function erase(data) {
          return browser.downloads.erase(data);
        }
        switch (method) {
          case "getDownload":
            return browser.test.sendMessage(method, await getDownload(data));
          case "checkDownloadError":
            return browser.test.sendMessage(
              method,
              await checkDownloadError(data)
            );
          case "search":
            return browser.test.sendMessage(method, await search(data));
          case "erase":
            return browser.test.sendMessage(method, await erase(data));
        }
      });
    },
  };
  if (incognito) {
    extensionOptions.incognitoOverride = "spanning";
  }
  return ExtensionTestUtils.loadExtension(extensionOptions);
}

function getResult(extension, method, data) {
  extension.sendMessage(method, data);
  return extension.awaitMessage(method);
}

async function getCookies(extension, data) {
  let download = await getResult(extension, "getDownload", data);
  // The "/download" endpoint mirrors received cookies via Content-Type.
  let cookies = mimeToCookies(download.mime);
  return cookies;
}

async function runTests(extension, containerDownloadAllowed, privateAllowed) {
  let forcedIncognitoException = null;
  if (!privateAllowed) {
    forcedIncognitoException = /private browsing access not allowed/;
  } else if (!containerDownloadAllowed) {
    forcedIncognitoException = /No permission for cookieStoreId/;
  }

  // Test default container download
  if (containerDownloadAllowed) {
    equal(
      await getCookies(extension, {
        url: DOWNLOAD_URL,
        cookieStoreId: "firefox-default",
      }),
      "cookie_normal=1",
      "Default container cookies for downloads.download"
    );
  } else {
    await getResult(extension, "checkDownloadError", {
      exceptionRe: /No permission for cookieStoreId/,
      downloadData: {
        url: DOWNLOAD_URL,
        cookieStoreId: "firefox-default",
      },
    });
  }

  // Test private container download
  if (privateAllowed && containerDownloadAllowed) {
    equal(
      await getCookies(extension, {
        url: DOWNLOAD_URL,
        cookieStoreId: "firefox-private",
        incognito: true,
      }),
      "cookie_private=1",
      "Private container cookies for downloads.download"
    );
  } else {
    await getResult(extension, "checkDownloadError", {
      exceptionRe: forcedIncognitoException,
      downloadData: {
        url: DOWNLOAD_URL,
        cookieStoreId: "firefox-private",
        incognito: true,
      },
    });
  }

  // Test firefox-container-1 download
  if (containerDownloadAllowed) {
    equal(
      await getCookies(extension, {
        url: DOWNLOAD_URL,
        cookieStoreId: "firefox-container-1",
      }),
      "cookie_container=1",
      "firefox-container-1 cookies for downloads.download"
    );
  } else {
    await getResult(extension, "checkDownloadError", {
      exceptionRe: /No permission for cookieStoreId/,
      downloadData: {
        url: DOWNLOAD_URL,
        cookieStoreId: "firefox-container-1",
      },
    });
  }

  // Test mismatched incognito and cookieStoreId download
  await getResult(extension, "checkDownloadError", {
    exceptionRe: forcedIncognitoException
      ? forcedIncognitoException
      : /Illegal to set non-private cookieStoreId in a private window/,
    downloadData: {
      url: DOWNLOAD_URL,
      incognito: true,
      cookieStoreId: "firefox-container-1",
    },
  });
  await getResult(extension, "checkDownloadError", {
    exceptionRe: containerDownloadAllowed
      ? /Illegal to set private cookieStoreId in a non-private window/
      : /No permission for cookieStoreId/,
    downloadData: {
      url: DOWNLOAD_URL,
      incognito: false,
      cookieStoreId: "firefox-private",
    },
  });

  // Test invalid cookieStoreId download
  await getResult(extension, "checkDownloadError", {
    exceptionRe: containerDownloadAllowed
      ? /Illegal cookieStoreId/
      : /No permission for cookieStoreId/,
    downloadData: {
      url: DOWNLOAD_URL,
      cookieStoreId: "invalid-invalid-invalid",
    },
  });

  let searchRes, searchResDownload;
  // Test default container search
  searchRes = await getResult(extension, "search", {
    cookieStoreId: "firefox-default",
  });
  equal(
    searchRes.length,
    1,
    "Default container results length for downloads.search"
  );
  [searchResDownload] = searchRes;
  equal(
    mimeToCookies(searchResDownload.mime),
    "cookie_normal=1",
    "Default container cookies for downloads.search"
  );
  // Test default container search with mismatched container
  searchRes = await getResult(extension, "search", {
    mime: cookiesToMime("cookie_normal=1"),
    cookieStoreId: "firefox-container-1",
  });
  equal(
    searchRes.length,
    0,
    "Default container results length for downloads.search when container mismatched"
  );

  // Test private container search
  searchRes = await getResult(extension, "search", {
    cookieStoreId: "firefox-private",
  });
  if (privateAllowed) {
    equal(
      searchRes.length,
      1,
      "Private container results length for downloads.search"
    );
    [searchResDownload] = searchRes;
    equal(
      mimeToCookies(searchResDownload.mime),
      "cookie_private=1",
      "Private container cookies for downloads.search"
    );
    // Test private container search with mismatched container
    searchRes = await getResult(extension, "search", {
      mime: cookiesToMime("cookie_private=1"),
      cookieStoreId: "firefox-container-1",
    });
    equal(
      searchRes.length,
      0,
      "Private container results length for downloads.search when container mismatched"
    );
  } else {
    equal(
      searchRes.length,
      0,
      "Private container results length for downloads.search when private disallowed"
    );
  }

  // Test firefox-container-1 search
  searchRes = await getResult(extension, "search", {
    cookieStoreId: "firefox-container-1",
  });
  equal(
    searchRes.length,
    1,
    "firefox-container-1 results length for downloads.search"
  );
  [searchResDownload] = searchRes;
  equal(
    mimeToCookies(searchResDownload.mime),
    "cookie_container=1",
    "firefox-container-1 cookies for downloads.search"
  );
  // Test firefox-container-1 search with mismatched container
  searchRes = await getResult(extension, "search", {
    mime: cookiesToMime("cookie_container=1"),
    cookieStoreId: "firefox-default",
  });
  equal(
    searchRes.length,
    0,
    "firefox-container-1 container results length for downloads.search when container mismatched"
  );

  // Test default container erase with mismatched container
  await getResult(extension, "erase", {
    mime: cookiesToMime("cookie_normal=1"),
    cookieStoreId: "firefox-container-1",
  });
  searchRes = await getResult(extension, "search", {
    mime: cookiesToMime("cookie_normal=1"),
  });
  equal(
    searchRes.length,
    1,
    "Default container results length for downloads.search after erase with mismatched container"
  );

  // Test private container erase with mismatched container
  await getResult(extension, "erase", {
    mime: cookiesToMime("cookie_private=1"),
    cookieStoreId: "firefox-container-1",
  });
  searchRes = await getResult(extension, "search", {
    mime: cookiesToMime("cookie_private=1"),
  });
  equal(
    searchRes.length,
    privateAllowed ? 1 : 0,
    "Private container results length for downloads.search after erase with mismatched container"
  );

  // Test firefox-container-1 erase with mismatched container
  await getResult(extension, "erase", {
    mime: cookiesToMime("cookie_container=1"),
    cookieStoreId: "firefox-default",
  });
  searchRes = await getResult(extension, "search", {
    mime: cookiesToMime("cookie_container=1"),
  });
  equal(
    searchRes.length,
    1,
    "firefox-container-1 results length for downloads.search after erase with mismatched container"
  );

  // Test default container erase
  await getResult(extension, "erase", {
    cookieStoreId: "firefox-default",
  });
  searchRes = await getResult(extension, "search", {
    mime: cookiesToMime("cookie_normal=1"),
  });
  equal(
    searchRes.length,
    0,
    "Default container results length for downloads.search after erase"
  );

  // Test private container erase
  await getResult(extension, "erase", {
    cookieStoreId: "firefox-private",
  });
  searchRes = await getResult(extension, "search", {
    mime: cookiesToMime("cookie_private=1"),
  });
  // The following will also pass when incognito disabled
  equal(
    searchRes.length,
    0,
    "Private container results length for downloads.search after erase"
  );

  // Test firefox-container-1 erase
  await getResult(extension, "erase", {
    cookieStoreId: "firefox-container-1",
  });
  searchRes = await getResult(extension, "search", {
    mime: cookiesToMime("cookie_container=1"),
  });
  equal(
    searchRes.length,
    0,
    "firefox-container-1 results length for downloads.search after erase"
  );
}

async function populateDownloads(extension) {
  await getResult(extension, "erase", {});
  await getResult(extension, "getDownload", {
    url: DOWNLOAD_URL,
  });
  await getResult(extension, "getDownload", {
    url: DOWNLOAD_URL,
    incognito: true,
  });
  await getResult(extension, "getDownload", {
    url: DOWNLOAD_URL,
    cookieStoreId: "firefox-container-1",
  });
}

add_task(async function setup() {
  const nsIFile = Ci.nsIFile;
  const downloadDir = FileUtils.getDir("TmpD", ["downloads"]);
  downloadDir.createUnique(nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);
  Services.prefs.setIntPref("browser.download.folderList", 2);
  Services.prefs.setComplexValue("browser.download.dir", nsIFile, downloadDir);
  Services.prefs.setBoolPref("privacy.userContext.enabled", true);
  await setUpCookies();
  registerCleanupFunction(() => {
    Services.cookies.removeAll();
    Services.prefs.clearUserPref("browser.download.folderList");
    Services.prefs.clearUserPref("browser.download.dir");
    downloadDir.remove(false);
  });
});

add_task(async function download_cookieStoreId() {
  // Test extension with cookies permission and incognito enabled
  let extension = createDownloadTestExtension(["cookies"], true);
  await extension.startup();
  await runTests(extension, true, true);

  // Test extension with incognito enabled and no cookies permission
  await populateDownloads(extension);
  let noCookiesExtension = createDownloadTestExtension([], true);
  await noCookiesExtension.startup();
  await runTests(noCookiesExtension, false, true);
  await noCookiesExtension.unload();

  // Test extension with incognito disabled and no cookies permission
  await populateDownloads(extension);
  let noCookiesAndPrivateExtension = createDownloadTestExtension([], false);
  await noCookiesAndPrivateExtension.startup();
  await runTests(noCookiesAndPrivateExtension, false, false);
  await noCookiesAndPrivateExtension.unload();

  // Verify that incognito disabled test did not delete private download
  let searchRes = await getResult(extension, "search", {
    mime: cookiesToMime("cookie_private=1"),
  });
  ok(searchRes.length, "Incognito disabled does not delete private download");

  await extension.unload();
});
