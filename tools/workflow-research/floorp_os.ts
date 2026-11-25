// SPDX-License-Identifier: MPL-2.0

import { createOsTestClient } from "../src/os_test_client.ts";
import type { ContextChunk } from "./types.ts";

function isRecord(value: unknown): value is Record<string, unknown> {
  return typeof value === "object" && value !== null && !Array.isArray(value);
}

function stringField(value: unknown): string {
  return typeof value === "string" ? value : "";
}

export function workspaceStateChunk(body: unknown): ContextChunk {
  const lines: string[] = [];
  if (isRecord(body)) {
    lines.push(`currentId=${stringField(body.currentId) || "null"}`);
    if (Array.isArray(body.workspaces)) {
      lines.push("workspaces:");
      for (const workspace of body.workspaces) {
        if (!isRecord(workspace)) continue;
        const selected = workspace.isSelected === true ? " selected" : "";
        lines.push(
          `- ${stringField(workspace.name) || "(unnamed)"} ${
            stringField(workspace.id)
          }${selected}`,
        );
      }
    }
  }
  const text = lines.length > 0
    ? lines.join("\n")
    : JSON.stringify(body).slice(0, 1000);
  return {
    id: "live-workspace-state",
    source: "browser-state",
    title: "Live Floorp workspace state",
    text,
    tokenEstimate: Math.ceil(text.length / 4),
  };
}

export function browserContextSummaryChunk(body: unknown): ContextChunk {
  const lines: string[] = [];
  if (isRecord(body)) {
    for (const key of ["tabs", "history", "downloads"] as const) {
      const value = body[key];
      lines.push(`${key}: ${Array.isArray(value) ? value.length : 0} entries`);
    }
    if (Array.isArray(body.tabs)) {
      for (const tab of body.tabs.slice(0, 8)) {
        if (!isRecord(tab)) continue;
        lines.push(
          `tab: ${stringField(tab.title) || "(untitled)"} ${
            stringField(tab.url)
          }`,
        );
      }
    }
  }
  const text = lines.length > 0
    ? lines.join("\n")
    : JSON.stringify(body).slice(0, 1000);
  return {
    id: "live-browser-context-summary",
    source: "browser-state",
    title: "Live Floorp browser context summary",
    text,
    tokenEstimate: Math.ceil(text.length / 4),
  };
}

export async function fetchFloorpBrowserStateChunks(
  baseUrl: string,
  tokenFile?: string,
): Promise<ContextChunk[]> {
  const client = await createOsTestClient({ baseUrl, tokenFile });
  const workspaces = await client.request("/workspaces");
  const context = await client.request("/browser/context");
  if (workspaces.status !== 200 || context.status !== 200) {
    throw new Error("Floorp OS browser-state endpoints did not return 200");
  }
  return [
    workspaceStateChunk(workspaces.body),
    browserContextSummaryChunk(context.body),
  ];
}
