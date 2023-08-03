/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function testBasicNavigation({ client }) {
  const { Page, Network } = client;
  await Page.enable();
  await Network.enable();
  const loadEventFired = Page.loadEventFired();
  const requestEvent = Network.requestWillBeSent();
  const { frameId, loaderId, errorText } = await Page.navigate({
    url: PAGE_URL,
  });
  const { loaderId: requestLoaderId } = await requestEvent;

  ok(!!loaderId, "Page.navigate returns loaderId");
  is(
    loaderId,
    requestLoaderId,
    "Page.navigate returns same loaderId as corresponding request"
  );
  is(errorText, undefined, "No errorText on a successful navigation");

  await loadEventFired;
  const currentFrame = await getTopFrame(client);
  is(frameId, currentFrame.id, "Page.navigate returns expected frameId");

  is(gBrowser.selectedBrowser.currentURI.spec, PAGE_URL, "Expected URL loaded");
});

add_task(async function testTwoNavigations({ client }) {
  const { Page, Network } = client;
  await Page.enable();
  await Network.enable();
  let requestEvent = Network.requestWillBeSent();
  let loadEventFired = Page.loadEventFired();
  const { frameId, loaderId, errorText } = await Page.navigate({
    url: PAGE_URL,
  });
  const { loaderId: requestLoaderId } = await requestEvent;
  await loadEventFired;
  is(gBrowser.selectedBrowser.currentURI.spec, PAGE_URL, "Expected URL loaded");

  loadEventFired = Page.loadEventFired();
  requestEvent = Network.requestWillBeSent();
  const {
    frameId: frameId2,
    loaderId: loaderId2,
    errorText: errorText2,
  } = await Page.navigate({
    url: PAGE_URL,
  });
  const { loaderId: requestLoaderId2 } = await requestEvent;
  ok(!!loaderId, "Page.navigate returns loaderId");
  ok(!!loaderId2, "Page.navigate returns loaderId");
  isnot(loaderId, loaderId2, "Page.navigate returns different loaderIds");
  is(
    loaderId,
    requestLoaderId,
    "Page.navigate returns same loaderId as corresponding request"
  );
  is(
    loaderId2,
    requestLoaderId2,
    "Page.navigate returns same loaderId as corresponding request"
  );
  is(errorText, undefined, "No errorText on a successful navigation");
  is(errorText2, undefined, "No errorText on a successful navigation");
  is(frameId, frameId2, "Page.navigate return same frameId");

  await loadEventFired;
  is(gBrowser.selectedBrowser.currentURI.spec, PAGE_URL, "Expected URL loaded");
});

add_task(async function testRedirect({ client }) {
  const { Page, Network } = client;
  const sjsURL =
    "https://example.com/browser/remote/cdp/test/browser/page/sjs_redirect.sjs";
  const redirectURL = `${sjsURL}?${PAGE_URL}`;
  await Page.enable();
  await Network.enable();
  const requestEvent = Network.requestWillBeSent();
  const loadEventFired = Page.loadEventFired();

  const { frameId, loaderId, errorText } = await Page.navigate({
    url: redirectURL,
  });
  const { loaderId: requestLoaderId } = await requestEvent;
  ok(!!loaderId, "Page.navigate returns loaderId");
  is(
    loaderId,
    requestLoaderId,
    "Page.navigate returns same loaderId as original request"
  );
  is(errorText, undefined, "No errorText on a successful navigation");
  ok(!!frameId, "Page.navigate returns frameId");

  await loadEventFired;
  is(gBrowser.selectedBrowser.currentURI.spec, PAGE_URL, "Expected URL loaded");
});

add_task(async function testUnknownHost({ client }) {
  const { Page } = client;
  const { frameId, loaderId, errorText } = await Page.navigate({
    url: "https://example-does-not-exist.com",
  });
  ok(!!frameId, "Page.navigate returns frameId");
  ok(!!loaderId, "Page.navigate returns loaderId");
  is(errorText, "NS_ERROR_UNKNOWN_HOST", "Failed navigation returns errorText");
});

add_task(async function testExpiredCertificate({ client }) {
  const { Page } = client;
  const { frameId, loaderId, errorText } = await Page.navigate({
    url: "https://expired.example.com",
  });
  ok(!!frameId, "Page.navigate returns frameId");
  ok(!!loaderId, "Page.navigate returns loaderId");
  is(
    errorText,
    "SEC_ERROR_EXPIRED_CERTIFICATE",
    "Failed navigation returns errorText"
  );
});

add_task(async function testUnknownCertificate({ client }) {
  const { Page, Network } = client;
  await Network.enable();
  const requestEvent = Network.requestWillBeSent();
  const { frameId, loaderId, errorText } = await Page.navigate({
    url: "https://self-signed.example.com",
  });
  const { loaderId: requestLoaderId } = await requestEvent;
  ok(!!frameId, "Page.navigate returns frameId");
  ok(!!loaderId, "Page.navigate returns loaderId");
  is(
    loaderId,
    requestLoaderId,
    "Page.navigate returns same loaderId as original request"
  );
  is(errorText, "SSL_ERROR_UNKNOWN", "Failed navigation returns errorText");
});

add_task(async function testNotFound({ client }) {
  const { Page } = client;
  const { frameId, loaderId, errorText } = await Page.navigate({
    url: "https://example.com/browser/remote/doesnotexist.html",
  });
  ok(!!frameId, "Page.navigate returns frameId");
  ok(!!loaderId, "Page.navigate returns loaderId");
  is(errorText, undefined, "No errorText on a 404");
});

