// SPDX-License-Identifier: MPL-2.0
import type { TestWindow } from "../../../browser-features/chrome/common/workspaces/test/workspace-last-tab-test-types.ts";

export type ProductionWorkspaceWindow = TestWindow & {
  OpenBrowserWindow(): TestWindow;
};
