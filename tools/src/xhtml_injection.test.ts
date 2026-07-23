// SPDX-License-Identifier: MPL-2.0

import {
  assertEquals,
  assertMatch,
  assertRejects,
  assertStringIncludes,
  assertThrows,
} from "@std/assert";
import {
  BROWSER_HTTP_LOADER_ORIGIN,
  injectBrowserXhtml,
  parseXhtmlCliArgs,
  transformBrowserXhtml,
} from "../scripts/xhtml.ts";

const STARTUP_SCRIPT_SRC = "chrome://noraneko-startup/content/chrome_root.js";
const STRICT_CSP =
  "default-src 'none'; script-src chrome: moz-src: resource: 'report-sample'; img-src chrome: data:";

function fixture(
  csp: string | null = STRICT_CSP,
  options: { duplicateMeta?: boolean; scripts?: string } = {},
): string {
  const meta = csp === null
    ? ""
    : `<meta http-equiv="Content-Security-Policy" content="${csp}" />`;
  return `<?xml version="1.0" encoding="utf-8"?>
<html xmlns="http://www.w3.org/1999/xhtml">
  <head>
    ${meta}
    ${options.duplicateMeta ? meta : ""}
    ${options.scripts ?? ""}
  </head>
  <body />
</html>`;
}

function count(haystack: string, needle: string): number {
  return haystack.split(needle).length - 1;
}

function strictToDev(): string {
  return transformBrowserXhtml(fixture(), {
    allowBrowserHttpLoader: true,
  });
}

