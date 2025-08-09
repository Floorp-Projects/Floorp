import { DOMParser } from "https://deno.land/x/linkedom/deno-dom-wasm.ts";
import * as fs from "node:fs/promises";

async function injectXHTML(binPath: string) {
  const path_browserxhtml = `${binPath}/browser/chrome/browser/content/browser/browser.xhtml`;
  {
    const document = new DOMParser().parseFromString(
      (await fs.readFile(path_browserxhtml)).toString(),
      "text/xml",
    );

    for (const elem of document.querySelectorAll("[data-geckomixin]")) {
      elem.remove();
    }

    const script = document.createElement("script");
    script.setAttribute("type", "module");
    script.setAttribute(
      "src",
      "chrome://noraneko-startup/content/chrome_root.js",
    );
    script.setAttribute("async", "async");
    script.dataset.geckomixin = "";

    document.querySelector("head").appendChild(script);

    await fs.writeFile(path_browserxhtml, document.toString());
  }
}

async function injectXHTMLDev(binPath: string) {
  const path_preferencesxhtml = `${binPath}/browser/chrome/browser/content/browser/preferences/preferences.xhtml`;
  {
    const document = new DOMParser().parseFromString(
      (await fs.readFile(path_preferencesxhtml)).toString(),
      "text/xml",
    );

    const meta = document.querySelector("meta") as HTMLMetaElement;
    if (meta) {
        meta.setAttribute(
        "content",
        `default-src chrome: http://localhost:* ws://localhost:*; img-src chrome: moz-icon: https: blob: data:; style-src chrome: data: 'unsafe-inline'; object-src 'none'`,
        );
    }

    await fs.writeFile(path_preferencesxhtml, document.toString());
  }
}

if (import.meta.main) {
    const binPath = Deno.args[0];
    const isDev = Deno.args.includes('--dev');

    if (!binPath) {
        console.error("Error: binPath argument is required.");
        Deno.exit(1);
    }

    await injectXHTML(binPath);
    if (isDev) {
        await injectXHTMLDev(binPath);
    }
}
