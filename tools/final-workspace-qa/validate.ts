// SPDX-License-Identifier: MPL-2.0

import { validateReleaseBundle } from "../src/release_provenance.ts";
import type { PilotRequest, ProductSubject } from "./types.ts";

export function parseRequest(value: unknown): PilotRequest {
  const keys = [
    "schema",
    "controllerHead",
    "executionRun",
    "executionAttempt",
    "floorpHead",
    "bundleRun",
    "bundleAttempt",
    "bundleArtifact",
    "runtimeHead",
    "runtimeRun",
    "runtimeAttempt",
    "target",
  ];
  if (typeof value !== "object" || value === null || Array.isArray(value)) {
    throw new Error("Request must be an object");
  }
  const r = value as Record<string, unknown>;
  if (JSON.stringify(Object.keys(r).sort()) !== JSON.stringify(keys.sort())) {
    throw new Error("Request is closed and complete");
  }
  if (
    r.schema !== "qa3-final-workspace-request-v1" || r.target !== "linux-x86_64"
  ) throw new Error("Wrong request schema or native target");
  for (const key of ["controllerHead", "floorpHead", "runtimeHead"]) {
    if (typeof r[key] !== "string" || !/^[0-9a-f]{40}$/.test(r[key])) {
      throw new Error(`Invalid ${key}`);
    }
  }
  for (const key of ["executionRun", "bundleRun", "runtimeRun"]) {
    if (!Number.isSafeInteger(r[key]) || (r[key] as number) < 1) {
      throw new Error(`Invalid ${key}`);
    }
  }
  for (const key of ["executionAttempt", "bundleAttempt", "runtimeAttempt"]) {
    if (r[key] !== 1) {
      throw new Error(
        "Pilot preserves the original attempt; reruns are rejected",
      );
    }
  }
  if (
    typeof r.bundleArtifact !== "object" || r.bundleArtifact === null ||
    Array.isArray(r.bundleArtifact)
  ) throw new Error("Bundle artifact identity is missing");
  const a = r.bundleArtifact as Record<string, unknown>;
  if (
    JSON.stringify(Object.keys(a).sort()) !==
      JSON.stringify(["digest", "id", "name"]) ||
    !Number.isSafeInteger(a.id) || (a.id as number) < 1 ||
    typeof a.name !== "string" || !/^[a-zA-Z0-9._-]{1,128}$/.test(a.name) ||
    typeof a.digest !== "string" || !/^sha256:[0-9a-f]{64}$/.test(a.digest)
  ) throw new Error("Invalid exact bundle artifact identity");
  return r as unknown as PilotRequest;
}

export async function validateProduct(
  bundleDirectory: string,
  request: PilotRequest,
): Promise<ProductSubject> {
  let total = 0;
  for await (const entry of Deno.readDir(bundleDirectory)) {
    const info = await Deno.lstat(`${bundleDirectory}/${entry.name}`);
    if (!info.isFile || info.isSymlink || info.size > 1024 ** 3) {
      throw new Error("Bundle member type or memory bound rejected");
    }
    total += info.size;
    if (total > 4 * 1024 ** 3) {
      throw new Error("Bundle exceeds total byte bound");
    }
  }
  const manifest = await validateReleaseBundle(bundleDirectory, true);
  if (
    manifest.floorp.head_sha !== request.floorpHead ||
    manifest.floorp.workflow_run_id !== request.bundleRun ||
    manifest.runtime.head_sha !== request.runtimeHead ||
    manifest.runtime.workflow_run_id !== request.runtimeRun ||
    manifest.runtime.run_attempt !== request.runtimeAttempt
  ) {
    throw new Error(
      "Final product source/run/attempt differs from trusted expectations",
    );
  }
  const target = manifest.targets.find((value) => value.target_key === "linux");
  if (
    !target || target.native_verifications.length !== 1 ||
    target.native_verifications[0].native_arch !== "x86_64"
  ) throw new Error("Native Linux x64 verification missing");
  const native = target.native_verifications[0];
  return {
    request,
    manifestSetId: manifest.manifest_set_id,
    member: manifest.release_files.linux,
    expectedAppBuildID: native.app_build_id,
    expectedPlatformBuildID: native.platform_build_id,
    expectedBuildID2: native.build_id2,
    expectedFirefoxVersion: manifest.version,
    compileProfileQualification: "UNVERIFIED",
    publicationAuthorized: false,
  };
}

if (import.meta.main) {
  if (Deno.args.length !== 2) {
    throw new Error("Usage: validate.ts REQUEST_JSON BUNDLE_DIRECTORY");
  }
  const request = parseRequest(
    JSON.parse(await Deno.readTextFile(Deno.args[0])),
  );
  console.log(JSON.stringify(await validateProduct(Deno.args[1], request)));
}