Deno.test("browser XHTML strict-to-dev adds only the exact HTTP loader origin", () => {
  const output = strictToDev();

  assertEquals(count(output, BROWSER_HTTP_LOADER_ORIGIN), 1);
  assertStringIncludes(
    output,
    `script-src chrome: moz-src: resource: 'report-sample' ${BROWSER_HTTP_LOADER_ORIGIN}`,
  );
  assertStringIncludes(output, "default-src 'none';");
  assertStringIncludes(output, "; img-src chrome: data:");
  assertEquals(/(?:https?|ws):\/\/localhost:\*/i.test(output), false);
  assertEquals(/wss?:\/\//i.test(output), false);
});

Deno.test("browser XHTML dev-to-dev is byte-stable and deduplicates loader origins", () => {
  const duplicated = fixture(
    `script-src chrome: ${BROWSER_HTTP_LOADER_ORIGIN} resource: HTTP://LOCALHOST:5181`,
  );
  const first = transformBrowserXhtml(duplicated, {
    allowBrowserHttpLoader: true,
  });
  const second = transformBrowserXhtml(first, {
    allowBrowserHttpLoader: true,
  });

  assertEquals(second, first);
  assertEquals(count(first, BROWSER_HTTP_LOADER_ORIGIN), 1);
  assertEquals(first.includes("HTTP://LOCALHOST:5181"), false);
});

Deno.test("browser XHTML dev-to-stage removes the origin from the same source", () => {
  const dev = strictToDev();
  const stage = transformBrowserXhtml(dev, {
    allowBrowserHttpLoader: false,
  });

  assertEquals(stage.includes(BROWSER_HTTP_LOADER_ORIGIN), false);
  assertStringIncludes(
    stage,
    "script-src chrome: moz-src: resource: 'report-sample'",
  );
});

Deno.test("browser XHTML dev-to-production removes the development origin", () => {
  const production = transformBrowserXhtml(strictToDev());

  assertEquals(production.includes(BROWSER_HTTP_LOADER_ORIGIN), false);
  assertEquals(/(?:https?|ws):\/\/localhost:\*/i.test(production), false);
});

Deno.test("browser XHTML stage-to-stage is byte-stable", () => {
  const stage = transformBrowserXhtml(fixture(), {
    allowBrowserHttpLoader: false,
  });

  assertEquals(
    transformBrowserXhtml(stage, { allowBrowserHttpLoader: false }),
    stage,
  );
});

Deno.test("browser XHTML requires exactly one CSP meta", () => {
  assertThrows(
    () => transformBrowserXhtml(fixture(null)),
    Error,
    "expected exactly one CSP meta, found 0",
  );
  assertThrows(
    () => transformBrowserXhtml(fixture(STRICT_CSP, { duplicateMeta: true })),
    Error,
    "expected exactly one CSP meta, found 2",
  );
  assertThrows(
    () =>
      transformBrowserXhtml(
        fixture().replace(` content="${STRICT_CSP}"`, ""),
      ),
    Error,
    "CSP meta has no content attribute",
  );
});

Deno.test("browser XHTML requires exactly one usable script-src directive", () => {
  assertThrows(
    () => transformBrowserXhtml(fixture("default-src 'none'")),
    Error,
    "expected exactly one script-src directive, found 0",
  );
  assertThrows(
    () =>
      transformBrowserXhtml(
        fixture("script-src chrome:; img-src data:; SCRIPT-SRC resource:"),
      ),
    Error,
    "expected exactly one script-src directive, found 2",
  );
  assertThrows(
    () => transformBrowserXhtml(fixture("script-src")),
    Error,
    "script-src has no source expressions",
  );
  assertThrows(
    () => transformBrowserXhtml(fixture("script-src chrome:; bad@name data:")),
    Error,
    "malformed CSP directive",
  );
});

Deno.test("browser XHTML rejects malformed XML before DOM recovery", () => {
  assertThrows(
    () => transformBrowserXhtml(fixture().replace("</head>", "</body>")),
    Error,
    "unexpected closing tag body; expected head",
  );
  assertThrows(
    () =>
      transformBrowserXhtml(
        '<html><head><meta http-equiv="Content-Security-Policy" content="script-src chrome: /></head></html>',
      ),
    Error,
    "unterminated markup",
  );
});

Deno.test("browser XHTML rejects the loader origin in non-script directives", () => {
  assertThrows(
    () =>
      transformBrowserXhtml(
        fixture(
          `default-src ${BROWSER_HTTP_LOADER_ORIGIN}; script-src chrome:`,
        ),
        { allowBrowserHttpLoader: true },
      ),
    Error,
    "appears in non-script directive default-src",
  );
});

Deno.test("browser XHTML injection leaves source bytes unchanged on validation failure", async () => {
  const temp = await Deno.makeTempDir({ prefix: "floorp-xhtml-failure-" });
  const directory = `${temp}/browser/chrome/browser/content/browser`;
  const path = `${directory}/browser.xhtml`;
  const invalid = fixture("default-src 'none'");
  try {
    await Deno.mkdir(directory, { recursive: true });
    await Deno.writeTextFile(path, invalid);

    await assertRejects(
      () =>
        injectBrowserXhtml(temp, {
          allowBrowserHttpLoader: true,
        }),
      Error,
      "expected exactly one script-src directive",
    );
    assertEquals(await Deno.readTextFile(path), invalid);
  } finally {
    await Deno.remove(temp, { recursive: true });
  }
});

Deno.test("browser XHTML owns exactly one startup script after repeated injection", () => {
  const source = fixture(STRICT_CSP, {
    scripts: `
      <script data-geckomixin="" src="${STARTUP_SCRIPT_SRC}" />
      <script src="${STARTUP_SCRIPT_SRC}" />
      <script data-geckomixin="" src="chrome://obsolete/content/old.js" />
      <script src="chrome://browser/content/browser-main.js" />`,
  });
  const output = transformBrowserXhtml(source, {
    allowBrowserHttpLoader: true,
  });

  assertEquals(count(output, STARTUP_SCRIPT_SRC), 1);
  assertEquals(count(output, "data-geckomixin"), 1);
  assertStringIncludes(output, "chrome://browser/content/browser-main.js");
  assertEquals(output.includes("chrome://obsolete/content/old.js"), false);
  assertMatch(
    output,
    /<script[^>]*data-geckomixin=""[^>]*chrome_root\.js|<script[^>]*chrome_root\.js[^>]*data-geckomixin=""/,
  );
});

Deno.test("XHTML CLI keeps browser HTTP permission explicit and off by default", () => {
  assertEquals(parseXhtmlCliArgs(["_dist/bin"]), {
    binPath: "_dist/bin",
    isDev: false,
    allowBrowserHttpLoader: false,
  });
  assertEquals(
    parseXhtmlCliArgs([
      "_dist/bin",
      "--dev",
      "--allow-browser-http-loader",
    ]),
    {
      binPath: "_dist/bin",
      isDev: true,
      allowBrowserHttpLoader: true,
    },
  );
  assertThrows(
    () => parseXhtmlCliArgs(["_dist/bin", "--unknown"]),
    Error,
    "Unknown argument",
  );
});
