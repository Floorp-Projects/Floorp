// SPDX-License-Identifier: MPL-2.0
function assert(value: unknown, message: string) {
  if (!value) throw new Error(message);
}
const root = new URL("../../", import.meta.url);
Deno.test("welcome CSP permits packaged styles but not data styles", async () => {
  const html = await Deno.readTextFile(new URL("browser-features/pages-welcome/index.html", root));
  const csp = html.match(/Content-Security-Policy" content="([^"]+)"/)?.[1];
  assert(csp, "CSP must exist");
  const directives = new Map(csp!.split(";").map((d) => {
    const [key, ...sources] = d.trim().split(/\s+/);
    return [key, sources];
  }));
  assert(JSON.stringify(directives.get("style-src")) === JSON.stringify(["chrome:", "'unsafe-inline'"]), "Keep packaged CSS and existing inline playback styling only");
  assert(JSON.stringify(directives.get("object-src")) === JSON.stringify(["'none'"]), "Objects stay blocked");
  assert(JSON.stringify(directives.get("default-src")) === JSON.stringify(["chrome:"]), "No default-source relaxation");
});
Deno.test("welcome fonts remain local and Vite does not inline imported assets", async () => {
  const fonts = await Deno.readTextFile(new URL("libs/ui/fonts.css", root));
  const urls = [...fonts.matchAll(/url\(["']?([^"')]+)/g)].map((m) => m[1]);
  assert(urls.length > 0 && urls.every((url) => url.startsWith("./fonts/") && url.endsWith(".woff2")), "Font assets must remain bundled");
  const config = await Deno.readTextFile(new URL("browser-features/pages-welcome/vite.config.ts", root));
  assert(/assetsInlineLimit:\s*0/.test(config), "Imported assets must not become data URIs");
});
