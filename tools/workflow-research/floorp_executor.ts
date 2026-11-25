// SPDX-License-Identifier: MPL-2.0

import type {
  ExecutionResult,
  MissingSource,
  OracleSpec,
  Workflow,
} from "./types.ts";

export interface FloorpRequester {
  request(
    path: string,
    init?: RequestInit,
  ): Promise<{ status: number; body: unknown }>;
}

export interface FloorpExecutorOptions {
  sleep?: (ms: number) => Promise<void>;
  cleanup?: boolean;
}

function isRecord(value: unknown): value is Record<string, unknown> {
  return typeof value === "object" && value !== null && !Array.isArray(value);
}

function stringField(body: unknown, key: string): string | null {
  if (!isRecord(body)) return null;
  const value = body[key];
  return typeof value === "string" ? value : null;
}

function booleanField(body: unknown, key: string): boolean {
  return isRecord(body) && body[key] === true;
}

function resultFailure(
  kind: "runtime",
  message: string,
  missingSource: MissingSource,
  state: {
    finalUrl: string;
    activeWorkspaceId: string | null;
    extracted: Record<string, string>;
  },
): ExecutionResult {
  return {
    success: false,
    failureKind: kind,
    message,
    missingSource,
    extracted: { ...state.extracted },
    finalUrl: state.finalUrl,
    activeWorkspaceId: state.activeWorkspaceId,
  };
}

function encode(value: string): string {
  return encodeURIComponent(value);
}

