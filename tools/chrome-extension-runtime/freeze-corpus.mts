// SPDX-License-Identifier: MPL-2.0
import { unzipSync } from "fflate/browser";
import {
  CRX_DOWNLOAD_URLS,
  MAX_CRX_ENTRIES,
  MAX_CRX_SIZE,
  MAX_EXPANDED_CRX_SIZE,
  MAX_FILE_SIZE,
  STORE_DOWNLOAD_CLIENT_VERSION,
} from "../../browser-features/modules/modules/chrome-web-store/Constants.sys.mts";
import { verifyCRX } from "../../browser-features/modules/modules/chrome-web-store/CRXVerifier.sys.mts";
import { validateManifest } from "../../browser-features/modules/modules/chrome-web-store/ManifestTransformer.sys.mts";
import type { CorpusSelection } from "./corpus-types.ts";

const root = new URL("../../", import.meta.url);
const destination = new URL("_dist/chrome-extension-corpus/", root);
const seedBytes = await Deno.readFile(
  new URL("corpus-seed.json", import.meta.url),
);
const seed = JSON.parse(new TextDecoder().decode(seedBytes)) as {
  extensions: CorpusSelection[];
};
const fingerprint = async (bytes: Uint8Array<ArrayBuffer>) =>
  Array.from(
    new Uint8Array(await crypto.subtle.digest("SHA-256", bytes)),
    (byte) => byte.toString(16).padStart(2, "0"),
  ).join("");

await Deno.mkdir(new URL("packages/", destination), { recursive: true });
await Deno.mkdir(new URL("inventory/", destination), { recursive: true });
const records: Record<string, unknown>[] = [];

async function download(id: string): Promise<Uint8Array<ArrayBuffer>> {
  let failure: unknown;
  for (
    const builder of [CRX_DOWNLOAD_URLS.primary, CRX_DOWNLOAD_URLS.fallback]
  ) {
    const controller = new AbortController();
    const timer = setTimeout(() => controller.abort(), 300_000);
    try {
      const response = await fetch(builder(id, { os: "linux", arch: "x64" }), {
        credentials: "omit",
        signal: controller.signal,
        headers: { Accept: "application/x-chrome-extension" },
      });
      if (!response.ok || !response.body) {
        await response.body?.cancel();
        throw new Error(`Store response ${response.status}`);
      }
      const reader = response.body.getReader();
      let size = 0;
      const chunks: Uint8Array[] = [];
      try {
        while (true) {
          const { value, done } = await reader.read();
          if (done) break;
          size += value.byteLength;
          if (size > MAX_CRX_SIZE) throw new Error("CRX package size limit");
          chunks.push(value);
        }
      } finally {
        await reader.cancel();
        reader.releaseLock();
      }
      const bytes = new Uint8Array(size);
      let offset = 0;
      for (const chunk of chunks) {
        bytes.set(chunk, offset);
        offset += chunk.byteLength;
      }
      return bytes;
    } catch (error) {
      failure = error;
    } finally {
      clearTimeout(timer);
    }
  }
  throw failure;
}

