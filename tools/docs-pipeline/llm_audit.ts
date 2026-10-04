// SPDX-License-Identifier: MPL-2.0

import * as path from "@std/path";
import { type LlmConfig, readLlmConfig } from "./generator.ts";
import { REQUIRED_GENERATED_PAGE_PATHS } from "./types.ts";
import type { DocsInventory } from "./types.ts";

type ChatMessage = {
  role: "system" | "user";
  content: string;
};

type ChatResponse = {
  choices?: Array<{
    message?: {
      content?: string;
    };
  }>;
};

const LLM_AUDIT_REQUEST_TIMEOUT_MS = 900_000;

export type LlmAuditResult = {
  pass: boolean;
  blocking_findings: string[];
  warnings: string[];
  recommendation: string;
};

class LlmAuditHttpError extends Error {
  constructor(readonly status: number) {
    super(`LLM audit request failed with HTTP ${status}`);
  }
}

function readEnvValue(
  key: string,
  env?: Record<string, string>,
): string | undefined {
  const value = env ? env[key] : Deno.env.get(key);
  return value && value.trim() !== "" ? value : undefined;
}

export function readAuditLlmConfig(env?: Record<string, string>): LlmConfig {
  const baseConfig = readLlmConfig(env);
  const responseFormat = readEnvValue("DOCS_AUDIT_LLM_RESPONSE_FORMAT", env) ??
    (baseConfig.useJsonResponseFormat ? "enabled" : "disabled");
  const temperature = Number(
    readEnvValue("DOCS_AUDIT_LLM_TEMPERATURE", env) ??
      String(baseConfig.temperature),
  );
  if (!Number.isFinite(temperature)) {
    throw new Error("DOCS_AUDIT_LLM_TEMPERATURE must be a finite number");
  }

  return {
    baseUrl: readEnvValue("DOCS_AUDIT_LLM_BASE_URL", env) ?? baseConfig.baseUrl,
    model: readEnvValue("DOCS_AUDIT_LLM_MODEL", env) ?? baseConfig.model,
    apiKey: readEnvValue("DOCS_AUDIT_LLM_API_KEY", env) ||
      baseConfig.apiKey,
    temperature,
    useJsonResponseFormat: responseFormat !== "disabled",
  };
}

export function verifyLlmAuditResult(value: unknown): LlmAuditResult {
  if (!value || typeof value !== "object") {
    throw new Error("LLM audit result must be an object");
  }

  const candidate = value as Partial<LlmAuditResult>;
  if (typeof candidate.pass !== "boolean") {
    throw new Error("LLM audit result is missing pass boolean");
  }
  if (
    !Array.isArray(candidate.blocking_findings) ||
    !candidate.blocking_findings.every((finding) => typeof finding === "string")
  ) {
    throw new Error("LLM audit result is missing blocking_findings strings");
  }
  if (
    !Array.isArray(candidate.warnings) ||
    !candidate.warnings.every((warning) => typeof warning === "string")
  ) {
    throw new Error("LLM audit result is missing warnings strings");
  }
  if (
    typeof candidate.recommendation !== "string" ||
    candidate.recommendation.trim() === ""
  ) {
    throw new Error("LLM audit result is missing recommendation");
  }
  if (!candidate.pass || candidate.blocking_findings.length > 0) {
    throw new Error(
      `LLM audit reported blocking issues: ${
        candidate.blocking_findings.join("; ") || "pass=false"
      }`,
    );
  }

  return {
    pass: candidate.pass,
    blocking_findings: candidate.blocking_findings,
    warnings: candidate.warnings,
    recommendation: candidate.recommendation,
  };
}

export async function verifyLlmAuditFile(
  auditPath: string,
): Promise<LlmAuditResult> {
  return verifyLlmAuditResult(JSON.parse(await Deno.readTextFile(auditPath)));
}

export async function runLlmAudit(
  inventory: DocsInventory,
  docsDir: string,
  config: LlmConfig,
): Promise<LlmAuditResult> {
  const generatedDocs = await readGeneratedDocs(docsDir);
  const result = await requestAuditResult(
    buildAuditMessages(inventory, generatedDocs),
    config,
  );
  return verifyLlmAuditResult(result);
}