export async function executeWorkflowOnFloorpOs(
  workflow: Workflow,
  client: FloorpRequester,
  oracle: OracleSpec,
  options: FloorpExecutorOptions = {},
): Promise<ExecutionResult> {
  const sleep = options.sleep ??
    ((ms: number) => new Promise<void>((resolve) => setTimeout(resolve, ms)));
  const cleanup = options.cleanup ?? true;
  const state = {
    finalUrl: "about:blank",
    activeWorkspaceId: null as string | null,
    extracted: {} as Record<string, string>,
  };
  let instanceId: string | null = null;
  let createdInstance = false;

  async function request(
    path: string,
    method: string,
    body: unknown,
    missingSource: MissingSource,
  ): Promise<{ status: number; body: unknown } | ExecutionResult> {
    const init: RequestInit = { method };
    if (body !== undefined) {
      init.headers = { "content-type": "application/json" };
      init.body = JSON.stringify(body);
    }
    const response = await client.request(path, init);
    if (response.status < 200 || response.status >= 300) {
      return resultFailure(
        "runtime",
        `${method} ${path} returned HTTP ${response.status}`,
        missingSource,
        state,
      );
    }
    return response;
  }

  function isFailure(
    value: { status: number; body: unknown } | ExecutionResult,
  ): value is ExecutionResult {
    return "failureKind" in value;
  }

  function requireInstance(): string | ExecutionResult {
    if (instanceId) return instanceId;
    return resultFailure(
      "runtime",
      "workflow uses a page action before tabs.create or tabs.navigate",
      "api-schema",
      state,
    );
  }

  try {
    for (const step of workflow.steps) {
      if (step.action === "tabs.create") {
        const response = await request("/tabs/instances", "POST", {
          url: step.url,
          inBackground: false,
        }, "api-schema");
        if (isFailure(response)) return response;
        instanceId = stringField(response.body, "instanceId");
        if (!instanceId) {
          return resultFailure(
            "runtime",
            "tabs.instances response lacks instanceId",
            "api-schema",
            state,
          );
        }
        createdInstance = true;
        state.finalUrl = step.url;
        continue;
      }

      if (step.action === "workspace.switch") {
        const response = await request(
          `/workspaces/${encode(step.workspaceId)}/switch`,
          "POST",
          undefined,
          "browser-state",
        );
        if (isFailure(response)) return response;
        if (!booleanField(response.body, "success")) {
          return resultFailure(
            "runtime",
            `workspace switch failed: ${step.workspaceId}`,
            "browser-state",
            state,
          );
        }
        state.activeWorkspaceId = step.workspaceId;
        continue;
      }

      const id = requireInstance();
      if (typeof id !== "string") return id;

      if (step.action === "tabs.navigate") {
        const response = await request(
          `/tabs/instances/${encode(id)}/navigate`,
          "POST",
          { url: step.url },
          "api-schema",
        );
        if (isFailure(response)) return response;
        state.finalUrl = step.url;
      } else if (step.action === "page.click") {
        const response = await request(
          `/tabs/instances/${encode(id)}/click`,
          "POST",
          { fingerprint: step.fingerprint },
          "dom-structure",
        );
        if (isFailure(response)) return response;
      } else if (step.action === "page.fill") {
        const response = await request(
          `/tabs/instances/${encode(id)}/fillForm`,
          "POST",
          { formData: { [step.fingerprint]: step.value } },
          "dom-structure",
        );
        if (isFailure(response)) return response;
      } else if (step.action === "page.waitForText") {
        const timeoutMs = step.timeoutMs ?? 1000;
        const started = Date.now();
        let found = false;
        while (Date.now() - started <= timeoutMs) {
          const response = await client.request(
            `/tabs/instances/${encode(id)}/elementByText?text=${
              encode(step.text)
            }`,
          );
          if (
            response.status >= 200 && response.status < 300 &&
            stringField(response.body, "element")
          ) {
            found = true;
            break;
          }
          await sleep(100);
        }
        if (!found) {
          return resultFailure(
            "runtime",
            `text did not appear: ${step.text}`,
            "page-text",
            state,
          );
        }
      } else if (step.action === "page.extractText") {
        const response = await client.request(
          `/tabs/instances/${encode(id)}/elementTextContent?fingerprint=${
            encode(step.fingerprint)
          }`,
        );
        if (response.status < 200 || response.status >= 300) {
          return resultFailure(
            "runtime",
            `extract fingerprint failed: ${step.fingerprint}`,
            "dom-structure",
            state,
          );
        }
        const text = stringField(response.body, "text");
        if (!text) {
          return resultFailure(
            "runtime",
            `extract fingerprint not found or empty: ${step.fingerprint}`,
            "dom-structure",
            state,
          );
        }
        state.extracted[step.as] = text;
      } else if (step.action === "page.assertText") {
        const response = await client.request(
          `/tabs/instances/${encode(id)}/elementByText?text=${
            encode(step.text)
          }`,
        );
        if (
          response.status < 200 || response.status >= 300 ||
          !stringField(response.body, "element")
        ) {
          return resultFailure(
            "runtime",
            `assertion text is absent: ${step.text}`,
            "page-text",
            state,
          );
        }
      }
    }

    let success = false;
    let message = "oracle mismatch";
    let missingSource: MissingSource = "page-text";
    if (oracle.type === "extraction") {
      const value = state.extracted[oracle.as] ?? "";
      success = value.includes(oracle.includes);
      message = success
        ? "success"
        : `extracted ${oracle.as} did not include ${oracle.includes}`;
    } else if (oracle.type === "workspace") {
      success = state.activeWorkspaceId === oracle.workspaceId;
      missingSource = "browser-state";
      message = success
        ? "success"
        : `active workspace ${
          String(state.activeWorkspaceId)
        } != ${oracle.workspaceId}`;
    } else {
      const id = requireInstance();
      if (typeof id !== "string") return id;
      const response = await client.request(
        `/tabs/instances/${encode(id)}/html`,
      );
      const html = stringField(response.body, "html") ?? "";
      success = response.status >= 200 && response.status < 300 &&
        html.includes(oracle.includes);
      message = success
        ? "success"
        : `page html did not include ${oracle.includes}`;
    }

    return {
      success,
      failureKind: success ? "none" : "oracle",
      message,
      missingSource: success ? "none" : missingSource,
      extracted: { ...state.extracted },
      finalUrl: state.finalUrl,
      activeWorkspaceId: state.activeWorkspaceId,
    };
  } finally {
    if (cleanup && createdInstance && instanceId) {
      try {
        await client.request(`/tabs/instances/${encode(instanceId)}`, {
          method: "DELETE",
        });
      } catch {
        // Cleanup is best-effort; execution results must not expose token-bearing transport errors.
      }
    }
  }
}
