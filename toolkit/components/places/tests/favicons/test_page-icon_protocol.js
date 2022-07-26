/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const ICON_DATAURL =
  "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAAAAAA6fptVAAAACklEQVQI12NgAAAAAgAB4iG8MwAAAABJRU5ErkJggg==";
const TEST_URI = NetUtil.newURI("http://mozilla.org/");
const ICON_URI = NetUtil.newURI("http://mozilla.org/favicon.ico");

const { XPCShellContentUtils } = ChromeUtils.import(
  "resource://testing-common/XPCShellContentUtils.jsm"
);

const PAGE_ICON_TEST_URLS = [
  "page-icon:http://example.com/",
  "page-icon:http://a-site-never-before-seen.test",
  // For the following, the page-icon protocol is expected to successfully
  // return the default favicon.
  "page-icon:test",
  "page-icon:",
  "page-icon:chrome://something.html",
  "page-icon:foo://bar/baz",
];

XPCShellContentUtils.init(this);

const HTML = String.raw`<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
</head>
<body>
  Hello from example.com!
</body>
</html>`;

const server = XPCShellContentUtils.createHttpServer({
  hosts: ["example.com"],
});

server.registerPathHandler("/", (request, response) => {
  response.setHeader("Content-Type", "text/html");
  response.write(HTML);
});

function fetchIconForSpec(spec) {
  return new Promise((resolve, reject) => {
    NetUtil.asyncFetch(
      {
        uri: NetUtil.newURI(spec),
        loadUsingSystemPrincipal: true,
        contentPolicyType: Ci.nsIContentPolicy.TYPE_INTERNAL_IMAGE_FAVICON,
      },
      (input, status, request) => {
        if (!Components.isSuccessCode(status)) {
          reject(new Error("unable to load icon"));
          return;
        }

        try {
          let data = NetUtil.readInputStreamToString(input, input.available());
          let contentType = request.QueryInterface(Ci.nsIChannel).contentType;
          input.close();
          resolve({ data, contentType });
        } catch (ex) {
          reject(ex);
        }
      }
    );
  });
}

var gDefaultFavicon;
var gFavicon;

add_task(async function setup() {
  await PlacesTestUtils.addVisits(TEST_URI);

  PlacesUtils.favicons.replaceFaviconDataFromDataURL(
    ICON_URI,
    ICON_DATAURL,
    (Date.now() + 8640000) * 1000,
    Services.scriptSecurityManager.getSystemPrincipal()
  );

  await new Promise(resolve => {
    PlacesUtils.favicons.setAndFetchFaviconForPage(
      TEST_URI,
      ICON_URI,
      false,
      PlacesUtils.favicons.FAVICON_LOAD_NON_PRIVATE,
      resolve,
      Services.scriptSecurityManager.getSystemPrincipal()
    );
  });

  gDefaultFavicon = await fetchIconForSpec(
    PlacesUtils.favicons.defaultFavicon.spec
  );
  gFavicon = await fetchIconForSpec(ICON_DATAURL);
});

add_task(async function known_url() {
  let { data, contentType } = await fetchIconForSpec(
    "page-icon:" + TEST_URI.spec
  );
  Assert.equal(contentType, gFavicon.contentType);
  Assert.deepEqual(data, gFavicon.data, "Got the favicon data");
});

add_task(async function unknown_url() {
  let { data, contentType } = await fetchIconForSpec(
    "page-icon:http://www.moz.org/"
  );
  Assert.equal(contentType, gDefaultFavicon.contentType);
  Assert.deepEqual(data, gDefaultFavicon.data, "Got the default favicon data");
});

add_task(async function invalid_url() {
  let { data, contentType } = await fetchIconForSpec("page-icon:test");
  Assert.equal(contentType, gDefaultFavicon.contentType);
  Assert.ok(data == gDefaultFavicon.data, "Got the default favicon data");
});

add_task(async function subpage_url_fallback() {
  let { data, contentType } = await fetchIconForSpec(
    "page-icon:http://mozilla.org/missing"
  );
  Assert.equal(contentType, gFavicon.contentType);
  Assert.deepEqual(data, gFavicon.data, "Got the root favicon data");
});

add_task(async function svg_icon() {
  let faviconURI = NetUtil.newURI("http://places.test/favicon.svg");
  PlacesUtils.favicons.replaceFaviconDataFromDataURL(
    faviconURI,
    SMALLSVG_DATA_URI.spec,
    0,
    Services.scriptSecurityManager.getSystemPrincipal()
  );
  await setFaviconForPage(TEST_URI, faviconURI);
  let svgIcon = await fetchIconForSpec(SMALLSVG_DATA_URI.spec);
  info(svgIcon.contentType);
  let pageIcon = await fetchIconForSpec("page-icon:" + TEST_URI.spec);
  Assert.equal(svgIcon.contentType, pageIcon.contentType);
  Assert.deepEqual(svgIcon.data, pageIcon.data, "Got the root favicon data");
});

add_task(async function page_with_ref() {
  for (let url of [
    "http://places.test.ref/#myref",
    "http://places.test.ref/#!&b=16",
    "http://places.test.ref/#",
  ]) {
    await PlacesTestUtils.addVisits(url);
    await setFaviconForPage(url, ICON_URI, false);
    let { data, contentType } = await fetchIconForSpec("page-icon:" + url);
    Assert.equal(contentType, gFavicon.contentType);
    Assert.deepEqual(data, gFavicon.data, "Got the favicon data");
    await PlacesUtils.history.remove(url);
  }
});

/**
 * Tests that page-icon does not work in a normal content process.
 */
add_task(async function page_content_process() {
  let contentPage = await XPCShellContentUtils.loadContentPage(
    "http://example.com/",
    {
      remote: true,
    }
  );
  Assert.notEqual(
    contentPage.browsingContext.currentRemoteType,
    "privilegedabout"
  );

  await contentPage.spawn(PAGE_ICON_TEST_URLS, async URLS => {
    // We expect each of these URLs to produce an error event when
    // we attempt to load them in this process type.
    /* global content */
    for (let url of URLS) {
      let img = content.document.createElement("img");
      img.src = url;
      let imgPromise = new Promise((resolve, reject) => {
        img.addEventListener("error", e => {
          Assert.ok(true, "Got expected load error.");
          resolve();
        });
        img.addEventListener("load", e => {
          Assert.ok(false, "Did not expect a successful load.");
          reject();
        });
      });
      content.document.body.appendChild(img);
      await imgPromise;
    }
  });

  await contentPage.close();
});

/**
 * Tests that page-icon does work for privileged about content process
 */
add_task(async function page_privileged_about_content_process() {
  // about:certificate loads in the privileged about content process.
  let contentPage = await XPCShellContentUtils.loadContentPage(
    "about:certificate",
    {
      remote: true,
    }
  );
  Assert.equal(
    contentPage.browsingContext.currentRemoteType,
    "privilegedabout"
  );

  await contentPage.spawn(PAGE_ICON_TEST_URLS, async URLS => {
    // We expect each of these URLs to load correctly in this process
    // type.
    for (let url of URLS) {
      let img = content.document.createElement("img");
      img.src = url;
      let imgPromise = new Promise((resolve, reject) => {
        img.addEventListener("error", e => {
          Assert.ok(false, "Did not expect an error.");
          reject();
        });
        img.addEventListener("load", e => {
          Assert.ok(true, "Got expected load event.");
          resolve();
        });
      });
      content.document.body.appendChild(img);
      await imgPromise;
    }
  });

  await contentPage.close();
});
