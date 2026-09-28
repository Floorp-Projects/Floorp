/// <reference lib="dom" />
import { assertEquals } from "@std/assert";
import { openExternalLink } from "../../libs/ui/open-external.ts";

Deno.test("external link handoff accepts web URLs and rejects other schemes", () => {
  const globals = globalThis as unknown as Record<string, unknown>;
  const previous = globals.NROpenExternalLink;
  const opened: string[] = [];
  globals.NROpenExternalLink = (url: string) => opened.push(url);
  try {
    for (
      const url of [
        "javascript:alert(1)",
        "file:///tmp/example",
        "about:config",
        "relative/path",
        "not a url",
      ]
    ) {
      assertEquals(openExternalLink(url), false, url);
    }
    assertEquals(openExternalLink("https://example.org/help"), true);
    assertEquals(openExternalLink("http://example.org/help"), true);
    assertEquals(opened, [
      "https://example.org/help",
      "http://example.org/help",
    ]);
  } finally {
    if (previous === undefined) {
      delete globals.NROpenExternalLink;
    } else {
      globals.NROpenExternalLink = previous;
    }
  }
});
