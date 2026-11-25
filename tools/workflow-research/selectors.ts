// SPDX-License-Identifier: MPL-2.0

import type {
  ContextChunk,
  FailureFeedback,
  Selector,
  SelectorInput,
  SelectorResult,
} from "./types.ts";

function terms(text: string): Set<string> {
  return new Set(
    text.toLowerCase().split(/[^a-z0-9]+/).filter((part) => part.length >= 3),
  );
}

function lexicalScore(taskText: string, chunk: ContextChunk): number {
  const wanted = terms(taskText);
  const haystack = terms(`${chunk.title}\n${chunk.text}`);
  let score = 0;
  for (const term of wanted) {
    if (haystack.has(term)) score += 1;
  }
  return score;
}

function result(
  chunkIds: string[],
  rationale: string,
  selectorTokens = 12,
): SelectorResult {
  return { chunkIds, selectorTokens, rationale };
}

function unacquired(input: SelectorInput): ContextChunk[] {
  return input.chunks.filter((chunk) => !input.acquiredIds.has(chunk.id));
}

function latestFeedback(feedback: FailureFeedback[]): FailureFeedback | null {
  return feedback.at(-1) ?? null;
}

function lexicalTop(
  input: SelectorInput,
  candidates: ContextChunk[],
  limit: number,
): string[] {
  return [...candidates]
    .sort((a, b) => {
      const scoreDelta = lexicalScore(input.task.instruction, b) -
        lexicalScore(input.task.instruction, a);
      if (scoreDelta !== 0) return scoreDelta;
      return input.chunks.indexOf(a) - input.chunks.indexOf(b);
    })
    .slice(0, limit)
    .map((chunk) => chunk.id);
}

export const minimalSelector: Selector = {
  name: "minimal",
  select(input) {
    return result([], `round ${input.round}: no task context is acquired`);
  },
};

export const staticSelector: Selector = {
  name: "static",
  select(input) {
    if (input.round > 0) {
      return result([], "static retrieval does not react to feedback", 0);
    }
    const fixed = ["api-tabs-core", "noise-dom", "noise-history"]
      .filter((id) =>
        input.chunks.some((chunk) => chunk.id === id) &&
        !input.acquiredIds.has(id)
      );
    return result(fixed, "fixed retrieval set independent of feedback");
  },
};

export const playwrightLikeStaticSelector: Selector = {
  name: "playwright-like-static",
  select(input) {
    if (input.round > 0) {
      return result(
        [],
        "playwright-like baseline has no Floorp-aware repair loop",
        0,
      );
    }
    const candidates = unacquired(input).filter((chunk) =>
      chunk.source === "dom-structure" || chunk.source === "page-text"
    );
    return result(
      lexicalTop(input, candidates, 3),
      "DOM/page-text only; Floorp API schema and browser state are outside the baseline",
    );
  },
};

export const embeddingSelector: Selector = {
  name: "embedding",
  select(input) {
    if (input.round > 0) {
      return result(
        [],
        "embedding baseline retrieves once at generation time",
        0,
      );
    }
    return result(
      lexicalTop(input, unacquired(input), 3),
      "lexical-overlap stand-in for embedding retrieval",
      24,
    );
  },
};

export const ruleSelector: Selector = {
  name: "rule",
  select(input) {
    const feedback = latestFeedback(input.feedback);
    if (!feedback) {
      return result(
        [],
        "initially minimal; wait for diagnostics before acquiring context",
      );
    }
    if (feedback.missingSource === "none") {
      return result([], "feedback does not indicate a missing source", 0);
    }
    const candidates = unacquired(input).filter((chunk) =>
      chunk.source === feedback.missingSource
    );
    return result(
      lexicalTop(input, candidates, 2),
      `rule: ${feedback.kind} feedback -> ${feedback.missingSource}`,
      8,
    );
  },
};

export const oracleSelector: Selector = {
  name: "oracle",
  select(input) {
    const missing = input.task.goldContextIds.filter((id) =>
      !input.acquiredIds.has(id)
    );
    return result(missing, "gold context upper bound", 4);
  },
};

export const randomSelector: Selector = {
  name: "random",
  select(input) {
    const candidates = unacquired(input);
    if (candidates.length === 0) return result([], "no unacquired chunks", 0);
    let seed = input.round + 17;
    for (const character of input.task.id) {
      seed = (seed * 31 + character.charCodeAt(0)) % 9973;
    }
    const first = candidates[seed % candidates.length];
    return result([first.id], "deterministic random-source control", 6);
  },
};

export function selectorByName(name: string): Selector {
  const selectors = [
    minimalSelector,
    staticSelector,
    playwrightLikeStaticSelector,
    embeddingSelector,
    ruleSelector,
    randomSelector,
    oracleSelector,
  ];
  const selector = selectors.find((entry) => entry.name === name);
  if (!selector) {
    throw new Error(
      `unknown selector: ${name}; expected one of ${
        selectors.map((entry) => entry.name).join(", ")
      }`,
    );
  }
  return selector;
}

export function allSelectorNames(): string[] {
  return [
    minimalSelector.name,
    staticSelector.name,
    playwrightLikeStaticSelector.name,
    embeddingSelector.name,
    ruleSelector.name,
    randomSelector.name,
    oracleSelector.name,
  ];
}
