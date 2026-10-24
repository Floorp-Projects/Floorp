// SPDX-License-Identifier: MPL-2.0
import {
  automaId,
  browserflowId,
  uiVisionId,
  webscraperId,
} from "./corpus-automation-recipes.mjs";

export const automationIds = Object.freeze([
  uiVisionId,
  automaId,
  browserflowId,
  webscraperId,
]);

/** @param {string} selection @param {import("./corpus-types.ts").FrozenCorpus} corpus @returns {string[]} */
export function selectAutomationIds(selection, corpus) {
  const ids = selection ? selection.split(",") : [...automationIds];
  if (!ids.every(Boolean) || new Set(ids).size !== ids.length) {
    throw Error("Select a nonempty list of distinct automation extension IDs");
  }
  for (const id of ids) {
    if (!automationIds.includes(id)) {
      throw Error("Unknown frozen automation ID: " + id);
    }
    const records = corpus.extensions.filter((record) => record.id === id);
    if (records.length !== 1 || records[0].status !== "frozen") {
      throw Error(
        "Selected automation ID must have one frozen corpus record: " + id,
      );
    }
  }
  return ids;
}
