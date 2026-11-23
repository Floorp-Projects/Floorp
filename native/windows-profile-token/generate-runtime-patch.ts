import { createHash } from "node:crypto";
const sourceRoot = Deno.args.length === 0
  ? "_dist/fdh/windows-runtime-source-pin"
  : Deno.args.length === 2 && Deno.args[0] === "--source-root" &&
      Deno.args[1].trim()
  ? Deno.args[1]
  : (() => {
    throw new Error(
      "Usage: generate-runtime-patch.ts [--source-root PINNED_RUNTIME]",
    );
  })();
const base = sourceRoot + "/xpcom/io/moz.build";
// Match canonical Git blob bytes even when the read-only input checkout uses CRLF.
const before = (await Deno.readTextFile(base)).replaceAll("\r\n", "\n");
const bytes = new TextEncoder().encode(before);
const blob = createHash("sha1").update("blob " + bytes.length + "\0").update(
  bytes,
).digest("hex");
if (blob !== "38bfd488da606ee75034ec0e340027176669bde3") {
  throw new Error("Wrong pinned moz.build blob");
}
const lines = before.slice(0, -1).split("\n");
const marker = 'if CONFIG["MOZ_WIDGET_TOOLKIT"] == "windows":';
const index = lines.indexOf(marker);
if (index < 0 || lines[index + 1] !== '    EXPORTS += ["nsLocalFileWin.h"]') {
  throw new Error("Pinned Windows block changed");
}
const added = [
  '    XPIDL_SOURCES += ["nsIFloorpPrivateProfileToken.idl"]',
  '    EXPORTS += ["FloorpPrivateProfileTokenService.h"]',
  '    SOURCES += ["PrivateProfileToken.cpp"]',
  '    UNIFIED_SOURCES += ["FloorpPrivateProfileTokenService.cpp"]',
  '    XPCOM_MANIFESTS += ["FloorpPrivateProfileTokenComponents.conf"]',
  '    OS_LIBS += ["advapi32"]',
];
const context = lines.slice(index, index + 4);
let patch =
  "# Floorp private profile token native service; source pin 64024fee8215388debd8749f5e3917aaa234d78a\n" +
  "# Requires freshly compiled Windows Runtime artifacts; prebuilt package patching is not activation.\n" +
  "diff --git a/xpcom/io/moz.build b/xpcom/io/moz.build\n--- a/xpcom/io/moz.build\n+++ b/xpcom/io/moz.build\n" +
  "@@ -" + (index + 1) + ",4 +" + (index + 1) + "," + (4 + added.length) +
  " @@\n" +
  " " + context[0] + "\n" + added.map((line) => "+" + line + "\n").join("") +
  context.slice(1).map((line) => " " + line + "\n").join("");
const inputs = [
  [
    "native/windows-profile-token/PrivateProfileToken.h",
    "PrivateProfileToken.h",
  ],
  [
    "native/windows-profile-token/PrivateProfileToken.cpp",
    "PrivateProfileToken.cpp",
  ],
  ...[
    "nsIFloorpPrivateProfileToken.idl",
    "FloorpPrivateProfileTokenService.h",
    "FloorpPrivateProfileTokenService.cpp",
    "FloorpPrivateProfileTokenComponents.conf",
  ]
    .map((name) => ["native/windows-profile-token/runtime/" + name, name]),
];
for (const [source, name] of inputs) {
  const text = await Deno.readTextFile(source);
  if (!text.endsWith("\n") || text.includes("\r") || text.includes("\0")) {
    throw new Error("Noncanonical source: " + source);
  }
  const content = text.slice(0, -1).split("\n");
  const path = "xpcom/io/" + name;
  patch += "diff --git a/" + path + " b/" + path +
    "\nnew file mode 100644\n--- /dev/null\n+++ b/" + path +
    "\n@@ -0,0 +1," + content.length + " @@\n" + content.map((line) =>
      "+" + line + "\n"
    ).join("");
}
const output =
  ".github/patches/floorp-runtime/common/fdh-windows-private-profile-token.patch";
await Deno.writeTextFile(output, patch);
console.log(
  JSON.stringify({
    output,
    bytes: new TextEncoder().encode(patch).length,
    nativeInputs: inputs.length,
    pinnedMozBuildBlob: blob,
  }),
);
