/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
"use strict";

// This verifies that AddonUpdateChecker works correctly for JSON
// update manifests, particularly for behavior which does not
// cleanly overlap with RDF manifests.

const TOOLKIT_ID = "toolkit@mozilla.org";
const TOOLKIT_MINVERSION = "42.0a1";

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "42.0a2", "42.0a2");

const { AddonUpdateChecker } = ChromeUtils.importESModule(
  "resource://gre/modules/addons/AddonUpdateChecker.sys.mjs"
);

let testserver = createHttpServer();
gPort = testserver.identity.primaryPort;

let gUpdateManifests = {};

function mapManifest(aPath, aManifestData) {
  gUpdateManifests[aPath] = aManifestData;
  testserver.registerPathHandler(aPath, serveManifest);
}

function serveManifest(request, response) {
  let manifest = gUpdateManifests[request.path];

  response.setHeader("Content-Type", manifest.contentType, false);
  response.write(manifest.data);
}

const extensionsDir = gProfD.clone();
extensionsDir.append("extensions");

function checkUpdates(aData) {
  // Registers JSON update manifest for it with the testing server,
  // checks for updates, and yields the list of updates on
  // success.

  let extension = aData.manifestExtension || "json";

  let path = `/updates/${aData.id}.${extension}`;
  let updateUrl = `http://localhost:${gPort}${path}`;

  let addonData = {};
  if ("updates" in aData) {
    addonData.updates = aData.updates;
  }

  let manifestJSON = {
    addons: {
      [aData.id]: addonData,
    },
  };

  mapManifest(path.replace(/\?.*/, ""), {
    data: JSON.stringify(manifestJSON),
    contentType: aData.contentType || "application/json",
  });

  return new Promise((resolve, reject) => {
    AddonUpdateChecker.checkForUpdates(aData.id, updateUrl, {
      onUpdateCheckComplete: resolve,

      onUpdateCheckError(status) {
        reject(new Error("Update check failed with status " + status));
      },
    });
  });
}

add_task(async function test_default_values() {
  // Checks that the appropriate defaults are used for omitted values.

  await promiseStartupManager();

  let updates = await checkUpdates({
    id: "updatecheck-defaults@tests.mozilla.org",
    version: "0.1",
    updates: [
      {
        version: "0.2",
      },
    ],
  });

  equal(updates.length, 1);
  let update = updates[0];

  equal(update.targetApplications.length, 2);
  let targetApp = update.targetApplications[0];

  equal(targetApp.id, TOOLKIT_ID);
  equal(targetApp.minVersion, TOOLKIT_MINVERSION);
  equal(targetApp.maxVersion, "*");

  equal(update.version, "0.2");
  equal(update.strictCompatibility, false, "inferred strictConpatibility flag");
  equal(update.updateURL, null, "updateURL");
  equal(update.updateHash, null, "updateHash");
  equal(update.updateInfoURL, null, "updateInfoURL");

  // If there's no applications property, we default to using one
  // containing "gecko". If there is an applications property, but
  // it doesn't contain "gecko", the update is skipped.
  updates = await checkUpdates({
    id: "updatecheck-defaults@tests.mozilla.org",
    version: "0.1",
    updates: [
      {
        version: "0.2",
        applications: { foo: {} },
      },
    ],
  });

  equal(updates.length, 0);

  // Updates property is also optional. No updates, but also no error.
  updates = await checkUpdates({
    id: "updatecheck-defaults@tests.mozilla.org",
    version: "0.1",
  });

  equal(updates.length, 0);
});

