// SPDX-License-Identifier: MPL-2.0
import { runAllTests } from "../../browser-features/chrome/common/panel-sidebar/test/ChromeSidePanelUI.test.tsx";
const target = window as unknown as { __floorpSidePanelUI: unknown };
runAllTests().then(
  (checks) => target.__floorpSidePanelUI = { done: true, checks },
  (error) =>
    target.__floorpSidePanelUI = {
      done: true,
      error: String(error),
      stack: error.stack,
    },
);