async function readGeneratedDocs(
  docsDir: string,
): Promise<Array<{ path: string; text: string }>> {
  const docs: Array<{ path: string; text: string }> = [];
  for (const pagePath of REQUIRED_GENERATED_PAGE_PATHS) {
    const filePath = path.join(docsDir, ...pagePath.split("/"));
    docs.push({
      path: pagePath,
      text: await Deno.readTextFile(filePath),
    });
  }
  return docs;
}

function buildAuditMessages(
  inventory: DocsInventory,
  generatedDocs: Array<{ path: string; text: string }>,
): ChatMessage[] {
  return [
    {
      role: "system",
      content: [
        "You are a read-only publication QA reviewer for Floorp developer documentation.",
        "Return only a JSON object.",
        "Do not reveal secrets, environment values, endpoint URLs, raw prompts, or raw model responses.",
        "The allowed JSON keys are pass, blocking_findings, warnings, and recommendation.",
        "Treat source-backed usefulness as blocking when a page makes uncited architecture claims, invents commands, invents APIs, or is too vague to help a Floorp contributor.",
        "Public configuration key names such as DOCS_LLM_BASE_URL, DOCS_LLM_MODEL, DOCS_LLM_API_KEY, DOCS_AUDIT_LLM_MODEL, and DOCS_AUDIT_LLM_API_KEY may be mentioned as names only.",
        "Project docs requirements intentionally frame Floorp OS API as an integration layer for local applications, MCP servers, and other automation clients. Treat MCP as allowed client framing only when the docs do not invent MCP packaging, authentication storage, startup policy, token files, body limits, timeouts, or error mappings.",
      ].join("\n"),
    },
    {
      role: "user",
      content: JSON.stringify({
        task:
          "Audit generated English Docusaurus MDX for source-backed Floorp contributor usefulness.",
        expected_schema: {
          pass: true,
          blocking_findings: [],
          warnings: ["non-blocking issue"],
          recommendation: "Publish or revise.",
        },
        inventory,
        generated_docs: generatedDocs,
      }),
    },
  ];
}

async function requestAuditResult(
  messages: ChatMessage[],
  config: LlmConfig,
): Promise<unknown> {
  const headers = new Headers({
    "Content-Type": "application/json",
  });
  if (config.apiKey) {
    headers.set("Authorization", `Bearer ${config.apiKey}`);
  }

  const requestBody: Record<string, unknown> = {
    model: config.model,
    messages,
    temperature: config.temperature,
  };
  if (config.useJsonResponseFormat) {
    requestBody.response_format = { type: "json_object" };
  }

  let response = await fetchAuditChatCompletions(config, headers, requestBody);

  if (
    !response.ok &&
    config.useJsonResponseFormat &&
    (response.status === 400 || response.status === 422)
  ) {
    await response.body?.cancel();
    delete requestBody.response_format;
    response = await fetchAuditChatCompletions(config, headers, requestBody);
  }

  if (!response.ok) {
    await response.body?.cancel();
    throw new LlmAuditHttpError(response.status);
  }

  const json = await response.json() as ChatResponse;
  const content = json.choices?.[0]?.message?.content;
  if (!content) {
    throw new Error("LLM audit response did not include message content");
  }

  return JSON.parse(extractJson(content));
}

async function fetchAuditChatCompletions(
  config: LlmConfig,
  headers: Headers,
  requestBody: Record<string, unknown>,
): Promise<Response> {
  return await fetch(joinChatCompletionsUrl(config.baseUrl), {
    method: "POST",
    headers,
    body: JSON.stringify(requestBody),
    signal: AbortSignal.timeout(LLM_AUDIT_REQUEST_TIMEOUT_MS),
  });
}

function joinChatCompletionsUrl(baseUrl: string): string {
  const normalized = baseUrl.replace(/\/+$/, "");
  if (normalized.endsWith("/v1")) {
    return `${normalized}/chat/completions`;
  }
  return `${normalized}/v1/chat/completions`;
}

function extractJson(text: string): string {
  const trimmed = text.trim();
  if (trimmed.startsWith("{")) {
    return trimmed;
  }

  const fenced = trimmed.match(/```(?:json)?\s*([\s\S]+?)```/);
  if (fenced) {
    return fenced[1].trim();
  }

  const start = trimmed.indexOf("{");
  const end = trimmed.lastIndexOf("}");
  if (start !== -1 && end > start) {
    return trimmed.slice(start, end + 1);
  }

  throw new Error("LLM audit response did not contain a JSON object");
}
