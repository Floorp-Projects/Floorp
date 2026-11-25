// SPDX-License-Identifier: MPL-2.0

import { executeWorkflow, MiniBrowser } from "./executor.ts";
import {
  fixturePages,
  HOME_URL,
  MAIN_WORKSPACE_ID,
  RESEARCH_WORKSPACE_ID,
} from "./fixtures.ts";
import type {
  ContextChunk,
  ExecutionResult,
  ExperimentLog,
  FailureFeedback,
  ResearchTask,
  RoundLog,
  Selector,
  WorkflowGenerator,
} from "./types.ts";
import { firstValidationDiagnostic, validateWorkflow } from "./workflow.ts";

function schemaFailure(
  message: string,
  missingSource: ExecutionResult["missingSource"],
): ExecutionResult {
  return {
    success: false,
    failureKind: "schema",
    message,
    missingSource,
    extracted: {},
    finalUrl: HOME_URL,
    activeWorkspaceId: MAIN_WORKSPACE_ID,
  };
}

export function createFixtureBrowser(): MiniBrowser {
  return new MiniBrowser({
    pages: fixturePages(),
    initialUrl: HOME_URL,
    workspaceIds: [MAIN_WORKSPACE_ID, RESEARCH_WORKSPACE_ID],
    initialWorkspaceId: MAIN_WORKSPACE_ID,
  });
}

export async function runExperiment(
  task: ResearchTask,
  chunks: ContextChunk[],
  selector: Selector,
  generator: WorkflowGenerator,
  maxRounds = 5,
): Promise<ExperimentLog> {
  const acquiredIds = new Set<string>();
  const feedback: FailureFeedback[] = [];
  const rounds: RoundLog[] = [];
  let selection = await selector.select({
    task,
    chunks,
    acquiredIds,
    feedback,
    round: 0,
  });

  for (let round = 0; round < maxRounds; round++) {
    for (const id of selection.chunkIds) acquiredIds.add(id);
    const contexts = chunks.filter((chunk) => acquiredIds.has(chunk.id));
    const contextTokens = contexts.reduce(
      (sum, chunk) => sum + chunk.tokenEstimate,
      0,
    );

    const generation = generator.generate({ task, contexts, feedback, round });
    const validation = validateWorkflow(generation.workflow);
    let execution: ExecutionResult;
    if (validation.workflow) {
      execution = executeWorkflow(
        validation.workflow,
        createFixtureBrowser(),
        task.oracle,
      );
    } else {
      const diagnostic = firstValidationDiagnostic(validation.diagnostics);
      execution = schemaFailure(
        diagnostic?.message ?? "workflow schema validation failed",
        diagnostic?.missingSource ?? "api-schema",
      );
    }

    rounds.push({
      round,
      contextIds: contexts.map((chunk) => chunk.id),
      contextTokens,
      selectorTokens: selection.selectorTokens,
      generationTokens: generation.generationTokens,
      validationDiagnostics: validation.diagnostics,
      execution,
      generatorNotes: generation.notes,
    });

    if (execution.success) break;
    if (execution.failureKind === "none") break;
    feedback.push({
      kind: execution.failureKind,
      message: execution.message,
      missingSource: execution.missingSource,
      round,
    });

    const nextSelection = await selector.select({
      task,
      chunks,
      acquiredIds,
      feedback,
      round: round + 1,
    });
    rounds[round].selectorTokens += nextSelection.selectorTokens;
    const hasNewContext = nextSelection.chunkIds.some((id) =>
      !acquiredIds.has(id)
    );
    if (!hasNewContext) break;
    selection = nextSelection;
  }

  const totalContextTokens = chunks
    .filter((chunk) => acquiredIds.has(chunk.id))
    .reduce((sum, chunk) => sum + chunk.tokenEstimate, 0);
  const totalSelectorTokens = rounds.reduce(
    (sum, round) => sum + round.selectorTokens,
    0,
  );
  const totalGenerationTokens = rounds.reduce(
    (sum, round) => sum + round.generationTokens,
    0,
  );
  const last = rounds.at(-1);

  return {
    taskId: task.id,
    selector: selector.name,
    generator: generator.name,
    success: last?.execution.success ?? false,
    rounds,
    totalContextTokens,
    totalSelectorTokens,
    totalGenerationTokens,
    totalTokens: totalSelectorTokens + totalGenerationTokens,
    finalFailureKind: last?.execution.failureKind ?? "none",
    finalMessage: last?.execution.message ?? "no rounds executed",
  };
}

export function summarizeExperiments(logs: ExperimentLog[]): string {
  const lines = [
    "| task | selector | success | rounds | context tokens | selector tokens | generation tokens | total tokens | final failure |",
    "|---|---|---:|---:|---:|---:|---:|---:|---|",
  ];
  for (const log of logs) {
    lines.push(
      `| ${log.taskId} | ${log.selector} | ${
        log.success ? "yes" : "no"
      } | ${log.rounds.length} | ${log.totalContextTokens} | ${log.totalSelectorTokens} | ${log.totalGenerationTokens} | ${log.totalTokens} | ${log.finalFailureKind}: ${
        log.finalMessage.replaceAll("|", "\\|")
      } |`,
    );
  }
  return lines.join("\n");
}
