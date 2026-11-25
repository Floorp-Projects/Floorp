// SPDX-License-Identifier: MPL-2.0

import { assert, assertEquals, assertStringIncludes } from "@std/assert";
import { chunkOpenApiSpec } from "./api_context.ts";
import { workflowToTypeScriptArtifact } from "./artifact.ts";
import { executeWorkflow } from "./executor.ts";
import { createFixtureBrowser, runExperiment } from "./experiment.ts";
import {
  correctWorkflow,
  fixtureContextChunks,
  fixtureTasks,
  RESEARCH_WORKSPACE_ID,
} from "./fixtures.ts";
import { createDecisionModelSelector } from "./decision_selector.ts";
import {
  executeWorkflowOnFloorpOs,
  type FloorpRequester,
} from "./floorp_executor.ts";
import { workspaceStateChunk } from "./floorp_os.ts";
import { scriptedSmallModelGenerator } from "./generator.ts";
import {
  minimalSelector,
  oracleSelector,
  playwrightLikeStaticSelector,
  ruleSelector,
} from "./selectors.ts";
import { validateWorkflow } from "./workflow.ts";

function task(id: string) {
  const selected = fixtureTasks().find((entry) => entry.id === id);
  if (!selected) throw new Error(`missing fixture task ${id}`);
  return selected;
}

Deno.test("validateWorkflow accepts the typed fixture artifact", () => {
  const result = validateWorkflow(correctWorkflow("fixture-search-extract"));
  assertEquals(result.diagnostics, []);
  assert(result.workflow !== null);
});

Deno.test("validateWorkflow maps unknown actions to missing API schema context", () => {
  const result = validateWorkflow({
    version: "floorp-workflow/v1",
    steps: [{ action: "browser.open", url: "https://fixture.local/search" }],
  });
  assertEquals(result.workflow, null);
  assertEquals(result.diagnostics[0].code, "WFC2304");
  assertEquals(result.diagnostics[0].missingSource, "api-schema");
});

Deno.test("artifact serializer emits a satisfies Workflow object literal", () => {
  const artifact = workflowToTypeScriptArtifact(
    correctWorkflow("fixture-search-extract"),
  );
  assertStringIncludes(artifact, "satisfies Workflow");
  assertStringIncludes(artifact, '"action": "tabs.create"');
});

Deno.test("executor completes the correct search workflow", () => {
  const selected = task("fixture-search-extract");
  const result = executeWorkflow(
    correctWorkflow(selected.id),
    createFixtureBrowser(),
    selected.oracle,
  );
  assertEquals(result.success, true);
  assertEquals(result.extracted.firstResult, "Floorp OS typed workflows");
});

Deno.test("executor completes the correct workspace workflow", () => {
  const selected = task("fixture-workspace-docs");
  const result = executeWorkflow(
    correctWorkflow(selected.id),
    createFixtureBrowser(),
    selected.oracle,
  );
  assertEquals(result.success, true);
  assertEquals(result.activeWorkspaceId, RESEARCH_WORKSPACE_ID);
});

Deno.test("rule selector uses failure feedback to reach success", async () => {
  const selected = task("fixture-workspace-docs");
  const log = await runExperiment(
    selected,
    fixtureContextChunks(),
    ruleSelector,
    scriptedSmallModelGenerator,
  );
  assertEquals(log.success, true);
  assert(log.rounds.length > 1);
  assert(
    log.rounds.some((round) => round.contextIds.includes("state-workspaces")),
  );
});

Deno.test("minimal and playwright-like baselines fail the Floorp workspace task", async () => {
  const selected = task("fixture-workspace-docs");
  for (const selector of [minimalSelector, playwrightLikeStaticSelector]) {
    const log = await runExperiment(
      selected,
      fixtureContextChunks(),
      selector,
      scriptedSmallModelGenerator,
    );
    assertEquals(log.success, false);
    assertEquals(log.finalFailureKind, "schema");
  }
});

Deno.test("oracle selector succeeds in the first round", async () => {
  const selected = task("fixture-search-extract");
  const log = await runExperiment(
    selected,
    fixtureContextChunks(),
    oracleSelector,
    scriptedSmallModelGenerator,
  );
  assertEquals(log.success, true);
  assertEquals(log.rounds.length, 1);
});