async function freeze(selection: CorpusSelection): Promise<void> {
  const file = new URL(`packages/${selection.id}.crx`, destination);
  const record: Record<string, unknown> = {
    ...selection,
    status: "unavailable",
  };
  try {
    let bytes: Uint8Array<ArrayBuffer>;
    try {
      bytes = await Deno.readFile(file);
    } catch (error) {
      if (!(error instanceof Deno.errors.NotFound)) throw error;
      bytes = await download(selection.id);
    }
    const verified = await verifyCRX(bytes.buffer, selection.id);
    await Deno.writeFile(file, bytes, { mode: 0o600 });
    let total = 0;
    let entryCount = 0;
    const inventory = ["debugger", "network"].includes(selection.area);
    const contents = unzipSync(new Uint8Array(verified.zipData), {
      filter(entry) {
        total += entry.originalSize;
        entryCount++;
        if (
          entryCount > MAX_CRX_ENTRIES || entry.originalSize > MAX_FILE_SIZE ||
          total > MAX_EXPANDED_CRX_SIZE
        ) {
          throw new Error("Archive expansion limit");
        }
        return entry.name === "manifest.json" ||
          /^_locales\/[^/]+\/messages\.json$/.test(entry.name) ||
          (inventory && /\.(?:m?js)$/.test(entry.name));
      },
    });
    const manifest: unknown = JSON.parse(
      new TextDecoder().decode(contents["manifest.json"]),
    );
    if (!validateManifest(manifest)) throw new Error("Invalid source manifest");
    let resolvedName = manifest.name;
    const message = /^__MSG_(.+)__$/i.exec(resolvedName);
    if (message) {
      const locale = typeof manifest.default_locale === "string"
        ? manifest.default_locale
        : "en";
      const localeBytes = contents[`_locales/${locale}/messages.json`] ||
        contents["_locales/en/messages.json"];
      if (!localeBytes) {
        throw new Error("Default extension name locale is missing");
      }
      const messages = JSON.parse(
        new TextDecoder().decode(localeBytes),
      ) as Record<string, { message?: string }>;
      resolvedName = Object.entries(messages).find(([key]) =>
        key.toLowerCase() === message[1].toLowerCase()
      )?.[1].message || "";
    }
    record.manifestName = resolvedName;
    const normalizeName = (name: string) =>
      name.toLowerCase().replace(/[^a-z0-9]/g, "");
    const accepted = selection.acceptedManifestNames || [selection.name];
    if (
      !resolvedName ||
      !accepted.some((name) =>
        normalizeName(resolvedName).includes(normalizeName(name))
      )
    ) {
      throw new Error(`Selected product name mismatch: ${resolvedName}`);
    }
    const commands = new Set<string>();
    const calls = new Set<string>();
    if (inventory) {
      const directory = new URL(`inventory/${selection.id}/`, destination);
      await Deno.mkdir(directory, { recursive: true });
      for (const [name, content] of Object.entries(contents)) {
        if (
          name.split(/[\\/]/).some((part) => part === "..") ||
          name.startsWith("/")
        ) {
          throw new Error("Unsafe archive path");
        }
        const text = new TextDecoder().decode(content);
        for (
          const match of text.matchAll(
            /["']((?:Runtime|Page|DOM|Network|Input|Target)\.[A-Za-z][A-Za-z0-9]*)["']/g,
          )
        ) {
          commands.add(match[1]);
        }
        for (
          const match of text.matchAll(
            /(?:chrome|browser)\.([A-Za-z]+\.[A-Za-z]+)/g,
          )
        ) {
          calls.add(match[1]);
        }
        const target = new URL(name, directory);
        if (!target.href.startsWith(directory.href)) {
          throw new Error("Unsafe archive URL");
        }
        await Deno.mkdir(new URL("./", target), { recursive: true });
        await Deno.writeFile(target, content);
      }
    }
    Object.assign(record, {
      status: "frozen",
      version: manifest.version,
      packageSha256: await fingerprint(bytes),
      packageBytes: bytes.byteLength,
      manifestVersion: manifest.manifest_version,
      permissions: manifest.permissions ?? [],
      optionalPermissions: manifest.optional_permissions ?? [],
      background: manifest.background,
      signature: {
        format: verified.version,
        algorithm: verified.algorithm,
        publicKeySha256: verified.publicKeySha256,
      },
      cdpStringInventory: [...commands].sort(),
      apiCallStringInventory: [...calls].sort(),
      primaryOperationResult: "not-run",
    });
  } catch (error) {
    record.error = String(error);
    record.primaryOperationResult = "blocked-package";
  }
  records.push(record);
  console.log(record.status, selection.name, record.version ?? record.error);
}

for (let index = 0; index < seed.extensions.length; index += 3) {
  await Promise.all(seed.extensions.slice(index, index + 3).map(freeze));
}
const report = {
  schemaVersion: 1,
  frozenAtUTC: new Date().toISOString(),
  referenceChromeVersion: STORE_DOWNLOAD_CLIENT_VERSION,
  selectionSha256: await fingerprint(seedBytes),
  qualified: false,
  primaryOperationPassRate: null,
  extensions: seed.extensions.map((selection) =>
    records.find((record) => record.id === selection.id)
  ),
};
await Deno.writeTextFile(
  new URL("corpus-lock.json", destination),
  JSON.stringify(report, null, 2) + "\n",
);
console.log("Corpus lock:", new URL("corpus-lock.json", destination).pathname);
