// SPDX-License-Identifier: MPL-2.0
// Explicit read-only gh integration; no archive downloads, browser or writes.
// Host-installed gh/auth/config/TLS is TCB; this is not API-origin attestation.
// Run: deno run --cached-only --frozen --allow-run=gh tools/integration/fdh-qa-transport-live.ts
import { assert, assertEquals } from "@std/assert";
import { createQaGhTransport } from "../src/fdh_qa_gh_transport.ts";
const transport = createQaGhTransport();
const repository = await transport.request({ kind: "repository" });
assert(
  repository.ok && repository.kind === "metadata",
  "canonical repository read failed",
);
const parsed: unknown = JSON.parse(repository.jsonText);
assert(
  typeof parsed === "object" && parsed !== null && "id" in parsed &&
    "full_name" in parsed,
);
assertEquals(parsed.id, 437505831);
assertEquals(parsed.full_name, "Floorp-Projects/Floorp");
assertEquals(repository.publicationAuthorized, false);
console.log(
  JSON.stringify({
    operation: "repository",
    canonicalBodyMatched: true,
    publicationAuthorized: false,
  }),
);
const missing = await transport.request({
  kind: "releaseTag",
  version: "999999.0.0",
});
assert(
  !missing.ok,
  "reserved nonexistent release probe unexpectedly succeeded",
);
assertEquals(missing.code, "http");
assertEquals(missing.statusClass, "notFound");
console.log(
  JSON.stringify({
    operation: "releaseTag",
    expectedMissing: true,
    code: missing.code,
    statusClass: missing.statusClass,
  }),
);
console.log(
  "PASS: canonical read-only metadata and classified 404; no live approval or publication proof",
);
