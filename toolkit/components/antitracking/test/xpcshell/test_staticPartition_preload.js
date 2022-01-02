/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

"use strict";

const { NetUtil } = ChromeUtils.import("resource://gre/modules/NetUtil.jsm");

const { CookieXPCShellUtils } = ChromeUtils.import(
  "resource://testing-common/CookieXPCShellUtils.jsm"
);

let gHints = 0;

CookieXPCShellUtils.init(this);

function countMatchingCacheEntries(cacheEntries, domain, path) {
  return cacheEntries
    .map(entry => entry.uri.asciiSpec)
    .filter(spec => spec.includes(domain))
    .filter(spec => spec.includes(path)).length;
}

async function checkCache(originAttributes) {
  const loadContextInfo = Services.loadContextInfo.custom(
    false,
    originAttributes
  );

  const data = await new Promise(resolve => {
    let cacheEntries = [];
    let cacheVisitor = {
      onCacheStorageInfo(num, consumption) {},
      onCacheEntryInfo(uri, idEnhance) {
        cacheEntries.push({ uri, idEnhance });
      },
      onCacheEntryVisitCompleted() {
        resolve(cacheEntries);
      },
      QueryInterface: ChromeUtils.generateQI(["nsICacheStorageVisitor"]),
    };
    // Visiting the disk cache also visits memory storage so we do not
    // need to use Services.cache2.memoryCacheStorage() here.
    let storage = Services.cache2.diskCacheStorage(loadContextInfo);
    storage.asyncVisitStorage(cacheVisitor, true);
  });

  let foundEntryCount = countMatchingCacheEntries(
    data,
    "example.org",
    "style.css"
  );
  ok(
    foundEntryCount > 0,
    `Cache entries expected for style.css and OA=${originAttributes}`
  );
}

add_task(async () => {
  do_get_profile();

  Services.prefs.setBoolPref("network.preload", true);
  Services.prefs.setIntPref("network.cookie.cookieBehavior", 0);

  const server = CookieXPCShellUtils.createServer({
    hosts: ["example.org", "foo.com"],
  });

  server.registerPathHandler("/empty", (metadata, response) => {
    var body = "<h1>Hello!</h1>";
    response.bodyOutputStream.write(body, body.length);
  });

  server.registerPathHandler("/style.css", (metadata, response) => {
    gHints++;
    response.setHeader("Cache-Control", "max-age=10000", false);
    response.setStatusLine(metadata.httpVersion, 200, "OK");
    response.setHeader("Access-Control-Allow-Origin", "*", false);
    var body = "* { color: red }";
    response.bodyOutputStream.write(body, body.length);
  });

  server.registerPathHandler("/preload", (metadata, response) => {
    response.setStatusLine(metadata.httpVersion, 200, "OK");
    response.setHeader("Content-Type", "text/html", false);
    var body = `<html><head></head><body><script>
      const link = document.createElement("link")
      link.setAttribute("rel", "preload");
      link.setAttribute("as", "style");
      link.setAttribute("href", "http://example.org/style.css");
      document.head.appendChild(link);
      link.onload = () => {
        fetch("/done").then(() => {});
      };
    </script></body></html>`;
    response.bodyOutputStream.write(body, body.length);
  });

  const tests = [
    {
      // 2 hints because we have 2 different top-level origins, loading the
      // same resource. This will end up creating 2 separate cache entries.
      hints: 2,
      prefValue: true,
      originAttributes: { partitionKey: "(http,example.org)" },
    },
    {
      // 1 hint because, with network-state isolation, the cache entry will be
      // reused for the second loading, even if the top-level origins are
      // different.
      hints: 1,
      originAttributes: {},
      prefValue: false,
    },
  ];

  for (let test of tests) {
    await new Promise(resolve =>
      Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, resolve)
    );

    info("Reset the shared sheets");
    let contentPage = await CookieXPCShellUtils.loadContentPage(
      "http://example.org/empty"
    );

    await contentPage.spawn(null, () =>
      // eslint-disable-next-line no-undef
      content.windowUtils.clearSharedStyleSheetCache()
    );

    await contentPage.close();

    info("Reset the counter");
    gHints = 0;

    info("Enabling network state partitioning");
    Services.prefs.setBoolPref(
      "privacy.partition.network_state",
      test.prefValue
    );

    let complete = new Promise(resolve => {
      server.registerPathHandler("/done", (metadata, response) => {
        response.setHeader("Cache-Control", "max-age=10000", false);
        response.setStatusLine(metadata.httpVersion, 200, "OK");
        response.setHeader("Content-Type", "text/html", false);
        var body = "OK";
        response.bodyOutputStream.write(body, body.length);
        resolve();
      });
    });

    info("Let's load a page with origin A");
    contentPage = await CookieXPCShellUtils.loadContentPage(
      "http://example.org/preload"
    );

    await complete;
    await checkCache(test.originAttributes);
    await contentPage.close();

    complete = new Promise(resolve => {
      server.registerPathHandler("/done", (metadata, response) => {
        response.setHeader("Cache-Control", "max-age=10000", false);
        response.setStatusLine(metadata.httpVersion, 200, "OK");
        response.setHeader("Content-Type", "text/html", false);
        var body = "OK";
        response.bodyOutputStream.write(body, body.length);
        resolve();
      });
    });

    info("Let's load a page with origin B");
    contentPage = await CookieXPCShellUtils.loadContentPage(
      "http://foo.com/preload"
    );

    await complete;
    await checkCache(test.originAttributes);
    await contentPage.close();

    Assert.equal(
      gHints,
      test.hints,
      "We have the current number of requests with pref " + test.prefValue
    );
  }
});
