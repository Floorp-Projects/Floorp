// SPDX-License-Identifier: MPL-2.0

import { DOMParser } from "linkedom";
import * as fs from "node:fs/promises";

export const BROWSER_HTTP_LOADER_ORIGIN = "http://localhost:5181";

const CSP_HEADER = "content-security-policy";
const STARTUP_SCRIPT_SRC = "chrome://noraneko-startup/content/chrome_root.js";
const PREFERENCES_DEV_CSP =
  "default-src chrome: http://localhost:* ws://localhost:*; img-src chrome: moz-icon: https: blob: data:; style-src chrome: data: 'unsafe-inline'; object-src 'none'";

type ParsedXmlDocument = ReturnType<DOMParser["parseFromString"]>;
type ParsedElement = NonNullable<
  ReturnType<ParsedXmlDocument["querySelector"]>
>;

export interface BrowserXhtmlTransformOptions {
  allowBrowserHttpLoader?: boolean;
}

export interface XhtmlCliOptions {
  binPath: string;
  isDev: boolean;
  allowBrowserHttpLoader: boolean;
}

function fail(message: string): never {
  throw new Error(`Invalid XHTML: ${message}`);
}

function assertEntityReferences(value: string): void {
  for (let index = value.indexOf("&"); index !== -1;) {
    const rest = value.slice(index);
    const match = rest.match(
      /^&(?:amp|lt|gt|apos|quot|#[0-9]+|#x[0-9A-Fa-f]+);/,
    );
    if (!match) {
      fail(`malformed entity reference at byte ${index}`);
    }
    index = value.indexOf("&", index + match[0].length);
  }
}

function findMarkupEnd(source: string, start: number): number {
  let quote: '"' | "'" | null = null;
  for (let index = start; index < source.length; index++) {
    const character = source[index];
    if (quote !== null) {
      if (character === quote) quote = null;
      continue;
    }
    if (character === '"' || character === "'") {
      quote = character;
    } else if (character === ">") {
      return index;
    } else if (character === "<") {
      fail(`unexpected '<' inside markup at byte ${index}`);
    }
  }
  fail("unterminated markup");
}

function assertAttributes(source: string, start: number): void {
  const attributes = new Set<string>();
  let index = start;

  while (index < source.length) {
    while (/\s/.test(source[index] ?? "")) index++;
    if (index === source.length) return;

    const nameMatch = source.slice(index).match(/^[A-Za-z_:][A-Za-z0-9_.:-]*/);
    if (!nameMatch) fail("malformed attribute name");
    const normalizedName = nameMatch[0].toLowerCase();
    if (attributes.has(normalizedName)) {
      fail(`duplicate attribute ${nameMatch[0]}`);
    }
    attributes.add(normalizedName);
    index += nameMatch[0].length;

    while (/\s/.test(source[index] ?? "")) index++;
    if (source[index] !== "=") {
      fail(`attribute ${nameMatch[0]} has no '='`);
    }
    index++;
    while (/\s/.test(source[index] ?? "")) index++;

    const quote = source[index];
    if (quote !== '"' && quote !== "'") {
      fail(`attribute ${nameMatch[0]} is not quoted`);
    }
    const valueStart = ++index;
    const valueEnd = source.indexOf(quote, valueStart);
    if (valueEnd === -1) fail(`attribute ${nameMatch[0]} is unterminated`);
    const value = source.slice(valueStart, valueEnd);
    if (value.includes("<")) fail(`attribute ${nameMatch[0]} contains '<'`);
    assertEntityReferences(value);
    index = valueEnd + 1;
  }
}

function findDoctypeEnd(source: string, start: number): number {
  let quote: '"' | "'" | null = null;
  let subsetDepth = 0;
  for (let index = start; index < source.length; index++) {
    const character = source[index];
    if (quote !== null) {
      if (character === quote) quote = null;
      continue;
    }
    if (character === '"' || character === "'") {
      quote = character;
    } else if (character === "[") {
      subsetDepth++;
    } else if (character === "]") {
      subsetDepth--;
      if (subsetDepth < 0) fail("malformed DOCTYPE internal subset");
    } else if (character === ">" && subsetDepth === 0) {
      return index;
    }
  }
  fail("unterminated DOCTYPE");
}

/**
 * Linkedom intentionally performs HTML-style recovery even in XML mode. This
 * small validator therefore runs first so a damaged Firefox source file is
 * rejected instead of being silently repaired and overwritten.
 */
export function assertWellFormedXml(source: string): void {
  const stack: string[] = [];
  const documentStart = source.charCodeAt(0) === 0xfeff ? 1 : 0;
  let index = documentStart;
  let sawRoot = false;
  let closedRoot = false;
  let sawDoctype = false;
  let sawXmlDeclaration = false;

  while (index < source.length) {
    const markupStart = source.indexOf("<", index);
    const textEnd = markupStart === -1 ? source.length : markupStart;
    const text = source.slice(index, textEnd);
    if (stack.length === 0 && text.trim() !== "") {
      fail(`text outside the document element at byte ${index}`);
    }
    if (text.includes("]]>")) fail("text contains forbidden ']]>'");
    assertEntityReferences(text);
    if (markupStart === -1) break;
    index = markupStart;

    if (source.startsWith("<!--", index)) {
      const end = source.indexOf("-->", index + 4);
      if (end === -1) fail("unterminated comment");
      if (source.slice(index + 4, end).includes("--")) {
        fail("comment contains '--'");
      }
      index = end + 3;
      continue;
    }

    if (source.startsWith("<![CDATA[", index)) {
      if (stack.length === 0) fail("CDATA outside the document element");
      const end = source.indexOf("]]>", index + 9);
      if (end === -1) fail("unterminated CDATA section");
      index = end + 3;
      continue;
    }

    if (source.startsWith("<?", index)) {
      const end = source.indexOf("?>", index + 2);
      if (end === -1) fail("unterminated processing instruction");
      const body = source.slice(index + 2, end).trim();
      if (!/^[A-Za-z_:][A-Za-z0-9_.:-]*(?:\s[\s\S]*)?$/.test(body)) {
        fail("malformed processing instruction");
      }
      const target = body.match(/^[^\s]+/)?.[0] ?? "";
      if (target.toLowerCase() === "xml") {
        if (
          sawXmlDeclaration || sawRoot || sawDoctype || index !== documentStart
        ) {
          fail("misplaced or duplicate XML declaration");
        }
        sawXmlDeclaration = true;
      }
      index = end + 2;
      continue;
    }

    if (/^<!DOCTYPE(?:\s|>)/i.test(source.slice(index))) {
      if (sawRoot || sawDoctype) fail("misplaced or duplicate DOCTYPE");
      const end = findDoctypeEnd(source, index + 9);
      const body = source.slice(index + 9, end).trim();
      if (!/^[A-Za-z_:][A-Za-z0-9_.:-]*(?:\s[\s\S]*)?$/.test(body)) {
        fail("malformed DOCTYPE");
      }
      sawDoctype = true;
      index = end + 1;
      continue;
    }

    if (source.startsWith("<!", index)) {
      fail(`unsupported declaration at byte ${index}`);
    }

    const end = findMarkupEnd(source, index + 1);
    const markup = source.slice(index + 1, end);
    if (markup.startsWith("/")) {
      const closeMatch = markup.match(/^\/([A-Za-z_:][A-Za-z0-9_.:-]*)\s*$/);
      if (!closeMatch) fail("malformed closing tag");
      const expected = stack.pop();
      if (expected !== closeMatch[1]) {
        fail(
          `unexpected closing tag ${closeMatch[1]}; expected ${
            expected ?? "none"
          }`,
        );
      }
      if (stack.length === 0) closedRoot = true;
      index = end + 1;
      continue;
    }

    const selfClosing = markup.endsWith("/");
    const opening = selfClosing ? markup.slice(0, -1) : markup;
    const nameMatch = opening.match(/^([A-Za-z_:][A-Za-z0-9_.:-]*)/);
    if (!nameMatch) fail("malformed opening tag");
    if (stack.length === 0) {
      if (closedRoot || sawRoot) fail("multiple document elements");
      sawRoot = true;
    }
    assertAttributes(opening, nameMatch[0].length);
    if (selfClosing) {
      if (stack.length === 0) closedRoot = true;
    } else {
      stack.push(nameMatch[1]);
    }
    index = end + 1;
  }

  if (!sawRoot) fail("document element is missing");
  if (stack.length !== 0) fail(`unclosed element ${stack.at(-1)}`);
  if (!closedRoot) fail("document element is not closed");
}

function getCaseInsensitiveAttribute(
  element: ParsedElement,
  name: string,
): string | null {
  const matches = [...element.attributes].filter((attribute) =>
    attribute.name.toLowerCase() === name.toLowerCase()
  );
  if (matches.length > 1) fail(`duplicate ${name} attribute`);
  return matches[0]?.value ?? null;
}

function setExistingCaseInsensitiveAttribute(
  element: ParsedElement,
  name: string,
  value: string,
): void {
  const matches = [...element.attributes].filter((attribute) =>
    attribute.name.toLowerCase() === name.toLowerCase()
  );
  if (matches.length !== 1) {
    fail(`expected exactly one ${name} attribute, found ${matches.length}`);
  }
  element.setAttribute(matches[0].name, value);
}

function isLoaderOrigin(token: string): boolean {
  return token.toLowerCase() === BROWSER_HTTP_LOADER_ORIGIN;
}

function rewriteScriptSrcDirective(
  content: string,
  allowBrowserHttpLoader: boolean,
): string {
  const directives = content.split(";");
  let scriptSrcCount = 0;

  const rewritten = directives.map((rawDirective) => {
    const trimmed = rawDirective.trim();
    if (trimmed === "") return rawDirective;

    const tokens = trimmed.split(/\s+/);
    const directiveName = tokens[0];
    if (!/^[A-Za-z][A-Za-z0-9-]*$/.test(directiveName)) {
      fail(`malformed CSP directive ${directiveName}`);
    }
    const normalizedName = directiveName.toLowerCase();
    const sources = tokens.slice(1);

    if (normalizedName !== "script-src") {
      if (sources.some(isLoaderOrigin)) {
        fail(
          `${BROWSER_HTTP_LOADER_ORIGIN} appears in non-script directive ${directiveName}`,
        );
      }
      return rawDirective;
    }

    scriptSrcCount++;
    if (sources.length === 0) fail("script-src has no source expressions");
    const retainedSources = sources.filter((source) => !isLoaderOrigin(source));
    if (retainedSources.length === 0) {
      fail("script-src would have no source expressions after loader removal");
    }

    const desiredSources = allowBrowserHttpLoader
      ? [...retainedSources, BROWSER_HTTP_LOADER_ORIGIN]
      : retainedSources;
    const alreadyDesired = sources.length === desiredSources.length &&
      sources.every((source, sourceIndex) =>
        source === desiredSources[sourceIndex]
      );
    if (alreadyDesired) return rawDirective;

    const leading = rawDirective.match(/^\s*/)?.[0] ?? "";
    const trailing = rawDirective.match(/\s*$/)?.[0] ?? "";
    return `${leading}${directiveName} ${desiredSources.join(" ")}${trailing}`;
  });

  if (scriptSrcCount !== 1) {
    fail(`expected exactly one script-src directive, found ${scriptSrcCount}`);
  }

  return rewritten.join(";");
}

function findCspMeta(document: ParsedXmlDocument): ParsedElement {
  const headElements = [...document.querySelectorAll("head")];
  if (headElements.length !== 1) {
    fail(`expected exactly one head element, found ${headElements.length}`);
  }
  const cspMetas = [...document.querySelectorAll("meta")].filter((meta) =>
    getCaseInsensitiveAttribute(meta, "http-equiv")?.trim().toLowerCase() ===
      CSP_HEADER
  );
  if (cspMetas.length !== 1) {
    fail(`expected exactly one CSP meta, found ${cspMetas.length}`);
  }
  if (!headElements[0].contains(cspMetas[0])) {
    fail("CSP meta is outside the head element");
  }
  if (getCaseInsensitiveAttribute(cspMetas[0], "content") === null) {
    fail("CSP meta has no content attribute");
  }
  return cspMetas[0];
}

function assertTransformedBrowserXhtml(
  output: string,
  allowBrowserHttpLoader: boolean,
): void {
  assertWellFormedXml(output);
  const document = new DOMParser().parseFromString(output, "text/xml");
  const meta = findCspMeta(document);
  const content = getCaseInsensitiveAttribute(meta, "content");
  if (content === null) fail("CSP meta has no content attribute");
  const verified = rewriteScriptSrcDirective(content, allowBrowserHttpLoader);
  if (verified !== content) {
    fail("CSP transformation did not reach a fixed point");
  }

  const startupScripts = [...document.querySelectorAll("script")].filter((
    script,
  ) => getCaseInsensitiveAttribute(script, "src") === STARTUP_SCRIPT_SRC);
  const ownedScripts = [
    ...document.querySelectorAll("script[data-geckomixin]"),
  ];
  if (startupScripts.length !== 1 || ownedScripts.length !== 1) {
    fail("expected exactly one owned startup script");
  }
  if (startupScripts[0] !== ownedScripts[0]) {
    fail("owned startup marker does not identify the startup script");
  }
}

/** Pure, fail-closed browser.xhtml transformation used by the CLI and tests. */
export function transformBrowserXhtml(
  source: string,
  options: BrowserXhtmlTransformOptions = {},
): string {
  const allowBrowserHttpLoader = options.allowBrowserHttpLoader ?? false;
  assertWellFormedXml(source);
  const document = new DOMParser().parseFromString(source, "text/xml");
  const meta = findCspMeta(document);
  const content = getCaseInsensitiveAttribute(meta, "content");
  if (content === null) fail("CSP meta has no content attribute");
  setExistingCaseInsensitiveAttribute(
    meta,
    "content",
    rewriteScriptSrcDirective(content, allowBrowserHttpLoader),
  );

  for (const script of document.querySelectorAll("script")) {
    if (
      script.hasAttribute("data-geckomixin") ||
      getCaseInsensitiveAttribute(script, "src") === STARTUP_SCRIPT_SRC
    ) {
      script.remove();
    }
  }

  const head = document.querySelector("head");
  if (head === null) fail("head element is missing");
  const script = document.createElement("script");
  script.setAttribute("type", "module");
  script.setAttribute("src", STARTUP_SCRIPT_SRC);
  script.setAttribute("async", "async");
  script.setAttribute("data-geckomixin", "");
  head.appendChild(script);

  const output = document.toString();
  assertTransformedBrowserXhtml(output, allowBrowserHttpLoader);
  return output;
}

export async function injectBrowserXhtml(
  binPath: string,
  options: BrowserXhtmlTransformOptions = {},
): Promise<void> {
  const browserXhtmlPath =
    `${binPath}/browser/chrome/browser/content/browser/browser.xhtml`;
  const source = await fs.readFile(browserXhtmlPath, "utf8");
  const output = transformBrowserXhtml(source, options);
  await fs.writeFile(browserXhtmlPath, output);
}

/** Pure, fail-closed development preferences.xhtml CSP transformation. */
export function transformPreferencesXhtmlForDev(source: string): string {
  assertWellFormedXml(source);
  const document = new DOMParser().parseFromString(source, "text/xml");
  const meta = findCspMeta(document);
  setExistingCaseInsensitiveAttribute(meta, "content", PREFERENCES_DEV_CSP);

  const output = document.toString();
  assertWellFormedXml(output);
  const outputDocument = new DOMParser().parseFromString(output, "text/xml");
  const outputMeta = findCspMeta(outputDocument);
  if (
    getCaseInsensitiveAttribute(outputMeta, "content") !== PREFERENCES_DEV_CSP
  ) {
    fail("preferences CSP transformation did not reach the expected value");
  }
  return output;
}

export async function injectPreferencesXhtmlDev(
  binPath: string,
): Promise<void> {
  const preferencesXhtmlPath =
    `${binPath}/browser/chrome/browser/content/browser/preferences/preferences.xhtml`;
  const source = await fs.readFile(preferencesXhtmlPath, "utf8");
  const output = transformPreferencesXhtmlForDev(source);
  await fs.writeFile(preferencesXhtmlPath, output);
}

export function parseXhtmlCliArgs(args: string[]): XhtmlCliOptions {
  const binPath = args[0];
  if (!binPath || binPath.startsWith("--")) {
    throw new Error("binPath argument is required");
  }
  const flags = new Set(args.slice(1));
  for (const flag of flags) {
    if (flag !== "--dev" && flag !== "--allow-browser-http-loader") {
      throw new Error(`Unknown argument: ${flag}`);
    }
  }
  return {
    binPath,
    isDev: flags.has("--dev"),
    allowBrowserHttpLoader: flags.has("--allow-browser-http-loader"),
  };
}

if (import.meta.main) {
  const options = parseXhtmlCliArgs(Deno.args);
  await injectBrowserXhtml(options.binPath, {
    allowBrowserHttpLoader: options.allowBrowserHttpLoader,
  });
  if (options.isDev) {
    await injectPreferencesXhtmlDev(options.binPath);
  }
}
