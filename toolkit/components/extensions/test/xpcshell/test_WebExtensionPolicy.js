/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const { newURI } = Services.io;

add_task(async function test_WebExtensionPolicy() {
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

    allowedOrigins: new MatchPatternSet(["http://foo.bar/", "*://*.baz/"], {
      ignorePath: true,
    }),
    permissions: ["<all_urls>"],
    webAccessibleResources: ["/foo/*", "/bar.baz"].map(
      glob => new MatchGlob(glob)
    ),
  });

  equal(policy.active, false, "Active attribute should initially be false");

  // GetURL

  equal(
    policy.getURL(),
    mozExtURL,
    "getURL() should return the correct root URL"
  );
  equal(
    policy.getURL("path/foo.html"),
    `${mozExtURL}path/foo.html`,
    "getURL(path) should return the correct URL"
  );

  // Permissions

  deepEqual(
    policy.permissions,
    ["<all_urls>"],
    "Initial permissions should be correct"
  );

  ok(
    policy.hasPermission("<all_urls>"),
    "hasPermission should match existing permission"
  );
  ok(
    !policy.hasPermission("history"),
    "hasPermission should not match nonexistent permission"
  );

  Assert.throws(
    () => {
      policy.permissions[0] = "foo";
    },
    TypeError,
    "Permissions array should be frozen"
  );

  policy.permissions = ["history"];
  deepEqual(
    policy.permissions,
    ["history"],
    "Permissions should be updateable as a set"
  );

  ok(
    policy.hasPermission("history"),
    "hasPermission should match existing permission"
  );
  ok(
    !policy.hasPermission("<all_urls>"),
    "hasPermission should not match nonexistent permission"
  );

  // Origins

  ok(
    policy.canAccessURI(newURI("http://foo.bar/quux")),
    "Should be able to access permitted URI"
  );
  ok(
    policy.canAccessURI(newURI("https://x.baz/foo")),
    "Should be able to access permitted URI"
  );

  ok(
    !policy.canAccessURI(newURI("https://foo.bar/quux")),
    "Should not be able to access non-permitted URI"
  );

  policy.allowedOrigins = new MatchPatternSet(["https://foo.bar/"], {
    ignorePath: true,
  });

  ok(
    policy.canAccessURI(newURI("https://foo.bar/quux")),
    "Should be able to access updated permitted URI"
  );
  ok(
    !policy.canAccessURI(newURI("https://x.baz/foo")),
    "Should not be able to access removed permitted URI"
  );

  // Web-accessible resources

  ok(
    policy.isPathWebAccessible("/foo/bar"),
    "Web-accessible glob should be web-accessible"
  );
  ok(
    policy.isPathWebAccessible("/bar.baz"),
    "Web-accessible path should be web-accessible"
  );
  ok(
    !policy.isPathWebAccessible("/bar.baz/quux"),
    "Non-web-accessible path should not be web-accessible"
  );

  // Localization

  equal(
    policy.localize("foo"),
    "<foo>",
    "Localization callback should work as expected"
  );

  // Protocol and lookups.

  let proto = Services.io
    .getProtocolHandler("moz-extension", uuid)
    .QueryInterface(Ci.nsISubstitutingProtocolHandler);

  deepEqual(
    WebExtensionPolicy.getActiveExtensions(),
    [],
    "Should have no active extensions"
  );
  equal(
    WebExtensionPolicy.getByID(id),
    null,
    "ID lookup should not return extension when not active"
  );
  equal(
    WebExtensionPolicy.getByHostname(uuid),
    null,
    "Hostname lookup should not return extension when not active"
  );
  Assert.throws(
    () => proto.resolveURI(mozExtURI),
    /NS_ERROR_NOT_AVAILABLE/,
    "URL should not resolve when not active"
  );

  policy.active = true;
  equal(policy.active, true, "Active attribute should be updated");

  let exts = WebExtensionPolicy.getActiveExtensions();
  equal(exts.length, 1, "Should have one active extension");
  equal(exts[0], policy, "Should have the correct active extension");

  equal(
    WebExtensionPolicy.getByID(id),
    policy,
    "ID lookup should return extension when active"
  );
  equal(
    WebExtensionPolicy.getByHostname(uuid),
    policy,
    "Hostname lookup should return extension when active"
  );

  equal(
    proto.resolveURI(mozExtURI),
    baseURL,
    "URL should resolve correctly while active"
  );

  policy.active = false;
  equal(policy.active, false, "Active attribute should be updated");

  deepEqual(
    WebExtensionPolicy.getActiveExtensions(),
    [],
    "Should have no active extensions"
  );
  equal(
    WebExtensionPolicy.getByID(id),
    null,
    "ID lookup should not return extension when not active"
  );
  equal(
    WebExtensionPolicy.getByHostname(uuid),
    null,
    "Hostname lookup should not return extension when not active"
  );
  Assert.throws(
    () => proto.resolveURI(mozExtURI),
    /NS_ERROR_NOT_AVAILABLE/,
    "URL should not resolve when not active"
  );

  // Conflicting policies.

  // This asserts in debug builds, so only test in non-debug builds.
  if (!AppConstants.DEBUG) {
    policy.active = true;

    let attrs = [
      { id, uuid },
      { id, uuid: "d916886c-cfdf-482e-b7b1-d7f5b0facfa5" },
      { id: "foo@quux", uuid },
    ];

    // eslint-disable-next-line no-shadow
    for (let { id, uuid } of attrs) {
      let policy2 = new WebExtensionPolicy({
        id,
        mozExtensionHostname: uuid,
        baseURL: "file://bar/",

        localizeCallback() {},

        allowedOrigins: new MatchPatternSet([]),
      });

      Assert.throws(
        () => {
          policy2.active = true;
        },
        /NS_ERROR_UNEXPECTED/,
        `Should not be able to activate conflicting policy: ${id} ${uuid}`
      );
    }

    policy.active = false;
  }
});

