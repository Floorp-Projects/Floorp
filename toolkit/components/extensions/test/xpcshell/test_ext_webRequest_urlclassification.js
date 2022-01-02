"use strict";

const { Schemas } = ChromeUtils.import("resource://gre/modules/Schemas.jsm");

/**
 * If this test fails, likely nsIClassifiedChannel has added or changed a
 * CLASSIFIED_* flag.  Those changes must be in sync with
 * ChannelWrapper.webidl/cpp and the web_request.json schema file.
 */
add_task(async function test_webrequest_url_classification_enum() {
  // use normalizeManifest to get the schema loaded.
  await ExtensionTestUtils.normalizeManifest({ permissions: ["webRequest"] });

  let ns = Schemas.getNamespace("webRequest");
  let schema_enum = ns.get("UrlClassificationFlags").enumeration;
  ok(
    !!schema_enum.length,
    `UrlClassificationFlags: ${JSON.stringify(schema_enum)}`
  );

  let prefix = /^(?:CLASSIFIED_)/;
  let entries = 0;
  for (let c of Object.keys(Ci.nsIClassifiedChannel).filter(name =>
    prefix.test(name)
  )) {
    let entry = c.replace(prefix, "").toLowerCase();
    if (!entry.startsWith("socialtracking")) {
      ok(schema_enum.includes(entry), `schema ${entry} is in IDL`);
      entries++;
    }
  }
  equal(schema_enum.length, entries, "same number of entries");
});
