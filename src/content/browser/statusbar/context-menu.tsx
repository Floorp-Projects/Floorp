/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { showStatusbar, setShowStatusbar } from "./browser-statusbar";

export function ContextMenu() {
  return (
    <xul:menuitem
      data-l10n-id="status-bar"
      label="Status Bar"
      type="checkbox"
      id="toggle_statusBar"
      checked={showStatusbar()}
      onCommand={() => setShowStatusbar((value) => !value)}
    />
  );
}
