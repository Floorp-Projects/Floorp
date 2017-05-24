/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const {newURI} = Services.io;

add_task(async function test_WebExtensinonPolicy() {
  const id = "foo@bar.baz";
  const uuid = "ca9d3f23-125c-4b24-abfc-1ca2692b0610";

  const baseURL = "file:///foo/";
  const mozExtURL = `moz-extension://${uuid}/`;
  const mozExtURI = newURI(mozExtURL);

  let policy = new WebExtensionPolicy({
    id,
    mozExtensionHostname: uuid,
    baseURL,

    localizeCallback(str) {
      return `<${str}>`;
    },

    allowedOrigins: new MatchPatternSet(["http://foo.bar/", "*://*.baz/"], {ignorePath: true}),
    permissions: ["<all_urls>"],
    webAccessibleResources: ["/foo/*", "/bar.baz"].map(glob => new MatchGlob(glob)),
  });

  equal(policy.active, false, "Active attribute should initially be false");

  // GetURL

  equal(policy.getURL(), mozExtURL, "getURL() should return the correct root URL");
  equal(policy.getURL("path/foo.html"), `${mozExtURL}path/foo.html`, "getURL(path) should return the correct URL");


  // Permissions

  deepEqual(policy.permissions, ["<all_urls>"], "Initial permissions should be correct");

  ok(policy.hasPermission("<all_urls>"), "hasPermission should match existing permission");
  ok(!policy.hasPermission("history"), "hasPermission should not match nonexistent permission");

  Assert.throws(() => { policy.permissions[0] = "foo"; },
                TypeError,
                "Permissions array should be frozen");

  policy.permissions = ["history"];
  deepEqual(policy.permissions, ["history"], "Permissions should be updateable as a set");

  ok(policy.hasPermission("history"), "hasPermission should match existing permission");
  ok(!policy.hasPermission("<all_urls>"), "hasPermission should not match nonexistent permission");


  // Origins

  ok(policy.canAccessURI(newURI("http://foo.bar/quux")), "Should be able to access whitelisted URI");
  ok(policy.canAccessURI(newURI("https://x.baz/foo")), "Should be able to access whitelisted URI");

  ok(!policy.canAccessURI(newURI("https://foo.bar/quux")), "Should not be able to access non-whitelisted URI");

  policy.allowedOrigins = new MatchPatternSet(["https://foo.bar/"], {ignorePath: true});

  ok(policy.canAccessURI(newURI("https://foo.bar/quux")), "Should be able to access updated whitelisted URI");
  ok(!policy.canAccessURI(newURI("https://x.baz/foo")), "Should not be able to access removed whitelisted URI");


  // Web-accessible resources

  ok(policy.isPathWebAccessible("/foo/bar"), "Web-accessible glob should be web-accessible");
  ok(policy.isPathWebAccessible("/bar.baz"), "Web-accessible path should be web-accessible");
  ok(!policy.isPathWebAccessible("/bar.baz/quux"), "Non-web-accessible path should not be web-accessible");


  // Localization

  equal(policy.localize("foo"), "<foo>", "Localization callback should work as expected");
});