Deno.test("OpenAPI chunker extracts operation-level Floorp API context", () => {
  const yaml = `openapi: 3.0.3
paths:
  /workspaces:
    get:
      summary: List all workspaces
      operationId: listWorkspaces
      description: Returns workspace metadata
      responses:
        "200":
          description: ok
  /workspaces/{workspaceId}/switch:
    post:
      summary: Switch to specific workspace
      operationId: switchToWorkspace
      description: Switches workspace
components:
  schemas: {}
`;
  const chunks = chunkOpenApiSpec(yaml);
  assertEquals(chunks.map((chunk) => chunk.id), [
    "api-listWorkspaces",
    "api-switchToWorkspace",
  ]);
  assertStringIncludes(chunks[1].text, "workspaceId");
});

Deno.test("workspaceStateChunk summarizes live workspace state", () => {
  const chunk = workspaceStateChunk({
    currentId: RESEARCH_WORKSPACE_ID,
    workspaces: [
      { id: RESEARCH_WORKSPACE_ID, name: "Research", isSelected: true },
    ],
  });
  assertStringIncludes(chunk.text, `Research ${RESEARCH_WORKSPACE_ID}`);
  assertStringIncludes(chunk.text, "selected");
});

interface RecordedRequest {
  path: string;
  method: string;
  body: unknown;
}

function fakeFloorpClient(): {
  client: FloorpRequester;
  requests: RecordedRequest[];
} {
  const requests: RecordedRequest[] = [];
  return {
    requests,
    client: {
      request(path, init = {}) {
        requests.push({
          path,
          method: init.method ?? "GET",
          body: typeof init.body === "string" ? JSON.parse(init.body) : null,
        });
        if (path === "/tabs/instances") {
          return Promise.resolve({
            status: 200,
            body: { instanceId: "tab-1" },
          });
        }
        if (path.includes("/elementByText")) {
          return Promise.resolve({
            status: 200,
            body: { element: "<h1>Floorp OS API</h1>" },
          });
        }
        if (path.includes("/elementTextContent")) {
          return Promise.resolve({
            status: 200,
            body: { text: "Floorp OS typed workflows" },
          });
        }
        if (path.includes("/workspaces/")) {
          return Promise.resolve({
            status: 200,
            body: { success: true, workspaceId: RESEARCH_WORKSPACE_ID },
          });
        }
        return Promise.resolve({ status: 200, body: { ok: true } });
      },
    },
  };
}

Deno.test("Floorp OS executor maps workflow actions to tab API calls", async () => {
  const { client, requests } = fakeFloorpClient();
  const selected = task("fixture-search-extract");
  const result = await executeWorkflowOnFloorpOs(
    correctWorkflow(selected.id),
    client,
    selected.oracle,
    { sleep: () => Promise.resolve() },
  );
  assertEquals(result.success, true);
  assertEquals(result.extracted.firstResult, "Floorp OS typed workflows");
  assertEquals(requests[0].path, "/tabs/instances");
  assertEquals(requests[0].method, "POST");
  assert(requests.some((entry) => entry.path.includes("/fillForm")));
  assert(requests.some((entry) => entry.path.includes("/elementTextContent")));
  assertEquals(requests.at(-1)?.method, "DELETE");
});

Deno.test("Floorp OS executor maps workspace actions and cleans up the tab", async () => {
  const { client, requests } = fakeFloorpClient();
  const selected = task("fixture-workspace-docs");
  const result = await executeWorkflowOnFloorpOs(
    correctWorkflow(selected.id),
    client,
    selected.oracle,
    { sleep: () => Promise.resolve() },
  );
  assertEquals(result.success, true);
  assertEquals(result.activeWorkspaceId, RESEARCH_WORKSPACE_ID);
  assert(
    requests[0].path.includes(`/workspaces/${RESEARCH_WORKSPACE_ID}/switch`),
  );
  assertEquals(requests.at(-1)?.method, "DELETE");
});

Deno.test("decision-model selector adapter can drive source acquisition", async () => {
  const selector = createDecisionModelSelector("mock-jev", {
    choose(request) {
      assert(request.choices.includes("browser-state"));
      return Promise.resolve({ choice: "browser-state", probability: 0.91 });
    },
  });
  const selected = task("fixture-workspace-docs");
  const result = await selector.select({
    task: selected,
    chunks: fixtureContextChunks(),
    acquiredIds: new Set<string>(),
    feedback: [{
      kind: "runtime",
      message: "workspace not found: research",
      missingSource: "none",
      round: 0,
    }],
    round: 1,
  });
  assert(result.chunkIds.includes("state-workspaces"));
  assertStringIncludes(result.rationale, "probability=0.91");
});
