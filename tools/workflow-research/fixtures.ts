// SPDX-License-Identifier: MPL-2.0

import type { ContextChunk, ResearchTask, Workflow } from "./types.ts";
import { WORKFLOW_VERSION } from "./types.ts";
import type { FixturePage } from "./executor.ts";

export const SEARCH_URL = "https://fixture.local/search";
export const DOCS_URL = "https://fixture.local/floorp-os-api";
export const HOME_URL = "https://fixture.local/home";
export const RESEARCH_WORKSPACE_ID = "8b8f46c8-4c35-4e24-a2c7-b4fb2f821001";
export const MAIN_WORKSPACE_ID = "85f8149f-c21d-4ea7-9239-8db576f63000";

function chunk(
  id: string,
  source: ContextChunk["source"],
  title: string,
  text: string,
): ContextChunk {
  return { id, source, title, text, tokenEstimate: Math.ceil(text.length / 4) };
}

export function fixturePages(): FixturePage[] {
  return [
    {
      url: HOME_URL,
      title: "Fixture Home",
      baseText: "Fixture Home",
      elements: [],
    },
    {
      url: SEARCH_URL,
      title: "Fixture Search",
      baseText: "Fixture Search\nQuery documentation examples",
      elements: [
        { fingerprint: "fp-search-input", text: "Search input", input: true },
        {
          fingerprint: "fp-search-submit",
          text: "Search",
          revealsText:
            "Results\nFloorp OS typed workflows\nWorkflow synthesis notes",
        },
        { fingerprint: "fp-first-result", text: "Floorp OS typed workflows" },
      ],
    },
    {
      url: DOCS_URL,
      title: "Floorp OS API Fixture",
      baseText:
        "Floorp OS API\nUse workspaces and tab instances with element fingerprints.",
      elements: [
        { fingerprint: "fp-docs-heading", text: "Floorp OS API" },
      ],
    },
  ];
}

export function fixtureTasks(): ResearchTask[] {
  return [
    {
      id: "fixture-search-extract",
      instruction:
        "Open the fixture search page, search for floorp, wait for the result, and extract the first result title as firstResult.",
      startUrl: SEARCH_URL,
      oracle: { type: "extraction", as: "firstResult", includes: "Floorp OS" },
      goldContextIds: ["api-tabs-core", "dom-search", "text-search"],
    },
    {
      id: "fixture-workspace-docs",
      instruction:
        "Switch to the existing Research workspace, open the Floorp OS API fixture documentation, and assert that the Floorp OS API heading is visible.",
      startUrl: DOCS_URL,
      oracle: { type: "workspace", workspaceId: RESEARCH_WORKSPACE_ID },
      goldContextIds: [
        "api-workspace",
        "state-workspaces",
        "api-tabs-core",
        "text-docs",
      ],
    },
  ];
}

export function fixtureContextChunks(): ContextChunk[] {
  return [
    chunk(
      "api-tabs-core",
      "api-schema",
      "Floorp tab/page operations",
      [
        "createTabInstance: POST /tabs/instances body {url}",
        "navigate: POST /tabs/instances/{id}/navigate body {url}",
        "click: POST /tabs/instances/{id}/click body {fingerprint}",
        "fillForm/input: use element fingerprints, not CSS selectors",
        "waitForElement: waits for a fingerprint",
        "elementText: extracts visible text for a fingerprint",
      ].join("\n"),
    ),
    chunk(
      "api-workspace",
      "api-schema",
      "Floorp workspace operations",
      [
        "listWorkspaces: GET /workspaces returns workspaces with UUID ids and currentId",
        "switchToWorkspace: POST /workspaces/{workspaceId}/switch",
        "workspaceId must be the UUID from listWorkspaces, not a display name",
      ].join("\n"),
    ),
    chunk(
      "state-workspaces",
      "browser-state",
      "Current workspace state",
      `currentId=${MAIN_WORKSPACE_ID}\nworkspaces:\n- Main ${MAIN_WORKSPACE_ID}\n- Research ${RESEARCH_WORKSPACE_ID}`,
    ),
    chunk(
      "dom-search",
      "dom-structure",
      "Fixture search interactive elements",
      [
        "fp-search-input: editable query input",
        "fp-search-submit: button that submits the query",
        "fp-first-result: first result title element",
      ].join("\n"),
    ),
    chunk(
      "text-search",
      "page-text",
      "Fixture search result text",
      "After submitting a floorp query, the page shows Results and the first result title is exactly: Floorp OS typed workflows",
    ),
    chunk(
      "text-docs",
      "page-text",
      "Floorp OS API documentation visible text",
      "The documentation page contains the exact heading: Floorp OS API",
    ),
    chunk(
      "dom-docs",
      "dom-structure",
      "Floorp OS API documentation elements",
      "fp-docs-heading: documentation heading element",
    ),
    chunk(
      "noise-history",
      "browser-state",
      "Recent history",
      "Example history entries unrelated to the fixture task.",
    ),
    chunk(
      "noise-downloads",
      "browser-state",
      "Recent downloads",
      "Example download entries unrelated to the fixture task.",
    ),
    chunk(
      "noise-dom",
      "dom-structure",
      "Unrelated DOM chunk",
      "fp-newsletter-email and fp-newsletter-submit belong to an unrelated newsletter form.",
    ),
  ];
}

export function correctWorkflow(taskId: string): Workflow {
  if (taskId === "fixture-search-extract") {
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
        { action: "page.waitForText", text: "Floorp OS typed workflows" },
        {
          action: "page.extractText",
          fingerprint: "fp-first-result",
          as: "firstResult",
        },
      ],
    };
  }
  if (taskId === "fixture-workspace-docs") {
    return {
      version: WORKFLOW_VERSION,
      steps: [
        { action: "workspace.switch", workspaceId: RESEARCH_WORKSPACE_ID },
        { action: "tabs.create", url: DOCS_URL },
        { action: "page.assertText", text: "Floorp OS API" },
      ],
    };
  }
  throw new Error(`unknown fixture task: ${taskId}`);
}
