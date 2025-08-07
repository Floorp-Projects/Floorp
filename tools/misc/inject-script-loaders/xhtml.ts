import { DOMParser } from "linkedom";
import * as fs from "node:fs/promises";

export async function injectXHTML(binPath: string) {
  // Inject noraneko script into browser.xhtml
  const pathBrowserXhtml =
    `${binPath}/browser/chrome/browser/content/browser/browser.xhtml`;
  const doc = new DOMParser().parseFromString(
    (await fs.readFile(pathBrowserXhtml)).toString(),
    "text/xml",
  );
  for (const elem of doc.querySelectorAll("[data-geckomixin]")) {
    elem.remove();
  }
  const script = doc.createElement("script");
  script.setAttribute("type", "module");
  script.setAttribute(
    "src",
    "chrome://noraneko-startup/content/chrome_root.js",
  );
  script.setAttribute("async", "async");
  script.dataset.geckomixin = "";
  doc.querySelector("head").appendChild(script);
  await fs.writeFile(pathBrowserXhtml, doc.toString());
}

export async function injectXHTMLDev(binPath: string) {
  // For development: Inject noraneko script into browser.xhtml and relax CSP
  const pathBrowserXhtml =
    `${binPath}/browser/chrome/browser/content/browser/browser.xhtml`;
  const doc = new DOMParser().parseFromString(
    (await fs.readFile(pathBrowserXhtml)).toString(),
    "text/xml",
  );
  for (const elem of doc.querySelectorAll("[data-geckomixin]")) {
    elem.remove();
  }
  const script = doc.createElement("script");
  script.setAttribute("type", "module");
  script.setAttribute(
    "src",
    "chrome://noraneko-startup/content/chrome_root.js",
  );
  script.setAttribute("async", "async");
  script.dataset.geckomixin = "";
  doc.querySelector("head").appendChild(script);
  // From Floorp: Relax CSP for development
  const meta = doc.querySelector(
    "meta[http-equiv='Content-Security-Policy']",
  ) as HTMLMetaElement;
  meta?.setAttribute(
    "content",
    "script-src chrome: moz-src: resource: http://localhost:* 'report-sample'",
  );
  await fs.writeFile(pathBrowserXhtml, doc.toString());

  // Also relax CSP for preferences.xhtml in development
  const pathPreferencesXhtml =
    `${binPath}/browser/chrome/browser/content/browser/preferences/preferences.xhtml`;
  const docPref = new DOMParser().parseFromString(
    (await fs.readFile(pathPreferencesXhtml)).toString(),
    "text/xml",
  );
  const metaPref = docPref.querySelector("meta") as HTMLMetaElement;
  metaPref.setAttribute(
    "content",
    `default-src chrome: http://localhost:* ws://localhost:*; img-src chrome: moz-icon: https: blob: data:; style-src chrome: data: 'unsafe-inline'; object-src 'none'`,
  );
  await fs.writeFile(pathPreferencesXhtml, docPref.toString());
}
