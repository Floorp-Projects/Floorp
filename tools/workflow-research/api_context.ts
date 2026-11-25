// SPDX-License-Identifier: MPL-2.0

import type { ContextChunk } from "./types.ts";

const HTTP_METHODS = new Set(["get", "post", "put", "patch", "delete"]);

function sanitizeId(value: string): string {
  return value.replace(/[^A-Za-z0-9_-]+/g, "-").replace(/^-+|-+$/g, "");
}

function scalarAfterColon(line: string): string {
  const index = line.indexOf(":");
  return index < 0 ? "" : line.slice(index + 1).trim().replace(/^"|"$/g, "");
}

/** Extracts operation-level chunks without requiring a YAML dependency. */
export function chunkOpenApiSpec(yamlText: string): ContextChunk[] {
  const lines = yamlText.split(/\r?\n/);
  const chunks: ContextChunk[] = [];
  let inPaths = false;
  let currentPath = "";

  for (let index = 0; index < lines.length; index++) {
    const line = lines[index];
    if (line === "paths:") {
      inPaths = true;
      continue;
    }
    if (inPaths && line === "components:") break;
    if (!inPaths) continue;

    const pathMatch = line.match(/^ {2}(\/[^:]+):$/);
    if (pathMatch) {
      currentPath = pathMatch[1];
      continue;
    }

    const methodMatch = line.match(/^ {4}([a-z]+):$/);
    if (!methodMatch || !HTTP_METHODS.has(methodMatch[1]) || !currentPath) {
      continue;
    }

    const method = methodMatch[1].toUpperCase();
    const end = Math.min(lines.length, index + 28);
    const block = lines.slice(index + 1, end);
    const summary = block.find((entry) =>
      entry.trimStart().startsWith("summary:")
    );
    const operationId = block.find((entry) =>
      entry.trimStart().startsWith("operationId:")
    );
    const description = block.find((entry) =>
      entry.trimStart().startsWith("description:")
    );
    const operationName = operationId
      ? scalarAfterColon(operationId)
      : `${method.toLowerCase()}-${sanitizeId(currentPath)}`;
    const text = [
      `${method} ${currentPath}`,
      operationName ? `operationId: ${operationName}` : "",
      summary ? `summary: ${scalarAfterColon(summary)}` : "",
      description ? `description: ${scalarAfterColon(description)}` : "",
    ].filter(Boolean).join("\n");

    chunks.push({
      id: `api-${sanitizeId(operationName)}`,
      source: "api-schema",
      title: `${method} ${currentPath}`,
      text,
      tokenEstimate: Math.ceil(text.length / 4),
    });
  }
  return chunks;
}

export async function loadOpenApiChunks(path: string): Promise<ContextChunk[]> {
  return chunkOpenApiSpec(await Deno.readTextFile(path));
}
