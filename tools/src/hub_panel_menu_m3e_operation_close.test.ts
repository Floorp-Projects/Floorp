// SPDX-License-Identifier: MPL-2.0

import { assertThrows } from "@std/assert";
import { assertHubPanelMenuM3eOperationCloseTerminal } from "./hub_panel_menu_m3e_operation_close.ts";

Deno.test("operation-close oracle requires the actual successful terminal identity and revision", () => {
  const outcome = {
    status: "succeeded",
    expectedRevision: 8,
    revision: 14,
    selectedVersion: "hub-menu-v2",
    selectedDigest: "digest",
    engineSelectedVersion: "digest",
  };
  const valid = { terminal: { settled: true, rejected: false, outcome } };
  assertHubPanelMenuM3eOperationCloseTerminal(valid, 8, "digest");
  for (
    const change of [
      { status: "failed_quarantined" },
      { expectedRevision: 7 },
      { revision: 8 },
      { selectedDigest: "other" },
      { engineSelectedVersion: null },
    ]
  ) {
    assertThrows(() =>
      assertHubPanelMenuM3eOperationCloseTerminal(
        { terminal: { ...valid.terminal, outcome: { ...outcome, ...change } } },
        8,
        "digest",
      )
    );
  }
  for (
    const value of [null, {}, { terminal: { settled: false } }, {
      terminal: { ...valid.terminal, rejected: true },
    }]
  ) {
    assertThrows(() =>
      assertHubPanelMenuM3eOperationCloseTerminal(value, 8, "digest")
    );
  }
});