add_task(async function test_explicit_values() {
  // Checks that the appropriate explicit values are used when
  // provided.

  let updates = await checkUpdates({
    id: "updatecheck-explicit@tests.mozilla.org",
    version: "0.1",
    updates: [
      {
        version: "0.2",
        update_link: "https://example.com/foo.xpi",
        update_hash: "sha256:0",
        update_info_url: "https://example.com/update_info.html",
        applications: {
          gecko: {
            strict_min_version: "42.0a2.xpcshell",
            strict_max_version: "43.xpcshell",
          },
        },
      },
    ],
  });

  equal(updates.length, 1);
  let update = updates[0];

  equal(update.targetApplications.length, 2);
  let targetApp = update.targetApplications[0];

  equal(targetApp.id, TOOLKIT_ID);
  equal(targetApp.minVersion, "42.0a2.xpcshell");
  equal(targetApp.maxVersion, "43.xpcshell");

  equal(update.version, "0.2");
  equal(update.strictCompatibility, true, "inferred strictCompatibility flag");
  equal(update.updateURL, "https://example.com/foo.xpi", "updateURL");
  equal(update.updateHash, "sha256:0", "updateHash");
  equal(
    update.updateInfoURL,
    "https://example.com/update_info.html",
    "updateInfoURL"
  );
});

add_task(async function test_secure_hashes() {
  // Checks that only secure hash functions are accepted for
  // non-secure update URLs.

  let hashFunctions = ["sha512", "sha256", "sha1", "md5", "md4", "xxx"];

  let updateItems = hashFunctions.map((hash, idx) => ({
    version: `0.${idx}`,
    update_link: `http://localhost:${gPort}/updates/${idx}-${hash}.xpi`,
    update_hash: `${hash}:08ac852190ecd81f40a514ea9299fe9143d9ab5e296b97e73fb2a314de49648a`,
  }));

  let { messages, result: updates } = await promiseConsoleOutput(() => {
    return checkUpdates({
      id: "updatecheck-hashes@tests.mozilla.org",
      version: "0.1",
      updates: updateItems,
    });
  });

  equal(updates.length, hashFunctions.length);

  updates = updates.filter(update => update.updateHash || update.updateURL);
  equal(updates.length, 2, "expected number of update hashes were accepted");

  ok(updates[0].updateHash.startsWith("sha512:"), "sha512 hash is present");
  ok(updates[0].updateURL);

  ok(updates[1].updateHash.startsWith("sha256:"), "sha256 hash is present");
  ok(updates[1].updateURL);

  messages = messages.filter(msg =>
    /Update link.*not secure.*strong enough hash \(needs to be sha256 or sha512\)/.test(
      msg.message
    )
  );
  equal(
    messages.length,
    hashFunctions.length - 2,
    "insecure hashes generated the expected warning"
  );
});

add_task(async function test_strict_compat() {
  // Checks that strict compatibility is enabled for strict max
  // versions other than "*", but not for advisory max versions.
  // Also, ensure that strict max versions take precedence over
  // advisory versions.

  let { messages, result: updates } = await promiseConsoleOutput(() => {
    return checkUpdates({
      id: "updatecheck-strict@tests.mozilla.org",
      version: "0.1",
      updates: [
        {
          version: "0.2",
          applications: { gecko: { strict_max_version: "*" } },
        },
        {
          version: "0.3",
          applications: { gecko: { strict_max_version: "43" } },
        },
        {
          version: "0.4",
          applications: { gecko: { advisory_max_version: "43" } },
        },
        {
          version: "0.5",
          applications: {
            gecko: { advisory_max_version: "43", strict_max_version: "44" },
          },
        },
      ],
    });
  });

  equal(updates.length, 4, "all update items accepted");

  equal(updates[0].targetApplications[0].maxVersion, "*");
  equal(updates[0].strictCompatibility, false);

  equal(updates[1].targetApplications[0].maxVersion, "43");
  equal(updates[1].strictCompatibility, true);

  equal(updates[2].targetApplications[0].maxVersion, "43");
  equal(updates[2].strictCompatibility, false);

  equal(updates[3].targetApplications[0].maxVersion, "44");
  equal(updates[3].strictCompatibility, true);

  messages = messages.filter(msg =>
    /Ignoring 'advisory_max_version'.*'strict_max_version' also present/.test(
      msg.message
    )
  );
  equal(
    messages.length,
    1,
    "mix of advisory_max_version and strict_max_version generated the expected warning"
  );
});

