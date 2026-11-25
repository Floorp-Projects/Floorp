// SPDX-License-Identifier: MPL-2.0

import { correctWorkflow, DOCS_URL, SEARCH_URL } from "./fixtures.ts";
import type {
  GenerationInput,
  GenerationOutput,
  WorkflowGenerator,
} from "./types.ts";
import { WORKFLOW_VERSION } from "./types.ts";

function hasContext(input: GenerationInput, id: string): boolean {
  return input.contexts.some((chunk) => chunk.id === id);
}

function contextTokens(input: GenerationInput): number {
  return input.contexts.reduce((sum, chunk) => sum + chunk.tokenEstimate, 0);
}

function output(
  input: GenerationInput,
  workflow: unknown,
  notes: string[],
): GenerationOutput {
  const stepCount =
    typeof workflow === "object" && workflow !== null && "steps" in workflow &&
      Array.isArray(workflow.steps)
      ? workflow.steps.length
      : 1;
  return {
    workflow,
    generationTokens: 90 + contextTokens(input) + stepCount * 9,
    notes,
  };
}

function searchWorkflow(input: GenerationInput): unknown {
  if (!hasContext(input, "api-tabs-core")) {
    return {
      version: WORKFLOW_VERSION,
      steps: [
        { action: "browser.open", url: SEARCH_URL },
        { action: "page.type", selector: "#q", value: "floorp" },
        { action: "page.click", selector: "#submit" },
      ],
    };
  }
  if (!hasContext(input, "dom-search")) {
    return {
      version: WORKFLOW_VERSION,
      steps: [
        { action: "tabs.create", url: SEARCH_URL },
        { action: "page.fill", fingerprint: "#q", value: "floorp" },
        { action: "page.click", fingerprint: "#submit" },
        { action: "page.waitForText", text: "Floorp OS typed workflows" },
        {
          action: "page.extractText",
          fingerprint: ".result-title",
          as: "firstResult",
        },
      ],
    };
  }
  if (!hasContext(input, "text-search")) {
    return {
      version: WORKFLOW_VERSION,
      steps: [
        { action: "tabs.create", url: SEARCH_URL },
        {
          action: "page.fill",
          fingerprint: "fp-search-input",
          value: "floorp",
        },
        { action: "page.click", fingerprint: "fp-search-submit" },
        { action: "page.waitForText", text: "Search results for floorp" },
        {
          action: "page.extractText",
          fingerprint: "fp-first-result",
          as: "firstResult",
        },
      ],
    };
  }
  return correctWorkflow("fixture-search-extract");
}

function workspaceWorkflow(input: GenerationInput): unknown {
  if (!hasContext(input, "api-workspace")) {
    return {
      version: WORKFLOW_VERSION,
      steps: [
        { action: "browser.workspace.switch", name: "Research" },
        { action: "browser.open", url: DOCS_URL },
      ],
    };
  }
  if (!hasContext(input, "api-tabs-core")) {
    return {
      version: WORKFLOW_VERSION,
      steps: [
        { action: "workspace.switch", workspaceId: "research" },
        { action: "browser.open", url: DOCS_URL },
      ],
    };
  }
  if (!hasContext(input, "state-workspaces")) {
    return {
      version: WORKFLOW_VERSION,
      steps: [
        { action: "workspace.switch", workspaceId: "research" },
        { action: "tabs.create", url: DOCS_URL },
        { action: "page.assertText", text: "Floorp OS API" },
      ],
    };
  }
  if (!hasContext(input, "text-docs")) {
    return {
      version: WORKFLOW_VERSION,
      steps: [
        {
          action: "workspace.switch",
          workspaceId: "8b8f46c8-4c35-4e24-a2c7-b4fb2f821001",
        },
        { action: "tabs.create", url: DOCS_URL },
        { action: "page.assertText", text: "API Reference" },
      ],
    };
  }
  return correctWorkflow("fixture-workspace-docs");
}

export const scriptedSmallModelGenerator: WorkflowGenerator = {
  name: "scripted-small-model",
  generate(input) {
    const notes: string[] = [];
    if (input.round === 0) {
      notes.push("simulates a small generator with only the selected context");
    }
    if (input.feedback.length > 0) {
      notes.push(
        `uses feedback: ${input.feedback.at(-1)?.kind}/${
          input.feedback.at(-1)?.missingSource
        }`,
      );
    }
    const workflow = input.task.id === "fixture-search-extract"
      ? searchWorkflow(input)
      : input.task.id === "fixture-workspace-docs"
      ? workspaceWorkflow(input)
      : correctWorkflow(input.task.id);
    return output(input, workflow, notes);
  },
};
