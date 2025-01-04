/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import type { TWorkspacesServicesConfigs } from "./utils/type.js";

export const oldObjectConfigs: TWorkspacesServicesConfigs = {
  manageOnBms: Services.prefs.getBoolPref(
    "floorp.browser.workspace.manageOnBMS",
    false,
  ),
  showWorkspaceNameOnToolbar: Services.prefs.getBoolPref(
    "floorp.browser.workspace.showWorkspaceName",
    true,
  ),
  closePopupAfterClick: Services.prefs.getBoolPref(
    "floorp.browser.workspace.closePopupAfterClick",
    false,
  ),
};

export const getOldConfigs = JSON.stringify(oldObjectConfigs);
