/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { CookieXPCShellUtils } = ChromeUtils.import(
  "resource://testing-common/CookieXPCShellUtils.jsm"
);

CookieXPCShellUtils.init(this);

const TEST_CASES = [
  // Tests for different schemes.
  {
    url: "http://example.com/",
    partitionKeySite: "(http,example.com)",
    partitionKeyWithoutSite: "example.com",
  },
  {
    url: "https://example.com/",
    partitionKeySite: "(https,example.com)",
    partitionKeyWithoutSite: "example.com",
  },
  // Tests for sub domains
  {
    url: "http://sub.example.com/",
    partitionKeySite: "(http,example.com)",
    partitionKeyWithoutSite: "example.com",
  },
  {
    url: "http://sub.sub.example.com/",
    partitionKeySite: "(http,example.com)",
    partitionKeyWithoutSite: "example.com",
  },
  // Tests for path and query.
  {
    url: "http://www.example.com/path/to/somewhere/",
    partitionKeySite: "(http,example.com)",
    partitionKeyWithoutSite: "example.com",
  },
  {
    url: "http://www.example.com/?query=string",
    partitionKeySite: "(http,example.com)",
    partitionKeyWithoutSite: "example.com",
  },
  // Tests for other ports.
  {
    url: "http://example.com:8080/",
    partitionKeySite: "(http,example.com)",
    partitionKeyWithoutSite: "example.com",
  },
  {
    url: "https://example.com:8080/",
    partitionKeySite: "(https,example.com)",
    partitionKeyWithoutSite: "example.com",
  },
  // Tests for about urls
  {
    url: "about:about",
    partitionKeySite:
      "(about,about.ef2a7dd5-93bc-417f-a698-142c3116864f.mozilla)",
    partitionKeyWithoutSite:
      "about.ef2a7dd5-93bc-417f-a698-142c3116864f.mozilla",
  },
  {
    url: "about:preferences",
    partitionKeySite:
      "(about,about.ef2a7dd5-93bc-417f-a698-142c3116864f.mozilla)",
    partitionKeyWithoutSite:
      "about.ef2a7dd5-93bc-417f-a698-142c3116864f.mozilla",
  },
  // Test for ip addresses
  {
    url: "http://127.0.0.1/",
    partitionKeySite: "(http,127.0.0.1)",
    partitionKeyWithoutSite: "127.0.0.1",
  },
  {
    url: "http://127.0.0.1:8080/",
    partitionKeySite: "(http,127.0.0.1,8080)",
    partitionKeyWithoutSite: "127.0.0.1",
  },
  {
    url: "http://[2001:db8::ff00:42:8329]",
    partitionKeySite: "(http,[2001:db8::ff00:42:8329])",
    partitionKeyWithoutSite: "[2001:db8::ff00:42:8329]",
  },
  {
    url: "http://[2001:db8::ff00:42:8329]:8080",
    partitionKeySite: "(http,[2001:db8::ff00:42:8329],8080)",
    partitionKeyWithoutSite: "[2001:db8::ff00:42:8329]",
  },
  // Tests for moz-extension
  {
    url: "moz-extension://bafa4a3f-5c49-48d6-9788-03489419b70e",
    partitionKeySite: "",
    partitionKeyWithoutSite: "",
  },
  // Tests for non tld
  {
    url: "http://notld",
    partitionKeySite: "(http,notld)",
    partitionKeyWithoutSite: "notld",
  },
  {
    url: "http://com",
    partitionKeySite: "(http,com)",
    partitionKeyWithoutSite: "com",
  },
  {
    url: "http://com:8080",
    partitionKeySite: "(http,com,8080)",
    partitionKeyWithoutSite: "com",
  },
];

const TEST_INVALID_URLS = [
  "",
  "/foo",
  "An invalid URL",
  "https://",
  "http:///",
  "http://foo:bar",
];

add_task(async function test_get_partition_key_from_url() {
  for (const test of TEST_CASES) {
    info(`Testing url: ${test.url}`);
    let partitionKey = ChromeUtils.getPartitionKeyFromURL(test.url);

    Assert.equal(
      partitionKey,
      test.partitionKeySite,
      "The partitionKey is correct."
    );
  }
});

add_task(async function test_get_partition_key_from_url_without_site() {
  Services.prefs.setBoolPref("privacy.dynamic_firstparty.use_site", false);

  for (const test of TEST_CASES) {
    info(`Testing url: ${test.url}`);
    let partitionKey = ChromeUtils.getPartitionKeyFromURL(test.url);

    Assert.equal(
      partitionKey,
      test.partitionKeyWithoutSite,
      "The partitionKey is correct."
    );
  }

  Services.prefs.clearUserPref("privacy.dynamic_firstparty.use_site");
});

add_task(async function test_blob_url() {
  do_get_profile();

  const server = CookieXPCShellUtils.createServer({
    hosts: ["example.org", "foo.com"],
  });

  server.registerPathHandler("/empty", (metadata, response) => {
    var body = "<h1>Hello!</h1>";
    response.write(body);
  });

  server.registerPathHandler("/iframe", (metadata, response) => {
    var body = `
      <script>
      var blobUrl = URL.createObjectURL(new Blob([]));
      parent.postMessage(blobUrl, "http://example.org");
      </script>
    `;
    response.write(body);
  });

  let contentPage = await CookieXPCShellUtils.loadContentPage(
    "http://example.org/empty"
  );

  let blobUrl = await contentPage.spawn(null, async () => {
    // Create a third-party iframe and create a blob url in there.
    let f = this.content.document.createElement("iframe");
    f.src = "http://foo.com/iframe";

    let blob_url = await new Promise(resolve => {
      this.content.addEventListener("message", event => resolve(event.data), {
        once: true,
      });
      this.content.document.body.append(f);
    });

    return blob_url;
  });

  let partitionKey = ChromeUtils.getPartitionKeyFromURL(blobUrl);

  // The partitionKey of the blob url is empty because the principal of the
  // blob url is the JS principal of the global, which doesn't have
  // partitionKey. And ChromeUtils.getPartitionKeyFromURL() will get
  // partitionKey from that principal. So, we will get an empty partitionKey
  // here.
  // XXX: The behavior here is debatable.
  Assert.equal(partitionKey, "", "The partitionKey of blob url is correct.");

  await contentPage.close();
});

add_task(async function test_throw_with_invalid_URL() {
  // The API should throw if the url is invalid.
  for (const invalidURL of TEST_INVALID_URLS) {
    info(`Testing invalid url: ${invalidURL}`);

    Assert.throws(
      () => {
        ChromeUtils.getPartitionKeyFromURL(invalidURL);
      },
      /NS_ERROR_MALFORMED_URI/,
      "It should fail on invalid URLs."
    );
  }
});
