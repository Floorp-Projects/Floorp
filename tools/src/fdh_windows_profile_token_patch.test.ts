// SPDX-License-Identifier: MPL-2.0
// Offline source-parity checks, not proof of a compiled Gecko Runtime.
import { assertEquals, assertStringIncludes } from "@std/assert";
const root = new URL("../../", import.meta.url);
const read = (path: string) => Deno.readTextFile(new URL(path, root));
const patchPath =
  ".github/patches/floorp-runtime/common/fdh-windows-private-profile-token.patch";

Deno.test("Windows native Runtime patch embeds the exact tested core and bridge sources", async () => {
  const patch = (await read(patchPath)).replaceAll("\r\n", "\n");
  const names = [
    "PrivateProfileToken.h",
    "PrivateProfileToken.cpp",
    "runtime/nsIFloorpPrivateProfileToken.idl",
    "runtime/FloorpPrivateProfileTokenService.h",
    "runtime/FloorpPrivateProfileTokenService.cpp",
    "runtime/FloorpPrivateProfileTokenComponents.conf",
  ];
  for (const name of names) {
    const target = "xpcom/io/" + name.split("/").at(-1);
    const start = patch.indexOf("+++ b/" + target + "\n");
    assertEquals(start >= 0, true, target);
    const section = patch.slice(start).split("\ndiff --git ", 1)[0];
    const content = section.split("\n").filter((line) =>
      line.startsWith("+") && !line.startsWith("+++")
    )
      .map((line) =>
        line.slice(1)
      ).join("\n") + "\n";
    assertEquals(
      content,
      (await read("native/windows-profile-token/" + name)).replaceAll(
        "\r\n",
        "\n",
      ),
      name,
    );
  }
  assertStringIncludes(patch, 'if CONFIG["MOZ_WIDGET_TOOLKIT"] == "windows":');
  assertStringIncludes(patch, '+    SOURCES += ["PrivateProfileToken.cpp"]');
  assertStringIncludes(
    patch,
    '+    XPIDL_SOURCES += ["nsIFloorpPrivateProfileToken.idl"]',
  );
  assertStringIncludes(
    patch,
    '+    XPCOM_MANIFESTS += ["FloorpPrivateProfileTokenComponents.conf"]',
  );
  assertStringIncludes(
    patch,
    "Requires freshly compiled Windows Runtime artifacts",
  );
});
Deno.test("native bridge derives ProfD only after policy checks; cleanup never resolves another profile", async () => {
  const bridge = await read(
    "native/windows-profile-token/runtime/FloorpPrivateProfileTokenService.cpp",
  );
  const prepare = bridge.slice(
    bridge.indexOf("PrivateProfileTokenService::Prepare("),
    bridge.indexOf("PrivateProfileTokenService::Write("),
  );
  assertEquals(
    prepare.indexOf("if (!Allowed())") <
      prepare.indexOf("NS_GetSpecialDirectory"),
    true,
  );
  assertStringIncludes(prepare, "NS_APP_USER_PROFILE_50_DIR");
  assertStringIncludes(prepare, "path.Equals(aExpectedProfileRoot)");
  assertStringIncludes(
    bridge,
    'mode.EqualsLiteral("dev") || mode.EqualsLiteral("test")',
  );
  const cleanup = bridge.slice(
    bridge.indexOf("PrivateProfileTokenService::Remove("),
  );
  assertEquals(cleanup.includes("NS_GetSpecialDirectory"), false);
  assertEquals(cleanup.includes("Allowed()"), false);
  assertStringIncludes(cleanup, "mStorage->Remove()");
});
Deno.test("native core avoids inheritance propagation and pins object identity for mutation", async () => {
  const core = await read(
    "native/windows-profile-token/PrivateProfileToken.cpp",
  );
  assertStringIncludes(core, "OBJ_CASE_INSENSITIVE | kDontReparse");
  assertStringIncludes(core, "SetKernelObjectSecurity(handle");
  assertEquals(core.includes("SetSecurityInfo("), false);
  assertStringIncludes(core, "standard.NumberOfLinks != 1");
  assertStringIncludes(core, "FileIdInfo");
  assertStringIncludes(core, "!Same(identity, observed)");
  assertStringIncludes(core, "FileDispositionInfo");
});
Deno.test("Windows native CI compiles Debug and Release without authority or publication", async () => {
  const workflow = await read(".github/workflows/colocated_runner_test.yml");
  const native = workflow.slice(
    workflow.indexOf("  native-windows-profile-token:"),
  );
  assertStringIncludes(native, "runs-on: windows-latest");
  assertStringIncludes(native, "configuration: [Debug, Release]");
  assertStringIncludes(native, "cmake --build");
  assertStringIncludes(native, "ctest --test-dir");
  assertStringIncludes(native, "persist-credentials: false");
  for (
    const forbidden of [
      "secrets.",
      "contents: write",
      "id-token:",
      "GH_TOKEN",
      "pull_request_target",
      "--admin",
    ]
  ) assertEquals(native.includes(forbidden), false);
});
