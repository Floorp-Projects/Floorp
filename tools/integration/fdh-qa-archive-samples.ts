// SPDX-License-Identifier: MPL-2.0
// Explicit historical-sample integration, NOT a browser/QA/publication execution.
// Samples are operator-downloaded separately and are never fetched by this test.
// Run from repository root: deno run --allow-read tools/integration/fdh-qa-archive-samples.ts
import { assertEquals } from "@std/assert";
import { verifyFinalPackageQaArchive } from "../src/fdh_final_package_qa_archive.ts";

const samples = [
  {
    path: "_dist/fdh/qa-zip-round3-windows.zip",
    id: 14009161864,
    name: "final-package-qa-windows-x86_64-43363023598",
    digest:
      "sha256:ddec0b93c2a6a32248109ad82ad715837404d09cfe29be74fbace4065aa4b42b",
    count: 8,
  },
  {
    path: "_dist/fdh/qa-zip-round3-14008869255.zip",
    id: 14008869255,
    name: "final-package-qa-mac-x86_64-43363023598",
    digest:
      "sha256:270b6afedc50c30fc939db8bdc8122f5209d2a3fd295f2e8528033373ca01982",
    count: 19,
  },
  {
    path: "_dist/fdh/qa-zip-round3-14008634582.zip",
    id: 14008634582,
    name: "final-package-qa-mac-aarch64-43363023598",
    digest:
      "sha256:ef69e355aec687ac81d9e6398c9a5b43e37d870e7fe81eb80235bd1756cf4297",
    count: 19,
  },
  {
    path: "_dist/fdh/qa-zip-round3-14008374789.zip",
    id: 14008374789,
    name: "final-package-qa-linux-x86_64-43363023598",
    digest:
      "sha256:9c4e8198e49c8d47c13998daebf0e2775ef1a72ee7e73ab40d45aea4096a893d",
    count: 8,
  },
];
for (const sample of samples) {
  const bytes = await Deno.readFile(sample.path);
  const result = await verifyFinalPackageQaArchive({
    bytes,
    artifact: { id: sample.id, name: sample.name },
    expected: { id: sample.id, name: sample.name, digest: sample.digest },
  });
  if (!result.archiveIntegrityMatched) {
    throw new Error(
      "Historical sample integrity rejected: " + sample.id + " code=" +
        result.code,
    );
  }
  assertEquals(result.entryCount, sample.count);
  assertEquals(result.artifact.id, sample.id);
  assertEquals(result.artifact.digest, sample.digest);
  assertEquals(result.input.runId, 43359316284);
  assertEquals(
    result.input.floorpHead,
    "9e6b24e5fd561250ab04a011b70ee0b1280b1adc",
  );
  assertEquals(result.publicationAuthorized, false);
  assertEquals(result.apiAuthenticityVerified, false);
  assertEquals(result.nativeQaVerified, false);
  assertEquals(result.liveApprovalVerified, false);
  console.log(
    JSON.stringify({
      artifactId: sample.id,
      archiveIntegrityMatched: true,
      entries: result.entryCount,
      expandedBytes: result.expandedBytes,
      publicationAuthorized: false,
    }),
  );
}
console.log(
  "PASS: four historical QA ZIP byte-integrity samples only; no native QA or live approval proof",
);
