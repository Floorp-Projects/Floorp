/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const pageEmptyURL =
  "http://example.com/browser/remote/test/browser/page/doc_empty.html";

add_task(async function testBasicNavigation({ client }) {
  const { Page } = client;
  await Page.enable();
  const loadEventFired = Page.loadEventFired();
  const { frameId, loaderId, errorText } = await Page.navigate({
    url: pageEmptyURL,
  });

  await compareFrame(frameId);
  todo(!!loaderId, "Page.navigate returns loaderId");
  is(errorText, undefined, "No errorText on a successful navigation");

  await loadEventFired;
  is(
    gBrowser.selectedBrowser.currentURI.spec,
    pageEmptyURL,
    "Expected URL loaded"
  );
});

add_task(async function testTwoNavigations({ client }) {
  const { Page } = client;
  await Page.enable();
  let loadEventFired = Page.loadEventFired();
  const { frameId, loaderId, errorText } = await Page.navigate({
    url: pageEmptyURL,
  });
  await loadEventFired;
  is(
    gBrowser.selectedBrowser.currentURI.spec,
    pageEmptyURL,
    "Expected URL loaded"
  );

  loadEventFired = Page.loadEventFired();
  const {
    frameId: frameId2,
    loaderId: loaderId2,
    errorText: errorText2,
  } = await Page.navigate({
    url: pageEmptyURL,
  });
  todo(loaderId !== loaderId2, "Page.navigate returns different loaderIds");
  is(errorText, undefined, "No errorText on a successful navigation");
  is(errorText2, undefined, "No errorText on a successful navigation");
  is(frameId, frameId2, "Page.navigate return same frameId");

  await loadEventFired;
  is(
    gBrowser.selectedBrowser.currentURI.spec,
    pageEmptyURL,
    "Expected URL loaded"
  );
});

add_task(async function testRedirect({ client }) {
  const { Page } = client;
  const sjsURL =
    "http://example.com/browser/remote/test/browser/page/sjs_redirect.sjs";
  const redirectURL = `${sjsURL}?${pageEmptyURL}`;
  await Page.enable();
  const loadEventFired = Page.loadEventFired();
  const { frameId, loaderId, errorText } = await Page.navigate({
    url: redirectURL,
  });
  todo(!!loaderId, "Page.navigate returns loaderId");
  is(errorText, undefined, "No errorText on a successful navigation");
  ok(!!frameId, "Page.navigate returns frameId");

  await loadEventFired;
  is(
    gBrowser.selectedBrowser.currentURI.spec,
    pageEmptyURL,
    "Expected URL loaded"
  );
});

add_task(async function testUnknownHost({ client }) {
  const { Page } = client;
  const { frameId, loaderId, errorText } = await Page.navigate({
    url: "http://example-does-not-exist.com",
  });
  ok(!!frameId, "Page.navigate returns frameId");
  todo(!!loaderId, "Page.navigate returns loaderId");
  is(errorText, "NS_ERROR_UNKNOWN_HOST", "Failed navigation returns errorText");
});

add_task(async function testExpiredCertificate({ client }) {
  const { Page } = client;
  const { frameId, loaderId, errorText } = await Page.navigate({
    url: "https://expired.example.com",
  });
  ok(!!frameId, "Page.navigate returns frameId");
  todo(!!loaderId, "Page.navigate returns loaderId");
  is(
    errorText,
    "SEC_ERROR_EXPIRED_CERTIFICATE",
    "Failed navigation returns errorText"
  );
});

add_task(async function testUnknownCertificate({ client }) {
  const { Page } = client;
  const { frameId, loaderId, errorText } = await Page.navigate({
    url: "https://self-signed.example.com",
  });
  ok(!!frameId, "Page.navigate returns frameId");
  todo(!!loaderId, "Page.navigate returns loaderId");
  is(errorText, "SSL_ERROR_UNKNOWN", "Failed navigation returns errorText");
});

add_task(async function testNotFound({ client }) {
  const { Page } = client;
  const { frameId, loaderId, errorText } = await Page.navigate({
    url: "http://example.com/browser/remote/doesnotexist.html",
  });
  ok(!!frameId, "Page.navigate returns frameId");
  todo(!!loaderId, "Page.navigate returns loaderId");
  is(errorText, undefined, "No errorText on a 404");
});

add_task(async function testInvalidURL({ client }) {
  const { Page } = client;
  let message = "";
  for (let url of ["blah.com", "foo", "https\n//", "http", ""]) {
    message = "";
    try {
      await Page.navigate({ url });
    } catch (e) {
      message = e.response.message;
    }
    ok(message.includes("invalid URL"), `Invalid url ${url} causes error`);
  }

  for (let url of [2, {}, true]) {
    message = "";
    try {
      await Page.navigate({ url });
    } catch (e) {
      message = e.response.message;
    }
    ok(
      message.includes("string value expected"),
      `Invalid url ${url} causes error`
    );
  }
});

add_task(async function testDataURL({ client }) {
  const { Page } = client;
  const url = toDataURL("first");
  await Page.enable();
  const loadEventFired = Page.loadEventFired();
  const { frameId, loaderId, errorText } = await Page.navigate({ url });
  await compareFrame(frameId);
  is(errorText, undefined, "No errorText on a successful navigation");
  todo(!!loaderId, "Page.navigate returns loaderId");

  await loadEventFired;
  is(gBrowser.selectedBrowser.currentURI.spec, url, "Expected URL loaded");
});

add_task(async function testAbout({ client }) {
  const { Page } = client;
  await Page.enable();
  let loadEventFired = Page.loadEventFired();
  const { frameId, loaderId, errorText } = await Page.navigate({
    url: "about:blank",
  });
  todo(!!loaderId, "Page.navigate returns loaderId");
  is(errorText, undefined, "No errorText on a successful navigation");
  await compareFrame(frameId);

  await loadEventFired;
  is(
    gBrowser.selectedBrowser.currentURI.spec,
    "about:blank",
    "Expected URL loaded"
  );
});

async function compareFrame(frameId) {
  const frames = await getFlattendFrameList();
  const currentFrame = Array.from(frames.values())[0];
  is(frameId, currentFrame.id, "Page.navigate returns expected frameId");
}