add_task(async function test_update_url_security() {
  // Checks that update links to privileged URLs are not accepted.

  let { messages, result: updates } = await promiseConsoleOutput(() => {
    return checkUpdates({
      id: "updatecheck-security@tests.mozilla.org",
      version: "0.1",
      updates: [
        {
          version: "0.2",
          update_link: "chrome://browser/content/browser.xhtml",
          update_hash:
            "sha256:08ac852190ecd81f40a514ea9299fe9143d9ab5e296b97e73fb2a314de49648a",
        },
        {
          version: "0.3",
          update_link: "http://example.com/update.xpi",
          update_hash:
            "sha256:18ac852190ecd81f40a514ea9299fe9143d9ab5e296b97e73fb2a314de49648a",
        },
      ],
    });
  });

  equal(updates.length, 2, "both updates were processed");
  equal(updates[0].updateURL, null, "privileged update URL was removed");
  equal(
    updates[1].updateURL,
    "http://example.com/update.xpi",
    "safe update URL was accepted"
  );

  messages = messages.filter(msg =>
    /http:\/\/localhost.*\/updates\/.*may not load or link to chrome:/.test(
      msg.message
    )
  );
  equal(
    messages.length,
    1,
    "privileged update URL generated the expected console message"
  );
});

add_task(async function test_type_detection() {
  // Checks that JSON update manifests are detected correctly
  // regardless of extension or MIME type.

  let tests = [
    { contentType: "application/json", extension: "json", valid: true },
    { contentType: "application/json", extension: "php", valid: true },
    { contentType: "text/plain", extension: "json", valid: true },
    { contentType: "application/octet-stream", extension: "json", valid: true },
    { contentType: "text/plain", extension: "json?foo=bar", valid: true },
    { contentType: "text/plain", extension: "php", valid: true },
    { contentType: "text/plain", extension: "rdf", valid: true },
    { contentType: "application/json", extension: "rdf", valid: true },
    { contentType: "text/xml", extension: "json", valid: true },
    { contentType: "application/rdf+xml", extension: "json", valid: true },
  ];

  for (let [i, test] of tests.entries()) {
    let { messages } = await promiseConsoleOutput(async function () {
      let id = `updatecheck-typedetection-${i}@tests.mozilla.org`;
      let updates;
      try {
        updates = await checkUpdates({
          id,
          version: "0.1",
          contentType: test.contentType,
          manifestExtension: test.extension,
          updates: [{ version: "0.2" }],
        });
      } catch (e) {
        ok(!test.valid, "update manifest correctly detected as RDF");
        return;
      }

      ok(test.valid, "update manifest correctly detected as JSON");
      equal(updates.length, 1, "correct number of updates");
      equal(updates[0].id, id, "update is for correct extension");
    });

    if (test.valid) {
      // Make sure we don't get any XML parsing errors from the
      // XMLHttpRequest machinery.
      ok(
        !messages.some(msg => /not well-formed/.test(msg.message)),
        "expect XMLHttpRequest not to attempt XML parsing"
      );
    }

    messages = messages.filter(msg =>
      /Update manifest was not valid XML/.test(msg.message)
    );
    equal(
      messages.length,
      !test.valid,
      "expected number of XML parsing errors"
    );
  }
});

add_task(async function test_empty_manifest() {
  function checkUpdatesForUnlistedAddon(aData) {
    // Registers an empty JSON update manifest with the test server to simulate
    // the update server's actual response in the case of an unlisted add-on.

    let path = `/updates/${aData.id}.json`;
    let updateUrl = `http://localhost:${gPort}${path}`;

    let manifestJSON = {};

    mapManifest(path.replace(/\?.*/, ""), {
      data: JSON.stringify(manifestJSON),
      contentType: "application/json",
    });

    return new Promise((resolve, reject) => {
      AddonUpdateChecker.checkForUpdates(aData.id, updateUrl, {
        onUpdateCheckComplete: resolve,

        onUpdateCheckError(status) {
          reject(new Error("Update check failed with status " + status));
        },
      });
    });
  }

  let { messages, result: updates } = await promiseConsoleOutput(() => {
    return checkUpdatesForUnlistedAddon({
      id: "unlisted@example.org",
    });
  });

  equal(updates.length, 0, "no update could be found");

  messages = messages.filter(msg =>
    /Received empty update manifest for .*/.test(msg.message)
  );
  equal(
    messages.length,
    1,
    "unlisted addon generated the expected console message"
  );
});
