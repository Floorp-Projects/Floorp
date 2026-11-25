// SPDX-License-Identifier: MPL-2.0

import {
  type MissingSource,
  type ValidationDiagnostic,
  type Workflow,
  WORKFLOW_VERSION,
  type WorkflowStep,
} from "./types.ts";

function isRecord(value: unknown): value is Record<string, unknown> {
  return typeof value === "object" && value !== null && !Array.isArray(value);
}

function diagnostic(
  code: string,
  message: string,
  missingSource: MissingSource,
): ValidationDiagnostic {
  return { code, message, severity: "error", missingSource };
}

function requireString(
  step: Record<string, unknown>,
  key: string,
  action: string,
  index: number,
): ValidationDiagnostic | null {
  if (typeof step[key] !== "string" || String(step[key]).length === 0) {
    return diagnostic(
      "WFC1002",
      `steps[${index}]: ${action}.${key} must be a nonempty string`,
      "api-schema",
    );
  }
  return null;
}

function requireOptionalNumber(
  step: Record<string, unknown>,
  key: string,
  action: string,
  index: number,
): ValidationDiagnostic | null {
  if (step[key] !== undefined && typeof step[key] !== "number") {
    return diagnostic(
      "WFC1003",
      `steps[${index}]: ${action}.${key} must be a number when present`,
      "api-schema",
    );
  }
  return null;
}

const STRING_FIELDS: Record<string, string[]> = {
  "tabs.create": ["url"],
  "tabs.navigate": ["url"],
  "page.click": ["fingerprint"],
  "page.fill": ["fingerprint", "value"],
  "page.waitForText": ["text"],
  "page.extractText": ["fingerprint", "as"],
  "page.assertText": ["text"],
  "workspace.switch": ["workspaceId"],
};

function stringValue(step: Record<string, unknown>, key: string): string {
  return step[key] as string;
}

function parseStep(
  rawStep: unknown,
  index: number,
): { step: WorkflowStep | null; diagnostics: ValidationDiagnostic[] } {
  const diagnostics: ValidationDiagnostic[] = [];
  if (!isRecord(rawStep)) {
    return {
      step: null,
      diagnostics: [
        diagnostic(
          "WFC1005",
          `steps[${index}] must be an object`,
          "api-schema",
        ),
      ],
    };
  }
  const action = rawStep.action;
  if (typeof action !== "string" || !(action in STRING_FIELDS)) {
    return {
      step: null,
      diagnostics: [diagnostic(
        "WFC2304",
        `steps[${index}].action is unknown: ${String(action)}`,
        "api-schema",
      )],
    };
  }

  for (const field of STRING_FIELDS[action]) {
    const error = requireString(rawStep, field, action, index);
    if (error) diagnostics.push(error);
  }
  if (action === "page.waitForText") {
    const error = requireOptionalNumber(rawStep, "timeoutMs", action, index);
    if (error) diagnostics.push(error);
  }
  if (diagnostics.length > 0) return { step: null, diagnostics };

  if (action === "tabs.create") {
    return { step: { action, url: stringValue(rawStep, "url") }, diagnostics };
  }
  if (action === "tabs.navigate") {
    return { step: { action, url: stringValue(rawStep, "url") }, diagnostics };
  }
  if (action === "page.click") {
    return {
      step: { action, fingerprint: stringValue(rawStep, "fingerprint") },
      diagnostics,
    };
  }
  if (action === "page.fill") {
    return {
      step: {
        action,
        fingerprint: stringValue(rawStep, "fingerprint"),
        value: stringValue(rawStep, "value"),
      },
      diagnostics,
    };
  }
  if (action === "page.waitForText") {
    return {
      step: {
        action,
        text: stringValue(rawStep, "text"),
        ...(typeof rawStep.timeoutMs === "number"
          ? { timeoutMs: rawStep.timeoutMs }
          : {}),
      },
      diagnostics,
    };
  }
  if (action === "page.extractText") {
    return {
      step: {
        action,
        fingerprint: stringValue(rawStep, "fingerprint"),
        as: stringValue(rawStep, "as"),
      },
      diagnostics,
    };
  }
  if (action === "page.assertText") {
    return {
      step: { action, text: stringValue(rawStep, "text") },
      diagnostics,
    };
  }
  return {
    step: {
      action: "workspace.switch",
      workspaceId: stringValue(rawStep, "workspaceId"),
    },
    diagnostics,
  };
}

export function validateWorkflow(value: unknown): {
  workflow: Workflow | null;
  diagnostics: ValidationDiagnostic[];
} {
  const diagnostics: ValidationDiagnostic[] = [];
  if (!isRecord(value)) {
    return {
      workflow: null,
      diagnostics: [
        diagnostic("WFC1000", "workflow must be an object", "api-schema"),
      ],
    };
  }
  if (value.version !== WORKFLOW_VERSION) {
    diagnostics.push(diagnostic(
      "WFC1001",
      `workflow.version must be ${WORKFLOW_VERSION}`,
      "api-schema",
    ));
  }
  if (!Array.isArray(value.steps)) {
    diagnostics.push(
      diagnostic("WFC1004", "workflow.steps must be an array", "api-schema"),
    );
    return { workflow: null, diagnostics };
  }

  const steps: WorkflowStep[] = [];
  value.steps.forEach((rawStep, index) => {
    const parsed = parseStep(rawStep, index);
    diagnostics.push(...parsed.diagnostics);
    if (parsed.step) steps.push(parsed.step);
  });

  if (diagnostics.length > 0) return { workflow: null, diagnostics };
  return { workflow: { version: WORKFLOW_VERSION, steps }, diagnostics };
}

export function firstValidationDiagnostic(
  diagnostics: ValidationDiagnostic[],
): ValidationDiagnostic | null {
  return diagnostics.find((entry) => entry.severity === "error") ?? null;
}
