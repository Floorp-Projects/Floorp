import * as fs from "node:fs/promises";
import { DOMParser } from "linkedom";

export async function injectXHTML(binPath: string) {
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
    script.innerHTML = `Services.scriptloader.loadSubScript("chrome://noraneko/content/startup/browser.js", this);`;
    script.dataset.geckomixin = "";

    document.querySelector("head").appendChild(script);

    await fs.writeFile(path_browserxhtml, document.toString());
  }

  const path_preferencesxhtml = `${binPath}/browser/chrome/browser/content/browser/preferences/preferences.xhtml`;
  {
    const document = new DOMParser().parseFromString(
      (await fs.readFile(path_preferencesxhtml)).toString(),
      "text/xml",
    );

    for (const elem of document.querySelectorAll("[data-geckomixin]")) {
      elem.remove();
    }

    const script = document.createElement("script");
    script.innerHTML = `Services.scriptloader.loadSubScript("chrome://noraneko/content/startup/about-preferences.js", this);`;
    script.dataset.geckomixin = "";

    document.querySelector("head").appendChild(script);

    await fs.writeFile(path_preferencesxhtml, document.toString());
  }
}

export async function injectXHTMLDev(binPath: string) {
  const path_preferencesxhtml = `${binPath}/browser/chrome/browser/content/browser/preferences/preferences.xhtml`;
  {
    const document = new DOMParser().parseFromString(
      (await fs.readFile(path_preferencesxhtml)).toString(),
      "text/xml",
    );

    const meta = document.querySelector("meta") as HTMLMetaElement;
    meta.setAttribute(
      "content",
      `default-src chrome: http://localhost:* ws://localhost:*; img-src chrome: moz-icon: https: blob: data:; style-src chrome: data: 'unsafe-inline'; object-src 'none'`,
    );

    await fs.writeFile(path_preferencesxhtml, document.toString());
  }
}
