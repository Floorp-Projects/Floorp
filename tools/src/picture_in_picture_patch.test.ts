// SPDX-License-Identifier: MPL-2.0
import { assertEquals } from "@std/assert";

Deno.test("PiP AppWindow fix matches packaged and extracted Runtime paths", async () => {
  const path = "toolkit/components/pictureinpicture/PictureInPicture.sys.mjs";
  const packaged = await Deno.readTextFile(
    ".github/patches/floorp-runtime/common/picture-in-picture-app-window.patch",
  );
  const extracted = await Deno.readTextFile(
    "tools/patches/moz-src-toolkit-components-pictureinpicture-PictureInPicture.sys.patch",
  );
  assertEquals(extracted, packaged.replaceAll(path, `moz-src/${path}`));
});
