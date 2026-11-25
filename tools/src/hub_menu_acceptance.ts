// SPDX-License-Identifier: MPL-2.0

import { createHash } from "node:crypto";
import * as path from "@std/path";

export function isHubDocumentUrl(url: string): boolean {
  return url.split("#", 1)[0] === "about:hub";
}

export async function hashDirectoryContents(root: string): Promise<string> {
  const entries: { path: string; type: "directory" | "file" | "symlink" }[] =
    [];
  const visit = async (directory: string, relative: string): Promise<void> => {
    for await (const entry of Deno.readDir(directory)) {
      const childRelative = relative ? `${relative}/${entry.name}` : entry.name;
      const childPath = path.join(directory, entry.name);
      const type = entry.isDirectory
        ? "directory"
        : entry.isSymlink
        ? "symlink"
        : "file";
      entries.push({ path: childRelative, type });
      if (type === "directory") await visit(childPath, childRelative);
    }
  };
  await visit(root, "");
  entries.sort((left, right) =>
    left.path < right.path ? -1 : left.path > right.path ? 1 : 0
  );
  const digest = createHash("sha256");
  for (const entry of entries) {
    digest.update(`${entry.path}\0${entry.type}\0`);
    if (entry.type === "file") {
      const file = await Deno.open(path.join(root, entry.path), { read: true });
      try {
        const buffer = new Uint8Array(1024 * 1024);
        while (true) {
          const count = await file.read(buffer);
          if (count === null) break;
          digest.update(buffer.subarray(0, count));
        }
      } finally {
        file.close();
      }
    } else if (entry.type === "symlink") {
      digest.update(await Deno.readLink(path.join(root, entry.path)));
    }
    digest.update("\0");
  }
  return digest.digest("hex");
}

function sameBytes(left: Uint8Array, right: Uint8Array): boolean {
  return left.length === right.length &&
    left.every((byte, index) => byte === right[index]);
}

export async function withExactSourceRestoration<T>(
  sourcePath: string,
  mutate: (source: string) => string,
  duringMutation: () => Promise<T>,
  afterRestore: () => Promise<void>,
): Promise<T> {
  const originalBytes = await Deno.readFile(sourcePath);
  const originalText = new TextDecoder().decode(originalBytes);
  const mutatedText = mutate(originalText);
  if (mutatedText === originalText) {
    throw new Error("source mutation did not change the file");
  }

  let result: T | undefined;
  let completed = false;
  let operationError: unknown;
  let operationFailed = false;
  let restorationError: unknown;
  let restorationFailed = false;
  try {
    await Deno.writeFile(sourcePath, new TextEncoder().encode(mutatedText));
    result = await duringMutation();
    completed = true;
  } catch (error) {
    operationError = error;
    operationFailed = true;
  } finally {
    try {
      await Deno.writeFile(sourcePath, originalBytes);
      const restoredBytes = await Deno.readFile(sourcePath);
      if (!sameBytes(originalBytes, restoredBytes)) {
        restorationError = new Error("source bytes differ after restoration");
        restorationFailed = true;
      } else {
        await afterRestore();
      }
    } catch (error) {
      restorationError = error;
      restorationFailed = true;
    }
  }
  if (operationFailed && restorationFailed) {
    throw new AggregateError(
      [operationError, restorationError],
      "source mutation and restoration both failed",
    );
  }
  if (operationFailed) throw operationError;
  if (restorationFailed) throw restorationError;
  if (!completed) throw new Error("source mutation did not complete");
  return result as T;
}
