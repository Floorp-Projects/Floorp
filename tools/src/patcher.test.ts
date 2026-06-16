import { assertEquals } from "@std/assert";
import { patchAppliesToCurrentPlatform } from "./patcher.ts";

Deno.test("patchAppliesToCurrentPlatform accepts generic patch files", () => {
  assertEquals(
    patchAppliesToCurrentPlatform("browser-modules-BrowserGlue.sys.patch"),
    true,
  );
});

Deno.test("patchAppliesToCurrentPlatform accepts unified patch files on all platforms", () => {
  assertEquals(
    patchAppliesToCurrentPlatform(
      "browser-chrome-browser-content-browser-browser-init.patch",
    ),
    true,
  );
  assertEquals(
    patchAppliesToCurrentPlatform(
      "browser-chrome-browser-content-browser-aboutDialog.patch",
    ),
    true,
  );
});

Deno.test("patchAppliesToCurrentPlatform filters platform-specific patch files", () => {
  const platform = Deno.build.os === "darwin"
    ? "darwin"
    : Deno.build.os === "windows"
    ? "windows"
    : "linux";

  assertEquals(
    patchAppliesToCurrentPlatform(
      "browser-chrome-browser-content-browser-browser-init.windows.patch",
    ),
    platform === "windows",
  );
});
