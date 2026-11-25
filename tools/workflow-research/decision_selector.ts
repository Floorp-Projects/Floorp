// SPDX-License-Identifier: MPL-2.0

import type {
  ContextChunk,
  MissingSource,
  Selector,
  SelectorInput,
} from "./types.ts";

export interface DecisionChoiceRequest {
  question: string;
  choices: string[];
  context: string;
}

export interface DecisionChoiceResponse {
  choice: string;
  probability: number;
}

export interface DecisionModelClient {
  choose(request: DecisionChoiceRequest): Promise<DecisionChoiceResponse>;
}

const SOURCE_CHOICES: MissingSource[] = [
  "api-schema",
  "browser-state",
  "dom-structure",
  "page-text",
  "none",
];

function lexicalScore(text: string, chunk: ContextChunk): number {
  const wanted = new Set(
    text.toLowerCase().split(/[^a-z0-9]+/).filter((part) => part.length >= 3),
  );
  const haystack = new Set(
    `${chunk.title}\n${chunk.text}`.toLowerCase().split(/[^a-z0-9]+/),
  );
  let score = 0;
  for (const term of wanted) if (haystack.has(term)) score += 1;
  return score;
}

function topChunks(
  input: SelectorInput,
  source: MissingSource,
  limit: number,
): string[] {
  return input.chunks
    .filter((chunk) =>
      chunk.source === source && !input.acquiredIds.has(chunk.id)
    )
    .sort((a, b) =>
      lexicalScore(input.task.instruction, b) -
      lexicalScore(input.task.instruction, a)
    )
    .slice(0, limit)
    .map((chunk) => chunk.id);
}

/**
 * Adapter for Jev-like typed decision APIs. The model chooses a missing context
 * source; this selector owns the deterministic mapping from that source to chunks.
 */
export function createDecisionModelSelector(
  name: string,
  client: DecisionModelClient,
): Selector {
  return {
    name,
    async select(input) {
      const feedback = input.feedback.at(-1);
      if (!feedback) {
        return {
          chunkIds: [],
          selectorTokens: 0,
          rationale:
            `${name}: no initial acquisition before the first diagnostic`,
        };
      }
      const question = [
        "Which context source is most likely missing for this workflow generation failure?",
        `failure kind: ${feedback.kind}`,
        `failure message: ${feedback.message}`,
      ].join("\n");
      const decision = await client.choose({
        question,
        choices: SOURCE_CHOICES,
        context: input.task.instruction,
      });
      if (
        !SOURCE_CHOICES.includes(decision.choice as MissingSource) ||
        decision.choice === "none"
      ) {
        return {
          chunkIds: [],
          selectorTokens: Math.ceil(question.length / 4),
          rationale:
            `${name}: decision=${decision.choice} probability=${decision.probability}`,
        };
      }
      return {
        chunkIds: topChunks(input, decision.choice as MissingSource, 2),
        selectorTokens: Math.ceil(
          (question.length + input.task.instruction.length) / 4,
        ),
        rationale:
          `${name}: decision=${decision.choice} probability=${decision.probability}`,
      };
    },
  };
}