add_task(async function testInvalidURL({ client }) {
  const { Page } = client;

  for (let url of ["blah.com", "foo", "https\n//", "http", ""]) {
    await Assert.rejects(
      Page.navigate({ url }),
      err => err.message.includes("invalid URL"),
      `Invalid url ${url} causes error`
    );
  }

  for (let url of [2, {}, true]) {
    await Assert.rejects(
      Page.navigate({ url }),
      err => err.message.includes("string value expected"),
      `Invalid url ${url} causes error`
    );
  }
});

add_task(async function testDataURL({ client }) {
  const { Page } = client;
  const url = toDataURL("first");
  await Page.enable();
  const loadEventFired = Page.loadEventFired();
  const frameNavigatedFired = Page.frameNavigated();
  const { frameId, loaderId, errorText } = await Page.navigate({ url });
  is(errorText, undefined, "No errorText on a successful navigation");
  ok(!!loaderId, "Page.navigate returns loaderId");

  await loadEventFired;
  const { frame } = await frameNavigatedFired;
  is(frame.loaderId, loaderId, "Page.navigate returns expected loaderId");
  const currentFrame = await getTopFrame(client);
  is(frameId, currentFrame.id, "Page.navigate returns expected frameId");
  is(gBrowser.selectedBrowser.currentURI.spec, url, "Expected URL loaded");
});

add_task(async function testFileURL({ client }) {
  const { AppConstants } = ChromeUtils.importESModule(
    "resource://gre/modules/AppConstants.sys.mjs"
  );

  if (AppConstants.DEBUG) {
    // Bug 1634695 Navigating to a file URL forces the TabSession to destroy
    // abruptly and content domains are not properly destroyed, which creates
    // window leaks and fails the test in DEBUG mode.
    return;
  }

  const { Page } = client;
  const dir = getChromeDir(getResolvedURI(gTestPath));
  dir.append("doc_empty.html");

  // The file can be a symbolic link on local build.  Normalize it to make sure
  // the path matches to the actual URI opened in the new tab.
  dir.normalize();
  const url = Services.io.newFileURI(dir).spec;
  const browser = gBrowser.selectedTab.linkedBrowser;
  const loaded = BrowserTestUtils.browserLoaded(browser, false, url);

  const { /* frameId, */ loaderId, errorText } = await Page.navigate({ url });
  is(errorText, undefined, "No errorText on a successful navigation");
  ok(!!loaderId, "Page.navigate returns loaderId");

  // Bug 1634693 Page.loadEventFired isn't emitted after file: navigation
  await loaded;
  is(browser.currentURI.spec, url, "Expected URL loaded");
  // Bug 1634695 Navigating to file: returns wrong frame id and hangs
  // content page domain methods
  // const currentFrame = await getTopFrame(client);
  // ok(frameId === currentFrame.id, "Page.navigate returns expected frameId");
});

add_task(async function testAbout({ client }) {
  const { Page } = client;
  await Page.enable();
  let loadEventFired = Page.loadEventFired();
  let frameNavigatedFired = Page.frameNavigated();
  const { frameId, loaderId, errorText } = await Page.navigate({
    url: "about:blank",
  });
  ok(!!loaderId, "Page.navigate returns loaderId");
  is(errorText, undefined, "No errorText on a successful navigation");

  await loadEventFired;
  const { frame } = await frameNavigatedFired;
  is(frame.loaderId, loaderId, "Page.navigate returns expected loaderId");
  const currentFrame = await getTopFrame(client);
  is(frameId, currentFrame.id, "Page.navigate returns expected frameId");
  is(
    gBrowser.selectedBrowser.currentURI.spec,
    "about:blank",
    "Expected URL loaded"
  );
});

add_task(async function testSameDocumentNavigation({ client }) {
  const { Page } = client;
  const { frameId, loaderId } = await Page.navigate({
    url: PAGE_URL,
  });
  ok(!!loaderId, "Page.navigate returns loaderId");

  await Page.enable();
  const navigatedWithinDocument = Page.navigatedWithinDocument();

  info("Check that Page.navigate can navigate to an anchor");
  const sameDocumentURL = `${PAGE_URL}#hash`;
  const { frameId: sameDocumentFrameId, loaderId: sameDocumentLoaderId } =
    await Page.navigate({ url: sameDocumentURL });
  ok(
    !sameDocumentLoaderId,
    "Page.navigate does not return a loaderId for same document navigation"
  );
  is(
    sameDocumentFrameId,
    frameId,
    "Page.navigate returned the expected frame id"
  );

  const { frameId: navigatedFrameId, url } = await navigatedWithinDocument;
  is(
    frameId,
    navigatedFrameId,
    "navigatedWithinDocument returns the expected frameId"
  );
  is(url, sameDocumentURL, "navigatedWithinDocument returns the expected url");
  is(
    gBrowser.selectedBrowser.currentURI.spec,
    sameDocumentURL,
    "Expected URL loaded"
  );

  info("Check that navigating to the same hash URL does not timeout");
  const { frameId: sameHashFrameId, loaderId: sameHashLoaderId } =
    await Page.navigate({ url: sameDocumentURL });
  ok(
    !sameHashLoaderId,
    "Page.navigate does not return a loaderId for same document navigation"
  );
  is(sameHashFrameId, frameId, "Page.navigate returned the expected frame id");
});

async function getTopFrame(client) {
  const frames = await getFlattenedFrameTree(client);
  return Array.from(frames.values())[0];
}
