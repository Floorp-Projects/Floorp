/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { EXIT_CODE } from "resource://gre/modules/BackgroundTasksManager.sys.mjs";

// Increase the minimum runtime before shutdown
export const backgroundTaskMinRuntimeMS = 2000;

export async function runBackgroundTask() {
  return EXIT_CODE.SUCCESS;
}
