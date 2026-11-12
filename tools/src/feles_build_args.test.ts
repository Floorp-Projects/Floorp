// SPDX-License-Identifier: MPL-2.0

import { assertEquals, assertThrows } from "@std/assert";
import { parseFelesBuildGlobalArgs } from "./feles_build_args.ts";

Deno.test("parses --artifact before the command", () => {
  assertEquals(
    parseFelesBuildGlobalArgs(["--artifact", "/tmp/runtime.zip", "dev"]),
    {
      artifactPath: "/tmp/runtime.zip",
      argv: ["dev"],
    },
  );
});

Deno.test("parses --artifact=<path>", () => {
  assertEquals(
    parseFelesBuildGlobalArgs(["--artifact=/tmp/runtime.dmg", "dev"]),
    {
      artifactPath: "/tmp/runtime.dmg",
      argv: ["dev"],
    },
  );
});

Deno.test("preserves other arguments in order", () => {
  assertEquals(
    parseFelesBuildGlobalArgs([
      "--artifact",
      "/tmp/runtime.tar.xz",
      "dev",
      "--port-offset",
      "auto",
      "extra",
    ]),
    {
      artifactPath: "/tmp/runtime.tar.xz",
      argv: ["dev", "--port-offset", "auto", "extra"],
    },
  );
});

Deno.test("does not mutate the input array", () => {
  const input = ["--artifact", "/tmp/runtime.zip", "dev"];
  const copy = [...input];
  parseFelesBuildGlobalArgs(input);
  assertEquals(input, copy);
});

Deno.test("throws when --artifact has no value", () => {
  assertThrows(
    () => parseFelesBuildGlobalArgs(["--artifact"]),
    Error,
    "--artifact requires a non-empty path value",
  );
});

Deno.test("throws when --artifact value is another option", () => {
  assertThrows(
    () => parseFelesBuildGlobalArgs(["--artifact", "--port-offset", "dev"]),
    Error,
    "--artifact requires a non-empty path value",
  );
});

Deno.test("throws when the spaced --artifact value starts with a single dash", () => {
  assertThrows(
    () => parseFelesBuildGlobalArgs(["--artifact", "-port", "dev"]),
    Error,
    "--artifact requires a non-empty path value",
  );
});

Deno.test("throws when --artifact is duplicated", () => {
  assertThrows(
    () =>
      parseFelesBuildGlobalArgs([
        "--artifact",
        "/tmp/one.zip",
        "--artifact=/tmp/two.zip",
        "dev",
      ]),
    Error,
    "--artifact may be specified only once",
  );
});

Deno.test("throws when --artifact value is empty", () => {
  assertThrows(
    () => parseFelesBuildGlobalArgs(["--artifact=", "dev"]),
    Error,
    "--artifact requires a non-empty path value",
  );
  assertThrows(
    () => parseFelesBuildGlobalArgs(["--artifact", "", "dev"]),
    Error,
    "--artifact requires a non-empty path value",
  );
});
