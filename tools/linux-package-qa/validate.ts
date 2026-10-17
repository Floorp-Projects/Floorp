// SPDX-License-Identifier: MPL-2.0
import {
  canonicalJson,
  validateLinuxPackageRecord,
  validateRuntimeProvenance,
} from "../src/release_provenance.ts";

export function validateMaterials(value: unknown) {
  if (typeof value !== "object" || value === null || Array.isArray(value)) {
    throw new Error("Linux materials must be a closed object");
  }
  const input = value as Record<string, unknown>;
  if (
    Object.keys(input).sort().join(",") !==
      "floorp,manifest,normalized,package,record,snapshot"
  ) throw new Error("Linux materials are incomplete or contain unknown fields");
  const runtime = validateRuntimeProvenance(input.manifest, input.snapshot);
  if (canonicalJson(runtime) !== canonicalJson(input.normalized)) {
    throw new Error(
      "Producer normalization differs from fresh Runtime provenance",
    );
  }
  const record = validateLinuxPackageRecord(
    input.record,
    runtime,
    input.floorp,
    input.package,
  );
  return { runtime, record };
}

if (import.meta.main) {
  if (Deno.args.length !== 1) {
    throw new Error("Usage: validate.ts MATERIALS_JSON");
  }
  const material: unknown = JSON.parse(await Deno.readTextFile(Deno.args[0]));
  console.log(JSON.stringify(validateMaterials(material)));
}