add_task(async function test_WebExtensionPolicy_registerContentScripts() {
  const id = "foo@bar.baz";
  const uuid = "77a7b9d3-e73c-4cf3-97fb-1824868fe00f";

  const id2 = "foo-2@bar.baz";
  const uuid2 = "89383c45-7db4-4999-83f7-f4cc246372cd";

  const baseURL = "file:///foo/";

  const mozExtURL = `moz-extension://${uuid}/`;
  const mozExtURL2 = `moz-extension://${uuid2}/`;

  let policy = new WebExtensionPolicy({
    id,
    mozExtensionHostname: uuid,
    baseURL,
    localizeCallback() {},
    allowedOrigins: new MatchPatternSet([]),
    permissions: ["<all_urls>"],
  });

  let policy2 = new WebExtensionPolicy({
    id: id2,
    mozExtensionHostname: uuid2,
    baseURL,
    localizeCallback() {},
    allowedOrigins: new MatchPatternSet([]),
    permissions: ["<all_urls>"],
  });

  let script1 = new WebExtensionContentScript(policy, {
    run_at: "document_end",
    js: [`${mozExtURL}/registered-content-script.js`],
    matches: new MatchPatternSet(["http://localhost/data/*"]),
  });

  let script2 = new WebExtensionContentScript(policy, {
    run_at: "document_end",
    css: [`${mozExtURL}/registered-content-style.css`],
    matches: new MatchPatternSet(["http://localhost/data/*"]),
  });

  let script3 = new WebExtensionContentScript(policy2, {
    run_at: "document_end",
    css: [`${mozExtURL2}/registered-content-style.css`],
    matches: new MatchPatternSet(["http://localhost/data/*"]),
  });

  deepEqual(
    policy.contentScripts,
    [],
    "The policy contentScripts is initially empty"
  );

  policy.registerContentScript(script1);

  deepEqual(
    policy.contentScripts,
    [script1],
    "script1 has been added to the policy contentScripts"
  );

  Assert.throws(
    () => policy.registerContentScript(script1),
    e => e.result == Cr.NS_ERROR_ILLEGAL_VALUE,
    "Got the expected NS_ERROR_ILLEGAL_VALUE when trying to register a script more than once"
  );

  Assert.throws(
    () => policy.registerContentScript(script3),
    e => e.result == Cr.NS_ERROR_ILLEGAL_VALUE,
    "Got the expected NS_ERROR_ILLEGAL_VALUE when trying to register a script related to " +
      "a different extension"
  );

  Assert.throws(
    () => policy.unregisterContentScript(script3),
    e => e.result == Cr.NS_ERROR_ILLEGAL_VALUE,
    "Got the expected NS_ERROR_ILLEGAL_VALUE when trying to unregister a script related to " +
      "a different extension"
  );

  deepEqual(
    policy.contentScripts,
    [script1],
    "script1 has not been added twice"
  );

  policy.registerContentScript(script2);

  deepEqual(
    policy.contentScripts,
    [script1, script2],
    "script2 has the last item of the policy contentScripts array"
  );

  policy.unregisterContentScript(script1);

  deepEqual(
    policy.contentScripts,
    [script2],
    "script1 has been removed from the policy contentscripts"
  );

  Assert.throws(
    () => policy.unregisterContentScript(script1),
    e => e.result == Cr.NS_ERROR_ILLEGAL_VALUE,
    "Got the expected NS_ERROR_ILLEGAL_VALUE when trying to unregister a script more than once"
  );

  deepEqual(
    policy.contentScripts,
    [script2],
    "the policy contentscripts is unmodified when unregistering an unknown contentScript"
  );

  policy.unregisterContentScript(script2);

  deepEqual(
    policy.contentScripts,
    [],
    "script2 has been removed from the policy contentScripts"
  );
});
